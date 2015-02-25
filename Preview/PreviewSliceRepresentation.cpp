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

#include "PreviewSliceRepresentation.h"
#include <Core/Utils/Bounds.h>


using namespace ESPINA;
using namespace ESPINA::CCB;

//-----------------------------------------------------------------------------
PreviewSliceRepresentation::PreviewSliceRepresentation(View2D *view)
: m_view(view)
, m_opacity(0.4)
, m_threshold(0.5)
, m_probabilityMaxValue(50)
, m_probabilityMinValue(50)
, m_isVisible(true)
// m_textActor(vtkTextActor::New()),
, m_imageActor(vtkImageActor::New())
, m_vtkVolume(vtkVolume::New())
, m_extractFilter(ExtractFilterType::New())
, m_thresholdFilter(ThresholdImageFilterType::New())
, m_itk2vtk(Itk2vtkFilterType::New())
, m_lut(LUTSPtr::New())
, m_mapToColors(vtkImageMapToColorsSPtr::New())
, m_plane(view->plane())
, m_pos(0)
{

  m_thresholdFilter->SetLowerThreshold(m_threshold);
  m_thresholdFilter->SetInPlace(false);
  m_thresholdFilter->ReleaseDataFlagOff();
  m_thresholdFilter->SetInsideValue(255);
  m_thresholdFilter->SetOutsideValue(0);

  m_extractFilter->SetInput(m_thresholdFilter->GetOutput());
  m_extractFilter->SetInPlace(false);
  m_extractFilter->ReleaseDataFlagOff();
  m_extractFilter->SetDirectionCollapseToIdentity();

  m_itk2vtk->ReleaseDataFlagOn();
  m_itk2vtk->SetInput(m_extractFilter->GetOutput());

  setDefaultColors();

  m_mapToColors->SetOutputFormatToRGBA();
  m_mapToColors->SetInputData(m_itk2vtk->GetOutput());
  m_mapToColors->SetLookupTable(m_lut);
  m_imageActor->GetMapper()->SetInputConnection(m_mapToColors->GetOutputPort());
  m_imageActor->SetInterpolate(false);
  m_imageActor->GetMapper()->BorderOn();


  view->addActor(m_imageActor);

  connect(view, SIGNAL(sliceChanged(Plane,Nm)),
          this, SLOT(setSlice(Plane,Nm)));
}

//-----------------------------------------------------------------------------
PreviewSliceRepresentation::~PreviewSliceRepresentation()
{
  m_view->removeActor(m_imageActor);

  // m_textActor->Delete();
  m_imageActor->Delete();
  m_vtkVolume ->Delete();
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setPreviewVolume(LabelImageType::Pointer volume)
{
  m_volume = volume;

  m_thresholdFilter->SetInput(m_volume);

  typedef itk::MinimumMaximumImageCalculator <LabelImageType>
          ImageCalculatorFilterType;

  ImageCalculatorFilterType::Pointer imageCalculatorFilter
          = ImageCalculatorFilterType::New ();
  imageCalculatorFilter->SetImage(m_volume);
  imageCalculatorFilter->Compute();

  m_probabilityMaxValue = imageCalculatorFilter->GetMaximum();
  m_probabilityMinValue = imageCalculatorFilter->GetMinimum();

  if(m_volume->GetBufferedRegion() != m_region)
  {
    updateRegion();
  }

  m_itk2vtk   ->Update();
  m_imageActor->Update();
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setOpacity(float opacity)
{
  m_opacity = opacity;
  updateColors();
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setThreshold(float threshold)
{
  m_threshold = threshold;
  //updateColors(); //do we need this?
  update(); //is this one the right function to call? or maybe updateRegion?
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setVisibility(bool value)
{
  m_isVisible = value;
  updateColors();
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::updateColors()
{

    float opacity = m_isVisible? m_opacity : 0.0;

    m_lut->SetNumberOfTableValues(2);
    m_lut->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
    m_lut->SetTableValue(1, 1.0, 0.0, 0.0, opacity);
    m_lut->Build();
    m_lut->Modified();

    // m_lut->SetIndexedLookup(1);
    m_lut->SetRange(0, 1);

    m_imageActor->Update();

}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::update()
{
  //TODO we can also use the SetLowerThresholdInput for automatic update
  m_thresholdFilter->SetLowerThreshold( m_threshold*(m_probabilityMaxValue - m_probabilityMinValue) + m_probabilityMinValue);
  setSlice(m_plane, m_pos);
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setSlice(Plane plane, Nm pos)
{
  m_pos   = pos;
  m_plane = plane;

  updateRegion();

  double p[3];
  m_imageActor->GetPosition(p);
  p[idx(m_plane)] += m_view->segmentationDepth()/2;
  m_imageActor->SetPosition(p);

  m_itk2vtk   ->Update();
  m_imageActor->Update();

  if (plane == m_view->plane())
  {
    m_view->updateView();
  }
}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::setDefaultColors()
{
//  m_lut->SetNumberOfTableValues(5);
//  m_lut->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
//  m_lut->SetTableValue(1, 1.0, 0.0, 0.0, m_opacity);
//  m_lut->SetTableValue(2, 0.0, 1.0, 0.0, m_opacity);
//  m_lut->SetTableValue(3, 0.0, 1.0, 1.0, m_opacity);
//  m_lut->SetTableValue(4, 1.0, 1.0, 0.0, m_opacity);
//  m_lut->Build();
//  m_lut->Modified();

//  // m_lut->SetIndexedLookup(1);
//  m_lut->SetRange(0, 4);

    updateColors();

}

//-----------------------------------------------------------------------------
void PreviewSliceRepresentation::updateRegion()
{
  if (m_volume)
  {
    LabelImageType::RegionType region = m_volume->GetBufferedRegion();
    m_region = region;

    LabelImageType::SizeType  size = region.GetSize();
    LabelImageType::IndexType index;
    LabelImageType::PointType point;
    point.Fill(0);

    auto i = idx(m_plane);

    size [i] = 0;
    point[i] = m_pos;

    m_volume->TransformPhysicalPointToIndex(point, index);

    region.SetSize(size);
    region.SetIndex(index);

    m_extractFilter->SetExtractionRegion(region);
  }
}
