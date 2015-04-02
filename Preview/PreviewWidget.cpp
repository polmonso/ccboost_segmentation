
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
using namespace ESPINA::CCB;

//-----------------------------------------------------------------------------
PreviewWidget::PreviewWidget()
{
}

//-----------------------------------------------------------------------------
void PreviewWidget::registerView(RenderView* view)
{
  if (isView2D(view))
  {
    auto view2D = view2D_cast(view);

    m_representations[view] = PreviewSliceRepresentationSPtr{new PreviewSliceRepresentation(view2D)};

  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::unregisterView(RenderView* view)
{
  if (m_representations.contains(view))
  {
    m_representations.remove(view);
  }
}

//-----------------------------------------------------------------------------
void PreviewWidget::setEnabled(bool enable)
{

}

//-----------------------------------------------------------------------------
void PreviewWidget::setPreviewVolume(FloatTypeImage::Pointer volume)
{
  for(auto representation : m_representations)
  {
    representation->setPreviewVolume(volume);
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
void PreviewWidget::setThreshold(float threshold)
{
  for(auto representation : m_representations)
  {
      representation->setThreshold(threshold);
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
