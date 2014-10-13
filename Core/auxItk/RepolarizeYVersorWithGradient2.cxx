/************************************************************************/
/* (c) 2012-2013 Ecole Polytechnique Federale de Lausanne               */
/* All rights reserved.                                                 */
/*                                                                      */
/* EPFL grants a non-exclusive and non-transferable license for non     */
/* commercial use of the Software for education and research purposes   */
/* only. Any other use of the Software is expressly excluded.           */
/*                                                                      */
/* Redistribution of the Software in source and binary forms, with or   */
/* without modification, is not permitted.                              */
/*                                                                      */
/* Written by Carlos Becker                                             */
/*                                                                      */
/* http://cvlab.epfl.ch/software/synapse                                */
/* Contact <carlos.becker@epfl.ch> for comments & bug reports.          */
/************************************************************************/

#include "itkImageFileWriter.h"
#include "itkImageFileReader.h"
#include "itkGradientRecursiveGaussianImageFilter.h"
#include "itkImage.h"

#include <itkImageRegionIteratorWithIndex.h>

#include "RepolarizeYVersorWithGradient2.h"
#include "itkImageDuplicator.h"

  typedef unsigned char         InputPixelType;
  typedef float                 ComponentType;
  const   unsigned int          Dimension = 3;

  typedef itk::CovariantVector< ComponentType, Dimension > GradientPixelType;
 
  typedef itk::Image< InputPixelType, Dimension >     InputImageType;
  typedef itk::Image< GradientPixelType, Dimension >  GradientImageType;
  
  typedef itk::VectorImage< float, Dimension >        VectorImageType;
 
  typedef itk::ImageFileReader< InputImageType  >  ReaderType;
  typedef itk::ImageFileWriter< VectorImageType >  WriterType;
  
  typedef itk::ImageFileReader< VectorImageType  >  EigVecReaderType;

 typedef itk::ImageDuplicator< InputImageType > DuplicatorType;

 typedef itk::ImageDuplicator< VectorImageType > VectorDuplicatorType;


/**
 * This filter will read an image + its eigenvector of hessian image
 * and for each pixel
 *     	1) do dot product between gradient vector and 2nd eigenvector
 *     	2) if the dot product is negative, invert 2nd eigenvector
 *     	3) save
 */

int RepolarizeYVersorWithGradient2Execute(float sigma, const itk::Image<unsigned char, 3>::Pointer image,
                                       std::string outputFile,  std::string eigVecImageFile, float zAnisotropyFactor)
{  
 
   
    DuplicatorType::Pointer duplicator = DuplicatorType::New();
    duplicator->SetInputImage(image);
    duplicator->Update();
    InputImageType::Pointer inpImg = duplicator->GetOutput();
    inpImg->DisconnectPipeline();
    InputImageType::SpacingType spacing = inpImg->GetSpacing();
    spacing[2] = zAnisotropyFactor;
  
  // input image
    {
        InputImageType::SpacingType spacing = inpImg->GetSpacing();
        spacing[2] = zAnisotropyFactor;

        std::cout << "Using spacing: " << spacing << ", anisotr factor = " << zAnisotropyFactor << std::endl;
        inpImg->SetSpacing(spacing);
    }

    std::cout << "Reading image " << eigVecImageFile << std::endl;

    EigVecReaderType::Pointer eigvecReader = EigVecReaderType::New();
    eigvecReader->SetFileName( eigVecImageFile );
    eigvecReader->Update();

     // eigvec image
    VectorImageType::Pointer eigVecImg = eigvecReader->GetOutput();
    {
        VectorImageType::SpacingType spacing = eigVecImg->GetSpacing();
        spacing[2] = zAnisotropyFactor;

        std::cout << "Using spacing: " << spacing << ", anisotr factor = " << zAnisotropyFactor << std::endl;
        eigVecImg->SetSpacing(spacing);
    }
  
    
  // check that images are same size
  {
      InputImageType::RegionType region1 = inpImg->GetLargestPossibleRegion();
      InputImageType::RegionType region2 = eigVecImg->GetLargestPossibleRegion();
      
      if ((region1.GetSize()[0] != region2.GetSize()[0]) || (region1.GetSize()[1] != region2.GetSize()[1]) || (region1.GetSize()[2] != region2.GetSize()[2]))
      {
    	  printf("Region sizes don't match. Aborting..\n");
    	  return -1;
      }
      
      if (eigVecImg->GetNumberOfComponentsPerPixel() != 9)
      {
    	  printf("Components per pixel of eigvector image must be 9, %d found\n", eigVecImg->GetNumberOfComponentsPerPixel());
    	  return -1;
      }
  }
  
  //Filter class is instantiated
  typedef itk::GradientRecursiveGaussianImageFilter<InputImageType, GradientImageType> GradientFilterType;
 
  GradientFilterType::Pointer filter = GradientFilterType::New();
 
  filter->SetSigma( sigma );  
  filter->SetInput( inpImg );
  filter->Update();
  
  // let's do the processing manually and modify the image in eigvecReader
  VectorImageType::Pointer   evImg = eigVecImg;
  GradientImageType::Pointer grImg = filter->GetOutput();
  


  itk::ImageRegionIteratorWithIndex<VectorImageType> iter(evImg, evImg->GetLargestPossibleRegion());
  itk::ImageRegionIteratorWithIndex<GradientImageType> grImgIter(grImg, grImg->GetLargestPossibleRegion());

  for( iter.GoToBegin(), grImgIter.GoToBegin(); !iter.IsAtEnd(); ++iter, ++grImgIter){

      GradientPixelType &grad = grImg->GetPixel( grImgIter.GetIndex() );

      // no reference :(
      VectorImageType::PixelType ev = iter.Get();

      // dot product with 2nd eigvec
      float dotP = grad[0] * ev[3] + grad[1] * ev[4] + grad[2] * ev[5];

      if (dotP < 0)
      {
          // revert that eigvec
          ev[3] = -ev[3];
          ev[4] = -ev[4];
          ev[5] = -ev[5];

          iter.Set(ev);
      }

  }

  /*
  while(!iter.IsAtEnd())
  {

    VectorImageType::IndexType index = iter.GetIndex();

    GradientPixelType &grad = grImg->GetPixel( iter.GetIndex() );
    
    // no reference :(
    VectorImageType::PixelType ev = iter.Get();
    
    float a = grad[0];

    // dot product with 2nd eigvec
    float dotP = grad[0] * ev[3] + grad[1] * ev[4] + grad[2] * ev[5];
 
    if (dotP < 0)
    {
    	// revert that eigvec
    	ev[3] = -ev[3];
    	ev[4] = -ev[4];
    	ev[5] = -ev[5];
    	
    	iter.Set(ev);
    }
 
    ++iter;
  }
 */
  
  // re-set spacing before writing to disk
  {
    VectorImageType::SpacingType spacing;
    spacing[0] = spacing[1] = spacing[2] = 1.0;
        
    evImg->SetSpacing( spacing );

//    VectorImageType::RegionType region;
//    VectorImageType::IndexType index;
//    index.Fill(0);
//    region = evImg->GetLargestPossibleRegion();
//    region.SetIndex(index);
//    evImg->SetLargestPossibleRegion(region);
  }

  // write to HD
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputFile);
  writer->SetInput( evImg );
 
  try 
    { 
    //execute the pipeline
    writer->Update(); 
    } 
  catch( itk::ExceptionObject & err ) 
    { 
    std::cerr << "ExceptionObject caught !" << std::endl; 
    std::cerr << err << std::endl; 
    return EXIT_FAILURE;
    } 
 
  return EXIT_SUCCESS;
}
