#ifndef __itkSimpleProcessing_h
#define __itkSimpleProcessing_h
 
#include "itkImageToImageFilter.h"
#include "Matrix3D.h"
 
namespace itk
{
    class SimpleProcessing
    {
    public:

       SimpleProcessing(){
       }

       static void removeSmallComponents(itk::Image<unsigned char, 3> & segmentation, int minCCSize = 1000, int threshold = 128){
       
           typedef unsigned int LabelScalarType;
       
           Matrix3D<LabelScalarType> CCMatrix;
           LabelScalarType labelCount;
       
           // we need to store info to remove small regions
           std::vector<ShapeStatistics<itk::ShapeLabelObject<LabelScalarType, 3> > > shapeDescr;

           Matrix3D< unsigned char > scoreMatrix;

           scoreMatrix.loadItkImage(segmentation, true);
       
           scoreMatrix.createLabelMap< unsigned int >( threshold, 255, &CCMatrix,
                                                    false, &labelCount, &shapeDescr );
       
           // now create image
           {
             typedef itk::ImageFileWriter<itk::Image<unsigned char, 3> > WriterType;
             typedef Matrix3D<LabelScalarType>::ItkImageType LabelImageType;
       
             // original image
             LabelImageType::Pointer labelsImage = CCMatrix.asItkImage();
       
             // now copy pixel values, get an iterator for each
             itk::ImageRegionConstIterator<LabelImageType> labelsIterator(labelsImage, labelsImage->GetLargestPossibleRegion());
             itk::ImageRegionIterator< itk::Image<unsigned char, 3> > imageIterator(segmentation, segmentation->GetLargestPossibleRegion());
       
             // WARNING: assuming same searching order-- could be wrong!!
             while( (! labelsIterator.IsAtEnd()) && (!imageIterator.IsAtEnd()) )
             {
                 if ((labelsIterator.Value() == 0) || ( shapeDescr[labelsIterator.Value() - 1].numVoxels() < minCCSize ) )
                     imageIterator.Set( 0 );
                 else
                 {
                     imageIterator.Set( 255 );
                 }
       
                 ++labelsIterator;
                 ++imageIterator;
             }
           }

//           WriterType::Pointer writer = WriterType::New();
//           writer->SetFileName("outputSegmentation-nosmallcomponents-in.tif");
//           writer->SetInput(segmentation);
//           writer->Update();
       
       }



    };
} //namespace ITK
 
 
#ifndef ITK_MANUAL_INSTANTIATION
#include "SimpleProcessing.txx"
#endif
 
 
#endif // __itkSimpleProcessing_h
