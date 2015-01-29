
#ifndef ESPINA_AUTO_SEGMENT_PANEL_H
#define ESPINA_AUTO_SEGMENT_PANEL_H

#include <Support/Widgets/DockWidget.h>

//#include "types.h"
#include "AutoSegmentManager.h"
#include <Preview/PreviewWidget.h>
#include <vector>
#include <QElapsedTimer>

namespace ESPINA
{
  namespace CCB
  {
    class LabelWidget;

    class CvlabPanel
    : public DockWidget
    {
      Q_OBJECT

      class GUI;
      class TableModel;
      class AddAutoSegmentationCommand;
      class RemoveAutoSegmentationCommand;

      using MRASSPtr           = MultiRegularizerAutoSegmenterSPtr;
      using TrainingWidgets    = QMap<Label, LabelWidget *>;
      using MRASTrainingWigets = QMap<MRASSPtr, TrainingWidgets>;
      using PreviewSList       = QList<PreviewWidgetSPtr>;
      using MRASPreviews       = QMap<MRASSPtr, PreviewSList>;

    public:
      explicit CvlabPanel(AutoSegmentManager* manager,
                                ModelAdapterSPtr    model,
                                ViewManagerSPtr     viewManager,
                                ModelFactorySPtr    factory,
                                QUndoStack*         undoStack,
                                QWidget*            parent = nullptr);
      virtual ~CvlabPanel();

      virtual void reset();

    private slots:
      void changeActiveMRAS(QModelIndex index);

      void displaySettingsDialog();

      void createAutoSegmenter();

      void deleteAutoSegmenter();

      void changePreviewVisibility(bool value);

      void changePreviewOpacity(int opacity);

      void addLabels();

      void loadLabels();

      void exportLabels();

      void removeLabel(Label label);

      void updateProgress(int progress);

      void onFeaturesChanged();

      void onActiveChannelChanged();

      void abort();

      void updateActiveMRASPreview();

      void updateUsingSingleRAS();

      void updateUsingMultipleRAS();

      void extractSegmentations();

      void onTrainingVolumeChanged();

      void onFinished();

    private:
      void bindGUISignals();

      void changeActiveMRAS(MRASSPtr mras);

      void addActiveTrainingWidget(const Label& label, LabelWidget* widget)
      { setTrainingWidget(m_activeMRAS, label, widget); }

      void setTrainingWidget(MRASSPtr mras, const Label& label, LabelWidget* widget)
      { m_trainingWidgets[mras][label] = widget; }

      TrainingWidgets activeTrainingWidgets() const
      { return trainingWidgets(m_activeMRAS); }

      TrainingWidgets trainingWidgets(MRASSPtr mras) const
      { return m_trainingWidgets[mras]; }

      LabelWidget * activeTrainingWidget(const Label& label) const
      { return trainingWidget(m_activeMRAS, label); }

      LabelWidget * trainingWidget(MRASSPtr mras, const Label& label) const
      { return m_trainingWidgets[mras][label]; }


      void addActiveTrainingPreview(PreviewWidgetSPtr preview)
      { m_previews[m_activeMRAS] << preview; }

      PreviewSList activePreviews() const
      { return m_previews[m_activeMRAS]; }

      void createLabelEntry(MRASSPtr mras, Label label);

      LabelWidget *updateLabelControls(MRASSPtr mras, Label label);

      void showLabelControls(MRASSPtr mras, Label label);

      bool useSelectionForTraining();

      void setProcessingStatus(bool processing);

      void addActivePreviews();
      void removeActivePreviews();
      void updateActivePreviews();

      void showActiveLabels();
      void removeActiveLabels();

      void updateTableView();

      void updateActiveWidgetsState();

      void updateWidgetsState();

      void updateTrainingWidgetState();

      void stopDrawingTrainingVolumes();

    private:
      GUI*        m_gui;
      TableModel* m_tableModel;

      AutoSegmentManager* m_manager;

      ModelAdapterSPtr m_model;
      ViewManagerSPtr  m_viewManager;
      ModelFactorySPtr m_factory;
      QUndoStack*      m_undoStack;

      MRASSPtr           m_activeMRAS;
      MRASTrainingWigets m_trainingWidgets;
      MRASPreviews       m_previews;

      ChannelAdapterPtr  m_pendingFeaturesChannel;

      QElapsedTimer m_timer;
    };
  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_AUTO_SEGMENT_PANEL_H

