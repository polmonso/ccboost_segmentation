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
#include <Filter/CcboostSegmentationFilter.h>

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
, m_tool     {SASToolSPtr{new CcboostSegmentationTool{QIcon(":/AppSurface.svg"), tr("Create a synaptic ccboost segmentation from selected segmentations.")}}}
, m_tool_ccboost     {SASToolSPtr{new CcboostSegmentationTool{QIcon(":/SynapseDetection.svg"), tr("Create a simple synaptic ccboost segmentation from selected segmentations.")}}}
, m_enabled  {true}
, m_plugin   {plugin}
{
  m_tool->setToolTip("Create a synaptic ccboost segmentation from selected segmentations.");
  connect(m_tool.get(), SIGNAL(triggered()), this, SLOT(createSAS()));

  m_tool_ccboost->setToolTip("Create a simple synaptic ccboost segmentation from selected segmentations.");
  connect(m_tool_ccboost.get(), SIGNAL(triggered()), this, SLOT(createSimpleCcboostSegmentation()));

  connect(viewManager->selection().get(), SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
}

//-----------------------------------------------------------------------------
CcboostSegmentationToolGroup::~CcboostSegmentationToolGroup()
{
  disconnect(m_tool.get(), SIGNAL(triggered()), this, SLOT(createSAS()));
  disconnect(m_viewManager->selection().get(), SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::setEnabled(bool enable)
{
  m_enabled = enable;
  selectionChanged();
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

  return tools;
}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::selectionChanged()
{
  QString toolTip("Create a synaptic ccboost segmentation from selected segmentations.");
  bool enabled = false;

  for(auto segmentation: m_model->segmentations())
  {
      if (m_plugin->isSynapse(segmentation.get()))
    {
      enabled = true;
      break;
    }
  }

  if (!enabled)
    toolTip += QString("\n(Requires the presence of one or more segmentations from 'Synapse' taxonomy)");

  m_tool->setToolTip(toolTip);
  m_tool->setEnabled(enabled && m_enabled);
}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::createSAS()
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

  //ChannelAdapterPtr channel;
  if(m_viewManager->activeChannel() != NULL) {
      auto channel = m_viewManager->activeChannel();
      channelInput = channel->asInput();
   } else {
      auto channel = m_model->channels().at(0);
      channelInput = channel->asInput();
  }

  qDebug() << "Using channel " << m_viewManager->activeChannel()->data(Qt::DisplayRole);

  inputs << channelInput;


  //TODO espina2 remove this segmentation input dependency
  for(auto seg: validSegmentations)
  {
    inputs << seg->asInput();
  }
  auto filterSPtr = m_factory->createFilter<CcboostSegmentationFilter>(inputs, AS_FILTER);

  //TODO espina2. this is a überdirty hack to get the segmentations inside the filter
  //TODO find out how to do it properly
  //auto filter = adapter.get()->get().get();
  auto filter = filterSPtr.get();
  filter->m_groundTruthSegList = validSegmentations;
  filter->m_backgroundGroundTruthSegList = validBgSegmentations;

  //FIXME fails when validSegmentations is empty and crashes espina
  struct CcboostSegmentationPlugin::Data data(filterSPtr, m_model->smartPointer(validSegmentations.at(0)));
  m_plugin->m_executingTasks.insert(filterSPtr.get(), data);

  connect(filterSPtr.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
  Task::submit(filterSPtr);

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


  //create and run task
  SchedulerSPtr scheduler = m_plugin->getScheduler();
  CCB::CcboostTaskSPtr ccboostTask{new CCB::CcboostTask(channel, scheduler)};
  connect(ccboostTask.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
  Task::submit(ccboostTask);

  return;
  //FIXME //TODO remove old code:

  inputs << channelInput;


  //TODO espina2 remove this segmentation input dependency
  for(auto seg: validSegmentations)
  {
    inputs << seg->asInput();
  }
  auto filterSPtr = m_factory->createFilter<CcboostSegmentationFilter>(inputs, AS_FILTER);

  //TODO espina2. this is a überdirty hack to get the segmentations inside the filter
  //TODO find out how to do it properly
  //auto filter = adapter.get()->get().get();
  auto filter = filterSPtr.get();
  filter->m_groundTruthSegList = validSegmentations;
  filter->m_backgroundGroundTruthSegList = validBgSegmentations;

  //FIXME fails when validSegmentations is empty and crashes espina
  struct CcboostSegmentationPlugin::Data data(filterSPtr, m_model->smartPointer(validSegmentations.at(0)));
  m_plugin->m_executingTasks.insert(filterSPtr.get(), data);

  connect(filterSPtr.get(), SIGNAL(finished()), m_plugin, SLOT(finishedTask()));
  Task::submit(filterSPtr);

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

