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

#ifndef CCBOOSTSEGMENTATIONTOOLBAR_H
#define CCBOOSTSEGMENTATIONTOOLBAR_H

// Plugin
#include <Core/MultiTasking/Task.h>
#include <GUI/Model/ModelAdapter.h>
#include "CcboostSegmentationPlugin_Export.h"
//#include "Core/Extensions/CcboostSegmentationExtension.h"

// ESPINA
#include <Support/ViewManager.h>
#include <Support/Widgets/Tool.h>

class QUndoStack;
class QIcon;
class QObject;
class QString;

namespace ESPINA
{
  class CcboostSegmentationPlugin;

  class CcboostSegmentationTool;
  using CVLToolPtr   = CcboostSegmentationTool *;
  using CVLToolSPtr  = std::shared_ptr<CcboostSegmentationTool>;

  //-----------------------------------------------------------------------------
  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationToolGroup
  : public ToolGroup
  {
    Q_OBJECT
  public:
    /* \brief CcboostSegmentationToolGroup class constructor.
     *
     */
    explicit CcboostSegmentationToolGroup(ModelAdapterSPtr model,
                                        QUndoStack *undoStack,
                                        ModelFactorySPtr factory,
                                        ViewManagerSPtr viewManager,
                                        CcboostSegmentationPlugin *plugin);

    /* \brief CcboostSegmentationToolGroup class virtual destructor.
     *
     */
    virtual ~CcboostSegmentationToolGroup();

    /* \brief Implements ToolGroup::setEnabled().
     *
     */
    virtual void setEnabled(bool value);

    /* \brief Implements ToolGroup::enabled().
     *
     */
    virtual bool enabled() const;

    /* \brief Implements ToolGroup::tools().
     *
     */
    virtual ToolSList tools();

  public slots:
    void createSimpleMitochondriaSegmentation();
    void createSimpleSynapseSegmentation();
    void createSegmentationImport();

  private:
    ModelAdapterSPtr         m_model;
    ModelFactorySPtr         m_factory;
    QUndoStack              *m_undoStack;
    CVLToolSPtr              m_tool_mitochondria;
    CVLToolSPtr              m_tool_synapse;
    CVLToolSPtr              m_tool_import;
    bool                     m_enabled;
    CcboostSegmentationPlugin *m_plugin;
  };

  //-----------------------------------------------------------------------------
  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationTool
  : public Tool
  {
    Q_OBJECT
    public:
      /* \brief CcboostSegmentationTool class constructor.
       * \param[in] icon, icon for the QAction.
       * \param[in] text, text to use as the QAction tooltip.
       *
       */
      explicit CcboostSegmentationTool(const QIcon& icon, const QString& text);

      /* \brief CcboostSegmentationTool class virtual destructor.
       *
       */
      virtual ~CcboostSegmentationTool();

      /* \brief Implements Tool::setEnabled().
       *
       */
      virtual void setEnabled(bool value)
      { m_action->setEnabled(value); }

      /* \brief Implements Tool::enabled().
       *
       */
      virtual bool enabled() const
      { return m_action->isEnabled(); }

      /* \brief Implements Tool::actions().
       *
       */
      virtual QList<QAction *> actions() const;

      /* \brief Sets the tooltip of the action.
       *
       */
      void setToolTip(const QString &tooltip)
      { m_action->setToolTip(tooltip); }

    signals:
      /* \brief Signal emmited when the QAction has been triggered.
       *
       */
      void triggered();

    private slots:
      /* \brief Emits the triggered signal for the toolgroup.
       *
       */
      void activated()
      { emit triggered(); }

    private:
      QAction *m_action;
  };

} // namespace ESPINA

#endif// CCBOOSTSEGMENTATIONTOOLBAR_H
