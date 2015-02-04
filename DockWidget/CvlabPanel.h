
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
      class AddAutoSegmentationCommand;
      class RemoveAutoSegmentationCommand;

    public:
      explicit CvlabPanel(CcboostSegmentationPlugin* manager,
                                ModelAdapterSPtr    model,
                                ViewManagerSPtr     viewManager,
                                ModelFactorySPtr    factory,
                                QUndoStack*         undoStack,
                                QWidget*            parent = nullptr);
      virtual ~CvlabPanel();

      virtual void reset();

    private slots:      
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

      void updatePreview();

      void extractSegmentations();

      void onFinished();

    private:
      void bindGUISignals();

      PreviewWidgetSPtr getPreview() const
      { return m_preview; }

      void updateWidgetsState();

    private:

      ItkFloatVolumeTypeSPtr m_volume;

      GUI*        m_gui;

      CcboostSegmentationPlugin* m_manager;

      ModelAdapterSPtr m_model;
      ViewManagerSPtr  m_viewManager;
      ModelFactorySPtr m_factory;
      QUndoStack*      m_undoStack;

      PreviewWidgetSPtr  m_preview;

      ChannelAdapterPtr  m_pendingFeaturesChannel;

    };
  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_AUTO_SEGMENT_PANEL_H

