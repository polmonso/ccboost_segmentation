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

#ifndef CCBOOSTSEGMENTATION_H
#define CCBOOSTSEGMENTATION_H

#include "CcboostSegmentationPlugin_Export.h"

// Plugin
//#include "Core/Extensions/CcboostSegmentationExtension.h"

// ESPINA
#include <Support/ViewManager.h>
#include <Support/Plugin.h>
#include <Core/Analysis/Input.h>
#include <Core/Analysis/FetchBehaviour.h>
#include <Core/Factory/FilterFactory.h>
#include <Core/EspinaTypes.h>
#include <Tasks/CcboostTask.h>
#include <Tasks/ImportTask.h>

//Qt
#include <QMessageBox>


namespace ESPINA
{
  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationPlugin
  : public Plugin
  {
    Q_OBJECT
    Q_INTERFACES(ESPINA::Plugin)

//    class CCBFilterFactory
//    : public FilterFactory
//    {
//        virtual FilterTypeList providedFilters() const;
//        virtual FilterSPtr createFilter(InputSList inputs, const Filter::Type& filter, SchedulerSPtr scheduler) const throw (Unknown_Filter_Exception);
//    };

  public:
    explicit CcboostSegmentationPlugin();
    virtual ~CcboostSegmentationPlugin();

    virtual void init(ModelAdapterSPtr model,
                      ViewManagerSPtr  viewManager,
                      ModelFactorySPtr factory,
                      SchedulerSPtr    scheduler,
                      QUndoStack      *undoStack);

    virtual ChannelExtensionFactorySList channelExtensionFactories() const;

    virtual SegmentationExtensionFactorySList segmentationExtensionFactories() const;

    virtual NamedColorEngineSList colorEngines() const;

    virtual QList<ToolGroup *> toolGroups() const;

    virtual QList<DockWidget *> dockWidgets() const;

    virtual RendererSList renderers() const;

    virtual SettingsPanelSList settingsPanels() const;

    virtual QList<MenuEntry> menuEntries() const;

    virtual AnalysisReaderSList analysisReaders() const;

    virtual FilterFactorySList filterFactories() const;

    static void getGTSegmentations(const SegmentationAdapterSList segmentations,
                                   SegmentationAdapterSList &validSegmentations,
                                   SegmentationAdapterSList &validBgSegmentations);

    void createCcboostTask(SegmentationAdapterSList segmentations);
    void createImportTask();

    //FIXME //TODO hack
  public:
    SchedulerSPtr getScheduler() {
        return m_scheduler;
    }

  public slots:
    void segmentationsAdded(SegmentationAdapterSList segmentations);
    void finishedTask();
    void finishedImportTask();
    void publishMsg(QString msg);
    void questionContinue(QString msg);

  private:
    struct Data2
        {
//         std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList;

//         Data2(std::vector<itkVolumeType::Pointer>& predSegList): predictedSegmentationsList{predSegList} {};
//         Data2(): predictedSegmentationsList{nullptr} {};
        };
    struct ImportData
        {
        };

    static bool isSynapse(SegmentationAdapterPtr segmentation);

  private:
    ModelAdapterSPtr                 m_model;
    ModelFactorySPtr                 m_factory;
    ViewManagerSPtr                  m_viewManager;
    SchedulerSPtr                    m_scheduler;
    QUndoStack                      *m_undoStack;
    SettingsPanelSPtr                m_settings;
    SegmentationExtensionFactorySPtr m_extensionFactory;
    ToolGroupPtr                     m_toolGroup;
    MenuEntry                        m_menuEntry;
    FilterFactorySPtr                m_filterFactory;
    SegmentationAdapterList          m_analysisSynapses;

    QMap<CCB::CcboostTaskPtr, struct Data2> m_executingTasks;
    QMap<CCB::CcboostTaskPtr, struct Data2> m_finishedTasks;

    QMap<CCB::ImportTaskPtr, struct ImportData> m_executingImportTasks;
    QMap<CCB::ImportTaskPtr, struct ImportData> m_finishedImportTasks;

    SegmentationAdapterSList createSegmentations(std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList, const QString &categoryName);

    typedef std::shared_ptr<CcboostSegmentation> CcboostSegmentationSPtr;


    friend class CcboostSegmentationToolGroup;
  };

} // namespace ESPINA

#endif// CCBOOSTSEGMENTATION_H
