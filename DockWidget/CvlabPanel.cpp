
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

#include <Support/Settings/EspinaSettings.h>

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
  }

  void setAutoSegmentWidgetsEnabled(bool value)
  {
    previewGroup        ->setEnabled(value);
    previewVisibility   ->setEnabled(value);
    previewOpacity      ->setEnabled(value);
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
: m_manager(manager)
, m_gui(new GUI())
, m_model(model)
, m_viewManager(viewManager)
, m_factory(factory)
, m_undoStack(undoStack)
, m_pendingFeaturesChannel(nullptr)
, m_threshold(0.50)
{
  qRegisterMetaType<CcboostAdapter::FloatTypeImage::Pointer>("CcboostAdapter::FloatTypeImage::Pointer");

  setObjectName("CvlabPanel");

  setWindowTitle(tr("Refined Automatic Segmentation"));

  setWidget(m_gui);

  connect(m_viewManager.get(), SIGNAL(activeChannelChanged(ChannelAdapterPtr)),
          this,                SLOT(onActiveChannelChanged()));

//  connect(m_manager,           SIGNAL(volumeChanged()),
//          this,                SLOT(onVolumeOverlayChanged()));

  connect(m_manager,           SIGNAL(predictionChanged(QString)),
               this,                SLOT(volumeOverlayChanged(QString)));

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

void CvlabPanel::setVolume(QFileInfo volumeFile){
    typedef itk::ImageFileReader< CcboostAdapter::FloatTypeImage > fReaderType;
    fReaderType::Pointer freader = fReaderType::New();
    freader->SetFileName(volumeFile.absoluteFilePath().toStdString());
    try {
        freader->Update();
        m_volume = freader->GetOutput();

        m_preview = PreviewWidgetSPtr{new PreviewWidget()};

        //for some reason, addWidget destroys the m_volume if we set it before.
        m_viewManager->addWidget(m_preview);

        m_preview->setPreviewVolume(m_volume);
        m_preview->setThreshold(m_threshold);
        m_preview->setVisibility(true); //visibility does not work

        m_gui->createSegmentations->setEnabled(true);
        m_gui->previewOpacity->setEnabled(true);
        m_gui->previewVisibility->setEnabled(true);
        m_gui->previewGroup->setEnabled(true);
        m_gui->abortAutoSegmenter->setEnabled(true);

    } catch( itk::ExceptionObject & err ) {
          std::cerr << "ExceptionObject caught !" << std::endl;
          std::cerr << err << std::endl;
    }

}

/**
* @brief CvlabPanel::updatePreview
*
* Updates the preview by reloading the Volume
*
* @deprecated It is probably unnecessary if we assume there's only one preview
*             and it is created/destroyed or updated on demand
*/
void CvlabPanel::volumeOverlayChanged(QString volumeFilePath)
{
    QFileInfo volumeFileInfo(volumeFilePath);
    setVolume(volumeFileInfo);
    m_preview->update();
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
void CvlabPanel::openOverlay(){

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setWindowTitle(tr("Open Volume Image Overlay"));
    fileDialog.setFilter(tr("Volume Image Files (*.tif *.tiff *.mha)"));

    if (fileDialog.exec() != QDialog::Accepted)
           return;

    if (m_preview) m_preview.reset();

    QString file = fileDialog.selectedFiles().first();
    QFileInfo volumeFileInfo(file);
    setVolume(volumeFileInfo);

}

//------------------------------------------------------------------------
void CvlabPanel::changePreviewVisibility(bool visible)
{
    //FIXME assert we have a volume
  Q_ASSERT(m_volume);

  if (m_preview)
  {
      m_preview->setVisibility(visible);

      m_viewManager->updateViews();
  }

  if (visible)
  {
    m_gui->previewVisibility->setIcon(QIcon(":/espina/show_all.svg"));
  }
  else
  {
    m_gui->previewVisibility->setIcon(QIcon(":/espina/hide_all.svg"));
  }
}

//------------------------------------------------------------------------
void CvlabPanel::changePreviewOpacity(int opacity)
{
    //assert we have a volume
  Q_ASSERT(m_volume);

  if (m_preview)
  {
    double alpha = opacity/100.0;
    m_preview->setOpacity(alpha);
    m_viewManager->updateViews();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::changePreviewThreshold(int pthreshold)
{
    //assert we have a volume
  Q_ASSERT(m_volume);

  if (m_preview)
  {
    m_threshold = pthreshold/100.0;
    m_preview->setThreshold(m_threshold);
    m_viewManager->updateViews();
  }
}

//------------------------------------------------------------------------
void CvlabPanel::onActiveChannelChanged()
{
  bool channelAvailable = m_viewManager->activeChannel() != nullptr;

  //hide preview?
}

//------------------------------------------------------------------------
void CvlabPanel::abort(){

    m_gui->createSegmentations->setEnabled(false);
    m_gui->previewOpacity->setEnabled(false);
    m_gui->previewVisibility->setEnabled(false);
    m_gui->previewGroup->setEnabled(false);
    m_gui->abortAutoSegmenter->setEnabled(false);

    //TODO discard segmentation and destroy data/preview
    m_viewManager->removeWidget(m_preview);
    m_preview.reset();

}

//------------------------------------------------------------------------
void CvlabPanel::extractSegmentations()
{
    if (m_volume)
    {

        typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
                       fBinaryThresholdImageFilterType;

               fBinaryThresholdImageFilterType::Pointer thresholdFilter
                       = fBinaryThresholdImageFilterType::New();
               thresholdFilter->SetInput(m_volume);
               thresholdFilter->SetLowerThreshold(m_threshold);
               thresholdFilter->SetInsideValue(255);
               thresholdFilter->SetOutsideValue(0);
               thresholdFilter->Update();

        itkVolumeType::Pointer outputSegmentation = thresholdFilter->GetOutput();

        outputSegmentation->DisconnectPipeline();

        ESPINA_SETTINGS(settings);
        std::string cacheDir = "./";
        bool saveIntermediateVolumes = false;

        if (settings.contains("Save Intermediate Volumes"))
             saveIntermediateVolumes = settings.value("Save Intermediate Volumes").toBool();
        if (settings.contains("Features Directory"))
             cacheDir = settings.value("Features Directory").toString().toStdString();

        //warning, cache dir will be different than the one used for storing features and other data

        itkVolumeType::SpacingType spacing = volumetricData(m_viewManager->activeChannel()->asInput()->output())->itkImage()->GetSpacing();

        double zAnisotropyFactor = 2*spacing[2]/(spacing[0] + spacing[1]);

        CcboostAdapter::postprocessing(outputSegmentation, zAnisotropyFactor,
                                       saveIntermediateVolumes, cacheDir);

        outputSegmentation->SetSpacing(spacing);

        std::vector<itkVolumeType::Pointer> outSegList;

        CcboostAdapter::splitSegmentations(outputSegmentation, outSegList,
                                           saveIntermediateVolumes, cacheDir);

        m_undoStack->beginMacro("Create import segmentations");

        SegmentationAdapterSList createdSegmentations = m_manager->createSegmentations(outSegList,
                                                   CCB::ImportTask::IMPORTED);

        int i = 1;
        for(auto segmentation: createdSegmentations){

            SampleAdapterSList samples;
            samples << QueryAdapter::sample(m_viewManager->activeChannel());
            Q_ASSERT(!samples.empty());
            std::cout << "Create segmentation " << i++ << "/" << outSegList.size() << "." << std::endl;
            m_undoStack->push(new AddSegmentations(segmentation, samples, m_model));

        }
        m_undoStack->endMacro();

        m_viewManager->updateViews();

        m_gui->createSegmentations->setEnabled(false);
        m_gui->previewOpacity->setEnabled(false);
        m_gui->previewVisibility->setEnabled(false);
        m_gui->previewGroup->setEnabled(false);
        m_gui->abortAutoSegmenter->setEnabled(false);

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

  connect(m_gui->previewThreshold,   SIGNAL(valueChanged(int)),
           this,                     SLOT(changePreviewThreshold(int)));

  connect(m_gui->createSegmentations,SIGNAL(clicked(bool)),
          this,                      SLOT(extractSegmentations()));

  connect(m_gui->abortAutoSegmenter, SIGNAL(clicked(bool)),
            this,                    SLOT(abort()));

  connect(m_gui->openOverlay,        SIGNAL(clicked(bool)),
            this,                    SLOT(openOverlay()));

}
