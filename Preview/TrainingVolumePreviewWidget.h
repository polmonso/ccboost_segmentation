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

#ifndef ESPINA_RAS_TRAINING_VOLUME_PREVIEW_WIDGET_H
#define ESPINA_RAS_TRAINING_VOLUME_PREVIEW_WIDGET_H

#include <GUI/View/Widgets/EspinaWidget.h>

#include "TrainingVolumeSliceRepresentation.h"

#include <Core/Utils/Spatial.h>

#include <QMap>

namespace ESPINA {

  namespace RAS {

    class TrainingVolumePreviewWidget
    : public QObject
    , public EspinaWidget
    {
      Q_OBJECT
    public:
      explicit TrainingVolumePreviewWidget();

      virtual ~TrainingVolumePreviewWidget();

      virtual void registerView(RenderView* view);

      virtual void unregisterView(RenderView* view);

      virtual void setEnabled(bool enable);

    public:
      void addTrainingVolume(Label::TrainingVolume volume);

      void removeTrainingVolume(Label::TrainingVolume volume);

      void discardTrainingVolumes();

      void setColor(const QColor& color);

      void setOpacity(float opacity);

      void setVisibility(bool value);

      void update();

    private:
      QColor m_color;
      float  m_opacity;
      bool   m_isVisible;

      QMap<RenderView *, TrainingVolumeSliceRepresentationSPtr> m_representations;

      //Label::TrainingVolumeList m_volumes;
    };

    using TrainingVolumePreviewWidgetSPtr = std::shared_ptr<TrainingVolumePreviewWidget>;

  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_TRAINING_VOLUME_PREVIEW_WIDGET_H