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

// Qt
#include <QColorDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QVariant>

const QString CVL = QObject::tr("CVL");
const QString CVLTAG_PREPEND = QObject::tr("CVL ");

using namespace ESPINA;
using namespace CCB;

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
  return QList<DockWidget *>();
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

void CcboostSegmentationPlugin::getGTSegmentations(const SegmentationAdapterList segmentations,
                                                   SegmentationAdapterList& validSegmentations,
                                                   SegmentationAdapterList& validBgSegmentations) {
    for(auto seg: segmentations)
    {
        if(seg->hasExtension(SegmentationTags::TYPE))
            auto extension = retrieveExtension<SegmentationTags>(seg);
        QStringList tags = extension->tags();
        if(tags.contains(CcboostTask::POSITIVETAG + CcboostTask::ELEMENT, Qt::CaseInsensitive))
            validSegmentations << seg;
        else if(tags.contains(CcboostTask::NEGATIVETAG + CcboostTask::ELEMENT, Qt::CaseInsensitive))
            validBgSegmentations << seg;

    }

    if(validSegmentations.isEmpty()){
        qDebug() << "No tags named " << CcboostTask::POSITIVETAG << CcboostTask::ELEMENT << " and "
                 << CcboostTask::NEGATIVETAG << CcboostTask::ELEMENT << " found, using category name "
                 << CcboostTask::ELEMENT << " and " << CcboostTask::BACKGROUND;

        //TODO add tag automatically
        for(auto seg: segmentations) {
            //TODO espina2 const the string
            if (seg->category()->classificationName().contains(CcboostTask::ELEMENT, Qt::CaseInsensitive))
                validSegmentations << seg.get(); //invalid if the shared pointer goes out of scope
            else if(seg->category()->classificationName().contains(CcboostTask::BACKGROUND, Qt::CaseInsensitive))
                validBgSegmentations << seg.get();
        }
    }

}

void CcboostSegmentationPlugin::createCcboostTask(SegmentationAdapterList segmentations){
    SegmentationAdapterList validSegmentations;
    SegmentationAdapterList validBgSegmentations;

    CcboostSegmentationPlugin::getGTSegmentations(segmentations, validSegmentations, validBgSegmentations);

    ChannelAdapterPtr channel;
    if(m_viewManager->activeChannel() != NULL)
        channel = m_viewManager->activeChannel();
    else
        channel = m_model->channels().at(0).get();

    qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);

    //create and run task //FIXME is that the correct way of getting the scheduler?
    SchedulerSPtr scheduler = m_plugin->getScheduler();
    CCB::CcboostTaskSPtr ccboostTask{new CCB::CcboostTask(channel, scheduler)};
    ccboostTask.get()->m_groundTruthSegList = validBgSegmentations;
    ccboostTask.get()->m_backgroundGroundTruthSegList = validSegmentations;
    struct CcboostSegmentationPlugin::Data2 data;
    m_plugin->m_executingTasks.insert(ccboostTask.get(), data);
    connect(ccboostTask.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
    connect(ccboostTask.get(), SIGNAL(message(std::string)), m_plugin, SLOT(processMsg(std::string)));
    Task::submit(ccboostTask);

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
        SegmentationAdapterList validSegmentations;
        SegmentationAdapterList validBgSegmentations;

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
                                                                         m_groundTruthSegList,
                                                                         m_backgroundGroundTruthSegList);

        SetConfigData data(task->ccboostconfig.train.at(0));

        data.groundTruthImage = segmentedGroundTruth;

        //Get bounding box of annotated data
        typedef itk::ImageMaskSpatialObject< 3 > ImageMaskSpatialObjectType;
        ImageMaskSpatialObjectType::Pointer
          imageMaskSpatialObject  = ImageMaskSpatialObjectType::New();
        imageMaskSpatialObject->SetImage ( segmentedGroundTruth );
        itkVolumeType::RegionType annotatedRegion = imageMaskSpatialObject->GetAxisAlignedBoundingBoxRegion();
        itkVolumeType::OffsetValueType offset(CcboostSegmentationFilter::ANNOTATEDPADDING);
        annotatedRegion.PadByRadius(offset);

        annotatedRegion.Crop(channelItk->GetLargestPossibleRegion());
        data.annotatedRegion = annotatedRegion;

        //FIXME TODO set new ROIs in some other vector and let the task decide when to get them
        task->ccboostconfig.train.push_back(data);

    }
}

void CcboostSegmentationPlugin::processMsg(std::string &msg){

    if(!msg.empty())
        QMessageBox::critical(NULL, "Synapse Segmentation Message Box",
                             QString(msg), QMessageBox::Yes, QMessageBox::Yes);

}

//-----------------------------------------------------------------------------
void CcboostSegmentationPlugin::finishedTask()
{

    CcboostTaskPtr ccboostTask = dynamic_cast<CcboostTaskPtr>(sender());
    disconnect(ccboostTask, SIGNAL(finished()), this, SLOT(finishedTask()));
    if(!ccboostTask->isAborted())
        m_finishedTasks.insert(ccboostTask, m_executingTasks[ccboostTask]);

    m_executingTasks.remove(ccboostTask);

    //if everyone did not finish yet, wait.
    if (!m_executingTasks.empty())
        return;

    // maybe all tasks have been aborted.
    if(m_finishedTasks.empty())
        return;

    m_undoStack->beginMacro("Create Synaptic ccboost segmentations");


    SegmentationAdapterSList createdSegmentations;
    for(CCB::CcboostTaskPtr ccbtask: m_finishedTasks.keys())
    {
        createdSegmentations = createSegmentations(ccbtask->predictedSegmentationsList);
        for(auto segmentation: createdSegmentations){

            SampleAdapterSList samples;
            samples << QueryAdapter::sample(m_viewManager->activeChannel());
            Q_ASSERT(!samples.empty());

            m_undoStack->push(new AddSegmentations(segmentation, samples, m_model));

        }

    }
    m_undoStack->endMacro();

    //FIXME TODO do I need this?
    //m_viewManager->updateSegmentationRepresentations(createdSegmentations);
    m_viewManager->updateViews();

    m_finishedTasks.clear();

    return;

    //TODO connect autosegmentation slot

}

SegmentationAdapterSList CcboostSegmentationPlugin::createSegmentations(std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList)
{

    auto sourceFilter = m_factory->createFilter<SourceFilter>(InputSList(), "AutoSegmentFilter");

    auto classification = m_model->classification();
    if (classification->category(CVL) == nullptr)
    {
        m_undoStack->push(new AddCategoryCommand(m_model->classification()->root(), CVL, m_model, QColor(255,255,0)));

        m_model->classification()->category(CVL)->addProperty(QString("Dim_X"), QVariant("500"));
        m_model->classification()->category(CVL)->addProperty(QString("Dim_Y"), QVariant("500"));
        m_model->classification()->category(CVL)->addProperty(QString("Dim_Z"), QVariant("500"));
    }

    CategoryAdapterSPtr category = classification->category(CVL);
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

