#include "CcboostAdapter.h"

//Boost
#include "Booster.h"
#include "utils/TimerRT.h"

//Qt
#include <QSettings>

//Espina
#include <Support/Settings/EspinaSettings.h>

//auxITK
#include "EigenOfStructureTensorImageFilter2.h"
#include "GradientMagnitudeImageFilter2.h"
#include "SingleEigenVectorOfHessian2.h"
#include "RepolarizeYVersorWithGradient2.h"
#include "AllEigenVectorsOfHessian2.h"

//ITK
#include <itkTestingHashImageFilter.h>
#include <itkBinaryDilateImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkImageToImageFilter.h>
#include <itkImageMaskSpatialObject.h>
#include <itkChangeInformationImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkImageRegionIterator.h>
#include <itkBinaryImageToLabelMapFilter.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkPasteImageFilter.h>
#include <itkOrImageFilter.h>
#include <itkMultiplyImageFilter.h>

#include <itkConstantPadImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>

using namespace EspINA;

CcboostAdapter::CcboostAdapter()
{
}

bool CcboostAdapter::core(const ConfigData<itkVolumeType>& cfgdata,
                          itkVolumeType::Pointer& probabilisticOutSeg,
                          std::vector<itkVolumeType::Pointer>& outSegList) {

    MultipleROIData allROIs;

    //    for(auto imgItk: channels) {
    //        for(auto gtItk: groundTruths) {
    Matrix3D<ImagePixelType> img, gt;

    img.loadItkImage(cfgdata.train.at(0).rawVolumeImage, true);

    gt.loadItkImage(cfgdata.train.at(0).groundTruthImage, true);

    ROIData roi;
    roi.init( img.data(), gt.data(), 0, 0, img.width(), img.height(), img.depth() );

    // raw image integral image
    ROIData::IntegralImageType ii;
    ii.compute( img );
    roi.addII( ii.internalImage().data() );

    //features
    computeAllFeatures(cfgdata);

    addAllFeatures(cfgdata, roi);

    allROIs.add( &roi );

    // }
    // }

    qDebug() << "running core plugin";

    BoosterInputData bdata;
    bdata.init( &allROIs );
    bdata.showInfo();

    Booster adaboost;

    qDebug() << "training";

    adaboost.train( bdata, 100 );

    qDebug() << "predict";

    // predict
    Matrix3D<float> predImg;
    TimerRT timer; timer.reset();
    adaboost.predict( &allROIs, &predImg );
    qDebug("Elapsed: %f", timer.elapsed());
    predImg.save("/tmp/test.nrrd");

    // save JSON model
    if (!adaboost.saveModelToFile( "/tmp/model.json" ))
        std::cout << "Error saving JSON model" << std::endl;

    auto outSegmentation = predImg.asItkImage();

    qDebug() << "output image";

    itk::ImageFileWriter< itk::Image<float, 3> >::Pointer writerf = itk::ImageFileWriter< itk::Image<float, 3> >::New();
    //TODO espina2
    //    if(ccboostconfig.saveIntermediateVolumes){
    //      writer->SetFileName(ccboostconfig.train.cacheDir + "labelmap.tif");
    writerf->SetFileName("predicted.tif");
    writerf->SetInput(outSegmentation);
    writerf->Update();
    qDebug() << "predicted.tif created";

    //TODO not sure why the types are different
    typedef itk::CastImageFilter< Matrix3D<float>::ItkImageType, itkVolumeType > CastFilterType;
    CastFilterType::Pointer castFilter = CastFilterType::New();
    castFilter->SetInput(outSegmentation);
    castFilter->Update();

    probabilisticOutSeg = castFilter->GetOutput();

//    emit progress(50);
//     if (!canExecute()) return;
    WriterType::Pointer writer = WriterType::New();

     if(cfgdata.saveIntermediateVolumes && probabilisticOutSeg->VerifyRequestedRegion()){
         writer->SetFileName(cfgdata.train.at(0).cacheDir + "thread-outputSegmentation.tif");
         writer->SetInput(probabilisticOutSeg);
         writer->Update();
     }

     QSettings settings(CESVIMA, ESPINA);
     settings.beginGroup("Synapse Segmentation");
     settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));


     typedef itk::BinaryThresholdImageFilter <itkVolumeType, itkVolumeType>
            BinaryThresholdImageFilterType;

     BinaryThresholdImageFilterType::Pointer thresholdFilter
       = BinaryThresholdImageFilterType::New();
     thresholdFilter->SetInput(probabilisticOutSeg);
     thresholdFilter->SetLowerThreshold(128);
     thresholdFilter->SetUpperThreshold(255);
     //TODO why here it's the inverse of on ccboost?
     thresholdFilter->SetInsideValue(0);
     thresholdFilter->SetOutsideValue(255);
     thresholdFilter->Update();

     if(cfgdata.saveIntermediateVolumes) {
         writer->SetFileName("thresholded-segmentation.tif");
         writer->SetInput(thresholdFilter->GetOutput());
         writer->Update();
     }

     itk::Image<unsigned char, 3>::Pointer outputSegmentation = thresholdFilter->GetOutput();
     outputSegmentation->DisconnectPipeline();

     postprocessing(outputSegmentation, 1, "./post-" );

     outputSegmentation->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

//     emit progress(70);
//     if (!canExecute()) return;

     splitSegmentations(outputSegmentation, outSegList);

    return true;

}

void CcboostAdapter::splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                                        std::vector<itkVolumeType::Pointer>& outSegList){

//    //If 255 components is not enough as output, switch to unsigned short
//    typedef itk::ImageToImageFilter <itkVolumeType, bigVolumeType > ConvertFilterType;

//    ConvertFilterType::Pointer convertFilter = (ConvertFilterType::New()).GetPointer();
//    convertFilter->SetInput(outputSegmentation);
//    convertFilter->Update();

    /*split the output into several segmentations*/
    typedef itk::ConnectedComponentImageFilter <itkVolumeType, bigVolumeType >
      ConnectedComponentImageFilterType;

    ConnectedComponentImageFilterType::Pointer connected = ConnectedComponentImageFilterType::New ();
    connected->SetInput(outputSegmentation);

    connected->Update();

    //save itk image (volume) as binary/classed volume here
    WriterType::Pointer writer = WriterType::New();
//TODO espina2
//    if(ccboostconfig.saveIntermediateVolumes){
//      writer->SetFileName(ccboostconfig.train.cacheDir + "labelmap.tif");
      writer->SetFileName("labelmap3.tif");
      writer->SetInput(outputSegmentation);
      writer->Update();
      qDebug() << "labelmap3.tif created";

    std::cout << "Number of objects: " << connected->GetObjectCount() << std::endl;

    qDebug("Connected components segmentation");
    BigWriterType::Pointer bigwriter = BigWriterType::New();
    //TODO espina2
//    if(ccboostconfig.saveIntermediateVolumes){
    if(true) {
        bigwriter->SetFileName(/*ccboostconfig.train.cacheDir + */"connected-segmentation.tif");
        bigwriter->SetInput(connected->GetOutput());
        bigwriter->Update();
    }

    typedef itk::RelabelComponentImageFilter<bigVolumeType, bigVolumeType> RelabelFilterType;
    RelabelFilterType::Pointer relabelFilter = RelabelFilterType::New();
    relabelFilter->SetInput(connected->GetOutput());
    relabelFilter->Update();
//TODO espina2
//    if(ccboostconfig.saveIntermediateVolumes) {
    if(true) {
        qDebug("Save relabeled segmentation");
        bigwriter->SetFileName(/*ccboostconfig.train.cacheDir + */"relabeled-segmentation.tif");
        bigwriter->SetInput(relabelFilter->GetOutput());
        bigwriter->Update();
    }

    //create Espina Segmentation
    typedef itk::BinaryThresholdImageFilter <bigVolumeType, itkVolumeType>
      BinaryThresholdImageFilterType;

    BinaryThresholdImageFilterType::Pointer labelThresholdFilter
        = BinaryThresholdImageFilterType::New();

    labelThresholdFilter->SetInsideValue(255);
    labelThresholdFilter->SetOutsideValue(0);

    qDebug("Create ESPina segmentations");

    //TODO espina2 ccboostconfig
//    for(int i=1; i < connected->GetObjectCount() && i < ccboostconfig.maxNumObjects; i++){
    for(int i=1; i < connected->GetObjectCount(); i++){
        labelThresholdFilter->SetInput(relabelFilter->GetOutput());
        labelThresholdFilter->SetLowerThreshold(i);
        labelThresholdFilter->SetUpperThreshold(i);
        labelThresholdFilter->Update();

        //TODO espina2 this probably doesn't work if the pointer points the same memory at every iteration
        //TODO espina2 create the outputs here?
        itkVolumeType::Pointer outSeg = labelThresholdFilter->GetOutput();
        outSeg->DisconnectPipeline();
        outSegList.push_back(outSeg);

    }


}

void CcboostAdapter::postprocessing(itkVolumeType::Pointer& outputSegmentation,
                                               double anisotropyFactor,
                                               std::string cacheDir,
                                               int minComponentSize,
                                               bool saveIntermediateVolumes){



    qDebug("remove borders");

    itkVolumeType::SizeType outputSize = outputSegmentation->GetLargestPossibleRegion().GetSize();

    const int borderToRemove = 2;
    const int borderZ = std::max((int)1, (int)(borderToRemove/anisotropyFactor + 0.5));
    const int borderOther = borderToRemove;

    // this is dimension-agnostic, though now we need to know which one is Z to handle anisotropy
    const unsigned zIndex = 2;

    const int numDim = itkVolumeType::ImageDimension;

    // go through each dimension and zero it out
    for (unsigned dim=0; dim < numDim; dim++)
    {
        const int thisBorder = (dim == zIndex) ? borderZ : borderOther;

        const int length = outputSize[dim];

        // check if image is too small, then we have to avoid over/underflows
        const unsigned toRemove = std::min( length, thisBorder );

        itkVolumeType::SizeType   regionSize = outputSegmentation->GetLargestPossibleRegion().GetSize();
        regionSize[dim] = toRemove;

        itkVolumeType::IndexType   regionIndex = outputSegmentation->GetLargestPossibleRegion().GetIndex();

        itkVolumeType::RegionType region;
        region.SetSize(regionSize);
        region.SetIndex(regionIndex);

        // first the low part of the border
        {
            itk::ImageRegionIterator<itkVolumeType> imageIterator(outputSegmentation, region);
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

            itk::ImageRegionIterator<itkVolumeType> imageIterator(outputSegmentation, region);
            while(!imageIterator.IsAtEnd())
            {
                // set to zero
                imageIterator.Set(0);
                ++imageIterator;
            }
        }
    }


    WriterType::Pointer writer = WriterType::New();
    if(saveIntermediateVolumes) {
        writer->SetFileName(cacheDir + "ccOutputSegmentation-noborders.tif");
        writer->SetInput(outputSegmentation);
        writer->Update();
        qDebug("cc boost segmentation saved");
    }

    qDebug("Threshold ccboost output");

    typedef itk::BinaryThresholdImageFilter <itkVolumeType, itkVolumeType>
           BinaryThresholdImageFilterType;

    int lowerThreshold = 128;
    int upperThreshold = 255;

       BinaryThresholdImageFilterType::Pointer thresholdFilter1
         = BinaryThresholdImageFilterType::New();
       thresholdFilter1->SetInput(outputSegmentation);
       //TODO parametrize threshold
       thresholdFilter1->SetLowerThreshold(lowerThreshold);
       thresholdFilter1->SetUpperThreshold(upperThreshold);
       thresholdFilter1->SetInsideValue(255);
       thresholdFilter1->SetOutsideValue(0);
       thresholdFilter1->Update();
       outputSegmentation = thresholdFilter1->GetOutput();

       if(saveIntermediateVolumes) {
                 writer->SetFileName(cacheDir + "outputSegmentation-withsmallcomponents.tif");
                 writer->SetInput(outputSegmentation);
                 writer->Update();
              }

       qDebug("Remove small components");

       outputSegmentation->DisconnectPipeline();

       removeSmallComponents(outputSegmentation);

       if(saveIntermediateVolumes) {
          writer->SetFileName(cacheDir + "outputSegmentation-nosmallcomponents.tif");
          writer->SetInput(outputSegmentation);
          writer->Update();
       }

      qDebug("Threshold segmentation");

      BinaryThresholdImageFilterType::Pointer thresholdFilter
        = BinaryThresholdImageFilterType::New();
      thresholdFilter->SetInput(outputSegmentation);
      thresholdFilter->SetLowerThreshold(lowerThreshold);
      thresholdFilter->SetUpperThreshold(upperThreshold);
      thresholdFilter->SetInsideValue(255);
      thresholdFilter->SetOutsideValue(0);
      thresholdFilter->Update();

      if(saveIntermediateVolumes) {
          writer->SetFileName(cacheDir + "thresholded-segmentation.tif");
          writer->SetInput(thresholdFilter->GetOutput());
          writer->Update();
      }

      typedef itk::BinaryBallStructuringElement< itkVolumeType::PixelType, 3> StructuringElementType;
      StructuringElementType structuringElement;
      int radius = 1;
      structuringElement.SetRadius(radius);
      structuringElement.CreateStructuringElement();

      typedef itk::BinaryDilateImageFilter <itkVolumeType, itkVolumeType, StructuringElementType>
                   BinaryDilateImageFilterType;

          BinaryDilateImageFilterType::Pointer dilateFilter
               = BinaryDilateImageFilterType::New();

          dilateFilter->SetInput(thresholdFilter->GetOutput());
          dilateFilter->SetKernel(structuringElement);
          dilateFilter->Update();

          qDebug("Dilate Segmentation");
          if(saveIntermediateVolumes) {
              writer->SetFileName(cacheDir + "dilated-segmentation.tif");
              writer->SetInput(dilateFilter->GetOutput());
              writer->Update();
          }

          outputSegmentation = thresholdFilter->GetOutput();
          outputSegmentation->DisconnectPipeline();

}

void CcboostAdapter::removeSmallComponents(itk::Image<unsigned char, 3>::Pointer & segmentation, int minCCSize, int threshold){

    typedef unsigned int LabelScalarType;

    Matrix3D<LabelScalarType> CCMatrix;
    LabelScalarType labelCount;

    // we need to store info to remove small regions
    std::vector<ShapeStatistics<itk::ShapeLabelObject<LabelScalarType, 3> > > shapeDescr;

    Matrix3D<itkVolumeType::PixelType> scoreMatrix;
    scoreMatrix.loadItkImage(segmentation, true);

    scoreMatrix.createLabelMap<LabelScalarType>( threshold, 255, &CCMatrix,
                                             false, &labelCount, &shapeDescr );

    // now create image
    {
      typedef itk::ImageFileWriter< itkVolumeType > WriterType;
      typedef Matrix3D<LabelScalarType>::ItkImageType LabelImageType;

      // original image
      LabelImageType::Pointer labelsImage = CCMatrix.asItkImage();

      // now copy pixel values, get an iterator for each
      itk::ImageRegionConstIterator<LabelImageType> labelsIterator(labelsImage, labelsImage->GetLargestPossibleRegion());
      itk::ImageRegionIterator<itkVolumeType> imageIterator(segmentation, segmentation->GetLargestPossibleRegion());

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

    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName("outputSegmentation-nosmallcomponents-in.tif");
    writer->SetInput(segmentation);
    writer->Update();

}

void CcboostAdapter::computeAllFeatures(const ConfigData<itkVolumeType> cfgData,
                                        ThreadStateDescription *stateDescription){

    for(auto cfgROI: cfgData.train){
        computeFeatures(cfgData, cfgROI);
    }
    for(auto cfgROI: cfgData.test){
        computeFeatures(cfgData, cfgROI);
    }

}

void CcboostAdapter::computeFeatures(const ConfigData<itkVolumeType> cfgData,
                                     const SetConfigData<itkVolumeType> cfgDataROI,
                                     ThreadStateDescription *stateDescription){


    //TODO can we give message with?
    if(stateDescription->getAbort())
        return;

    std::stringstream strstream;

    int featureNum = 0;
    for( const std::string& featureFile: cfgDataROI.otherFeatures ) {

       strstream.str(std::string());
       featureNum++;
       strstream << "Computing " << featureNum << "/" << cfgDataROI.otherFeatures.size()+1 << " features";
       std::cout << strstream.str() << std::endl;
       stateDescription->setTime(strstream.str());

       std::string featureFilepath(cfgDataROI.cacheDir + featureFile);
       ifstream featureStream(featureFilepath.c_str());
       //TODO check hash to prevent recomputing
       if(!featureStream.good() || cfgData.forceRecomputeFeatures){
           //TODO do this in a more flexible way
           int stringPos;
           if( (stringPos = featureFile.find(std::string("hessOrient-"))) != std::string::npos){
               float sigma;
               std::string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"hessOrient-s%f-repolarized.nrrd",&sigma);
              //FIXME break if error
               if(result < 0)
                   qDebug() << "Error scanning feature name " << featureFile.c_str();

               qDebug() << QString("Either you requested recomputing the features, "
                                   "the current channel is new or has changed or "
                                   "Hessian Orient Estimate feature not found at "
                                   "path: %2. Creating with sigma %1").arg(sigma).arg(QString::fromStdString(featureFilepath));


               AllEigenVectorsOfHessian2Execute(sigma, cfgDataROI.rawVolumeImage, featureFilepath, EByMagnitude, cfgDataROI.zAnisotropyFactor);

               RepolarizeYVersorWithGradient2Execute(sigma, cfgDataROI.rawVolumeImage, featureFilepath, featureFilepath, cfgDataROI.zAnisotropyFactor);

          } else if ( (stringPos = featureFile.find(std::string("gradient-magnitude"))) != std::string::npos ) {
               float sigma;
               std::string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"gradient-magnitude-s%f.nrrd",&sigma);
               if(result < 0)
                   qDebug() << "Error scanning feature name " << featureFile.c_str();

               qDebug() << QString("Current  or channel is new or has changed or gradient magnitude feature not found at path: %2. Creating with sigma %1").arg(sigma).arg(QString::fromStdString(featureFilepath));

               GradientMagnitudeImageFilter2Execute(sigma, cfgDataROI.rawVolumeImage, featureFilepath, cfgDataROI.zAnisotropyFactor);

           } else if( (stringPos = featureFile.find(std::string("stensor"))) != std::string::npos ) {
               float rho,sigma;
               std::string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"stensor-s%f-r%f.nrrd",&sigma,&rho);
               if(result < 0)
                   qDebug() << "Error scanning feature name" << featureFile.c_str();

               qDebug() << QString("Current channel is new or has changed or tensor feature not found at path %3. Creating with rho %1 and sigma %2").arg(rho).arg(sigma).arg(QString::fromStdString(featureFilepath));

               bool sortByMagnitude = true;
               EigenOfStructureTensorImageFilter2Execute(sigma, rho, cfgDataROI.rawVolumeImage, featureFilepath, sortByMagnitude, cfgDataROI.zAnisotropyFactor );

           } else {
               qDebug() << QString("feature %1 is not recognized.").arg(featureFile.c_str()).toUtf8();
           }
       }
    }

    stateDescription->setMessage("Features computed");
    stateDescription->setTime("");
}

MultipleROIData CcboostAdapter::preprocess(std::vector<itkVolumeType::Pointer> channels,
                                                      std::vector<itkVolumeType::Pointer> groundTruths,
                                                      std::string cacheDir,
                                                      std::vector<std::string> featuresList){

    MultipleROIData allROIs;

//    for(auto imgItk: channels) {
//        for(auto gtItk: groundTruths) {
    {
            Matrix3D<ImagePixelType> img, gt;

            img.loadItkImage(channels.at(0), true);

            gt.loadItkImage(groundTruths.at(0), true);

            ROIData roi;
            roi.init( img.data(), gt.data(), 0, 0, img.width(), img.height(), img.depth() );

            // raw image integral image
            ROIData::IntegralImageType ii;
            ii.compute( img );
            roi.addII( ii.internalImage().data() );

            //features
            //check hash
//            int zAnisotropyFactor = 1;
//            computeFeatures(channels.at(0), cacheDir, featuresList, zAnisotropyFactor, false);
//            for(std::string featureItk: featuresList){
//                Matrix3D<ImagePixelType> feature;
//                feature.load(cacheDir + featureItk);

//                // raw image integral image
//                ROIData::IntegralImageType ii;
//                ii.compute( feature );
//                roi.addII( ii.internalImage().data() );
//            }

            allROIs.add( &roi );

    }

//        }
//    }



            return allROIs;
}

void CcboostAdapter::addAllFeatures(const ConfigData<itkVolumeType> cfgData, ROIData &roi ){

    //add Orient "feature"

    for(auto cfgDataROI: cfgData.train)
        addFeatures(cfgDataROI, roi);
    for(auto cfgDataROI: cfgData.test)
        addFeatures(cfgDataROI, roi);
}

void CcboostAdapter::addFeatures(const SetConfigData<itkVolumeType> cfgDataROI,
                                         ROIData &roi )
{
    // now add channels
    typedef itk::VectorImage<float, 3>  ItkVectorImageType;
    for ( const auto& chanFName: cfgDataROI.otherFeatures )
    {
        const auto &featFName = cfgDataROI.cacheDir + "/" + chanFName;
        qDebug("Adding feature %s", featFName.c_str());

        itk::ImageFileReader<ItkVectorImageType>::Pointer reader = itk::ImageFileReader<ItkVectorImageType>::New();
        try {
            reader->SetFileName( featFName );
            reader->Update();
        } catch(std::exception &e)
        {
            qFatal("Reading feature exception, at: %s", featFName.c_str());
        }

        ItkVectorImageType::Pointer img = reader->GetOutput();


        ItkVectorImageType::SizeType imSize = img->GetLargestPossibleRegion().GetSize();

        // we want raw pixel access
        {
            ItkVectorImageType::RegionType reg = img->GetLargestPossibleRegion();


            ItkVectorImageType::RegionType::IndexType idx = reg.GetIndex();
            for (unsigned i=0; i < 3; i++)
                idx[i] = 0;

            reg.SetIndex( idx );

            img->SetRegions( reg );


            ItkVectorImageType::PointType origin = img->GetOrigin();

            for (unsigned i=0; i < 3; i++)
                origin[i] = 0;

            img->SetOrigin( origin );
        }

        unsigned int mWidth = imSize[0];
        unsigned int mHeight = imSize[1];
        unsigned int mDepth = imSize[2];
        unsigned int mComp = img->GetNumberOfComponentsPerPixel();


        if ( (mWidth != roi.rawImage.width()) || (mHeight != roi.rawImage.height()) || (mDepth != roi.rawImage.depth()) )
            qFatal("Feature image size differs from raw image: %s", featFName.c_str());

        for (unsigned q=0; q < mComp; q++)
        {
            Matrix3D<float> auxImg;
            auxImg.reallocSizeLike( roi.rawImage );

            // II itself
            IntegralImage<IntegralImagePixelType>	ii;


            for (unsigned pix=0; pix < auxImg.numElem(); pix++)
            {
                ItkVectorImageType::IndexType index;

                unsigned x,y,z;
                auxImg.idxToCoord(pix, x, y, z);

                index[0] = x;
                index[1] = y;
                index[2] = z;

                const ItkVectorImageType::PixelType &pixData = img->GetPixel(index);
                auxImg.data()[pix] = pixData[q];
            }

            ii.compute( auxImg );

            ROIData::ChannelType chanType = ROIData::ChannelType::GAUSS;
            auto strContains = [&](const char *x) -> bool
            { return chanFName.find(x) != std::string::npos; };

            if ( strContains("tens") ) chanType = ROIData::ChannelType::STENS_EIGVAL;
            if ( strContains("grad") ) chanType = ROIData::ChannelType::GRADIENT;
            if ( strContains("hess") ) chanType = ROIData::ChannelType::HESS_EIGVAL;

            roi.addII( std::move(ii), chanType );
        }
    }
}
