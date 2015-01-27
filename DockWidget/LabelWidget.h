/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2014  <copyright holder> <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ESPINA_RAS_LABEL_WIDGET_H
#define ESPINA_RAS_LABEL_WIDGET_H

#include <QWidget>
#include "ui_LabelWidget.h"
#include "Preview/TrainingVolumePreviewWidget.h"
#include <Label.h>

#include <Core/Analysis/Data/VolumetricData.hxx>
#include <GUI/Selectors/BrushSelector.h>
#include <Support/ViewManager.h>

class QUndoStack;
namespace ESPINA {

  namespace RAS {

    class LabelWidget
    : public QWidget
    , private Ui::LabelWidget
    {
      Q_OBJECT

      using PreviewWidgetSPtr = TrainingVolumePreviewWidgetSPtr;

      class DrawCommand;

    public:
      explicit LabelWidget(Label             label,
                           ChannelAdapterPtr channel,
                           ViewManagerSPtr   viewManager,
                           QUndoStack*       undoStack,
                           QWidget*          parent = 0,
                           Qt::WindowFlags   f = 0);
      virtual ~LabelWidget();

      void addTrainingVolume(Label::TrainingVolume volume);

      void removeTrainingVolume(Label::TrainingVolume volume);

      Label label() const
      { return m_label; }

      void stopDrawing();

    signals:
      void trainingVolumeChanged();

      void labelRemoved(Label);

    private slots:
      void onBrushEnabled(bool checked);

      void onBrushUseChanged(bool used);

      void onStroke(Selector::Selection selection);

      void discardTrainingVolumes();

      void onLabelRemoved();

      void onVisibilityChanged(bool visible);

      void onMinVoxelSizeChanged(int size);


    private:
      Label m_label;

      QUndoStack *m_undoStack;

      ViewManagerSPtr   m_viewManager;
      BrushSelectorSPtr m_brush;
      PreviewWidgetSPtr m_widget;
    };

  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_LABEL_WIDGET_H
