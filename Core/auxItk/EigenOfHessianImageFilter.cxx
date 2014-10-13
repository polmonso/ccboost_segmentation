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


#include <iostream>
#include <fstream>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>

#include <itkImage.h>
#include <itkIndex.h>
#include "itkImageFileWriter.h"
#include "itkImageFileReader.h"
#include "itkRescaleIntensityImageFilter.h"

#include "itkNthElementImageAdaptor.h"

#include "itkImageRegionIterator.h"

#include "itkHessianRecursiveGaussianImageFilter.h"
#include "itkSymmetricEigenAnalysisImageFilter2.h"

#include "itkVectorIndexSelectionCastImageFilter.h"
#include "itkDivideImageFilter.h"
#include "itkMultiplyImageFilter.h"


#include "SymmetricEigenAnalysisTest.h"

// Macro to avoid re-typing with itk
// --> For instance: 
//			makeNew( random, itk::RandomImageSource<FloatImage2DType> );
// is equivalent to:
//			itk::RandomImageSource<FloatImage2DType>::Pointer	random;
//			random = itk::RandomImageSource<FloatImage2DType>::New();
// The __VA_ARGS__ is there so that it can handle commas within the argument list 
//   in a natural way
#define makeNew(instanceName, ...)	\
    __VA_ARGS__::Pointer instanceName = __VA_ARGS__::New()



//////////////////////////////////////////////////////////////////////
// I love typedefing my data

//Image
const int Dimension = 3;
typedef float                                     PixelType;
typedef itk::Image<PixelType, Dimension>          ImageType;
typedef ImageType::IndexType                      IndexType;
typedef itk::ImageFileReader< ImageType >         ReaderType;

typedef itk::FixedArray<float, Dimension>         OrientationPixelType;
typedef itk::Image<OrientationPixelType, Dimension>
OrientationImageType;
typedef itk::ImageFileReader< OrientationImageType >     OrientationImageReaderType;
typedef itk::ImageFileWriter< ImageType >     FloatImageWriterType;


typedef itk::ImageRegionConstIterator< OrientationImageType > ConstOrientationIteratorType;
typedef itk::ImageRegionIterator< OrientationImageType>       OrientationIteratorType;

typedef itk::ImageRegionConstIterator< ImageType > ConstFloatIteratorType;
typedef itk::ImageRegionIterator< ImageType>       FloatIteratorType;


/** Hessian & utils **/
typedef itk::SymmetricSecondRankTensor<float,Dimension>                       HessianPixelType;
typedef itk::Image< HessianPixelType, Dimension >                             HessianImageType;
typedef itk::HessianRecursiveGaussianImageFilter<ImageType, HessianImageType> HessianFilterType;

typedef itk::Vector<float, Dimension>            VectorPixelType;

typedef itk::Vector<float, Dimension>          EigenValuePixelType;
typedef itk::Matrix<float, Dimension, Dimension>	EigenVectorPixelType;

typedef itk::Image<VectorPixelType, Dimension> VectorImageType;

typedef itk::Image<EigenValuePixelType, Dimension> EigenValueImageType;
typedef itk::Image<EigenVectorPixelType, Dimension> EigenVectorImageType;
typedef EigenValueImageType 		FirstEigenVectorOrientImageType;

typedef itk::SymmetricEigenAnalysisImageFilter2< HessianImageType, EigenValueImageType, EigenVectorImageType, FirstEigenVectorOrientImageType >        HessianToEigenFilter;

using namespace std;

#define showMsg(args...) \
    do { \
        printf("\x1b[32m" "\x1b[1m["); \
        printf(args); \
        printf("]\x1b[0m\n" ); \
    } while(0)

int execute(float sigma, string imageName, string outputFile, bool sortByMagnitude, float zAnisotropyFactor)
{
            //  typename RotationalFeatureType::Pointer featureRotational
            //    = RotationalFeatureType::New();

            //////////////////////////////////////////////////////////////////////
            // Reads the image

    showMsg( "Reading the image" );

    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(imageName);
    reader->Update();
    
    ImageType::Pointer inpImg = reader->GetOutput();
    
    ImageType::SpacingType spacing = inpImg->GetSpacing();
    spacing[2] = zAnisotropyFactor;
    
    std::cout << "Using spacing: " << spacing << ", anisotr factor = " << zAnisotropyFactor << std::endl;
    inpImg->SetSpacing(spacing);
    

    showMsg("Hessian filtering");
    makeNew( hessianFilt, HessianFilterType );
    hessianFilt->SetSigma( sigma );
    hessianFilt->SetInput( inpImg );

    showMsg("Computing eigenvalues");
    // now compute eigenvalues/main eigenvector
    makeNew( eigenFilt, HessianToEigenFilter );
    eigenFilt->SetInput( hessianFilt->GetOutput() );
    
    eigenFilt->SetOrderEigenMagnitudes(sortByMagnitude);
    eigenFilt->SetOrderEigenValues(!sortByMagnitude);
    eigenFilt->SetGenerateEigenVectorImage(false);
    eigenFilt->SetGenerateFirstEigenVectorOrientImage(false);

#if 0
    typedef itk::VectorResampleImageFilter< VectorImageType, VectorImageType > VectorResampleFilterType;
    makeNew( vecResampler, VectorResampleFilterType );

    vecResampler->SetInput(eigenFilt->GetOutput());
    vecResampler->SetSize(image->GetLargestPossibleRegion().GetSize());
#endif


    showMsg("Saving images");
    {
        eigenFilt->Update();
        EigenValueImageType* outImg = (EigenValueImageType *)eigenFilt->GetEigenValueImage();
        eigenFilt->Update();
        
        // reset spacing
        spacing[0] = spacing[1] = spacing[2] = 1.0;
        
        outImg->SetSpacing( spacing );
        
        makeNew( writer, itk::ImageFileWriter<EigenValueImageType> );
        writer->SetInput( outImg );
        writer->SetFileName( outputFile );
        writer->Update();
    }

    return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
    cjbCheckSymmetricEigenAnalysisEigMagnitudeOrdering();

    if(argc != 6){
        printf("Usage: EigenOfHessianImageFilter image sigma zAnisotropyFactor outputFileWithEigenvalues sortByMagnitude\n");
        exit(0);
    }

    string imageName(argv[1]);
    
    const bool sortByMagnitude = atoi(argv[5]) != 0;
	printf("Order by magnitude: %d\n", sortByMagnitude );

    float sigma  = atof(argv[2]);
    float zAnisotropyFactor = atof(argv[3]);
    string outputFile(argv[4]);


   return execute(sigma, imageName, outputFile, sortByMagnitude, zAnisotropyFactor);
}
