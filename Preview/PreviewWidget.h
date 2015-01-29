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

#ifndef ESPINA_RAS_AUTO_SEGMENT_PREVIEW_H
#define ESPINA_RAS_AUTO_SEGMENT_PREVIEW_H

#include <GUI/View/Widgets/EspinaWidget.h>

#include "PreviewSliceRepresentation.h"
#include "CcboostSegmentationPlugin.h"

#include <Core/Utils/Spatial.h>

#include <QMap>

namespace ESPINA {

  namespace CCB {

    class PreviewWidget
    : public QObject
    , public EspinaWidget
    {
      Q_OBJECT
    public:
      explicit PreviewWidget(CcboostSegmentationSPtr ccboost);

      virtual void registerView(RenderView* view);

      virtual void unregisterView(RenderView* view);

      virtual void setEnabled(bool enable);

    public slots:
      void setPreviewVolume(LabelImageType::Pointer volume);

      void setLabels(const LabelList& labels);

      void setOpacity(float opacity); // opacity in [0.0, 1.0].

      void setVisibility(bool value);

      void update();

    private:
      CcboostSegmentationPluginSPtr m_ccboost;
      QMap<RenderView *, PreviewSliceRepresentationSPtr> m_representations;

      LabelImageType::Pointer m_volume;
    };

    using PreviewWidgetSPtr = std::shared_ptr<PreviewWidget>;

  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_AUTO_SEGMENT_PREVIEW_Hs
