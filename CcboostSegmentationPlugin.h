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

#include <Core/EspinaTypes.h>

#include "CCBTypes.h"

// ESPINA
#include <Support/ViewManager.h> //may be context now?
#include <Support/Plugin.h>
#include <Core/Analysis/Input.h>
#include <Core/Analysis/DataFactory.h>
#include <Core/Factory/FilterFactory.h>
#include <Tasks/CcboostTask.h>
#include <Tasks/ImportTask.h>

#include <GUI/Model/SegmentationAdapter.h>

//Qt
#include <QMessageBox>


namespace ESPINA
{
  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationPlugin
  : public Plugin
  {
    Q_OBJECT
    Q_INTERFACES(ESPINA::Plugin)

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

    virtual RepresentationFactorySList representationFactories() const;

    virtual QList<CategorizedTool> tools() const;

    virtual QList<DockWidget *> dockWidgets() const;

    virtual SettingsPanelSList settingsPanels() const;

    virtual QList<MenuEntry> menuEntries() const;

    virtual AnalysisReaderSList analysisReaders() const;

    virtual FilterFactorySList filterFactories() const;

      /**
     * @brief getGTSegmentations splits the segmentations in two groups, background and segmentation
     * @param segmentations a mix of spina's segmenation
     * @param validSegmentations the positve examples (segmentations) of the element to be found in the stack
     * @param validBgSegmentations the negative examples of the elemnt to be found in the stack
     */
    static void getGTSegmentations(const SegmentationAdapterSList segmentations,
                                   SegmentationAdapterSList &validSegmentations,
                                   SegmentationAdapterSList &validBgSegmentations);

    /**
     * @brief createCcboostTask creates a ccboost task which predicts
     *
     * Splits the input segmentations in two groups: background and segmentations.
     * Then sets these into the task alongside with the channel
     * All signals are connected and finally the task is submited.
     * The finish signal is connected to finishedTask()
     *
     * @param segmentations training data provided (segmentation + background).
     *        Segmentations are distinguished by label and category.
     */
    void createCcboostTask(SegmentationAdapterSList segmentations);

    /**
     * @brief createImportTask creates an import task
     *
     * Builds an import task with the provided float segmentation
     * and threshold.
     * The finish signal is connected and finally the task is submited.
     * The finish signal is connected to finishedImportTask()
     *
     * @param segmentation float image to be thresholded. Connectedcomponents will be ran
     * and the espina segmentations created at finishedImportTask()
     * @param threshold threshold of the float image, 0.0 by default.
     */
    void createImportTask(FloatTypeImage::Pointer segmentation, float threshold = 0.0);

    void abortTasks(){
        for(auto task : m_executingTasks.keys())
                    task->abort();
        for(auto task : m_executingImportTasks.keys())
                    task->abort();

    }

    //FIXME //TODO hack
  public:
    SchedulerSPtr getScheduler() {
        return m_scheduler;
    }

    SegmentationAdapterSList createSegmentations(std::vector<itkVolumeType::Pointer>&  predictedSegmentationsList, const QString &categoryName);
    SegmentationAdapterSList createSegmentations(CCB::LabelMapType::Pointer& predictedSegmentationsList, const QString &categoryName);

  public slots:
    //NOT SUPPORTED AT THE MOMENT
    //void segmentationsAdded(ViewItemAdapterSList segmentations);
    /**
     * @brief finishedTask When the ccboost task finishes, an import task is called.
     *
     * If preview setting is active, it will send the volume to the preview class for
     * the user to threshold, whose threshold will be sent to an import task.
     *
     * If preview setting is not active, the volume is sent directly to the import task
     * with the default threshold.
     */
    void finishedTask();

    /**
     * @brief finishedImportTask creates the espina segmentations from the itk's
     */
    void finishedImportTask();

    /**
     * @brief processTaskMsg processes a message from the task and decides wether to send it
     * to the qt's event loop or not.
     *
     * Currently all task messages are forwarded through the publishMsg signal
     * @param msg
     */
    void processTaskMsg(QString msg);

    /**
     * @brief publishCritical captures a question/msg signal from the task and publishes it
     *
     * At the moment, the tasks do not fully support question answers, meanng that abort()
     * can't abort ccboost.
     *
     * @param msg question msg to be published
     */
    void publishCritical(QString msg);

    /**
     * @brief updateProgress publishes progress percentual value
     * @param progress progress percentual
     */
    void updateProgress(int progress);

    virtual void onAnalysisClosed();

  signals:
    /**
     * @brief predictionChanged Signal to notify the preview we have a prediction to threshold
     * @param volumeFilename the absolute filepath were the volume is stored
     */
    void predictionChanged(QString volumeFilename);
    void progress(int);
    void publishMsg(QString msg);

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
    MenuEntry                        m_menuEntry;
    FilterFactorySPtr                m_filterFactory;
    SegmentationAdapterList          m_analysisSynapses;
    DockWidget                      *m_dockWidget;


    QMap<CCB::CcboostTaskPtr, struct Data2> m_executingTasks;
    QMap<CCB::CcboostTaskPtr, struct Data2> m_finishedTasks;

    QMap<CCB::ImportTaskPtr, struct ImportData> m_executingImportTasks;
    QMap<CCB::ImportTaskPtr, struct ImportData> m_finishedImportTasks;

//    friend class CcboostSegmentationToolGroup;
  };

  typedef std::shared_ptr<CcboostSegmentationPlugin> CcboostSegmentationPluginSPtr;

} // namespace ESPINA

#endif// CCBOOSTSEGMENTATION_H
