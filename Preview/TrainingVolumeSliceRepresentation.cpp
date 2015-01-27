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

#include "TrainingVolumeSliceRepresentation.h"
#include <Core/Utils/Bounds.h>


using namespace ESPINA;
using namespace ESPINA::RAS;

//-----------------------------------------------------------------------------
TrainingVolumeSliceRepresentation::Representation::Representation(Label::TrainingVolume volume,
                                                                  LUTSPtr lut)
: Volume(volume)
, MapToColors(vtkImageMapToColorsSPtr::New())
, Actor(vtkImageActor::New())
{
  MapToColors->SetOutputFormatToRGBA();
  MapToColors->SetLookupTable(lut);
  MapToColors->SetNumberOfThreads(1);

  Actor->SetInterpolate(false);
  Actor->GetMapper()->BorderOn();
  Actor->GetMapper()->SetInputConnection(MapToColors->GetOutputPort());
}

//-----------------------------------------------------------------------------
TrainingVolumeSliceRepresentation::TrainingVolumeSliceRepresentation(View2D *view)
: m_view(view)
, m_opacity(0.4)
, m_isVisible(true)
, m_lut(LUTSPtr::New())
, m_plane(view->plane())
, m_planeIndex(idx(m_plane))
, m_pos(0)
{
  m_lut->Allocate();
  m_lut->SetNumberOfTableValues(2);
  m_lut->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
  m_lut->Build();

  setColor(Qt::black);

  connect(view, SIGNAL(sliceChanged(Plane,Nm)),
          this, SLOT(setSlice(Plane,Nm)));
}

//-----------------------------------------------------------------------------
TrainingVolumeSliceRepresentation::~TrainingVolumeSliceRepresentation()
{
  discardTrainingVolumes();
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::addTrainingVolume(Label::TrainingVolume volume)
{
  auto representation = Representation(volume, m_lut);

  m_representations << representation;

  setSlice(m_plane, m_pos);

  m_view->addActor(representation.Actor);
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::removeTrainingVolume(Label::TrainingVolume volume)
{
  for(auto it = m_representations.begin(); it != m_representations.end(); ++it)
  {
    if (it->Volume == volume)
    {
      m_view->removeActor(it->Actor);
      it = m_representations.erase(it);
      break;
    }
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::discardTrainingVolumes()
{
  for(auto representation : m_representations)
  {
    m_view->removeActor(representation.Actor);

    representation.Actor->Delete();
  }

  m_representations.clear();
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::setOpacity(float opacity)
{
  m_opacity = opacity;
  setColor(m_color);
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::setVisibility(bool value)
{
  m_isVisible = value;
  setColor(m_color);
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::setColor(const QColor& color)
{
  m_color = color;

  float opacity = m_isVisible?m_opacity:0.0;

  m_lut->SetTableValue(1, color.redF(), color.greenF(), color.blueF(), opacity);
  m_lut->Modified();
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::update()
{
  setSlice(m_plane, m_pos);

  for(auto representation : m_representations)
  {
    representation.Actor->Update();
  }
}

//-----------------------------------------------------------------------------
void TrainingVolumeSliceRepresentation::setSlice(Plane plane, Nm pos)
{
  m_pos   = pos;
  m_plane = plane;

  for(auto representation : m_representations)
  {
    Bounds imageBounds = representation.Volume->bounds();

    bool valid = imageBounds[2*m_planeIndex] <= pos && pos <= imageBounds[2*m_planeIndex+1];

    if (representation.Actor != nullptr && valid)
    {
      imageBounds.setLowerInclusion(true);
      imageBounds.setUpperInclusion(toAxis(m_planeIndex), true);
      imageBounds[2*m_planeIndex] = imageBounds[2*m_planeIndex+1] = m_pos;

      auto slice = vtkImage(representation.Volume, imageBounds);
      representation.MapToColors->SetInputData(slice);
      representation.MapToColors->Update();
      representation.Actor->SetDisplayExtent(slice->GetExtent());
      representation.Actor->Update();

      // need to reposition the actor so it will always be over the channels actors'
      double pos[3];
      representation.Actor->GetPosition(pos);
      pos[m_planeIndex] += m_view->segmentationDepth();
      representation.Actor->SetPosition(pos);
    }

    representation.Actor->SetVisibility(valid && m_isVisible);
  }

  if (plane == m_view->plane())
  {
    m_view->updateView();
  }
}