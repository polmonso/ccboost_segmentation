#ifndef __itkSplitterImageFilter_hxx
#define __itkSplitterImageFilter_hxx
 
#include "SplitterImageFilter.h"
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
SplitterImageFilter< TImage>::SplitterImageFilter() : selectedRegion(0), WorkAsSplitter(true), needUpdate(true)
{
}

template< class TImage>
void SplitterImageFilter< TImage>::SetWorkAsSplitter(bool split)
{
    this->WorkAsSplitter = split;
}

template< class TImage>
void SplitterImageFilter< TImage>::SetNumSplits(const typename TImage::SizeType numSplits)
{
    this->numSplits = numSplits;
    bool needUpdate = true;
}

template< class TImage>
void SplitterImageFilter< TImage>::SetOverlap(const typename TImage::OffsetType overlap)
{
    this->overlap = overlap;
    bool needUpdate = true;
}

template< class TImage>
void SplitterImageFilter< TImage>::GenerateOutputInformation()
{
    Superclass::GenerateOutputInformation();

//    typename TImage::ConstPointer  inputImage  = this->GetInput();
//    typename TImage::Pointer       outputImage = this->GetOutput();

//    typename TImage::SizeType rawSize = inputImage->GetLargestPossibleRegion().GetSize();

//    if(needUpdate) {
//        getCroppedRegions(rawSize, overlap, numSplits, cropRegions, pasteDRegions, pasteORegions);
//        needUpdate = false;
//    }

//    std::cout << rawSize << std::endl;
//    std::cout << cropRegions.at(selectedRegion) << std::endl;

//    outputImage->SetLargestPossibleRegion( cropRegions.at(selectedRegion) );
//    outputImage->SetSpacing( inputImage->GetSpacing() );
//    outputImage->SetOrigin( inputImage->GetOrigin() );
//    outputImage->SetDirection( inputImage->GetDirection() );
}

template< class TImage>
void SplitterImageFilter< TImage>::GenerateData()
{
  typename TImage::ConstPointer input = this->GetInput();
  typename TImage::Pointer output = this->GetOutput();
  
  typename TImage::SizeType rawSize = input->GetLargestPossibleRegion().GetSize();

  //TODO base-class <-- splitter/merger
  std::cerr << "splitter? " << WorkAsSplitter << std::endl;
  if(WorkAsSplitter) {

      if(needUpdate) {
          getCroppedRegions(rawSize, overlap, numSplits, cropRegions, pasteDRegions, pasteORegions);
          needUpdate = false;
      }

      output->SetRegions(cropRegions.at(selectedRegion));
      output->SetLargestPossibleRegion( cropRegions.at(selectedRegion) );
      this->AllocateOutputs();

      ImageAlgorithm::Copy(input.GetPointer(), output.GetPointer(), output->GetRequestedRegion(),
                             output->GetRequestedRegion() );

      typedef itk::ImageFileWriter< TImage > NormalWriterType;
      typename NormalWriterType::Pointer writer = NormalWriterType::New();
      writer->SetInput( output );
      std::stringstream filename1;
      filename1 << "selregion"<< selectedRegion << ".tif";

      writer->SetFileName(filename1.str().c_str());

      writer->Update();

      //saveCroppedRegions(input, cropRegions);

  } else { //WorkAsMerger

      //output = pasteCroppedRegions(input, rawSize, overlap, numSplits, cropRegions, pasteDRegions, pasteORegions);

  }

//  saveCroppedRegions(input, cropRegions);


}

template< class TImage>
void SplitterImageFilter< TImage>::computeNumSplits(const typename TImage::SizeType originalSize,
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
void SplitterImageFilter< TImage>::getCroppedRegions(const typename TImage::SizeType originalSize,
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


//template< class TImage>
//void SplitterImageFilter< TImage>::getCroppedRegions(const typename TImage::SizeType originalSize,
//                       const typename TImage::OffsetType overlap,
//                       const typename TImage::SizeType numSplits,
//                       std::vector<typename TImage::RegionType>& cropRegions,
//                       std::vector<typename TImage::RegionType>& pasteDRegions,
//                       std::vector<typename TImage::RegionType>& pasteORegions){

//    typedef typename TImage::SizeType MySizeType;
//    int dim = MySizeType::Dimension;
//    assert(dim == 3);

//    typename TImage::SizeType chunkSize;
//    chunkSize[0] = originalSize[0]/numSplits[0];
//    chunkSize[1] = originalSize[1]/numSplits[1];
//    chunkSize[2] = originalSize[2]/numSplits[2];

//    assert(overlap[0] < chunkSize[0]);
//    assert(overlap[1] < chunkSize[1]);
//    assert(overlap[2] < chunkSize[2]);

//    typename TImage::IndexType cropIndexO;
//    typename TImage::IndexType cropIndexD;
//    typename TImage::IndexType pasteDIndexO;
//    typename TImage::IndexType pasteDIndexD;
//    typename TImage::IndexType pasteOIndexO;
//    typename TImage::IndexType pasteOIndexD;

//    for(int k=0; k < numSplits[2]; k++){

//        if(k == 0){
//            cropIndexO[2]   = 0;
//            pasteDIndexO[2] = 0;
//            pasteOIndexO[2] = 0;
//        } else {
//            cropIndexO[2]   = k*chunkSize[2] - overlap[2];
//            pasteDIndexO[2] = k*chunkSize[2];
//            pasteOIndexO[2] = overlap[2];
//        }

//        if(k+1 == numSplits[2]){
//            cropIndexD[2]   = originalSize[2];
//            pasteDIndexD[2] = originalSize[2];
//            pasteOIndexD[2] = pasteOIndexO[2] + chunkSize[2] + originalSize[2]%chunkSize[2];
//            std::cout << "k " << k << " numSplits " << numSplits[2]
//                      << " pOiO " << pasteOIndexO[2] << " pOiD " << pasteOIndexD[2]
//                      << " cS " << chunkSize[2] << std::endl;
//        } else {
//            cropIndexD[2]   = cropIndexO[2]   + chunkSize[2] + overlap[2];
//            pasteDIndexD[2] = pasteDIndexO[2] + chunkSize[2];
//            pasteOIndexD[2] = pasteOIndexO[2] + chunkSize[2];
//        }

//        for(int j=0; j < numSplits[1]; j++){

//            if(j == 0){
//                cropIndexO[1]   = 0;
//                pasteDIndexO[1] = 0;
//                pasteOIndexO[1] = 0;
//            } else {
//                cropIndexO[1]   = j*chunkSize[1] - overlap[1];
//                pasteDIndexO[1] = j*chunkSize[1];
//                pasteOIndexO[1] = overlap[1];
//            }

//            if(j+1 == numSplits[1]){
//                cropIndexD[1]   = originalSize[1];
//                pasteDIndexD[1] = originalSize[1];
//                pasteOIndexD[1] = pasteOIndexO[1] + chunkSize[1];
//            } else {
//                cropIndexD[1]   = cropIndexO[1]   + chunkSize[1] + overlap[1];
//                pasteDIndexD[1] = pasteDIndexO[1] + chunkSize[1];
//                pasteOIndexD[1] = pasteOIndexO[1] + chunkSize[1];
//            }

//            for(int i=0; i < numSplits[0]; i++){

//                if( i==0 ){
//                    cropIndexO[0]   = 0;
//                    pasteDIndexO[0] = 0;
//                    pasteOIndexO[0] = 0;
//                } else {
//                    cropIndexO[0]   = i*chunkSize[0] - overlap[0];
//                    pasteDIndexO[0] = i*chunkSize[0];
//                    pasteOIndexO[0] = overlap[0];
//                }

//                if(i+1 == numSplits[0]){
//                    cropIndexD[0]   = originalSize[0];
//                    pasteDIndexD[0] = originalSize[0];
//                    pasteOIndexD[0] = pasteOIndexO[0] + chunkSize[0];
//                } else {
//                    cropIndexD[0]   = cropIndexO[0]   + chunkSize[0] + overlap[0];
//                    pasteDIndexD[0] = pasteDIndexO[0] + chunkSize[0];
//                    pasteOIndexD[0] = pasteOIndexO[0] + chunkSize[0];
//                }

//                typename TImage::SizeType cropSize;

//                cropSize[0] = cropIndexD[0] - cropIndexO[0];
//                cropSize[1] = cropIndexD[1] - cropIndexO[1];
//                cropSize[2] = cropIndexD[2] - cropIndexO[2];

//                typename TImage::SizeType pasteDSize;
//                pasteDSize[0] = pasteDIndexD[0] - pasteDIndexO[0];
//                pasteDSize[1] = pasteDIndexD[1] - pasteDIndexO[1];
//                pasteDSize[2] = pasteDIndexD[2] - pasteDIndexO[2];

//                typename TImage::SizeType pasteOSize;
//                pasteOSize[0] = pasteOIndexD[0] - pasteOIndexO[0];
//                pasteOSize[1] = pasteOIndexD[1] - pasteOIndexO[1];
//                pasteOSize[2] = pasteOIndexD[2] - pasteOIndexO[2];

//                assert( pasteDSize[0] == chunkSize[0] + (originalSize[0]%chunkSize[0])*(i+1==numSplits[0]));
//                assert( pasteDSize[1] == chunkSize[1] + (originalSize[1]%chunkSize[1])*(j+1==numSplits[1]));
//                assert( pasteDSize[2] == chunkSize[2] + (originalSize[2]%chunkSize[2])*(k+1==numSplits[2]));

//                //the chunk size will be chunkSize plus the residu if at last piece
//                assert( pasteOSize[0] == chunkSize[0] + (originalSize[0]%chunkSize[0])*(i+1==numSplits[0]) );
//                assert( pasteOSize[1] == chunkSize[1] + (originalSize[1]%chunkSize[1])*(j+1==numSplits[1]) );
//                assert( pasteOSize[2] == chunkSize[2] + (originalSize[2]%chunkSize[2])*(k+1==numSplits[2]) );

//                typename TImage::RegionType cropRegion(cropIndexO, cropSize);
//                cropRegions.push_back(cropRegion);
//                typename TImage::RegionType pasteDRegion(pasteDIndexO, pasteDSize);
//                pasteDRegions.push_back(pasteDRegion);
//                typename TImage::RegionType pasteORegion(pasteOIndexO, pasteOSize);
//                pasteORegions.push_back(pasteORegion);
//            }
//        }
//    }
//}

template< class TImage>
void SplitterImageFilter< TImage>::saveCroppedRegions(const typename TImage::ConstPointer rawVolume,
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
void SplitterImageFilter< TImage>::pasteCroppedRegions(const typename TImage::ConstPointer rawVolume,
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
void SplitterImageFilter< TImage>::pasteCroppedRegionsFromDisk(const typename TImage::ConstPointer rawVolume,
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
