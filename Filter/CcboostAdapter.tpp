#pragma once

#include "CcboostAdapter.h"

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
                // set to zero
                imageIterator.Set(0);
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
                imageIterator.Set(0);
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
