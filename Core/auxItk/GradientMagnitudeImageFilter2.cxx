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

#include <itkGradientMagnitudeRecursiveGaussianImageFilter.h>
#include <itkImageDuplicator.h>

// Macro to avoid re-typing with itk
// --> For instance: 
//    	    makeNew( random, itk::RandomImageSource<FloatImage2DType> );
// is equivalent to:
//    	    itk::RandomImageSource<FloatImage2DType>::Pointer    random;
//    	    random = itk::RandomImageSource<FloatImage2DType>::New();
// The __VA_ARGS__ is there so that it can handle commas within the argument list 
//   in a natural way
#define makeNew(instanceName, ...)    \
    __VA_ARGS__::Pointer instanceName = __VA_ARGS__::New()

//////////////////////////////////////////////////////////////////////
// I love typedefing my data

//Image
const int Dimension = 3;
typedef unsigned char                             PixelType;
typedef itk::Image<PixelType, Dimension>          ImageType;
typedef ImageType::IndexType                      IndexType;
typedef itk::ImageFileReader< ImageType >         ReaderType;

typedef itk::Image<float, Dimension> OutputImageType;

typedef itk::ImageFileWriter< OutputImageType >           WriterType;


typedef itk::GradientMagnitudeRecursiveGaussianImageFilter<ImageType, OutputImageType> FilterType;

using namespace std;

int GradientMagnitudeImageFilter2Execute(float sigma, const itk::Image<unsigned char, 3>::Pointer image,
                                         string outputFile, float zAnisotropyFactor)
{
    std::cout << "Reading the image" << std::endl;

    typedef itk::ImageDuplicator< ImageType > DuplicatorType;
    DuplicatorType::Pointer duplicator = DuplicatorType::New();
    duplicator->SetInputImage(image);
    duplicator->Update();
    ImageType::Pointer inpImg = duplicator->GetOutput();

    ImageType::SpacingType spacing = inpImg->GetSpacing();
    spacing[2] = zAnisotropyFactor;
    
    std::cout << "Using spacing: " << spacing << ", anisotropy factor = " << zAnisotropyFactor << std::endl;
    inpImg->SetSpacing(spacing);

    if(sigma <= 0) {
        std::cerr << "Sigma must be grater than zero. Aborting gradientMagnitudeFilter." << std::endl;
        return EXIT_FAILURE;
    }

    makeNew( filter, FilterType );
    filter->SetSigma( sigma );
    filter->SetInput( inpImg );
    {

        filter->Update();

        OutputImageType::Pointer outImg = filter->GetOutput();

        // reset spacing
        spacing[0] = spacing[1] = spacing[2] = 1.0;

        outImg->SetSpacing( spacing );

        makeNew( writer, WriterType );
        writer->SetInput( outImg );
        writer->SetFileName( outputFile );

        writer->Update();
    }

    return EXIT_SUCCESS;
}
