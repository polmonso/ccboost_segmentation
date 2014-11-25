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
#include "Tasks/ImportTask.h"

// ESPINA
#include <GUI/Model/Utils/QueryAdapter.h>
#include <Undo/AddCategoryCommand.h>
#include <Undo/AddRelationCommand.h>
#include <Undo/AddSegmentations.h>
#include <Core/Utils/AnalysisUtils.h>
#include <Extensions/Tags/SegmentationTags.h>
#include <Extensions/ExtensionUtils.h>

// Qt
#include <QApplication>
#include <QUndoStack>
#include <QIcon>
#include <QString>
#include <QVariant>
#include <QDebug>
#include <QMessageBox>

using namespace ESPINA;
using namespace CCB;

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
, m_tool_mitochondria     {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/MitochondriaDetection.svg"), tr("Create a mitochondric ccboost segmentation from Mitochondria segmentations.")}}}
, m_tool_synapse    {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/SynapseDetection.svg"), tr("Create a simple synaptic ccboost segmentation from Synapse segmentations.")}}}
, m_tool_import     {CVLToolSPtr{new CcboostSegmentationTool{QIcon(":/SegmentationImporter.svg"), tr("Import segmentations from greyscale volume.")}}}
, m_enabled  {true}
, m_plugin   {plugin}
{
  m_tool_mitochondria->setToolTip("Create a synaptic ccboost segmentation from selected segmentations.");
  connect(m_tool_mitochondria.get(), SIGNAL(triggered()), this, SLOT(createSimpleMitochondriaSegmentation()));

  m_tool_synapse->setToolTip("Create a simple synaptic ccboost segmentation from selected segmentations.");
  connect(m_tool_synapse.get(), SIGNAL(triggered()), this, SLOT(createSimpleSynapseSegmentation()));

  m_tool_import->setToolTip("Import segmentation from segmented greyscale image");
  connect(m_tool_import.get(), SIGNAL(triggered()), this, SLOT(createSegmentationImport()));

}

//-----------------------------------------------------------------------------
CcboostSegmentationToolGroup::~CcboostSegmentationToolGroup()
{

  disconnect(m_tool_mitochondria.get(), SIGNAL(triggered()), this, SLOT(createSimpleMitochondriaSegmentation()));
  disconnect(m_tool_synapse.get(), SIGNAL(triggered()), this, SLOT(createSimpleSynapseSegmentation()));
  disconnect(m_tool_import.get(), SIGNAL(triggered()), this, SLOT(createSegmentationImport()));

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

  tools << m_tool_mitochondria;
  tools << m_tool_synapse;
  tools << m_tool_import;

  return tools;
}

void CcboostSegmentationToolGroup::createSegmentationImport()
{

    m_plugin->createImportTask();

}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::createSimpleMitochondriaSegmentation()
{
    //FIXME Move the element setting to the plugin and load it from settings.
    //      This has to be set before merging the segmentations
    CcboostTask::ELEMENT = CcboostTask::MITOCHONDRIA;
    qWarning() << "Detecting " << CcboostTask::ELEMENT;

    m_plugin->createCcboostTask(m_model->segmentations());
}

//-----------------------------------------------------------------------------
void CcboostSegmentationToolGroup::createSimpleSynapseSegmentation()
{
    //FIXME Move the element setting to the plugin and load it from settings.
    //      This has to be set before merging the segmentations
    CcboostTask::ELEMENT = CcboostTask::SYNAPSE;
    qWarning() << "Detecting " << CcboostTask::ELEMENT;

    m_plugin->createCcboostTask(m_model->segmentations());
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

