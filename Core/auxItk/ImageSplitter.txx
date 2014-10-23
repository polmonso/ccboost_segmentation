#ifndef __itkImageSplitter_hxx
#define __itkImageSplitter_hxx
 
#include "ImageSplitter.h"
#include "itkObjectFactory.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
 
#include "itkExtractImageFilter.h"

#include "itkImageFileWriter.h"
#include "itkPasteImageFilter.h"

#include "itkImageFileReader.h"

namespace itk
{

template< class TImage>
void ImageSplitter< TImage >::SetNumSplits(const typename TImage::SizeType numSplits)
{
    this->numSplits = numSplits;
}

template< class TImage>
void ImageSplitter< TImage >::SetOverlap(const typename TImage::OffsetType overlap)
{
    this->overlap = overlap;
}

template< class TImage>
void ImageSplitter< TImage >::computeNumSplits(const typename TImage::SizeType originalSize,
                                                    const unsigned int numPredictRegions,
                                                    typename TImage::SizeType & numSplits) {
    typename TImage::SizeType pieceSize = originalSize;
    typedef typename TImage::SizeType mySizeType;
    const unsigned int Dimension = mySizeType::Dimension;
    for(int dim = 0; dim < Dimension; dim++)
        numSplits[dim] = 1;

    int numRegions = 1;
    while(numRegions < numPredictRegions ){
        int biggerDim = 0;
        for(int dim = 1; dim < Dimension; dim++)
            biggerDim = pieceSize[biggerDim] > pieceSize[dim] ? biggerDim : dim;

        numSplits[biggerDim]++;

        numRegions = 1;
        for(int dim = 0; dim < Dimension; dim++) {
            pieceSize[dim] = originalSize[dim]/numSplits[dim];
            numRegions *= numSplits[dim];
        }
    }
    std::cout << "numSplits " << numSplits << std::endl;
    std::cout << "pieceSize " << pieceSize << std::endl;
}

template< class TImage>
void ImageSplitter< TImage>::getCroppedRegions(const typename TImage::SizeType originalSize,
                       const typename TImage::OffsetType overlap,
                       const typename TImage::SizeType numSplits,
                       std::vector<typename TImage::RegionType>& cropRegions,
                       std::vector<typename TImage::RegionType>& pasteDRegions,
                       std::vector<typename TImage::RegionType>& pasteORegions) {

    typedef typename TImage::SizeType MySizeType;
    const unsigned int dim = MySizeType::Dimension;

    typename TImage::SizeType chunkSize;

    int numRegions = 1;

    for(unsigned int idim=0;idim<dim;idim++) {
        chunkSize[idim] = originalSize[idim]/numSplits[idim];
        assert(overlap[idim] < chunkSize[idim]);

        numRegions *= numSplits[idim];

    }

    for(int region = 0; region < numRegions; region++) {
        typename TImage::IndexType cropIndexO;
        typename TImage::IndexType cropIndexD;
        typename TImage::IndexType pasteDIndexO;
        typename TImage::IndexType pasteDIndexD;
        typename TImage::IndexType pasteOIndexO;
        typename TImage::IndexType pasteOIndexD;

        typename TImage::SizeType cropSize;
        typename TImage::SizeType pasteDSize;
        typename TImage::SizeType pasteOSize;

        for(unsigned int idim = 0; idim < dim; idim++){

            //remove the previous dimensions
            int count = region;
            for(int subidim = 0; subidim < idim; subidim++){
                count = count/numSplits[subidim];

            }
            count = count%numSplits[idim];

            bool lastsplit = false;
            if(count+1==numSplits[idim]) {
                lastsplit = true;
            }

            if(count == 0){
                cropIndexO[idim]   = 0;
                pasteDIndexO[idim] = 0;
                pasteOIndexO[idim] = 0;
            } else {
                cropIndexO[idim]   = count*chunkSize[idim] - overlap[idim];
                pasteDIndexO[idim] = count*chunkSize[idim];
                pasteOIndexO[idim] = overlap[idim];
            }

            if(count+1 == numSplits[idim]){
                cropIndexD[idim]   = originalSize[idim];
                pasteDIndexD[idim] = originalSize[idim];
                pasteOIndexD[idim] = pasteOIndexO[idim] + chunkSize[idim] + originalSize[idim]%chunkSize[idim];
            } else {
                cropIndexD[idim]   = cropIndexO[idim]   + chunkSize[idim] + overlap[idim];
                pasteDIndexD[idim] = pasteDIndexO[idim] + chunkSize[idim];
                pasteOIndexD[idim] = pasteOIndexO[idim] + chunkSize[idim];
            }

            cropSize[idim] = cropIndexD[idim] - cropIndexO[idim];
            pasteDSize[idim] = pasteDIndexD[idim] - pasteDIndexO[idim];
            pasteOSize[idim] = pasteOIndexD[idim] - pasteOIndexO[idim];

            //the chunk size will be chunkSize plus the residu if at last piece
            if(lastsplit){
                assert( pasteDSize[idim] == chunkSize[idim] + originalSize[idim]%chunkSize[idim] );
            } else {
                assert( pasteDSize[idim] == chunkSize[idim] );
            }
            assert( pasteOSize[idim] == pasteDSize[idim] );

        }

        typename TImage::RegionType cropRegion(cropIndexO, cropSize);
        typename TImage::RegionType pasteDRegion(pasteDIndexO, pasteDSize);
        typename TImage::RegionType pasteORegion(pasteOIndexO, pasteOSize);

        cropRegions.push_back(cropRegion);
        pasteDRegions.push_back(pasteDRegion);
        pasteORegions.push_back(pasteORegion);

    }

    //TODO remove this
    //output = pasteCroppedRegions(input, rawSize, overlap, numSplits, cropRegions, pasteDRegions, pasteORegions);
    //saveCroppedRegions(input, cropRegions);

}

template< class TImage>
void ImageSplitter< TImage>::saveCroppedRegions(const typename TImage::ConstPointer rawVolume,
                                                       const std::vector<typename TImage::RegionType> cropRegions) {

    typedef itk::ExtractImageFilter< TImage, TImage > ExtractFilterType;
    typename ExtractFilterType::Pointer extract = ExtractFilterType::New();
    extract->SetInput(rawVolume);
#if ITK_VERSION_MAJOR >= 4
    extract->SetDirectionCollapseToIdentity(); // This is required.
#endif

    typedef itk::ImageFileWriter< TImage > WriterType;
    typename WriterType::Pointer writer = WriterType::New();


    for(int i=0; i < cropRegions.size(); i++){

        //*Note: indexes are preserved http://public.kitware.com/pipermail/insight-users/2006-June/018175.html
        //TODO set them to 0?

        extract->SetExtractionRegion(cropRegions.at(i));
        extract->UpdateLargestPossibleRegion(); //necessary to reset indexes
        extract->Update();

        std::stringstream filename;
        filename << "cropRegion-"<< i << ".tif";

        //extract->GetOutput()->Print(std::cout);

        writer->SetInput(extract->GetOutput());
        writer->SetFileName(filename.str().c_str());
        writer->Update();
    }

}

//TODO does TImage need to be float? (as it originally was in the previous code)
template< class TImage>
void ImageSplitter< TImage>::pasteCroppedRegions(const typename TImage::ConstPointer rawVolume,
                                                       const typename TImage::SizeType originalSize,
                                                       const typename TImage::OffsetType overlap,
                                                       const typename TImage::SizeType numSplits,
                                                       std::vector<typename TImage::RegionType>& cropRegions,
                                                       std::vector<typename TImage::RegionType>& pasteDRegions,
                                                       std::vector<typename TImage::RegionType>& pasteORegions){

    typename TImage::RegionType rawRegion = rawVolume->GetLargestPossibleRegion();

    typename TImage::SizeType rawSize = rawVolume->GetLargestPossibleRegion().GetSize();

    typename TImage::Pointer outputImage = TImage::New();
    typename TImage::RegionType region;
    typename TImage::IndexType index;
    index = rawRegion.GetIndex();
    region.SetIndex(index);
    region.SetSize(rawRegion.GetSize());
    outputImage->SetRegions(rawRegion);
    outputImage->Allocate();

    typedef typename TImage::SizeType MySizeType;
    int numRegions = 1;
    for(int i=0; i < MySizeType::Dimension; i++) {
        numRegions *= numSplits[i];
    }

    for(int i=0; i<numRegions; i++){

        typedef itk::ImageFileReader< TImage > ReaderType;
        typename ReaderType::Pointer floatReader = ReaderType::New();

        std::stringstream filename;
        filename << "cropRegion-"<< i << ".tif";

        floatReader->SetFileName(filename.str().c_str());
        floatReader->Update();
        typename TImage::Pointer cropImage = floatReader->GetOutput();

        typedef itk::PasteImageFilter <TImage, TImage >
          PasteImageFilterType;

        //TODO copy the volume to the output volume
        typename PasteImageFilterType::Pointer pasteFilter = PasteImageFilterType::New ();
        pasteFilter->SetSourceImage(cropImage);
        pasteFilter->SetDestinationImage(outputImage);

        pasteFilter->SetSourceRegion(pasteORegions.at(i));
        pasteFilter->SetDestinationIndex(pasteDRegions.at(i).GetIndex());

#ifdef DEBUG
        std::cout << "Pasting information: " << std::endl;
        std::cout << "Output Image size " << outputImage->GetLargestPossibleRegion() << std::endl;
        std::cout << "crop Image Origin Image region " << cropImage->GetLargestPossibleRegion() << std::endl;
        std::cout << "paste Origin Region " << pasteORegions.at(i) << std::endl;
        std::cout << "paste Destination Region " << pasteDRegions.at(i) << std::endl;
        std::cout << "paste Destination Region Index " << pasteDRegions.at(i).GetIndex() << std::endl;
#endif
        try {
          pasteFilter->Update();
        } catch( itk::ExceptionObject & err ) {
          std::cerr << "ExceptionObject caught when pasting!" << std::endl;
          std::cerr << err << std::endl;
          return;
        }

        outputImage = pasteFilter->GetOutput();
        outputImage->DisconnectPipeline();

        typename  itk::ImageFileWriter< TImage>::Pointer floatWriter = itk::ImageFileWriter< TImage>::New();

        std::stringstream filenameOut;
        filenameOut << "pastedOutput" << i << ".tif";
        floatWriter->SetFileName(filenameOut.str().c_str());
        floatWriter->SetInput( outputImage );

        try {
          floatWriter->Update();
        } catch( itk::ExceptionObject & err ) {
          std::cerr << "ExceptionObject caught when writing!" << std::endl;
          std::cerr << err << std::endl;
          return;
        }
    }
}

//TODO does TImage need to be float? (as it originally was in the previous code)
template< class TImage>
void ImageSplitter< TImage>::pasteCroppedRegionsFromDisk(const typename TImage::ConstPointer rawVolume,
                                                       const typename TImage::SizeType originalSize,
                                                       const typename TImage::OffsetType overlap,
                                                       const typename TImage::SizeType numSplits,
                                                       std::vector<typename TImage::RegionType>& cropRegions,
                                                       std::vector<typename TImage::RegionType>& pasteDRegions,
                                                       std::vector<typename TImage::RegionType>& pasteORegions){

    typename TImage::RegionType rawRegion = rawVolume->GetLargestPossibleRegion();

    typename TImage::SizeType rawSize = rawVolume->GetLargestPossibleRegion().GetSize();

    typename TImage::Pointer outputImage = TImage::New();
    typename TImage::RegionType region;
    typename TImage::IndexType index;
    index = rawRegion.GetIndex();
    region.SetIndex(index);
    region.SetSize(rawRegion.GetSize());
    outputImage->SetRegions(rawRegion);
    outputImage->Allocate();

    typedef typename TImage::SizeType MySizeType;
    int numRegions = 1;
    for(int i=0; i < MySizeType::Dimension; i++) {
        numRegions *= numSplits[i];
    }

    for(int i=0; i<numRegions; i++){

        typedef itk::ImageFileReader< TImage > ReaderType;
        typename ReaderType::Pointer floatReader = ReaderType::New();

        std::stringstream filename;
        filename << "cropRegion-"<< i << ".tif";

        floatReader->SetFileName(filename.str().c_str());
        floatReader->Update();
        typename TImage::Pointer cropImage = floatReader->GetOutput();

        typedef itk::PasteImageFilter <TImage, TImage >
          PasteImageFilterType;

        //TODO copy the volume to the output volume
        typename PasteImageFilterType::Pointer pasteFilter = PasteImageFilterType::New ();
        pasteFilter->SetSourceImage(cropImage);
        pasteFilter->SetDestinationImage(outputImage);

        pasteFilter->SetSourceRegion(pasteORegions.at(i));
        pasteFilter->SetDestinationIndex(pasteDRegions.at(i).GetIndex());

#ifdef DEBUG
        std::cout << "Pasting information: " << std::endl;
        std::cout << "Output Image size " << outputImage->GetLargestPossibleRegion() << std::endl;
        std::cout << "crop Image Origin Image region " << cropImage->GetLargestPossibleRegion() << std::endl;
        std::cout << "paste Origin Region " << pasteORegions.at(i) << std::endl;
        std::cout << "paste Destination Region " << pasteDRegions.at(i) << std::endl;
        std::cout << "paste Destination Region Index " << pasteDRegions.at(i).GetIndex() << std::endl;
#endif
        try {
          pasteFilter->Update();
        } catch( itk::ExceptionObject & err ) {
          std::cerr << "ExceptionObject caught when pasting!" << std::endl;
          std::cerr << err << std::endl;
          return;
        }

        outputImage = pasteFilter->GetOutput();
        outputImage->DisconnectPipeline();

        typename  itk::ImageFileWriter< TImage>::Pointer floatWriter = itk::ImageFileWriter< TImage>::New();

        std::stringstream filenameOut;
        filenameOut << "pastedOutput" << i << ".tif";
        floatWriter->SetFileName(filenameOut.str().c_str());
        floatWriter->SetInput( outputImage );

        try {
          floatWriter->Update();
        } catch( itk::ExceptionObject & err ) {
          std::cerr << "ExceptionObject caught when writing!" << std::endl;
          std::cerr << err << std::endl;
          return;
        }
    }
}

}// end namespace
 
 
#endif
