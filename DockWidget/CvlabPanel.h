
#ifndef ESPINA_AUTO_SEGMENT_PANEL_H
#define ESPINA_AUTO_SEGMENT_PANEL_H

#include <Support/Widgets/DockWidget.h>

//#include "types.h"
#include "CcboostSegmentationPlugin.h"
#include <Preview/PreviewWidget.h>
#include <vector>
#include <QElapsedTimer>
#include "../Filter/ConfigData.h"

namespace ESPINA
{
  namespace CCB
  {
    class CvlabPanel
    : public DockWidget
    {
      Q_OBJECT

      class GUI;

    public:
      explicit CvlabPanel(CcboostSegmentationPlugin* manager,
                          ModelAdapterSPtr    model,
                                ViewManagerSPtr     viewManager,
                                ModelFactorySPtr    factory,
                                QUndoStack*         undoStack,
                                QWidget*            parent = nullptr);
      virtual ~CvlabPanel();

      virtual void reset();

      void setVolume(QFileInfo volumeFile);

    private slots:
      void createMitochondriaSegmentation();

      void createSynapseSegmentation();

      void openOverlay();

      void changePreviewVisibility(bool value);

      void changePreviewOpacity(int opacity);

      void changePreviewThreshold(int pthreshold);

      void volumeOverlayChanged(QString volumeFilePath);

      void onActiveChannelChanged();

      void abort();

      void updatePreview();

      void extractSegmentations();

      void onFinished();      

      void updateProgress(int progress);

      void updateMsg(QString message);

    private:
      void bindGUISignals();

      PreviewWidgetSPtr getPreview() const
      { return m_preview; }

    private:
      CcboostSegmentationPlugin* m_manager;
      FloatTypeImage::Pointer m_volume;

      GUI*        m_gui;
      QString     m_msg;

      ModelAdapterSPtr m_model;
      ViewManagerSPtr  m_viewManager;
      ModelFactorySPtr m_factory;
      QUndoStack*      m_undoStack;

      PreviewWidgetSPtr  m_preview;

      ChannelAdapterPtr  m_pendingFeaturesChannel;

      bool m_preprocess;

      float m_threshold;
      float m_probabilityMaxValue;
      float m_probabilityMinValue;

    };
  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_AUTO_SEGMENT_PANEL_H

