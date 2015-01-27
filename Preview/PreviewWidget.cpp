
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

#include "PreviewWidget.h"

#include <GUI/View/View2D.h>

using namespace ESPINA;
using namespace ESPINA::RAS;


//-----------------------------------------------------------------------------
PreviewWidget::PreviewWidget(CcboostSegmentationSPtr ccboost)
: m_ccboost(ccboost)
{
}

//-----------------------------------------------------------------------------
void PreviewWidget::registerView(RenderView* view)
{
  if (isView2D(view))
  {
    auto view2D = view2D_cast(view);

    m_representations[view] = PreviewSliceRepresentationSPtr{new PreviewSliceRepresentation(view2D)};

    if (Plane::XY == view2D->plane())
    {
      connect(view2D,      SIGNAL(sliceChanged(Plane,Nm)),
              m_ras.get(), SLOT(onSliceChanged(Plane,Nm)));
    }
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::unregisterView(RenderView* view)
{
  if (m_representations.contains(view))
  {
    m_representations.remove(view);

    if (isView2D(view))
    {
      auto view2D = view2D_cast(view);

      if (Plane::XY == view2D->plane())
      {
        disconnect(view2D,      SIGNAL(sliceChanged(Plane,Nm)),
                   m_ras.get(), SLOT(onSliceChanged(Plane,Nm)));
      }
    }
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::setEnabled(bool enable)
{

}

//-----------------------------------------------------------------------------
void PreviewWidget::setPreviewVolume(LabelImageType::Pointer volume)
{
  for(auto representation : m_representations)
  {
    representation->setPreviewVolume(volume);
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::setLabels(const LabelList& labels)
{
  for(auto representation : m_representations)
  {
    representation->setLabels(labels);
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::setOpacity(float opacity)
{
  for(auto representation : m_representations)
  {
      representation->setOpacity(opacity);
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::setVisibility(bool value)
{
  for(auto representation : m_representations)
  {
    representation->setVisibility(value);
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::update()
{
  for(auto representation : m_representations)
  {
    representation->update();
  }
}
