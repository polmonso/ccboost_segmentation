
#include "CvlabPanel.h"
#include "ui_CvlabPanel.h"
#include "Label.h"
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
using namespace ESPINA::CCB;

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

  }

  void setAutoSegmentWidgetsEnabled(bool value)
  {
    previewGroup        ->setEnabled(value);
    previewVisibility   ->setEnabled(value);
    previewOpacity      ->setEnabled(value);
    updateTrainer       ->setEnabled(value);
    createSegmentations ->setEnabled(value);
  }

public:
  QAction *SingleTrainer;
  QAction *MultiTrainer;
};

//------------------------------------------------------------------------
//------------------------------------------------------------------------
CvlabPanel::CvlabPanel(CcboostSegmentationPlugin* manager,
                                   ModelAdapterSPtr    model,
                                   ViewManagerSPtr     viewManager,
                                   ModelFactorySPtr    factory,
                                   QUndoStack*         undoStack,
                                   QWidget*            parent)
: m_gui(new GUI())
, m_manager(manager)
, m_model(model)
, m_viewManager(viewManager)
, m_factory(factory)
, m_undoStack(undoStack)
, m_pendingFeaturesChannel(nullptr)
{
  qRegisterMetaType<CcboostAdapter::FloatTypeImage::Pointer>("CcboostAdapter::FloatTypeImage::Pointer");

  setObjectName("CvlabPanel");

  setWindowTitle(tr("Refined Automatic Segmentation"));

  setWidget(m_gui);


  connect(m_viewManager.get(), SIGNAL(activeChannelChanged(ChannelAdapterPtr)),
          this,                SLOT(onActiveChannelChanged()));

  connect(m_manager,           SIGNAL(featuresChanged()),
          this,                SLOT(onFeaturesChanged()));

  bindGUISignals();

}

//------------------------------------------------------------------------
CvlabPanel::~CvlabPanel()
{
  delete m_gui;
}

//------------------------------------------------------------------------
void CvlabPanel::reset()
{
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
    //FIXME assert we have a volume
//  Q_ASSERT(m_activeMRAS);

  if (!m_preview)
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

  if (!m_preview)
  {
    double alpha = opacity/100.0;
    m_preview->setOpacity(alpha);
    m_viewManager->updateViews();
  }
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

    m_preview->update();
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
*/
void CvlabPanel::updatePreview()
{
  //TODO properly check that volume exists?
  if (m_volume)
  {

    m_timer.start();

  }
}

//------------------------------------------------------------------------
void CvlabPanel::extractSegmentations()
{
  if (m_volume)
  {
    //TODO create segmentations
    //m_manager->createModelSegmentations(m_activeMRAS);
    m_gui->updateTrainer->setEnabled(false);
    m_gui->createSegmentations->setEnabled(false);
  }
}

//------------------------------------------------------------------------
void CvlabPanel::onFinished()
{
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
