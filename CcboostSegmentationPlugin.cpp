/*
 *    
 *    Copyright (C) 2014  Jorge Peña Pastor <jpena@cesvima.upm.es>
 *
 *    This file is part of ESPINA.

    ESPINA is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// plugin
#include "CcboostSegmentationPlugin.h"
//#include <Filter/CcboostSegmentationFilter.h>
//#include <Filter/SASFetchBehaviour.h>
#include <GUI/Analysis/SASAnalysisDialog.h>
//#include <GUI/CcboostSegmentationToolGroup.h>
#include <GUI/Settings/CcboostSegmentationSettings.h>
//#include <Core/Extensions/ExtensionFactory.h>
#include <Tasks/CcboostTask.h>
// TODO: no filter inspectors yet
// #include <GUI/FilterInspector/CcboostSegmentationFilterInspector.h>

#include <itkChangeInformationImageFilter.h>
#include <itkImageMaskSpatialObject.h>

#include "DockWidget/CvlabPanel.h"

//#include <chrono>

// ESPINA
#include <GUI/Model/ModelAdapter.h>
#include <Core/IO/DataFactory/RasterizedVolumeFromFetchedMeshData.h>
#include <Extensions/Morphological/MorphologicalInformation.h>
#include <GUI/Model/Utils/QueryAdapter.h>
#include <Undo/AddCategoryCommand.h>
#include <Support/Settings/EspinaSettings.h>
#include <Undo/AddSegmentations.h>
#include <Undo/AddRelationCommand.h>
#include <Filters/SourceFilter.h>
#include <Extensions/Tags/SegmentationTags.h>
#include <Extensions/ExtensionUtils.h>

// Qt
#include <QColorDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QVariant>
#include <QFileDialog>

const QString UNSPECIFIED = QObject::tr("UNSPECIFIED");
const QString CVL = QObject::tr("CVL");
const QString IMPORTED = QObject::tr("IMPORTED");
const QString CVLTAG_PREPEND = QObject::tr("CVL ");
const QString IMPORTEDTAG_PREPEND = QObject::tr("IMPORTED ");

using namespace ESPINA;
using namespace ESPINA::CCB;

//-----------------------------------------------------------------------------
CcboostSegmentationPlugin::CcboostSegmentationPlugin()
: m_model           {nullptr}
, m_factory         {nullptr}
, m_viewManager     {nullptr}
, m_scheduler       {nullptr}
, m_undoStack       {nullptr}
, m_settings        {SettingsPanelSPtr(new CcboostSegmentationSettings())}
, m_dockWidget      {nullptr}
{
//    QStringList hierarchy;
//     hierarchy << "Analysis";

//     QAction *action = new QAction(tr("Synaptic ccboost segmentation"), this);
//     connect(action, SIGNAL(triggered(bool)),
//             this, SLOT());

//     m_menuEntry = MenuEntry(hierarchy, action);
}

//-----------------------------------------------------------------------------
CcboostSegmentationPlugin::~CcboostSegmentationPlugin()
{
//   qDebug() << "********************************************************";
//   qDebug() << "              Destroying ccboost segmentation Plugin";
//   qDebug() << "********************************************************";
}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::init(ModelAdapterSPtr model,
                             ViewManagerSPtr  viewManager,
                             ModelFactorySPtr factory,
                             SchedulerSPtr    scheduler,
                             QUndoStack      *undoStack)
{
  // if already initialized just return
  if(m_model != nullptr)
    return;

  m_model = model;
  m_viewManager = viewManager;
  m_factory = factory;
  m_scheduler = scheduler;
  m_undoStack = undoStack;

  //NOT SUPPORTED AT THE MOMENT:
  // for automatic computation of CVL
//  connect(m_model.get(), SIGNAL(segmentationsAdded(SegmentationAdapterSList)),
//          this, SLOT(segmentationsAdded(SegmentationAdapterSList)));

  //FIXME use a manager or a pointer to the output instead of passing the this pointer
   m_dockWidget = new CvlabPanel(this, m_model, m_viewManager, m_factory, m_undoStack);

}

//-----------------------------------------------------------------------------
ChannelExtensionFactorySList CcboostSegmentationPlugin::channelExtensionFactories() const
{
    ChannelExtensionFactorySList factories;

    return factories;
}

//-----------------------------------------------------------------------------
SegmentationExtensionFactorySList CcboostSegmentationPlugin::segmentationExtensionFactories() const
{
  SegmentationExtensionFactorySList extensionFactories;

  return extensionFactories;
}

//-----------------------------------------------------------------------------
FilterFactorySList CcboostSegmentationPlugin::filterFactories() const
{
    return FilterFactorySList();
}

//-----------------------------------------------------------------------------
NamedColorEngineSList CcboostSegmentationPlugin::colorEngines() const
{
  return NamedColorEngineSList();
}

//-----------------------------------------------------------------------------
RepresentationFactorySList CcboostSegmentationPlugin::representationFactories() const
{
  return RepresentationFactorySList();
}

//-----------------------------------------------------------------------------
QList<CategorizedTool> CcboostSegmentationPlugin::tools() const
{
  QList<CategorizedTool> tools;

  return tools;
}

//-----------------------------------------------------------------------------
QList<DockWidget *> CcboostSegmentationPlugin::dockWidgets() const
{
  QList<DockWidget *> docks;

  docks << m_dockWidget;

  return docks;
}

//-----------------------------------------------------------------------------
SettingsPanelSList CcboostSegmentationPlugin::settingsPanels() const
{
  SettingsPanelSList settingsPanels;

  settingsPanels << m_settings;

  return settingsPanels;
}

//-----------------------------------------------------------------------------
QList<MenuEntry> CcboostSegmentationPlugin::menuEntries() const
{
    return QList<MenuEntry>();
}

//-----------------------------------------------------------------------------
AnalysisReaderSList CcboostSegmentationPlugin::analysisReaders() const
{
  return AnalysisReaderSList();
}

void CcboostSegmentationPlugin::getGTSegmentations(const SegmentationAdapterSList segmentations,
                                                   SegmentationAdapterSList& validSegmentations,
                                                   SegmentationAdapterSList& validBgSegmentations) {

    if(CcboostTask::ELEMENT != CcboostTask::MITOCHONDRIA
       && CcboostTask::ELEMENT != CcboostTask::SYNAPSE) {
         qWarning() << "Error! Element is not set. Using default: " << CcboostTask::SYNAPSE;
         CcboostTask::ELEMENT = CcboostTask::SYNAPSE;
     }

    for(auto seg: segmentations)
    {
        if(seg->hasExtension(SegmentationTags::TYPE)) {
            auto extension = retrieveExtension<SegmentationTags>(seg);
            QStringList tags = extension->tags();
            if(tags.contains(CcboostTask::POSITIVETAG + CcboostTask::ELEMENT, Qt::CaseInsensitive))
                validSegmentations << seg;
            else if(tags.contains(CcboostTask::NEGATIVETAG + CcboostTask::ELEMENT, Qt::CaseInsensitive))
                validBgSegmentations << seg;
        }

    }

    if(validSegmentations.isEmpty() || validBgSegmentations.isEmpty()){
        qDebug() << "No tags named " << CcboostTask::POSITIVETAG << CcboostTask::ELEMENT << " and "
                 << CcboostTask::NEGATIVETAG << CcboostTask::ELEMENT << " found, using category name "
                 << CcboostTask::ELEMENT << " and " << CcboostTask::BACKGROUND;

        //TODO add tag automatically
        for(auto seg: segmentations) {
            //TODO espina2 const the string
            if (seg->category()->classificationName().contains(CcboostTask::ELEMENT, Qt::CaseInsensitive))
                validSegmentations << seg; //invalid if the shared pointer goes out of scope
            else if(seg->category()->classificationName().contains(CcboostTask::BACKGROUND, Qt::CaseInsensitive))
                validBgSegmentations << seg;
        }
    }

}

void CcboostSegmentationPlugin::createCcboostTask(SegmentationAdapterSList segmentations){

    if(m_executingTasks.size() > 0 || m_finishedTasks.size() > 0){
        qDebug() << "ccboost task already running. Aborting new request.";
        return;
    }

    SegmentationAdapterSList validSegmentations;
    SegmentationAdapterSList validBgSegmentations;

    CcboostSegmentationPlugin::getGTSegmentations(segmentations, validSegmentations, validBgSegmentations);

    qDebug() << QString("Retrieved %1 positive elements and %2 background elements").arg(validSegmentations.size()).arg(validBgSegmentations.size());

    ChannelAdapterPtr channel;
    if(m_viewManager->activeChannel() != NULL)
        channel = m_viewManager->activeChannel();
    else
        channel = m_model->channels().at(0).get();

    qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);

    //create and run task //FIXME is that the correct way of getting the scheduler?
    SchedulerSPtr scheduler = getScheduler();
    CCB::CcboostTaskSPtr ccboostTask{new CCB::CcboostTask(channel, scheduler)};
    ccboostTask.get()->m_groundTruthSegList = validSegmentations;
    ccboostTask.get()->m_backgroundGroundTruthSegList = validBgSegmentations;
    struct CcboostSegmentationPlugin::Data2 data;
    m_executingTasks.insert(ccboostTask.get(), data);
    connect(ccboostTask.get(), SIGNAL(finished()), this, SLOT(finishedTask()));
    connect(ccboostTask.get(), SIGNAL(message(QString)), this, SLOT(processTaskMsg(QString)));
    connect(ccboostTask.get(), SIGNAL(questionContinue(QString)), this, SLOT(publishCritical(QString)));
    connect(ccboostTask.get(), SIGNAL(progress(int)), this, SLOT(updateProgress(int)));
    Task::submit(ccboostTask);

    return;
}

void CcboostSegmentationPlugin::createImportTask(FloatTypeImage::Pointer segmentation, float threshold){

    ChannelAdapterPtr channel;
    if(m_viewManager->activeChannel() != NULL)
        channel = m_viewManager->activeChannel();
    else
        channel = m_model->channels().at(0).get();

    qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);

     SchedulerSPtr scheduler = getScheduler();
     CCB::ImportTaskSPtr importTask{new CCB::ImportTask(channel, scheduler, segmentation, threshold)};
     struct CcboostSegmentationPlugin::ImportData data;
     m_executingImportTasks.insert(importTask.get(), data);
     connect(importTask.get(), SIGNAL(finished()), this, SLOT(finishedImportTask()));
     Task::submit(importTask);

    return;
}

// Deprecated by preview
//void CcboostSegmentationPlugin::createImportTask(){


//    #warning "This function is deprecated"  __FILE__ ":"  __LINE__;

//    ChannelAdapterPtr channel;
//    if(m_viewManager->activeChannel() != NULL)
//        channel = m_viewManager->activeChannel();
//    else
//        channel = m_model->channels().at(0).get();

//    qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);


//    QFileDialog fileDialog;
//    fileDialog.setObjectName("SelectSegmentationFile");
//    fileDialog.setFileMode(QFileDialog::ExistingFiles);
//    fileDialog.setWindowTitle(QString("Select file "));
//    fileDialog.setFilter(ImportTask::SUPPORTED_FILES);

//    if (fileDialog.exec() != QDialog::Accepted)
//        return;

//    std::string filename = fileDialog.selectedFiles().first().toStdString();

//     SchedulerSPtr scheduler = getScheduler();
//     CCB::ImportTaskSPtr importTask{new CCB::ImportTask(channel, scheduler, filename)};
//     struct CcboostSegmentationPlugin::ImportData data;
//     m_executingImportTasks.insert(importTask.get(), data);
//     connect(importTask.get(), SIGNAL(finished()), this, SLOT(finishedImportTask()));
//     Task::submit(importTask);

//    return;
//}

// Automatic segmentation is not supported
#if 0
//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::segmentationsAdded(ViewItemAdapterSList segmentationsItems)
{


    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    if (!settings.contains("Automatic Computation") || !settings.value("Automatic Computation").toBool())
        return;

    SegmentationAdapterSList segmentations;
    for(auto item : segmentationsItems) {
        auto segmentation = segmentationPtr(item.get());
        segmentations << segmentation;
    }

    if(m_executingTasks.isEmpty()){
        //if task is not running, create
        createCcboostTask(segmentations);

    }else{
        //if task is running, add ROIs
        SegmentationAdapterSList validSegmentations;
        SegmentationAdapterSList validBgSegmentations;

        CcboostSegmentationPlugin::getGTSegmentations(segmentations,
                                                      validSegmentations,
                                                      validBgSegmentations);

        ChannelAdapterPtr channel;
        if(m_viewManager->activeChannel() != NULL)
            channel = m_viewManager->activeChannel();
        else
            channel = m_model->channels().at(0).get();

        if(validSegmentations.isEmpty() && validBgSegmentations.isEmpty())
            return;

        auto taskList = m_executingTasks.keys();
        CcboostTaskPtr task = taskList.at(0);

        itkVolumeType::Pointer channelItk = volumetricData(channel->asInput()->output())->itkImage();

        typedef itk::ChangeInformationImageFilter< itkVolumeType > ChangeInformationFilterType;
        ChangeInformationFilterType::Pointer normalizeFilter = ChangeInformationFilterType::New();
        normalizeFilter->SetInput(channelItk);
        itkVolumeType::SpacingType spacing = channelItk->GetSpacing();
        spacing[2] = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
                     + channelItk->GetSpacing()[1]);
        spacing[0] = 1;
        spacing[1] = 1;
        normalizeFilter->SetOutputSpacing( spacing );
        normalizeFilter->ChangeSpacingOn();
        normalizeFilter->Update();
        itkVolumeType::Pointer normalizedChannelItk = normalizeFilter->GetOutput();

        itkVolumeType::Pointer segmentedGroundTruth = CcboostTask::mergeSegmentations(normalizedChannelItk,
                                                                         validSegmentations,
                                                                         validBgSegmentations);

        SetConfigData<itkVolumeType> data(task->ccboostconfig.train.at(0));

        data.groundTruthImage = segmentedGroundTruth;

        //Get bounding box of annotated data
        typedef itk::ImageMaskSpatialObject< 3 > ImageMaskSpatialObjectType;
        ImageMaskSpatialObjectType::Pointer
          imageMaskSpatialObject  = ImageMaskSpatialObjectType::New();
        imageMaskSpatialObject->SetImage ( segmentedGroundTruth );
        itkVolumeType::RegionType annotatedRegion = imageMaskSpatialObject->GetAxisAlignedBoundingBoxRegion();
        itkVolumeType::OffsetValueType offset(CcboostTask::ANNOTATEDPADDING);
        annotatedRegion.PadByRadius(offset);

        annotatedRegion.Crop(channelItk->GetLargestPossibleRegion());
        data.annotatedRegion = annotatedRegion;

        //FIXME TODO set new ROIs in some other vector and let the task decide when to get them
        task->ccboostconfig.train.push_back(data);

    }
}
#endif

void CcboostSegmentationPlugin::publishCritical(QString msg){

    qDebug() << "Show message: " << msg;

    if(!msg.isEmpty()) {
        if(QMessageBox::critical(NULL, "Synapse Segmentation Message Box",
                             msg, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel){

            //Option 1
            //SchedulerSPtr scheduler = getScheduler();
            //scheduler->abortExecutingTasks();

            //Option 2
            CcboostTaskPtr ccboostTask = dynamic_cast<CcboostTaskPtr>(sender());
            ccboostTask->abort();

            //Option 3
            //emit aborted();
        }
    }


}

void CcboostSegmentationPlugin::updateProgress(int value) {
    emit progress(value);
}

void CcboostSegmentationPlugin::processTaskMsg(QString msg){

    qDebug() << "Show message: " << msg;

//     if(!msg.isEmpty())
//         QMessageBox::critical(NULL, "Synapse Segmentation Message Box",
//                              msg, QMessageBox::Yes, QMessageBox::Yes);

    emit publishMsg(msg);
}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::finishedTask()
{

    CcboostTaskPtr ccboostTask = dynamic_cast<CcboostTaskPtr>(sender());
    disconnect(ccboostTask, SIGNAL(finished()), this, SLOT(finishedTask()));
    disconnect(ccboostTask, SIGNAL(message(QString)), this, SLOT(processTaskMsg(QString)));
    disconnect(ccboostTask, SIGNAL(questionContinue(QString)), this, SLOT(publishCritical(QString)));
    //This is disconnected by the scheduler
    disconnect(ccboostTask, SIGNAL(progress(int)), this, SLOT(updateProgress(int)));

    if(!ccboostTask->isAborted())
        m_finishedTasks.insert(ccboostTask, m_executingTasks[ccboostTask]);

    m_executingTasks.remove(ccboostTask);


    // maybe all tasks have been aborted.
    if(m_finishedTasks.empty() || ccboostTask->hasFailed())
        return;

    //we shouldn't have several runs at the same time.
    Q_ASSERT(m_executingTasks.size() == 0);
    Q_ASSERT(m_finishedTasks.size() == 1);


    if(ccboostTask->ccboostconfig.usePreview)
    {

        //FIXME if a client creates a ccboost task while in a preview the former
        // would be overwritten and the behaviour is unknown. The clear could be done
        // at the finishedImportTask, but it's error-prone.
        // Right now we're preventing it at the GUI level.
        m_finishedTasks.clear();

        //this signal is captured by the cvlabPanel
        emit predictionChanged(QString::fromStdString(ccboostTask->ccboostconfig.cacheDir + ccboostTask->ccboostconfig.probabilisticOutputFilename));

    }
    else
    {

        m_finishedTasks.clear();

        createImportTask(ccboostTask->probabilisticSegmentation);

    }

    return;

}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::finishedImportTask()
{

    ImportTaskPtr importTask = dynamic_cast<ImportTaskPtr>(sender());
    disconnect(importTask, SIGNAL(finished()), this, SLOT(finishedImportTask()));

    if(!importTask->isAborted())
        m_finishedImportTasks.insert(importTask, m_executingImportTasks[importTask]);

    m_executingImportTasks.remove(importTask);

    Q_ASSERT(m_finishedTasks.size() == 1 && "more than one task is run");
    Q_ASSERT(m_executingTasks.size() == 0 && "more than one task is run");

    // maybe all tasks have been aborted.
    if(m_finishedImportTasks.empty())
        return;

    m_undoStack->beginMacro("Create import segmentations");

    SampleAdapterSList samples;
    samples << QueryAdapter::sample(m_viewManager->activeChannel());
    Q_ASSERT(!samples.empty());

    SegmentationAdapterSList createdSegmentations;
    createdSegmentations = createSegmentations(importTask->predictedSegmentationsList,
                                               CCB::ImportTask::IMPORTED);

    m_undoStack->push(new AddSegmentations(createdSegmentations, samples, m_model));

    m_undoStack->endMacro();

    //FIXME TODO do I need this?
    //m_viewManager->updateSegmentationRepresentations(createdSegmentations);
    m_viewManager->updateViews();

    m_finishedImportTasks.clear();

    std::cout << "Segmentations created. Returning control to ESPina" << std::endl;

}

SegmentationAdapterSList CcboostSegmentationPlugin::createSegmentations(CCB::LabelMapType::Pointer& predictedSegmentationsList,
                                                                        const QString& categoryName)
{

    auto sourceFilter = m_factory->createFilter<SourceFilter>(InputSList(), "ccboostSegmentFilter");

    auto classification = m_model->classification();
    CategoryAdapterSPtr category = classification->category(categoryName);
    if (category == nullptr)
    {
        m_undoStack->push(new AddCategoryCommand(m_model->classification()->root(), categoryName, m_model, QColor(255,255,0)));

        category = m_model->classification()->category(categoryName);

        category->addProperty(QString("Dim_X"), QVariant("500"));
        category->addProperty(QString("Dim_Y"), QVariant("500"));
        category->addProperty(QString("Dim_Z"), QVariant("500"));
    }

    int numObjects = predictedSegmentationsList->GetNumberOfLabelObjects();
    qDebug() << "Number of Label Objects" << numObjects;

    //auto spacing = ItkSpacing<itkVolumeType>(channel->output()->spacing());

    auto spacing = predictedSegmentationsList->GetSpacing();

    Output::Id id = 0;

    CCB::LabelMapType::LabelObjectType* object;
    SegmentationAdapterSList segmentations;

    for(int i=0; i < numObjects; i++)
      {
        try
        {
          //qDebug() << "Loading Segmentation " << seg.label;
          object      = predictedSegmentationsList->GetNthLabelObject(i);
          auto region = object->GetBoundingBox();

          auto segLabelMap = CCB::LabelMapType::New();
          segLabelMap->SetSpacing(spacing);
          segLabelMap->SetRegions(region);
          segLabelMap->Allocate();

          object->SetLabel(SEG_VOXEL_VALUE);

          segLabelMap->AddLabelObject(object);
          segLabelMap->Update();

          auto label2volumeFilter = LabelMap2VolumeFilterType::New();
          label2volumeFilter->SetInput(segLabelMap);
          label2volumeFilter->Update();

          auto volume = label2volumeFilter->GetOutput();

          auto output = std::make_shared<Output>(sourceFilter.get(), id, ToNmVector3<itkVolumeType>(spacing));

          Bounds    bounds  = equivalentBounds<itkVolumeType>(volume, volume->GetLargestPossibleRegion());
          NmVector3 spacing = ToNmVector3<itkVolumeType>(volume->GetSpacing());

          DefaultVolumetricDataSPtr volumetricData{new SparseVolume<itkVolumeType>(bounds, spacing)};
          volumetricData->draw(volume);

          MeshDataSPtr meshData{new MarchingCubesMesh<itkVolumeType>(volumetricData)};

          output->setData(volumetricData);
          output->setData(meshData);
          output->setSpacing(spacing);

          sourceFilter->addOutput(id, output);

          auto segmentation = m_factory->createSegmentation(sourceFilter, id);

          segmentation->setCategory(category);

          segmentations << segmentation;

          id++;
        } catch (...)
        {
          std::cerr << "Couldn't create segmentation " << i << std::endl;
        }
      }

    return segmentations;
}

SegmentationAdapterSList CcboostSegmentationPlugin::createSegmentations(std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList,
                                                                        const QString& categoryName)
{

    auto sourceFilter = m_factory->createFilter<SourceFilter>(InputSList(), "ccboostSegmentFilter");

    auto classification = m_model->classification();
    CategoryAdapterSPtr category = classification->category(categoryName);
    if (category == nullptr)
    {
        m_undoStack->push(new AddCategoryCommand(m_model->classification()->root(), categoryName, m_model, QColor(255,255,0)));

        category = m_model->classification()->category(categoryName);

        category->addProperty(QString("Dim_X"), QVariant("500"));
        category->addProperty(QString("Dim_Y"), QVariant("500"));
        category->addProperty(QString("Dim_Z"), QVariant("500"));
    }

    SegmentationAdapterSList segmentations;

    std::cout << "There are " << predictedSegmentationsList.size() << " objects." << std::endl;
    for (auto seg: predictedSegmentationsList)
    {

        //FIXME why there's a -1 there?
        Output::Id id = sourceFilter->numberOfOutputs();// - 1;

        //FIXME we could be smarter and get the bounding box when computing the connectedcomponents
        Q_ASSERT( itk::NumericTraits< itkVolumeType::PixelType >::max() != 255 && "something weird is going on");
        Bounds    bounds  = minimalBounds<itkVolumeType>(seg, 255);
        NmVector3 spacing = ToNmVector3<itkVolumeType>(seg->GetSpacing());

        OutputSPtr output{new Output(sourceFilter.get(), id, spacing)};

        DefaultVolumetricDataSPtr volumetricData{new SparseVolume<itkVolumeType>(bounds, spacing)};
        volumetricData->draw(seg);

        MeshDataSPtr meshData{new MarchingCubesMesh<itkVolumeType>(volumetricData)};

        output->setData(volumetricData);
        output->setData(meshData);
        output->setSpacing(spacing);

        sourceFilter->addOutput(id, output);

        std::cout << "Creating Segmentation " << id << "/" << predictedSegmentationsList.size() << ". Bounds: " << bounds << std::endl;

        auto segmentation = m_factory->createSegmentation(sourceFilter, id);
        segmentation->setCategory(category);

        segmentations << segmentation;
    }

    return segmentations;
}

//------------------------------------------------------------------------
void CcboostSegmentationPlugin::onAnalysisClosed()
{
  if(m_dockWidget != nullptr)
  {
    //FIXME check if CvlabPanel->reset() does what it is expected to do upon close
    dynamic_cast<CvlabPanel *>(m_dockWidget)->reset();
  }
}

Q_EXPORT_PLUGIN2(CcboostSegmentationPlugin, ESPINA::CcboostSegmentationPlugin)

