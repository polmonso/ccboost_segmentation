
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
, m_msg("ccboost")
, m_model(model)
, m_viewManager(viewManager)
, m_factory(factory)
, m_undoStack(undoStack)
, m_pendingFeaturesChannel(nullptr)
, m_threshold(0.50)
, m_probabilityMaxValue(50)
, m_probabilityMinValue(50)
{
  qRegisterMetaType<CcboostAdapter::FloatTypeImage::Pointer>("CcboostAdapter::FloatTypeImage::Pointer");

  setObjectName("CvlabPanel");

  setWindowTitle(tr("CCBoost segmentation extractor"));

  setWidget(m_gui);

  connect(m_viewManager.get(), SIGNAL(activeChannelChanged(ChannelAdapterPtr)),
          this,                SLOT(onActiveChannelChanged()));

//  connect(m_manager,           SIGNAL(volumeChanged()),
//          this,                SLOT(onVolumeOverlayChanged()));

  connect(m_manager,           SIGNAL(predictionChanged(QString)),
               this,                SLOT(volumeOverlayChanged(QString)));

  connect(m_manager, SIGNAL(progress(int)),
           this,     SLOT(updateProgress(int)));

  connect(m_manager, SIGNAL(publishMsg(QString)),
           this,     SLOT(updateMsg(QString)));


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
void CvlabPanel::setVolume(QFileInfo volumeFile){
    typedef itk::ImageFileReader< CcboostAdapter::FloatTypeImage > fReaderType;
    fReaderType::Pointer freader = fReaderType::New();
    freader->SetFileName(volumeFile.absoluteFilePath().toStdString());
    try {
        freader->Update();
        m_volume = freader->GetOutput();

        //we just allow one preview, so let's destroy the rest
        if(m_preview){
            m_viewManager->removeWidget(m_preview);
            m_preview.reset();
        }

        typedef itk::MinimumMaximumImageCalculator <CcboostAdapter::FloatTypeImage>
                ImageCalculatorFilterType;

        ImageCalculatorFilterType::Pointer imageCalculatorFilter
                = ImageCalculatorFilterType::New ();
        imageCalculatorFilter->SetImage(m_volume);
        imageCalculatorFilter->Compute();

        m_probabilityMaxValue = imageCalculatorFilter->GetMaximum();
        m_probabilityMinValue = imageCalculatorFilter->GetMinimum();

        //could we save the volume with the correect spacing, please? otherwise transform to physical fails and everything is wrong.
        m_volume->SetSpacing(volumetricData(m_viewManager->activeChannel()->asInput()->output())->itkImage()->GetSpacing());

        m_preview = PreviewWidgetSPtr{new PreviewWidget()};

        //for some reason, addWidget destroys the m_volume if we set it before.
        m_viewManager->addWidget(m_preview);

        m_preview->setPreviewVolume(m_volume);
        m_preview->setThreshold(m_threshold);
        m_preview->setOpacity(0.4);
        m_preview->setVisibility(true);
        m_preview->update();

        m_gui->createSegmentations->setEnabled(true);
        m_gui->previewOpacity->setEnabled(true);
        m_gui->previewVisibility->setEnabled(true);
        m_gui->previewGroup->setEnabled(true);
        m_gui->abortTask->setEnabled(true);

    } catch( itk::ExceptionObject & err ) {
        std::cerr << "ExceptionObject caught !" << std::endl;
        std::cerr << err << std::endl;

        if(m_preview){
            m_viewManager->removeWidget(m_preview);
            m_preview.reset();
        }
    }

    m_viewManager->updateViews();

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
void CvlabPanel::openOverlay(){

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setWindowTitle(tr("Open Volume Image Overlay"));
    fileDialog.setFilter(tr("Volume Image Files (*.tif *.tiff *.mha)"));

    if (fileDialog.exec() != QDialog::Accepted)
           return;

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

  //testing who messes up with lut
  m_preview->update();
//  emit sliceChanged(Plane::XY, 5);
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
    m_threshold = (pthreshold/100.0)*(m_probabilityMaxValue - m_probabilityMinValue) + m_probabilityMinValue;
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
    m_gui->abortTask->setEnabled(false);

    //TODO abort ccboost task if any
    //m_manager->abort();

    //TODO discard segmentation and destroy data/preview
    if(m_preview) {
        m_viewManager->removeWidget(m_preview);
        m_preview.reset();
    }

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
        settings.beginGroup("ccboost segmentation");
        std::string cacheDir = "./";
        bool saveIntermediateVolumes = false;

        if (settings.contains("Save Intermediate Volumes"))
             saveIntermediateVolumes = settings.value("Save Intermediate Volumes").toBool();

        //warning, cache dir will be different than the one used for storing features and other data
        if (settings.contains("Features Directory"))
             cacheDir = settings.value("Features Directory").toString().toStdString();

        typedef itk::ImageFileWriter< itkVolumeType > WriterType;
        WriterType::Pointer writer = WriterType::New();
        if(saveIntermediateVolumes) {
            writer->SetFileName(cacheDir + "binarized.tif");
            writer->SetInput(outputSegmentation);
            writer->Update();
        }

        itkVolumeType::SpacingType spacing = volumetricData(m_viewManager->activeChannel()->asInput()->output())->itkImage()->GetSpacing();

        outputSegmentation->SetSpacing(spacing);

        double zAnisotropyFactor = 2*spacing[2]/(spacing[0] + spacing[1]);

        CcboostAdapter::postprocessing(outputSegmentation, zAnisotropyFactor,
                                       saveIntermediateVolumes, cacheDir);

        m_manager->createImportTask(outputSegmentation);

        //destroy preview
        if(m_preview){
            m_viewManager->removeWidget(m_preview);
            m_preview.reset();
        }

        m_viewManager->updateViews();

        m_gui->createSegmentations->setEnabled(false);
        m_gui->previewOpacity->setEnabled(false);
        m_gui->previewVisibility->setEnabled(false);
        m_gui->previewGroup->setEnabled(false);
        m_gui->abortTask->setEnabled(false);

    }
}

//------------------------------------------------------------------------
void CvlabPanel::onFinished()
{
}


//------------------------------------------------------------------------
void CvlabPanel::updateProgress(int progress)
{
  //m_gui->progressBar->setVisible(progress > 0 && progress < 100);
  m_gui->progressBar->setValue(progress);
  updateMsg(m_msg);

  if(progress==100)
      m_gui->progressBar->setEnabled(false);

}

//------------------------------------------------------------------------
void CvlabPanel::updateMsg(QString msg)
{
  m_msg = msg;
  auto text = tr("%1%:%2%").arg(m_msg).arg(m_gui->progressBar->value());
  m_gui->progressBar->setFormat(text);

}

//------------------------------------------------------------------------
void CvlabPanel::bindGUISignals()
{

    m_gui->bMitochondriaCcboost->setToolTip("Create a synaptic ccboost segmentation from selected segmentations.");
    connect(m_gui->bMitochondriaCcboost, SIGNAL(clicked(bool)),
            this,                      SLOT(createMitochondriaSegmentation()));

    m_gui->bSynapseCcboost->setToolTip("Create a simple synaptic ccboost segmentation from selected segmentations.");
    connect(m_gui->bSynapseCcboost,    SIGNAL(clicked(bool)),
            this,                      SLOT(createSynapseSegmentation()));

    connect(m_gui->previewVisibility,  SIGNAL(toggled(bool)),
            this,                      SLOT(changePreviewVisibility(bool)));

    connect(m_gui->previewOpacity,     SIGNAL(valueChanged(int)),
            this,                      SLOT(changePreviewOpacity(int)));

    connect(m_gui->previewThreshold,   SIGNAL(valueChanged(int)),
            this,                      SLOT(changePreviewThreshold(int)));

    m_gui->createSegmentations->setToolTip("Create segmentations");
    connect(m_gui->createSegmentations,SIGNAL(clicked(bool)),
            this,                      SLOT(extractSegmentations()));

    connect(m_gui->abortTask,          SIGNAL(clicked(bool)),
            this,                      SLOT(abort()));

    m_gui->openOverlay->setToolTip("Import segmentation from segmented greyscale image");
    connect(m_gui->openOverlay,        SIGNAL(clicked(bool)),
            this,                      SLOT(openOverlay()));

}

//-----------------------------------------------------------------------------
void CvlabPanel::createSegmentationImport()
{

    m_manager->createImportTask();
    m_gui->progressBar->setEnabled(true);

}

//-----------------------------------------------------------------------------
void CvlabPanel::createMitochondriaSegmentation()
{
    //FIXME Move the element setting to the plugin and load it from settings.
    //      This has to be set before merging the segmentations
    CcboostTask::ELEMENT = CcboostTask::MITOCHONDRIA;
    qWarning() << "Detecting " << CcboostTask::ELEMENT;

    m_manager->createCcboostTask(m_model->segmentations());
    m_gui->progressBar->setEnabled(true);

}

//-----------------------------------------------------------------------------
void CvlabPanel::createSynapseSegmentation()
{
    //FIXME Move the element setting to the plugin and load it from settings.
    //      This has to be set before merging the segmentations
    CcboostTask::ELEMENT = CcboostTask::SYNAPSE;
    qWarning() << "Detecting " << CcboostTask::ELEMENT;

    m_manager->createCcboostTask(m_model->segmentations());
    m_gui->progressBar->setEnabled(true);

}
