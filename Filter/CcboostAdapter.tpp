#pragma once

#include "CcboostAdapter.h"

#include <itkBinaryImageToShapeLabelMapFilter.h>
#include <itkShapeOpeningLabelMapFilter.h>


template< typename TImageType >
void CcboostAdapter::removeborders(const ConfigData< TImageType >& cfgData,
                                   typename TImageType::Pointer& outputSegmentation) {

    removeborders< TImageType >(outputSegmentation,
                                cfgData.saveIntermediateVolumes, cfgData.cacheDir);

}

template< typename TImageType >
void CcboostAdapter::removeborders(typename TImageType::Pointer& outputSegmentation,
                                   bool saveIntermediateVolumes,
                                   std::string cacheDir) {

    typedef itk::ImageFileWriter< TImageType > TWriterType;

    qDebug("remove borders");
    typedef itk::MinimumMaximumImageCalculator <TImageType>
            ImageCalculatorFilterType;

    typename ImageCalculatorFilterType::Pointer imageCalculatorFilter
            = ImageCalculatorFilterType::New ();
    imageCalculatorFilter->SetImage(outputSegmentation);
    imageCalculatorFilter->Compute();
    typename TImageType::PixelType minimum = imageCalculatorFilter->GetMinimum();
    //typename TImageType::PixelType minimum = -itk::NumericTraits< typename TImageType::PixelType >::max();


    typename TImageType::SizeType outputSize = outputSegmentation->GetLargestPossibleRegion().GetSize();

    typename TImageType::SpacingType spacing = outputSegmentation->GetSpacing();

    double zAnisotropyFactor = 2*spacing[2]/(spacing[0] + spacing[1]);

    const int borderToRemove = 2;
    const int borderZ = std::max((int)1, (int)(borderToRemove/zAnisotropyFactor + 0.5));
    const int borderOther = borderToRemove;

    // this is dimension-agnostic, though now we need to know which one is Z to handle anisotropy
    const unsigned zIndex = 2;

    const int numDim = TImageType::ImageDimension;

    // go through each dimension and zero it out
    for (unsigned dim=0; dim < numDim; dim++)
    {
        const int thisBorder = (dim == zIndex) ? borderZ : borderOther;

        const int length = outputSize[dim];

        // check if image is too small, then we have to avoid over/underflows
        const unsigned toRemove = std::min( length, thisBorder );

        typename TImageType::SizeType   regionSize = outputSegmentation->GetLargestPossibleRegion().GetSize();
        regionSize[dim] = toRemove;

        typename TImageType::IndexType   regionIndex = outputSegmentation->GetLargestPossibleRegion().GetIndex();

        typename TImageType::RegionType region;
        region.SetSize(regionSize);
        region.SetIndex(regionIndex);

        // first the low part of the border
        {
            itk::ImageRegionIterator<TImageType> imageIterator(outputSegmentation, region);
            while(!imageIterator.IsAtEnd())
            {
                // set to minimum value
                imageIterator.Set(minimum);
                ++imageIterator;
            }
        }

        // now the last part (just offset in [dim])
        {
            regionIndex[dim] = outputSize[dim] - toRemove;
            region.SetIndex(regionIndex);

            itk::ImageRegionIterator<TImageType> imageIterator(outputSegmentation, region);
            while(!imageIterator.IsAtEnd())
            {
                // set to zero
                imageIterator.Set(minimum);
                ++imageIterator;
            }
        }
    }

    typename TWriterType::Pointer writer = TWriterType::New();
    if(saveIntermediateVolumes) {
        writer->SetFileName(cacheDir + "3" + "ccOutputSegmentation-noborders.tif");
        writer->SetInput(outputSegmentation);
        writer->Update();
        qDebug("cc boost segmentation saved");
    }

}

//throws ItkException
template< typename TImageType >
void CcboostAdapter::removeSmallComponents(typename TImageType::Pointer& segmentation, int minCCSize)
{

    // Create a ShapeLabelMap from the image
    typedef itk::BinaryImageToShapeLabelMapFilter<TImageType> BinaryImageToShapeLabelMapFilterType;
    typename BinaryImageToShapeLabelMapFilterType::Pointer binaryImageToShapeLabelMapFilter = BinaryImageToShapeLabelMapFilterType::New();
    binaryImageToShapeLabelMapFilter->SetInput(segmentation);

    // Remove label objects that have a physical size less than minCCSize
    typedef itk::ShapeOpeningLabelMapFilter< typename BinaryImageToShapeLabelMapFilterType::OutputImageType > ShapeOpeningLabelMapFilterType;
    typename ShapeOpeningLabelMapFilterType::Pointer shapeOpeningLabelMapFilter = ShapeOpeningLabelMapFilterType::New();
    shapeOpeningLabelMapFilter->SetInput( binaryImageToShapeLabelMapFilter->GetOutput() );
    shapeOpeningLabelMapFilter->SetLambda( minCCSize );
    shapeOpeningLabelMapFilter->ReverseOrderingOff();
    shapeOpeningLabelMapFilter->SetAttribute( ShapeOpeningLabelMapFilterType::LabelObjectType::PHYSICAL_SIZE);

    // Create a label image
    typedef itk::LabelMapToLabelImageFilter<typename BinaryImageToShapeLabelMapFilterType::OutputImageType, TImageType> LabelMapToLabelImageFilterType;
    typename LabelMapToLabelImageFilterType::Pointer labelMapToLabelImageFilter = LabelMapToLabelImageFilterType::New();
    labelMapToLabelImageFilter->SetInput(shapeOpeningLabelMapFilter->GetOutput());
    labelMapToLabelImageFilter->Update();

    segmentation = labelMapToLabelImageFilter->GetOutput();
    segmentation->DisconnectPipeline();

}
