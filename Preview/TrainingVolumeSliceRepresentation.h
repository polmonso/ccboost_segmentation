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

#ifndef ESPINA_RAS_TRAINING_VOLUME_SLICE_REPRESENTATION_H
#define ESPINA_RAS_TRAINING_VOLUME_SLICE_REPRESENTATION_H

#include <QObject>

#include "types.h"
#include <Label.h>

#include <Core/Utils/Spatial.h>
#include <GUI/View/View2D.h>

#include <itkExtractImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkVolume.h>
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>

namespace ESPINA {
  namespace RAS {

    class TrainingVolumeSliceRepresentation
    : public QObject
    {
      Q_OBJECT

    public:
      explicit TrainingVolumeSliceRepresentation(View2D *view);
      virtual ~TrainingVolumeSliceRepresentation();

      void addTrainingVolume(Label::TrainingVolume volume);

      void removeTrainingVolume(Label::TrainingVolume volume);

      void discardTrainingVolumes();

      void setColor(const QColor& color);

      void setOpacity(float opacity);

      void setVisibility(bool value);

      void update();

    private slots:
      void setSlice(Plane plane, Nm pos);

    private:
      using ExtractFilterType       = itk::ExtractImageFilter<LabelImageType, itk::Image<unsigned int, 2>>;
      using Itk2vtkFilterType       = itk::ImageToVTKImageFilter<itk::Image<unsigned int, 2>>;
      using LUTSPtr                 = vtkSmartPointer<vtkLookupTable>;
      using vtkImageMapToColorsSPtr = vtkSmartPointer<vtkImageMapToColors>;

      struct Representation
      {
        Representation(Label::TrainingVolume volume, LUTSPtr lut);

        Label::TrainingVolume Volume;

        vtkImageMapToColorsSPtr MapToColors;
        vtkImageActor*          Actor;

        bool operator==(const Representation& rhs) const
        { return Volume == rhs.Volume; }

      };

      View2D* m_view;

      QColor  m_color;
      float   m_opacity;
      bool    m_isVisible;
      LUTSPtr m_lut;

      QList<Representation> m_representations;

      Nm    m_pos;
      Plane m_plane;
      int   m_planeIndex;
    };

    using TrainingVolumeSliceRepresentationSPtr = std::shared_ptr<TrainingVolumeSliceRepresentation>;
  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_TRAINING_VOLUME_SLICE_REPRESENTATION_H
