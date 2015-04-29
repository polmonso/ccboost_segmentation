#pragma once

#include <itkLabelMap.h>
#include <itkShapeLabelObject.h>
#include <itkImage.h>
#include <itkLabelMapToLabelImageFilter.h>

//#ifdef ESPINATYPES_H
//#include "Core/EspinaTypes.h"
//#else
//for standalone version we input itkVolumeType ourselves
//FIXME this will define the typedef to every program that uses this header
typedef itk::Image<unsigned char, 3> itkVolumeType;
typedef itk::Image<float, 3> FloatTypeImage;
//#endif

//FIXME how to define our namespace but prevent having to add ESPINA:: everywhere?
namespace ESPINA {
namespace CCB {

    typedef itk::LabelMap< itk::ShapeLabelObject< itk::SizeValueType, itkVolumeType::ImageDimension > > LabelMapType;
    typedef itk::LabelMapToLabelImageFilter<LabelMapType, itkVolumeType> LabelMap2VolumeFilterType;

}
}
