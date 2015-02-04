
#include "CvlabPanel.h"
#include "LabelWidget.h"
#include "FeaturesDialog.h"
#include "ui_CvlabPanel.h"
#include "Label.h"
#include <Tasks/SegmentationExtractor.h>
#include <Settings/SettingsDialog.h>
#include <Filters/SourceFilter.h>
#include <GUI/Model/Utils/QueryAdapter.h>
#include <GUI/Dialogs/CategorySelectorDialog.h>
#include <GUI/Dialogs/DefaultDialogs.h>
#include <Undo/AddSegmentations.h>
#include <Core/Utils/AnalysisUtils.h>
#include <Core/Analysis/Data/Volumetric/SparseVolumeUtils.h>
#include <Core/Analysis/Data/Volumetric/SparseVolume.hxx>
#include <Core/IO/ZipUtils.h>

#include <algorithm>

// Qt headers.
#include <QPushButton>
#include <QDebug>
#include <QUndoCommand>
#include <QStackedLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>

using namespace ESPINA;
using namespace ESPINA::GUI;
using namespace ESPINA::IO;
using namespace ESPINA::RAS;

//------------------------------------------------------------------------
class CvlabPanel::GUI
: public QWidget
, public Ui::CvlabPanel
{
public:
  GUI()
  {
    setupUi(this);

    updateTrainer->setCheckable(false);
    updateTrainer->setIconSize(QSize(20, 20));

    SingleTrainer = new QAction(QIcon(":/ras/single_trainer.svg"), tr("Single Trainer"), this);
    updateTrainer->addAction(SingleTrainer);

    MultiTrainer = new QAction(QIcon(":/ras/multi_trainer.svg"), tr("Multiple Trainers"), this);
    updateTrainer->addAction(MultiTrainer);

    updateTrainer->setButtonAction(SingleTrainer);

    autoSegmenterTable->setSortingEnabled(true);
  }

  void setAutoSegmentWidgetsEnabled(bool value)
  {
    previewGroup        ->setEnabled(value);
    previewVisibility   ->setEnabled(value);
    previewOpacity      ->setEnabled(value);
    labelGroup          ->setEnabled(value);
    addLabels           ->setEnabled(value);
    loadLabels          ->setEnabled(value);
    exportLabels        ->setEnabled(value);
    labels              ->setEnabled(value);
    updateTrainer       ->setEnabled(value);
    createSegmentations ->setEnabled(value);
    deleteAutoSegmenter ->setEnabled(value);
  }

public:
  QAction *SingleTrainer;
  QAction *MultiTrainer;
};


//------------------------------------------------------------------------
//------------------------------------------------------------------------
class CvlabPanel::TableModel
: public QAbstractTableModel
{
public:
  TableModel(AutoSegmentManager *manager)
  : m_manager(manager)
  {}

  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const
  { return m_manager->mrasList().size(); }

  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const
  { return 3; }

  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

  virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

  virtual Qt::ItemFlags flags(const QModelIndex& index) const;

  MultiRegularizerAutoSegmenterSPtr mras(const QModelIndex &index) const
  { return m_manager->mrasList()[index.row()]; }

private:
  bool changeId(MultiRegularizerAutoSegmenterSPtr editedMRAS, QString requestedId)
  {
    bool alreadyUsed = false;
    bool accepted    = true;

    for (auto mras : m_manager->mrasList())
    {
      if (mras != editedMRAS)
        alreadyUsed |= mras->id() == requestedId;
    }

    if (alreadyUsed)
    {
      QString suggestedId = SuggestId(requestedId, m_manager->mrasList());
      while (accepted && suggestedId != requestedId)
      {
        requestedId = QInputDialog::getText(nullptr,
                                            tr("Id already used"),
                                            tr("Introduce new id (or accept suggested one)"),
                                            QLineEdit::Normal,
                                            suggestedId,
                                            &accepted);
        suggestedId = SuggestId(requestedId, m_manager->mrasList());
      }
    }

    if (accepted)
      editedMRAS->setId(requestedId);

    return accepted;
  }

private:
  AutoSegmentManager* m_manager;
};


//------------------------------------------------------------------------
QVariant CvlabPanel::TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (Qt::DisplayRole == role && Qt::Horizontal == orientation)
  {
    switch (section)
    {
      case 0:
        return tr("Id"); break;
      case 1:
        return tr("Channel"); break;
      case 2:
        return tr("Features");
    }
  }

  return QAbstractItemModel::headerData(section, orientation, role);
}

//------------------------------------------------------------------------
QVariant CvlabPanel::TableModel::data(const QModelIndex& index, int role) const
{
  auto autoSegmenter = mras(index);
  int  c  = index.column();

  if (0 == c)
  {
    if (Qt::DisplayRole == role || Qt::EditRole == role)
    {
      return autoSegmenter->id();
    } else if (Qt::CheckStateRole == role)
    {
      return QVariant();// autoSegmenter->isVisible()?Qt::Checked:Qt::Unchecked;
    }
  } else if (1 == c && Qt::DisplayRole == role)
  {
    return autoSegmenter->channel()->data(role);
  } else if (2 == c && Qt::DisplayRole == role)
  {
    return autoSegmenter->features().Path;
  }

  return QVariant();
}

//------------------------------------------------------------------------
bool CvlabPanel::TableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (Qt::EditRole == role)
  {
    auto item = mras(index);

    return changeId(item, value.toString().trimmed());

  } else if (Qt::CheckStateRole == role)
  {
    auto item = mras(index);

    //cf->setVisible(value.toBool());

    return true;
  }

  return QAbstractItemModel::setData(index, value, role);
}

//------------------------------------------------------------------------
Qt::ItemFlags CvlabPanel::TableModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = QAbstractItemModel::flags(index);

  if (0 == index.column())
  {
    f = f | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled  | Qt::ItemIsUserCheckable;
  }

  return f;
}


//------------------------------------------------------------------------
//------------------------------------------------------------------------
CvlabPanel::CvlabPanel(CcboostSegmentationPlugin* manager,
                                   ModelAdapterSPtr    model,
                                   ViewManagerSPtr     viewManager,
                                   ModelFactorySPtr    factory,
                                   QUndoStack*         undoStack,
                                   QWidget*            parent)
: m_gui(new GUI())
, m_tableModel(new TableModel(manager))
, m_manager(manager)
, m_model(model)
, m_viewManager(viewManager)
, m_factory(factory)
, m_undoStack(undoStack)
, m_pendingFeaturesChannel(nullptr)
{
  qRegisterMetaType<LabelImageType::Pointer>("LabelImageType::Pointer");
  qRegisterMetaType<RealVectorImageType::Pointer>("RealVectorImageType::Pointer");
  //qRegisterMetaType<QList<Label *> >("QList<Label*>");

  setObjectName("CvlabPanel");

  setWindowTitle(tr("Refined Automatic Segmentation"));

  setWidget(m_gui);

  m_gui->autoSegmenterTable->setModel(m_tableModel);

  setProcessingStatus(false);

  connect(m_viewManager.get(), SIGNAL(activeChannelChanged(ChannelAdapterPtr)),
          this,                SLOT(onActiveChannelChanged()));

  connect(m_manager,           SIGNAL(featuresChanged()),
          this,                SLOT(onFeaturesChanged()));

  bindGUISignals();


  removeActiveLabels();
}

//------------------------------------------------------------------------
CvlabPanel::~CvlabPanel()
{
  delete m_gui;
}

//------------------------------------------------------------------------
void CvlabPanel::reset()
{
    m_trainingWidgets.clear();
    m_previews.clear();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::displaySettingsDialog()
{
  SettingsDialog dialog;

  dialog.exec();
}


//------------------------------------------------------------------------
void CvlabPanel::createAutoSegmenter()
{
}

//------------------------------------------------------------------------
void CvlabPanel::deleteAutoSegmenter()
{
}

//------------------------------------------------------------------------
void CvlabPanel::changePreviewVisibility(bool visible)
{
    //assert we have a volume
//  Q_ASSERT(m_activeMRAS);

  if (!m_preview.isEmpty())
  {
      m_preview->setVisibility(visible);

    m_viewManager->updateViews();
  }

  if (visible) {
    m_gui->previewVisibility->setIcon(QIcon(":/espina/show_all.svg"));
  } else
  {
    m_gui->previewVisibility->setIcon(QIcon(":/espina/hide_all.svg"));
  }
}

//------------------------------------------------------------------------
void CvlabPanel::changePreviewOpacity(int opacity)
{
    //assert we have a volume
//  Q_ASSERT(m_activeMRAS);

  if (!m_preview.isEmpty())
  {
    double alpha = opacity/100.0;
    m_preview->setOpacity(alpha);
    m_viewManager->updateViews();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::addLabels()
{
  auto selection = m_viewManager->selection()->segmentations();

  if (selection.isEmpty()) {
    CategorySelectorDialog dialog(m_model);

    if (dialog.exec() == QDialog::Accepted) {
      for(auto category : dialog.categories())
      {
        Label label = Label(category);
        if (!activeTrainingWidgets().contains(label))
        {
          showLabelControls(m_activeMRAS, label);
        }
      }
    }
  } else
  {
    if (useSelectionForTraining())
    {
      for(auto segmentation : selection)
      {
        Label label = Label(segmentation->category());
        if (!activeTrainingWidgets().contains(label))
        {
          showLabelControls(m_activeMRAS, label);
        }

        auto labelWidet = activeTrainingWidget(label);
        auto volume     = volumetricData(segmentation->output());

        labelWidet->addTrainingVolume(volume);
      }
    }
  }

  updateTrainingWidgetState();
}

//------------------------------------------------------------------------
void CvlabPanel::loadLabels()
{
  if (m_activeMRAS)
  {
    auto title   = tr("Import Training Volumes");
    auto filter  = tr("Training Volumes (*.tv)");

    auto input = DefaultDialogs::OpenFile(title, filter);

    if (input.isEmpty())
    {
      return;
    }

    QFileInfo importPath(input);


    QuaZip zip(importPath.absoluteFilePath());
    if (!zip.open(QuaZip::mdUnzip))
    {
      qWarning() << "Failed to open zip file";
      return;
    }

    removeActiveLabels();

    m_trainingWidgets[m_activeMRAS].clear();


    TemporalStorageSPtr storage{new TemporalStorage()};

    bool hasFile = zip.goToFirstFile();
    while (hasFile)
    {
      QString file = zip.getCurrentFileName();

      auto currentFile = ZipUtils::readCurrentFileFromZip(zip);
      storage->saveSnapshot(SnapshotData(file, currentFile));

      hasFile = zip.goToNextFile();
    }

    QDir dir = storage->absoluteFilePath("");
    qDebug() << dir.absolutePath();
    for (auto entry : dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot))
    {
      qDebug() << entry.absoluteFilePath();
      auto name     = entry.baseName();
      auto category = m_model->classification()->category(name);
      Label label(category);

      SparseVolumeSPtr trainingVolume{new SparseVolume<itkVolumeType>()};
      trainingVolume->fetchData(storage, name + "/", "0");

      label.addTrainingVolume(trainingVolume);

      createLabelEntry(m_activeMRAS, label);
    }

    showActiveLabels();
    updateTrainingWidgetState();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::exportLabels()
{
  auto title   = tr("Export Training Volumes");
  auto suggest = tr("%1 - Training Volume.tv").arg(m_activeMRAS->id());
  auto filter  = tr("Training Volumes (*.tv)");
  auto suffix  = ".tv";

  QFileInfo exportPath = DefaultDialogs::SaveFile(title, filter, "",suffix, suggest);

  QuaZip zip(exportPath.absoluteFilePath());
  if (!zip.open(QuaZip::mdCreate))
  {
    qWarning() << "Failed to create zip file";
    return;
  }

  auto channel = m_activeMRAS->channel();
  auto output  = channel->output();
  for (auto widget : activeTrainingWidgets())
  {
    SparseVolumeSPtr volume{new SparseVolume<itkVolumeType>(Bounds(), output->spacing(), channel->position())};

    for (auto trainingVolume : widget->label().trainingVolumes())
    {
      expandAndDraw<itkVolumeType>(volume, trainingVolume->itkImage());
    }

    auto labelPath = widget->label().name() + "/";

    TemporalStorageSPtr storage{new TemporalStorage()};
    auto snapshots = volume->snapshot(storage, labelPath, "0");

    for (auto snapshot : snapshots)
    {
      ZipUtils::AddFileToZip(snapshot.first, snapshot.second, zip);
    }
  }

  zip.close();
  if (zip.getZipError() != UNZ_OK)
  {
    qWarning() << "Failed to close zip file";
  }
}

//------------------------------------------------------------------------
void CvlabPanel::removeLabel(Label label)
{
  for (int i = 0; i < m_gui->labels->count(); ++i)
  {
    if (m_gui->labels->itemText(i) == label.name())
    {
      m_gui->labels->removeItem(i);

      delete activeTrainingWidget(label);
      m_trainingWidgets[m_activeMRAS].remove(label);

      m_activeMRAS->setTrainingLabels(m_trainingWidgets[m_activeMRAS].keys());
      break;
    }
  }

  updateTrainingWidgetState();
}

//------------------------------------------------------------------------
void CvlabPanel::updateProgress(int progress)
{
  //m_gui->progressBar->setVisible(progress > 0 && progress < 100);
  auto text = tr("Update Preview: %1%").arg(progress);
  m_gui->progressBar->setFormat(text);
  m_gui->progressBar->setValue(progress);

  if (m_timer.elapsed() > 1000)
  {
    for (auto preview : activePreviews())
    {
      preview->update();
    }
    m_viewManager->updateViews();
    m_timer.restart();
  }
}


//------------------------------------------------------------------------
void CvlabPanel::onActiveChannelChanged()
{
  bool channelAvailable = m_viewManager->activeChannel() != nullptr;

  //hide preview?
}

//------------------------------------------------------------------------
void CvlabPanel::abort()
{

    //discard segmentation and destroy data
}

//------------------------------------------------------------------------
/**
* @brief CvlabPanel::updatePreview
*
* Updates the preview by reloading the Volume
*
* @deprecated It is probably unnecessary if we assume there's only one preview
*             and it is created/destroyed or updated on demand
*
*/
void CvlabPanel::updatePreview()
{
  //TODO properly check that volume exists?
  if (volume)
  {
    setProcessingStatus(true);

    updateActivePreviews();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::updateWidgetsState()
{
  bool mrasRemaining = m_volume exists;

  m_gui->createAutoSegmenter->setEnabled(!m_model->channels().isEmpty());
  m_gui->deleteAutoSegmenter->setEnabled(mrasRemaining);
  updateActiveWidgetsState();
}

//------------------------------------------------------------------------
void CvlabPanel::extractSegmentations()
{
  if (m_activeMRAS)
  {
    m_manager->createModelSegmentations(m_activeMRAS);
    m_gui->updateTrainer->setEnabled(false);
    m_gui->createSegmentations->setEnabled(false);
  }
}

//------------------------------------------------------------------------
void CvlabPanel::onFinished()
{
  setProcessingStatus(false);
}

//------------------------------------------------------------------------
void CvlabPanel::bindGUISignals()
{
  connect(m_gui->previewVisibility,  SIGNAL(toggled(bool)),
          this,                      SLOT(changePreviewVisibility(bool)));

  connect(m_gui->previewOpacity,     SIGNAL(valueChanged(int)),
          this,                      SLOT(changePreviewOpacity(int)));

  connect(m_gui->createSegmentations,SIGNAL(clicked(bool)),
          this,                      SLOT(extractSegmentations()));

}
