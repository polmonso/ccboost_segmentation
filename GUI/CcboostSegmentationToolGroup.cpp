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
#include "CcboostSegmentationToolGroup.h"
#include "CcboostSegmentationPlugin.h"
//#include <Filter/CcboostSegmentationFilter.h>

#include "Tasks/CcboostTask.h"

// ESPINA
#include <GUI/Model/Utils/QueryAdapter.h>
#include <Undo/AddCategoryCommand.h>
#include <Undo/AddRelationCommand.h>
#include <Undo/AddSegmentations.h>
#include <Core/Utils/AnalysisUtils.h>

// Qt
#include <QApplication>
#include <QUndoStack>
#include <QIcon>
#include <QString>
#include <QVariant>
#include <QDebug>
#include <QMessageBox>

using namespace ESPINA;

//-----------------------------------------------------------------------------
CcboostSegmentationToolGroup::CcboostSegmentationToolGroup(ModelAdapterSPtr model,
                                                       QUndoStack      *undoStack,
                                                       ModelFactorySPtr factory,
                                                       ViewManagerSPtr viewManager,
                                                       CcboostSegmentationPlugin *plugin)
: ToolGroup(viewManager, QIcon(":/SynapseDetection.svg"), tr("ccboost synapse segmentation"), nullptr)
, m_model    {model}
, m_factory  {factory}
, m_undoStack{undoStack}
, m_tool     {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/AppSurface.svg"), tr("Create a synaptic ccboost segmentation from selected segmentations.")}}}
, m_tool_ccboost    {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/SynapseDetection.svg"), tr("Create a simple synaptic ccboost segmentation from selected segmentations.")}}}
, m_tool_import     {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/SegmentationImporter.svg"), tr("Create a simple synaptic ccboost segmentation from selected segmentations.")}}}
, m_enabled  {true}
, m_plugin   {plugin}
{
  m_tool->setToolTip("Create a synaptic ccboost segmentation from selected segmentations.");
//  connect(m_tool.get(), SIGNAL(triggered()), this, SLOT(createSimpleMitochondriaCcboostSegmentation()));

  m_tool_ccboost->setToolTip("Create a simple synaptic ccboost segmentation from selected segmentations.");
  connect(m_tool_ccboost.get(), SIGNAL(triggered()), this, SLOT(createSimpleCcboostSegmentation()));

  m_tool_import->setToolTip("Import segmentation from segmented greyscale image");
  connect(m_tool_import.get(), SIGNAL(triggered()), this, SLOT(createSegmentationImporter()));

}

//-----------------------------------------------------------------------------
CcboostSegmentationToolGroup::~CcboostSegmentationToolGroup()
{

//  disconnect(m_tool.get(), SIGNAL(triggered()), this, SLOT(createSAS()));
  disconnect(m_tool_ccboost.get(), SIGNAL(triggered()), this, SLOT(createSimpleCcboostSegmentation()));
  disconnect( m_tool_import.get(), SIGNAL(triggered()), this, SLOT(createSegmentationImport()));

}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::setEnabled(bool enable)
{
  m_enabled = enable;
}

//-----------------------------------------------------------------------------
bool CcboostSegmentationToolGroup::enabled() const
{
  return m_enabled;
}

//-----------------------------------------------------------------------------
ToolSList CcboostSegmentationToolGroup::tools()
{
  ToolSList tools;

  tools << m_tool;
  tools << m_tool_ccboost;
  tools << m_tool_import;

  return tools;
}

void CcboostSegmentationToolGroup::createSegmentationImporter()
{

    //TODO create segmentation importer task
    //     SchedulerSPtr scheduler = m_plugin->getScheduler();
//     CCB::CcboostTaskSPtr ccboostTask{new CCB::CcboostTask(channel, scheduler)};
//     ccboostTask.get()->m_groundTruthSegList = validBgSegmentations;
//     ccboostTask.get()->m_backgroundGroundTruthSegList = validSegmentations;
//     struct CcboostSegmentationPlugin::Data2 data;
//     m_plugin->m_executingTasks2.insert(ccboostTask.get(), data);
//     connect(ccboostTask.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
//     Task::submit(ccboostTask);

     return;
}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::createSimpleCcboostSegmentation()
{
  auto segmentations = m_model->segmentations();
  SegmentationAdapterList validSegmentations;
  SegmentationAdapterList validBgSegmentations;
  for(auto seg: segmentations)
  {
          //TODO espina2
//      QStringList tags = SegmentationTags::extension(seg.get())->tags();
//          if(tags.contains(SynapseDetectionFilter::POSITIVETAG + SynapseDetectionFilter::ELEMENT, Qt::CaseInsensitive) ||
//             tags.contains(SynapseDetectionFilter::NEGATIVETAG + SynapseDetectionFilter::ELEMENT, Qt::CaseInsensitive)){
//              validSegmentations << seg;
//          }
  }

  if(validSegmentations.isEmpty()){
//TODO espina2
//      qDebug() << "No tags named " << SynapseDetectionFilter::POSITIVETAG << SynapseDetectionFilter::ELEMENT << " and "
//               << SynapseDetectionFilter::NEGATIVETAG << SynapseDetectionFilter::ELEMENT << " found, using category name "
//               << SynapseDetectionFilter::ELEMENT << " and " << SynapseDetectionFilter::BACKGROUND;

      //TODO add tag automatically
      for(auto seg: segmentations) {
          //TODO espina2 const the string
//          if (seg->taxonomy()->qualifiedName().contains(SynapseDetectionFilter::ELEMENT, Qt::CaseInsensitive)
        if (seg->category()->classificationName().contains("Synapse", Qt::CaseInsensitive))
          validSegmentations << seg.get(); //invalid if the shared pointer goes out of scope
        else if(seg->category()->classificationName().contains("Background", Qt::CaseInsensitive))
            validBgSegmentations << seg.get();
      }
  }

  InputSPtr channelInput;
  InputSList inputs;

  ChannelAdapterPtr channel;
  if(m_viewManager->activeChannel() != NULL) {
      channel = m_viewManager->activeChannel();
      channelInput = channel->asInput();
   } else {
      channel = m_model->channels().at(0).get();
      channelInput = channel->asInput();
  }

  qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);


  //create and run task //FIXME is that the correct way of getting the scheduler?
  SchedulerSPtr scheduler = m_plugin->getScheduler();
  CCB::CcboostTaskSPtr ccboostTask{new CCB::CcboostTask(channel, scheduler)};
  ccboostTask.get()->m_groundTruthSegList = validBgSegmentations;
  ccboostTask.get()->m_backgroundGroundTruthSegList = validSegmentations;
  struct CcboostSegmentationPlugin::Data2 data;
  m_plugin->m_executingTasks2.insert(ccboostTask.get(), data);
  connect(ccboostTask.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
  Task::submit(ccboostTask);

  return;
}

//-----------------------------------------------------------------------------
CcboostSegmentationTool::CcboostSegmentationTool(const QIcon& icon, const QString& text)
: m_action {new QAction{icon, text, nullptr}}
{
  connect(m_action, SIGNAL(triggered()), this, SLOT(activated()));
}

//-----------------------------------------------------------------------------
CcboostSegmentationTool::~CcboostSegmentationTool()
{
  disconnect(m_action, SIGNAL(triggered()), this, SLOT(activated()));
}

//-----------------------------------------------------------------------------
QList<QAction *> CcboostSegmentationTool::actions() const
{
  QList<QAction *> actions;
  actions << m_action;

  return actions;
}

