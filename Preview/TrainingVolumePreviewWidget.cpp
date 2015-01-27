
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

#include "TrainingVolumePreviewWidget.h"

#include <GUI/View/View2D.h>

using namespace ESPINA;
using namespace ESPINA::RAS;

//-----------------------------------------------------------------------------
TrainingVolumePreviewWidget::TrainingVolumePreviewWidget()
: m_color(Qt::black)
, m_opacity(0.4)
, m_isVisible(true)
{

}

//-----------------------------------------------------------------------------
TrainingVolumePreviewWidget::~TrainingVolumePreviewWidget()
{
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::registerView(RenderView* view)
{
  if (isView2D(view))
  {
    auto view2D = view2D_cast(view);

    auto representation = TrainingVolumeSliceRepresentationSPtr{new TrainingVolumeSliceRepresentation(view2D)};

    representation->setColor(m_color);
    representation->setOpacity(m_opacity);
    representation->setVisibility(m_isVisible);

    m_representations[view]  = representation;
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::unregisterView(RenderView* view)
{
  if (m_representations.contains(view))
  {
    m_representations.remove(view);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::setEnabled(bool enable)
{

}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::addTrainingVolume(Label::TrainingVolume volume)
{
  for(auto representation : m_representations)
  {
    representation->addTrainingVolume(volume);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::removeTrainingVolume(Label::TrainingVolume volume)
{
  for(auto representation : m_representations)
  {
    representation->removeTrainingVolume(volume);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::discardTrainingVolumes()
{
  for(auto representation : m_representations)
  {
      representation->discardTrainingVolumes();
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::setColor(const QColor& color)
{
  m_color = color;

  for(auto representation : m_representations)
  {
      representation->setColor(color);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::setOpacity(float opacity)
{
  m_opacity = opacity;

  for(auto representation : m_representations)
  {
      representation->setOpacity(opacity);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::setVisibility(bool value)
{
  m_isVisible = value;

  for(auto representation : m_representations)
  {
    representation->setVisibility(value);
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumePreviewWidget::update()
{
  for(auto representation : m_representations)
  {
    representation->update();
  }
}
