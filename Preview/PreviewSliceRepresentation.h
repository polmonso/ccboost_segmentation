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

#ifndef ESPINA_RAS_PREVIEW_SLICE_REPRESENTATION_H
#define ESPINA_RAS_PREVIEW_SLICE_REPRESENTATION_H

#include <QObject>

//#include "types.h"
#include "Label.h"

#include <Core/Utils/Spatial.h>
#include <GUI/View/View2D.h>

//TODO maybe template FloatImageType instead of adding CcboostAdapter dependence
#include "Filter/CcboostAdapter.h"

#include <itkExtractImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkVolume.h>
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

namespace ESPINA {
  namespace CCB {

    class PreviewSliceRepresentation
    : public QObject
    {
      Q_OBJECT

    public:
        typedef CcboostAdapter::FloatTypeImage LabelImageType;

      explicit PreviewSliceRepresentation(View2D *view);
      virtual ~PreviewSliceRepresentation();

      vtkImageActor *actor() const
      { return m_imageActor; }

      void setPreviewVolume(LabelImageType::Pointer volume);

      void setOpacity(float opacity);

      void setThreshold(float threshold);

      void setVisibility(bool value);

      void updateColors();

      void update();

    private slots:
      void setSlice(Plane plane, Nm pos);

    private:
      void setDefaultColors();
      void updateRegion();

    private:
      typedef itk::BinaryThresholdImageFilter <LabelImageType, itkVolumeType>
                  ThresholdImageFilterType;

      using ExtractFilterType       = itk::ExtractImageFilter<itkVolumeType, itk::Image<unsigned int, 2>>;
      using Itk2vtkFilterType       = itk::ImageToVTKImageFilter<itk::Image<unsigned int, 2>>;
      using LUTSPtr                 = vtkSmartPointer<vtkLookupTable>;
      using vtkImageMapToColorsSPtr = vtkSmartPointer<vtkImageMapToColors>;

      View2D* m_view;

      float m_opacity;
      float m_threshold;
      float m_probabilityMaxValue;
      float m_probabilityMinValue;
      bool  m_isVisible;

      // vtkTextActor* m_textActor;
      vtkImageActor*          m_imageActor;
      vtkVolume*              m_vtkVolume;
      LabelImageType::Pointer m_volume;

      ThresholdImageFilterType::Pointer m_thresholdFilter;
      ExtractFilterType::Pointer m_extractFilter;
      Itk2vtkFilterType::Pointer m_itk2vtk;
      LUTSPtr                    m_lut;
      vtkImageMapToColorsSPtr    m_mapToColors;

      Nm    m_pos;
      Plane m_plane;

      LabelImageType::RegionType m_region;
    };

    using PreviewSliceRepresentationSPtr = std::shared_ptr<PreviewSliceRepresentation>;
  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_PREVIEW_SLICE_REPRESENTATION_H
