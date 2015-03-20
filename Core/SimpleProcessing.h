#pragma once

#include "itkImage.h"
#include "itkImageFileWriter.h"
#include "itkImageFileReader.h"
#include "itkImageRegionIterator.h"
#include "itkBinaryImageToShapeLabelMapFilter.h"

#include "itkRelabelComponentImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"

#include "itkExtractImageFilter.h"
#include "QuickView.h"
#include "itkCustomColormapFunction.h"
#include "itkScalarToRGBColormapImageFilter.h"
#include "itkRGBPixel.h"
#include "itkMersenneTwisterRandomVariateGenerator.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkThresholdImageFilter.h"

#include <ostream>
#include <random>

enum Verbosity {NONE, LOW, MEDIUM, HIGH};

template< typename TImageType >
int getSlice(const typename TImageType::Pointer& image, typename itk::Image< typename TImageType::PixelType, 2>::Pointer& slice, const unsigned int sliceNumber) {

  typedef itk::Image< typename TImageType::PixelType, 2> TImage2DType;
  typedef itk::ExtractImageFilter< TImageType, TImage2DType > ExtractFilterType;
  typename ExtractFilterType::Pointer extractFilter = ExtractFilterType::New();

  typename TImageType::RegionType inputRegion = image->GetLargestPossibleRegion();
  typename TImageType::SizeType size;
  size.Fill(0);
  size[0] = inputRegion.GetSize()[0];
  size[1] = inputRegion.GetSize()[1];


  typename TImageType::IndexType start;
  start.Fill(0);
  start[2] = sliceNumber;

  typename TImageType::RegionType desiredRegion;
  desiredRegion.SetSize(  size  );
  desiredRegion.SetIndex( start );

  extractFilter->SetInput(image);
  extractFilter->SetExtractionRegion( desiredRegion );
  extractFilter->SetDirectionCollapseToIdentity();

  try {
    extractFilter->Update();
  } catch( itk::ExceptionObject & error ) {
    std::cerr << __FILE__ << ":" << __LINE__ << " Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  slice = extractFilter->GetOutput();

  return EXIT_SUCCESS;
}

template< typename TImage >
int process(typename TImage::Pointer& outputImage, const std::string& filename, const Verbosity& verboseLevel, const float& threshold, const int& sizeThreshold) {

  const unsigned int Dimension = TImage::ImageDimension;

  typedef itk::Image< typename TImage::PixelType, 2 > Image2DType;
  typedef float FloatPixelType;
  typedef itk::Image< FloatPixelType, Dimension > FloatImageType;

  typedef itk::Image< FloatPixelType, 2 > FloatImage2DType;

  typedef itk::ImageFileReader< FloatImageType > ReaderFilterType;
  typedef itk::ImageFileWriter< TImage > WriterFilterType;


  typename ReaderFilterType::Pointer reader = ReaderFilterType::New();
  reader->SetFileName(filename);

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< FloatImageType > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("input.tif");
    vwriter->SetInput(reader->GetOutput());

    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }

  typedef itk::BinaryThresholdImageFilter < FloatImageType, TImage > FloatBinaryThresholdImageFilterType;

  typename FloatBinaryThresholdImageFilterType::Pointer binaryThresholdFilter
      = FloatBinaryThresholdImageFilterType::New();
  binaryThresholdFilter->SetInput(reader->GetOutput());
  binaryThresholdFilter->SetLowerThreshold(threshold);
  binaryThresholdFilter->SetInsideValue(255);
  binaryThresholdFilter->SetOutsideValue(0);

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< TImage > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("thresholded.tif");
    vwriter->SetInput(binaryThresholdFilter->GetOutput());

    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }

  typedef itk::BinaryImageToShapeLabelMapFilter<TImage> BinaryImageToShapeLabelMapFilterType;
  typename BinaryImageToShapeLabelMapFilterType::Pointer binaryImageToShapeLabelMapFilter = BinaryImageToShapeLabelMapFilterType::New();
  binaryImageToShapeLabelMapFilter->SetInput(binaryThresholdFilter->GetOutput());

  try {

     binaryImageToShapeLabelMapFilter->Update();

  } catch( itk::ExceptionObject & error ) {
    std::cerr << __FILE__ << ":" << __LINE__ << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  const unsigned int numObjects = binaryImageToShapeLabelMapFilter->GetOutput()->GetNumberOfLabelObjects();

  if(verboseLevel >= MEDIUM)
    std::cout << "Originally there are " << numObjects << " objects." << std::endl;

  if(numObjects == 0) {
    std::cerr << "0 objects present. Stop." << std::endl;
    return EXIT_FAILURE;
  }

  typedef typename BinaryImageToShapeLabelMapFilterType::OutputImageType::LabelObjectType LabelObjectType;

  std::vector<LabelObjectType*> objects;

  for(unsigned int i = 0; i < numObjects; i++)
  {
    LabelObjectType* labelObject = binaryImageToShapeLabelMapFilter->GetOutput()->GetNthLabelObject(i);

    if(labelObject->GetPhysicalSize() < sizeThreshold)
      continue;

    if(verboseLevel >= MEDIUM) {
      std::cout << "Object " << i << std::endl;
      std::cout << " Bounding box " << labelObject->GetBoundingBox() << std::endl;
      std::cout << " Centroid " << labelObject->GetCentroid() << std::endl;
      std::cout << " Volume " << labelObject->GetPhysicalSize() << std::endl;
    }
    objects.push_back(labelObject);
  }

  if(verboseLevel >= LOW)
    std::cout << "The number of objects over " << sizeThreshold << " voxels are " << objects.size() << " objects." << std::endl;

  if(objects.size() == 0){
    std::cerr << "0 objects remaining. Stop." << std::endl;
    return EXIT_FAILURE;
  }

  auto greaterV = [] (const LabelObjectType* lx, const LabelObjectType* rx) { return lx->GetPhysicalSize() < rx->GetPhysicalSize(); };

  std::sort(objects.begin(), objects.end(), greaterV);

  int volumeThreshold = objects.at(std::floor(objects.size() / 2 ))->GetPhysicalSize();

  if(verboseLevel >= LOW)
    std::cout << "The volume threshold is " << volumeThreshold << std::endl;

  std::ofstream smallObjFile ("smallObjects.txt");
  std::ofstream bigObjFile ("bigObjects.txt");

  for(int i = 0; i < objects.size(); i++) {
    if(i < objects.size()/2) {
      smallObjFile << objects[i]->GetPhysicalSize() << " " << objects[i]->GetCentroid() << std::endl;
    } else {
      bigObjFile << objects[i]->GetPhysicalSize() << " " << objects[i]->GetCentroid() << std::endl;
    }
  }
  smallObjFile.close();
  bigObjFile.close();

  outputImage = binaryThresholdFilter->GetOutput();

}

template< typename TImage >
int visualize(const typename TImage::Pointer& inputImage, const Verbosity& verboseLevel, const int& sizeThreshold) {
  // VISUALIZATION SHIT

  // use this if we don't need more than 255 values on the connected components number of objects
  //typedef ImageType BigImageType;
  const unsigned int Dimension = TImage::ImageDimension;
  typedef itk::Image< int, Dimension > BigImageType;

  typedef itk::RGBPixel<unsigned char> RGBPixelType;
  typedef itk::Image<RGBPixelType, Dimension>  RGBImageType;

  typedef itk::Image<RGBPixelType, 2>  RGBImage2DType;

  typedef itk::Function::CustomColormapFunction<
      typename TImage::PixelType, RGBPixelType > ColormapType;

  typedef float FloatPixelType;
  typedef itk::Image< FloatPixelType, Dimension > FloatImageType;

  typedef itk::Image< FloatPixelType, 2 > FloatImage2DType;

  typedef itk::Image< typename TImage::PixelType, 2> TImage2D;

  /*split the output into several segmentations*/
  //using unsigned char as output image limits us to 255 results
  typedef itk::ConnectedComponentImageFilter < TImage, BigImageType >
      ConnectedComponentImageFilterType;

  typename ConnectedComponentImageFilterType::Pointer connectedFilter = ConnectedComponentImageFilterType::New ();
  connectedFilter->SetInput(inputImage);

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< BigImageType > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("connected.mha");
    vwriter->SetInput(connectedFilter->GetOutput());
    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }


  typedef itk::RelabelComponentImageFilter< BigImageType, BigImageType> RelabelFilterType;
  typename RelabelFilterType::Pointer relabelFilter = RelabelFilterType::New();
  relabelFilter->SetMinimumObjectSize(sizeThreshold);
  relabelFilter->SetInput(connectedFilter->GetOutput());

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< BigImageType > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("relabeled.nrrd");
    vwriter->SetInput(relabelFilter->GetOutput());
    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }

  typedef itk::RescaleIntensityImageFilter< BigImageType > RescaleFilterType;
  typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
  rescaleFilter->SetInput(relabelFilter->GetOutput());
  rescaleFilter->SetOutputMinimum(100);
  rescaleFilter->SetOutputMaximum(255);

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< BigImageType > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("rescaled.nrrd");
    vwriter->SetInput(rescaleFilter->GetOutput());
    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }
  }


  typedef itk::ThresholdImageFilter < BigImageType > ThresholdImageFilterType;

  typename ThresholdImageFilterType::Pointer thresholdFilter
      = ThresholdImageFilterType::New();
  thresholdFilter->SetInput(rescaleFilter->GetOutput());
  thresholdFilter->ThresholdOutside(101, 255);
  thresholdFilter->SetOutsideValue(0);

  typedef itk::ScalarToRGBColormapImageFilter<BigImageType, RGBImageType> ColormapFilterType;
  typename ColormapFilterType::Pointer colormapFilter = ColormapFilterType::New();

  colormapFilter->SetColormap( ColormapFilterType::Hot );
  colormapFilter->SetInput(thresholdFilter->GetOutput());

  try {

    colormapFilter->Update();
    if(verboseLevel >= LOW)
      std::cout << "Number of objects: " << connectedFilter->GetObjectCount() << std::endl;

  } catch( itk::ExceptionObject & error ) {
    std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

  if(verboseLevel >= HIGH){
    typedef itk::ImageFileWriter< RGBImageType > vWriterFilterType;
    typename vWriterFilterType::Pointer vwriter = vWriterFilterType::New();
    vwriter->SetFileName("colored.tif");
    vwriter->SetInput(colormapFilter->GetOutput());

    try {
      vwriter->Update();
    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }

  }

  QuickView viewer;

  if(Dimension <= 2) {

    viewer.AddImage(inputImage.GetPointer(), true, "Original");
    viewer.AddRGBImage(colormapFilter->GetOutput(), true, "Relabeled");

  } else {

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, inputImage->GetLargestPossibleRegion().GetSize()[2] - 1 );

    const unsigned int sliceNumber = dis(gen);

    if(verboseLevel >= MEDIUM)
      std::cout << "Selected slice: " << sliceNumber << std::endl;

    typename TImage2D::Pointer slice1 = TImage2D::New();

    if(getSlice< TImage >(inputImage, slice1, sliceNumber) == EXIT_FAILURE)
      return EXIT_FAILURE;

    viewer.AddImage(slice1.GetPointer(), true, "Original");

    try {

      colormapFilter->Update();

    } catch( itk::ExceptionObject & error ) {
      std::cerr << __FILE__ << ":" << __LINE__ << "Error: " << error << std::endl;
      return EXIT_FAILURE;
    }

    RGBImage2DType::Pointer slice2 = RGBImage2DType::New();

    if(getSlice<RGBImageType>(colormapFilter->GetOutput(), slice2, sliceNumber) == EXIT_FAILURE)
        return EXIT_FAILURE;

    viewer.AddRGBImage(slice2.GetPointer(), true, "Relabeled");

  }

  try {

    viewer.Visualize();

  } catch( itk::ExceptionObject & error ) {
    std::cerr << __FILE__ << ":" << __LINE__ << " Error: " << error << std::endl;
    return EXIT_FAILURE;
  }

}

int main( int argc, char* argv[] )
{

  const unsigned int Dimension = 3;

  typedef unsigned char              PixelType;
  typedef itk::Image< PixelType, Dimension > ImageType;

  std::string filename = "../data/score.tif";
  //std::string filename = "../data/oneSlice.tif";

  Verbosity verboseLevel = HIGH;

  double threshold  = 50.0;
  int sizeThreshold = 500; /** minimum volume size threshold in pixels */

  if(argc == 1) {

    std::cout << "Usage ./selector filename [threshold]" << std::endl;

  } else if(argc >= 2) {

    filename = argv[1];

    std::ifstream f(filename.c_str());
    if (!f.good()) {
      std::cerr << "file " << filename << " does not exist" << std::endl;
      return EXIT_FAILURE;
    }
  }
  if(argc == 3) {

    //TODO deal with parsing error here
    threshold = atof(argv[2]);

  }

  std::cout << "Filename: " << filename << std::endl;

  ImageType::Pointer image = ImageType::New();

  process<ImageType>(image, filename, verboseLevel, threshold, sizeThreshold);
  visualize<ImageType>(image, verboseLevel, sizeThreshold);

  if(verboseLevel >= LOW)
    std::cout << "Job's Done. Have a Nice Day!" << std::endl;

  return EXIT_SUCCESS;
}
