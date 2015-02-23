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
#include <GUI/CcboostSegmentationToolGroup.h>
#include <GUI/Settings/CcboostSegmentationSettings.h>
//#include <Core/Extensions/ExtensionFactory.h>
#include <Tasks/CcboostTask.h>
// TODO: no filter inspectors yet
// #include <GUI/FilterInspector/CcboostSegmentationFilterInspector.h>

#include <itkChangeInformationImageFilter.h>
#include <itkImageMaskSpatialObject.h>

#include "DockWidget/CvlabPanel.h"

// ESPINA
#include <GUI/Model/ModelAdapter.h>
#include <Core/IO/FetchBehaviour/RasterizedVolumeFromFetchedMeshData.h>
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
, m_toolGroup       {nullptr}
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

  m_toolGroup = new CcboostSegmentationToolGroup{m_model, m_undoStack, m_factory, m_viewManager, this};

  // for automatic computation of CVL
  connect(m_model.get(), SIGNAL(segmentationsAdded(SegmentationAdapterSList)),
          this, SLOT(segmentationsAdded(SegmentationAdapterSList)));

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
QList<ToolGroup *> CcboostSegmentationPlugin::toolGroups() const
{
  QList<ToolGroup *> tools;

  tools << m_toolGroup;

  return tools;
}

//-----------------------------------------------------------------------------
QList<DockWidget *> CcboostSegmentationPlugin::dockWidgets() const
{
  QList<DockWidget *> docks;

  //FIXME use a manager or a pointer to the output instead of passing the this pointer
  docks << new CvlabPanel(this, m_model, m_viewManager, m_factory, m_undoStack);

  return docks;
}

//-----------------------------------------------------------------------------
RendererSList CcboostSegmentationPlugin::renderers() const
{
  return RendererSList();
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
    connect(ccboostTask.get(), SIGNAL(message(QString)), this, SLOT(publishMsg(QString)));
    connect(ccboostTask.get(), SIGNAL(questionContinue(QString)), this, SLOT(publishCritical(QString)));
    Task::submit(ccboostTask);

    return;
}

void CcboostSegmentationPlugin::createImportTask(){

    ChannelAdapterPtr channel;
    if(m_viewManager->activeChannel() != NULL)
        channel = m_viewManager->activeChannel();
    else
        channel = m_model->channels().at(0).get();

    qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);


    QFileDialog fileDialog;
    fileDialog.setObjectName("SelectSegmentationFile");
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setWindowTitle(QString("Select file "));
    fileDialog.setFilter(ImportTask::SUPPORTED_FILES);

    if (fileDialog.exec() != QDialog::Accepted)
        return;

    std::string file = fileDialog.selectedFiles().first().toStdString();

     SchedulerSPtr scheduler = getScheduler();
     CCB::ImportTaskSPtr importTask{new CCB::ImportTask(channel, scheduler)};
     importTask.get()->filename = file;
     struct CcboostSegmentationPlugin::ImportData data;
     m_executingImportTasks.insert(importTask.get(), data);
     connect(importTask.get(), SIGNAL(finished()), this, SLOT(finishedImportTask()));
     Task::submit(importTask);

    return;
}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::segmentationsAdded(SegmentationAdapterSList segmentations)
{
    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    if (!settings.contains("Automatic Computation") || !settings.value("Automatic Computation").toBool())
        return;

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

void CcboostSegmentationPlugin::publishMsg(QString msg){

    qDebug() << "Show message: " << msg;

    if(!msg.isEmpty())
        QMessageBox::critical(NULL, "Synapse Segmentation Message Box",
                             msg, QMessageBox::Yes, QMessageBox::Yes);

}

void CcboostSegmentationPlugin::questionContinue(QString msg){

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

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::finishedTask()
{


    CcboostTaskPtr ccboostTask = dynamic_cast<CcboostTaskPtr>(sender());
    disconnect(ccboostTask, SIGNAL(finished()), this, SLOT(finishedTask()));

    if(!ccboostTask->isAborted())
        m_finishedTasks.insert(ccboostTask, m_executingTasks[ccboostTask]);

    m_executingTasks.remove(ccboostTask);

    //we don't consider having several runs at the same time.
    assert(m_finishedTasks.size() == 1);
    assert(m_executingTasks.size() == 0);
//    //if everyone did not finish yet, wait.
//    if (!m_executingTasks.empty())
//        return;

    // maybe all tasks have been aborted.
    if(m_finishedTasks.empty())
        return;

    if(ccboostTask->ccboostconfig.usePreview){
        emit predictionChanged(QString::fromStdString(ccboostTask->ccboostconfig.cacheDir + ccboostTask->ccboostconfig.probabilisticOutputFilename));
    } else {

        m_undoStack->beginMacro("Create Synaptic ccboost segmentations");

        SegmentationAdapterSList createdSegmentations;
        //    for(CCB::CcboostTaskPtr ccbtask: m_finishedTasks.keys())
        //    {
        //FIXME createSegmentations removes one. makes sense on import, but not here.
        createdSegmentations = createSegmentations(ccboostTask->predictedSegmentationsList, CVL);
        int i = 1;
        for(auto segmentation: createdSegmentations){

            SampleAdapterSList samples;
            samples << QueryAdapter::sample(m_viewManager->activeChannel());
            Q_ASSERT(!samples.empty());
            std::cout << "Create segmentation " << i++ << "/" << createdSegmentations.size() << "." << std::endl;

            m_undoStack->push(new AddSegmentations(segmentation, samples, m_model));

        }

        //    }
        m_undoStack->endMacro();
    }
    //FIXME TODO do I need this?
    //m_viewManager->updateSegmentationRepresentations(createdSegmentations);
    m_viewManager->updateViews();

    m_finishedTasks.clear();

    return;

    //TODO connect autosegmentation slot

}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::finishedImportTask()
{

    ImportTaskPtr importTask = dynamic_cast<ImportTaskPtr>(sender());
    disconnect(importTask, SIGNAL(finished()), this, SLOT(finishedImportTask()));
    if(!importTask->isAborted())
        m_finishedImportTasks.insert(importTask, m_executingImportTasks[importTask]);

    m_executingImportTasks.remove(importTask);

    //if everyone did not finish yet, wait.
    if (!m_executingImportTasks.empty())
        return;

    // maybe all tasks have been aborted.
    if(m_finishedImportTasks.empty())
        return;

    m_undoStack->beginMacro("Create import segmentations");

    SegmentationAdapterSList createdSegmentations;
    for(CCB::ImportTaskPtr imptask: m_finishedImportTasks.keys())
    {
        createdSegmentations = createSegmentations(imptask->predictedSegmentationsList,
                                                   CCB::ImportTask::IMPORTED);

        int i = 1;
        for(auto segmentation: createdSegmentations){

              SampleAdapterSList samples;
              samples << QueryAdapter::sample(m_viewManager->activeChannel());
              Q_ASSERT(!samples.empty());
              std::cout << "Create segmentation " << i++ << "/" << createdSegmentations.size() << "." << std::endl;
              m_undoStack->push(new AddSegmentations(segmentation, samples, m_model));

          }

        //FIXME the following doesn't work, it tries to add crossed relations and fails
//        //TODO there's no way to initialize the list with n-copies directly?
//        SampleAdapterSList samplesList;
//        for(int i=0; i < createdSegmentations.size(); i++){
//            SampleAdapterSList samples;
//            samples << QueryAdapter::sample(m_viewManager->activeChannel());
//            Q_ASSERT(!samples.empty());
//            samplesList << samples;
//        }

//        std::cout << "Create " << createdSegmentations.size() << " segmentations" << std::endl;
//        m_undoStack->push(new AddSegmentations(createdSegmentations, samplesList, m_model));
//        std::cout << "Segmentations created." << std::endl;


    }
    m_undoStack->endMacro();

    //FIXME TODO do I need this?
    //m_viewManager->updateSegmentationRepresentations(createdSegmentations);
    m_viewManager->updateViews();

    m_finishedImportTasks.clear();

    std::cout << "Segmentations created. Returning control to ESPina" << std::endl;

}

SegmentationAdapterSList CcboostSegmentationPlugin::createSegmentations(std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList,
                                                                        const QString& categoryName)
{

    auto sourceFilter = m_factory->createFilter<SourceFilter>(InputSList(), "AutoSegmentFilter");

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

        Output::Id id = sourceFilter->numberOfOutputs() - 1;

        OutputSPtr output{new Output(sourceFilter.get(), id)};

        Bounds    bounds  = equivalentBounds<itkVolumeType>(seg, seg->GetLargestPossibleRegion());
        NmVector3 spacing = ToNmVector3<itkVolumeType>(seg->GetSpacing());

        DefaultVolumetricDataSPtr volumetricData{new SparseVolume<itkVolumeType>(bounds, spacing)};
        volumetricData->draw(seg);

        MeshDataSPtr meshData{new MarchingCubesMesh<itkVolumeType>(volumetricData)};

        output->setData(volumetricData);
        output->setData(meshData);
        output->setSpacing(spacing);

        sourceFilter->addOutput(id, output);

        auto segmentation = m_factory->createSegmentation(sourceFilter, id);
        segmentation->setCategory(category);

        segmentations << segmentation;
    }

    return segmentations;
}

Q_EXPORT_PLUGIN2(CcboostSegmentationPlugin, ESPINA::CcboostSegmentationPlugin)

