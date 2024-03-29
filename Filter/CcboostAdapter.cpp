
#include "CcboostAdapter.h"

//Boost
#include "Booster.h"
#include "utils/TimerRT.h"

//Qt
#include <QSettings>

//Espina
//#include <Support/Settings/EspinaSettings.h>

//auxITK
#include "EigenOfStructureTensorImageFilter2.h"
#include "GradientMagnitudeImageFilter2.h"
#include "SingleEigenVectorOfHessian2.h"
#include "RepolarizeYVersorWithGradient2.h"
#include "AllEigenVectorsOfHessian2.h"
#include "ImageSplitter.h"

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
#include <itkMedianImageFilter.h>

#include <itkConstantPadImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkImageDuplicator.h>
#include <itkBinaryImageToShapeLabelMapFilter.h>
#include <itkShapeOpeningLabelMapFilter.h>

//using namespace ESPINA;

#ifndef ESPINA_SETTINGS
#define ESPINA_SETTINGS(settings) QSettings settings("CeSViMa", "ESPINA");
#endif

#ifdef CVL_EXPLICIT_INSTANTIATION
//Explicit templated member function instantiation, remove for implicit
#include "CcboostAdapter.tpp"
#warning using explicit instantiation, symbols might be missing
template void CcboostAdapter::removeborders< FloatTypeImage >( FloatTypeImage::Pointer&, bool, std::string);
template void CcboostAdapter::removeborders< itkVolumeType >(   itkVolumeType::Pointer&, bool, std::string);
#endif

bool CcboostAdapter::core(const ConfigData<itkVolumeType>& cfgdata,
                          std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs)
{

#define WORK
#ifndef WORK
    for(int roiidx = 0; roiidx < cfgdata.test.size(); roiidx++)
    {
        typedef itk::ImageFileReader< FloatTypeImage > fReaderType;
        fReaderType::Pointer freader = fReaderType::New();

        std::cout << "Reading: " << cfgdata.cacheDir << std::to_string(roiidx) << "-predicted.tif" << std::endl;
        freader->SetFileName(cfgdata.cacheDir + std::to_string(roiidx) + "-predicted.tif");
        try
        {
            freader->Update();
        }
        catch( itk::ExceptionObject & err )
        {

            std::cerr << "Itk exception on ccboost caught at " << __FILE__ << ":" << __LINE__ << ". Error: " << err.what() << std::endl;
            return false;
        }
        FloatTypeImage::Pointer probabilisticOutSeg = freader->GetOutput();
        probabilisticOutSeg->DisconnectPipeline();
        probabilisticOutSegs.push_back(probabilisticOutSeg);
    }
#else

    if(cfgdata.saveIntermediateVolumes)
        cfgdata.saveVolumes();

    BoosterInputData::MultipleROIDataPtr allTrainROIs = std::make_shared<MultipleROIData>();

    for(SetConfigData<itkVolumeType> trainData: cfgdata.train) {

        Matrix3D<ImagePixelType> img, gt;

        //itkVolumeType::Pointer annotatedImage = Splitter::crop<itkVolumeType>(cfgData.train.groundTruthImage,cfgData.train.annotatedRegion);

        img.loadItkImage(trainData.rawVolumeImage, true);
        gt.loadItkImage(trainData.groundTruthImage, true);

        MultipleROIData::ROIDataPtr roi = std::make_shared<ROIData>();
        roi->setGTNegativeSampleLabel(cfgdata.gtNegativeLabel);
        roi->setGTPositiveSampleLabel(cfgdata.gtPositiveLabel);
        roi->init( img.data(), gt.data(), 0, 0, img.width(), img.height(), img.depth(), cfgdata.train.at(0).zAnisotropyFactor );

        // raw image integral image
        {
            ROIData::IntegralImageType ii;
            ii.compute( img );
            roi->addII( std::move(ii), ROIData::ChannelType::GAUSS );
        }

        //features
        TimerRT timerF; timerF.reset();

        computeFeatures(cfgdata, trainData);

        qDebug("Compute all features Elapsed: %f", timerF.elapsed());

        timerF.reset();

        addFeatures(cfgdata.cacheDir, trainData, roi);

        qDebug("Add all features Elapsed: %f", timerF.elapsed());

        allTrainROIs->add( roi );

    }

    qDebug() << "running core plugin";

    BoosterInputData bdata;
    bdata.init( allTrainROIs );
    bdata.showInfo();

    Booster adaboost;

    qDebug() << "training";

    TimerRT timerF; timerF.reset();

    adaboost.train( bdata, cfgdata.numStumps );

    qDebug("train Elapsed: %f", timerF.elapsed());

    // save JSON model
    if (!adaboost.saveModelToFile( "/tmp/model.json" ))
        std::cout << "Error saving JSON model" << std::endl;

    // predict

    qDebug() << "predict";

    for(SetConfigData<itkVolumeType> testData: cfgdata.test) {

        Matrix3D<ImagePixelType> img;

        img.loadItkImage(testData.rawVolumeImage, true);

        MultipleROIData::ROIDataPtr roi = std::make_shared<ROIData>();
        roi->init( img.data(), 0, 0, 0, img.width(), img.height(), img.depth(), cfgdata.train.at(0).zAnisotropyFactor );

        // raw image integral image
        {
            ROIData::IntegralImageType ii;
            ii.compute( img );
            roi->addII( std::move(ii), ROIData::ChannelType::GAUSS );
        }

        //features
        TimerRT timerF; timerF.reset();

        computeFeatures(cfgdata, testData);

        qDebug("Compute all features Elapsed: %f", timerF.elapsed());

        timerF.reset();

        addFeatures(cfgdata.cacheDir, testData, roi);

        qDebug("Add all features Elapsed: %f", timerF.elapsed());
        //predict one at a time
        MultipleROIData testROI;
        testROI.add( roi );

        Matrix3D<float> predImg;
        TimerRT timer; timer.reset();
        adaboost.predictWithFeatureOrdering<true>( testROI, &predImg );
        //predImg.save("/tmp/test.nrrd");

        qDebug("Elapsed: %f", timer.elapsed());

        typedef itk::ImageDuplicator< FloatTypeImage > DuplicatorType;
        DuplicatorType::Pointer duplicator = DuplicatorType::New();
        duplicator->SetInputImage(predImg.asItkImage());
        duplicator->Update();
        FloatTypeImage::Pointer probabilisticOutSeg = duplicator->GetModifiableOutput();


        probabilisticOutSeg->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

        typedef itk::MedianImageFilter <FloatTypeImage, FloatTypeImage>
                MedianImageFilterType;

        const double medianFilterRadius = 2.0;

        MedianImageFilterType::Pointer medianFilter
                = MedianImageFilterType::New();
        MedianImageFilterType::InputSizeType radius;
        radius.Fill(medianFilterRadius);
        medianFilter->SetRadius(radius);
        medianFilter->SetInput(probabilisticOutSeg);
        medianFilter->Update();

        probabilisticOutSeg = medianFilter->GetOutput();

        probabilisticOutSeg->DisconnectPipeline();

        probabilisticOutSegs.push_back(probabilisticOutSeg);

    }

    for(int roiidx = 0; roiidx < probabilisticOutSegs.size(); roiidx++) {
           typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
           fWriterType::Pointer fwriter = fWriterType::New();

           std::cout << "Writing: " << cfgdata.cacheDir << std::to_string(roiidx) << "-predicted.tif" << std::endl;
           fwriter->SetFileName(cfgdata.cacheDir + std::to_string(roiidx) + "-predicted.tif");
           fwriter->SetInput(probabilisticOutSegs[roiidx]);
           try {
               fwriter->Update();
           }
           catch( itk::ExceptionObject & err )
           {

               std::cerr << "Itk exception on ccboost caught at " << __FILE__ << ":" << __LINE__ << ". Error: " << err.what() << std::endl;
               return false;
           }

       }

#endif

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));

    return true;
}

bool CcboostAdapter::core(const ConfigData<itkVolumeType>& cfgdata,
                          FloatTypeImage::Pointer& probabilisticOutSeg,
                          std::vector<itkVolumeType::Pointer>& outputSplittedSegList,
                          itkVolumeType::Pointer& outputSegmentation) {

    std::vector<FloatTypeImage::Pointer> probabilisticOutSegs;
    std::vector<itkVolumeType::Pointer> outputSegmentations;

    core(cfgdata, probabilisticOutSegs, outputSplittedSegList, outputSegmentations);

    probabilisticOutSeg = probabilisticOutSegs.at(0);
    outputSegmentation = outputSegmentations.at(0);

}

bool CcboostAdapter::core(const ConfigData<itkVolumeType>& cfgdata,
                          std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs,
                          std::vector<itkVolumeType::Pointer>& outputSplittedSegList,
                          std::vector<itkVolumeType::Pointer>& outputSegmentations) {


    core(cfgdata, probabilisticOutSegs);

    //provisory test, assume that we have the full volume and paste it into one volume

    //TODO handle multiple output ROIs

    qDebug() << "output image";

    for(int roiidx = 0; roiidx < cfgdata.test.size(); roiidx++) {
        typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
        if(cfgdata.saveIntermediateVolumes && probabilisticOutSegs[roiidx]->VerifyRequestedRegion()) {
            fWriterType::Pointer writerf = fWriterType::New();
            writerf->SetFileName(cfgdata.cacheDir + std::to_string(roiidx) + "-1-" + "predicted.tif");
            writerf->SetInput(probabilisticOutSegs[roiidx]);
            writerf->Update();
        }

        qDebug() << "predicted.tif created";

        typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
                fBinaryThresholdImageFilterType;

        float lowerThreshold = 0.0;

        fBinaryThresholdImageFilterType::Pointer thresholdFilter
                = fBinaryThresholdImageFilterType::New();
        thresholdFilter->SetInput(probabilisticOutSegs[0]);
        thresholdFilter->SetLowerThreshold(lowerThreshold);
        thresholdFilter->SetInsideValue(255);
        thresholdFilter->SetOutsideValue(0);
        thresholdFilter->Update();

        itkVolumeType::Pointer outputSegmentation = thresholdFilter->GetOutput();

        WriterType::Pointer writer = WriterType::New();
        if(cfgdata.saveIntermediateVolumes && outputSegmentation->VerifyRequestedRegion()){
            writer->SetFileName(cfgdata.cacheDir + std::to_string(roiidx) + "-2-" + "thread-outputSegmentation.tif");
            writer->SetInput(outputSegmentation);
            writer->Update();
        }

        outputSegmentation->DisconnectPipeline();

        removeborders(cfgdata, outputSegmentation);

        outputSegmentation->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

        splitSegmentations(outputSegmentation, outputSplittedSegList, false, cfgdata.saveIntermediateVolumes, cfgdata.cacheDir);

        outputSegmentation->DisconnectPipeline();

        outputSegmentations.push_back(outputSegmentation);

    }

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));

    return true;
}

void CcboostAdapter::splitSegmentations(const itkVolumeType::Pointer& outputSegmentation,
                                        ESPINA::CCB::LabelMapType::Pointer& labelMap,
                                        int minCCSize){

    //FIXME we should a binary image, fix by forcing type?
    Q_ASSERT( itk::NumericTraits<itkVolumeType::PixelType>::max() == 255 );
    Q_ASSERT( itk::NumericTraits<itkVolumeType::PixelType>::min() == 0 );

    typedef itk::BinaryImageToShapeLabelMapFilter<itkVolumeType> BinaryImageToShapeLabelMapFilterType;
    typename BinaryImageToShapeLabelMapFilterType::Pointer binaryImageToShapeLabelMapFilter = BinaryImageToShapeLabelMapFilterType::New();
    binaryImageToShapeLabelMapFilter->SetInput(outputSegmentation);
    binaryImageToShapeLabelMapFilter->FullyConnectedOn();
    binaryImageToShapeLabelMapFilter->SetInputForegroundValue(255);
    binaryImageToShapeLabelMapFilter->SetOutputBackgroundValue(0);

    // Remove label objects that have PHYSICAL_SIZE less than physicalSizeThreshold
    typedef itk::ShapeOpeningLabelMapFilter< BinaryImageToShapeLabelMapFilterType::OutputImageType > ShapeOpeningLabelMapFilterType;
    ShapeOpeningLabelMapFilterType::Pointer shapeOpeningLabelMapFilter = ShapeOpeningLabelMapFilterType::New();
    shapeOpeningLabelMapFilter->SetInput( binaryImageToShapeLabelMapFilter->GetOutput() );
    shapeOpeningLabelMapFilter->SetLambda( minCCSize );
    qDebug() << "Removing components smaller than " << minCCSize << " voxels.";
    shapeOpeningLabelMapFilter->ReverseOrderingOff();
    //For a list of attributes see http://www.itk.org/Doxygen/html/itkLabelMapUtilities_8h_source.html
    shapeOpeningLabelMapFilter->SetAttribute( ShapeOpeningLabelMapFilterType::LabelObjectType::PHYSICAL_SIZE);

//TODO where should I deal with exceptions? Currently dealt at CcboostTask level, that is, CcboostTask::run()
//    try {

        shapeOpeningLabelMapFilter->Update();

//    } catch( itk::ExceptionObject & error ) {
//        std::cerr << __FILE__ << ":" << __LINE__ << "Error: " << error << std::endl;
//        return EXIT_FAILURE;
//    }

    labelMap = shapeOpeningLabelMapFilter->GetOutput();

    const unsigned int numObjects = labelMap->GetNumberOfLabelObjects();

    const unsigned int prevObjects = binaryImageToShapeLabelMapFilter->GetOutput()->GetNumberOfLabelObjects();
    std::cout << "There were " << prevObjects
              << " and only " << numObjects
              << " were bigger than " << minCCSize << std::endl;

    if(numObjects == 0) {
        std::cerr << "Warning: 0 objects present." << std::endl;
    }

    //To recover a volume from a labelMap, use the filter LabelMapToLabelImageFilter. e.g:
    //      typedef itk::LabelMapToLabelImageFilter<LabelMapType, itkVolumeType> Label2VolumeFilter;
    //      auto label2volume = Label2VolumeFilter::New();
    //           label2volume->SetInput(labelmap);
    //           label2volume->Update();
    //      itkVolumeType::Pointer volume = label2volume->GetOutput();


}

void CcboostAdapter::splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                                        std::vector<itkVolumeType::Pointer>& outSegList,
                                        bool skipBiggest,
                                        bool saveIntermediateVolumes,
                                        std::string cacheDir){

    /*split the output into several segmentations*/
    typedef itk::ConnectedComponentImageFilter <itkVolumeType, bigVolumeType >
            ConnectedComponentImageFilterType;

    ConnectedComponentImageFilterType::Pointer connected = ConnectedComponentImageFilterType::New ();
    connected->SetInput(outputSegmentation);

    connected->Update();

    std::cout << "Number of objects: " << connected->GetObjectCount() << std::endl;

    qDebug("Connected components segmentation");
    BigWriterType::Pointer bigwriter = BigWriterType::New();

    if(saveIntermediateVolumes){
        bigwriter->SetFileName(cacheDir + "5" + "connected-segmentation.tif");
        bigwriter->SetInput(connected->GetOutput());
        bigwriter->Update();
    }

    typedef itk::RelabelComponentImageFilter<bigVolumeType, bigVolumeType> RelabelFilterType;
    RelabelFilterType::Pointer relabelFilter = RelabelFilterType::New();
    relabelFilter->SetInput(connected->GetOutput());
    relabelFilter->Update();
    if(saveIntermediateVolumes) {
        std::string saveFilenamePath = cacheDir + "6" + "relabeled-segmentation.tif";
        qDebug("Save relabeled segmentation at ");
        qDebug(saveFilenamePath.c_str());
        bigwriter->SetFileName(saveFilenamePath);
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

    qDebug("Create Itk segmentations");

    //TODO espina2 ccboostconfig
    for(int i = skipBiggest? 1 : 0; i < connected->GetObjectCount(); i++){
        labelThresholdFilter->SetInput(relabelFilter->GetOutput());
        labelThresholdFilter->SetLowerThreshold(i);
        labelThresholdFilter->SetUpperThreshold(i);
        labelThresholdFilter->Update();

        itkVolumeType::Pointer outSeg = labelThresholdFilter->GetOutput();
        outSeg->DisconnectPipeline();
        outSegList.push_back(outSeg);

    }
}



void CcboostAdapter::computeAllFeatures(const ConfigData<itkVolumeType> cfgData) {

    for(auto cfgROI: cfgData.train){
        //        if(!caller->canExecute())
        //                return;
        computeFeatures(cfgData, cfgROI);
    }
    for(auto cfgROI: cfgData.test){
        //        if(!caller->canExecute())
        //                return;
        computeFeatures(cfgData, cfgROI);
    }

}

void CcboostAdapter::computeFeatures(const ConfigData<itkVolumeType> cfgData,
                                     const SetConfigData<itkVolumeType> cfgDataROI) {


    //TODO can we give message with?
    //    if(!caller->canExecute())
    //        return;

    std::stringstream strstream;

    int featureNum = 0;
    for( const std::string& featureFile: cfgDataROI.otherFeatures ) {

        strstream.str(std::string());
        featureNum++;
        strstream << "Computing " << featureNum << "/" << cfgDataROI.otherFeatures.size() << " features";
        std::cout << strstream.str() << std::endl;
        //       if(!caller->canExecute())
        //           return;
        //stateDescription->setTime(strstream.str());

        std::string featureFilepath(cfgData.cacheDir + featureFile);
        ifstream featureStream(featureFilepath.c_str());
        //TODO check hash to prevent recomputing
        if(!featureStream.good() || cfgData.forceRecomputeFeatures){

            computeFeature(cfgDataROI, cfgData.cacheDir, featureFile);

        }
    }

    //    stateDescription->setMessage("Features computed");
    //    stateDescription->setTime("");
}

void CcboostAdapter::computeFeature(const SetConfigData<itkVolumeType> cfgDataROI,
                                    const std::string cacheDir,
                                    const std::string featureFile){
    std::string featureFilepath(cacheDir + featureFile);

    //TODO do this in a more flexible way
    int stringPos;
    if( (stringPos = featureFile.find(std::string("hessOrient-"))) != std::string::npos){
        float sigma;
        std::string filename(featureFile.substr(stringPos));
        std::string matchStr = std::string("hessOrient-s%f-repolarized") + FEATUREEXTENSION;
        int result = sscanf(filename.c_str(),matchStr.c_str(),&sigma);
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
        std::string matchStr = std::string("gradient-magnitude-s%f") + FEATUREEXTENSION;
        int result = sscanf(filename.c_str(),matchStr.c_str(),&sigma);
        if(result < 0)
            qDebug() << "Error scanning feature name " << featureFile.c_str();

        qDebug() << QString("Current  or channel is new or has changed or gradient magnitude feature not found at path: %2. Creating with sigma %1").arg(sigma).arg(QString::fromStdString(featureFilepath));

        GradientMagnitudeImageFilter2Execute(sigma, cfgDataROI.rawVolumeImage, featureFilepath, cfgDataROI.zAnisotropyFactor);

    } else if( (stringPos = featureFile.find(std::string("stensor"))) != std::string::npos ) {
        float rho,sigma;
        std::string filename(featureFile.substr(stringPos));
        std::string matchStr = std::string("stensor-s%f-r%f") + FEATUREEXTENSION;
        int result = sscanf(filename.c_str(),matchStr.c_str(),&sigma,&rho);
        if(result < 0)
            qDebug() << "Error scanning feature name" << featureFile.c_str();

        qDebug() << QString("Current channel is new or has changed or tensor feature not found at path %3. Creating with rho %1 and sigma %2").arg(rho).arg(sigma).arg(QString::fromStdString(featureFilepath));

        bool sortByMagnitude = true;
        EigenOfStructureTensorImageFilter2Execute(sigma, rho, cfgDataROI.rawVolumeImage, featureFilepath, sortByMagnitude, cfgDataROI.zAnisotropyFactor );

    } else {
        qDebug() << QString("feature %1 is not recognized.").arg(featureFile.c_str()).toUtf8();
    }
}

void CcboostAdapter::addAllFeatures(const ConfigData<itkVolumeType> &cfgData, MultipleROIData::ROIDataPtr roi ) {

    //do this first thing tomorrow
    //FIXME //TODO add Orient "feature"
    qDebug("Adding train's features");
    for(auto cfgDataROI: cfgData.train)
        addFeatures(cfgData.cacheDir, cfgDataROI, roi);
    qDebug("Adding test's features");
    for(auto cfgDataROI: cfgData.test)
        addFeatures(cfgData.cacheDir, cfgDataROI, roi);
}

void CcboostAdapter::addFeatures(const std::string& cacheDir,
                                 const SetConfigData<itkVolumeType>& cfgDataROI,
                                 MultipleROIData::ROIDataPtr roi ) {
    // now add channels
    typedef itk::VectorImage<float, 3>  ItkVectorImageType;
    for ( const auto& chanFName: cfgDataROI.otherFeatures )
    {
        const auto &featFName = cacheDir + chanFName;
        qDebug("Adding feature %s", featFName.c_str());

        itk::ImageFileReader<ItkVectorImageType>::Pointer reader = itk::ImageFileReader<ItkVectorImageType>::New();
        try {
            reader->SetFileName( featFName );
            reader->Update();
        } catch(std::exception &e)
        {
            std::cerr << "Error reading feature " << featFName << ". Message: " << e.what() << std::endl;
            continue;
        }

        ItkVectorImageType::Pointer img = reader->GetOutput();

        ItkVectorImageType::SizeType imSize = img->GetLargestPossibleRegion().GetSize();

        unsigned int mWidth = imSize[0];
        unsigned int mHeight = imSize[1];
        unsigned int mDepth = imSize[2];

        if ( (mWidth != roi->rawImage.width()) || (mHeight != roi->rawImage.height()) || (mDepth != roi->rawImage.depth()) ) {
            qDebug("Feature image size differs from raw image: %s. (%dx%dx%d vs %dx%dx%d). Recomputing.",featFName.c_str(), mWidth, mHeight, mDepth,
                   roi->rawImage.width(), roi->rawImage.height(), roi->rawImage.depth());

            computeFeature(cfgDataROI, cacheDir, chanFName);

            try {
                reader->SetFileName( featFName );
                reader->Modified(); //Since filename (or else) didn't change, we must manually notify the filter that the file changed
                //reader->UpdateOutputInformation();
                reader->UpdateLargestPossibleRegion();
                reader->Update();
            } catch(std::exception &e){
                std::cerr << "Error reading feature " << featFName << ". Message: " << e.what() << std::endl;
                continue;
            }

            img = reader->GetOutput();

            imSize = img->GetLargestPossibleRegion().GetSize();

            mWidth = imSize[0];
            mHeight = imSize[1];
            mDepth = imSize[2];

            if ( (mWidth != roi->rawImage.width()) || (mHeight != roi->rawImage.height()) || (mDepth != roi->rawImage.depth()) )
                qFatal("Feature image size still differs from raw image: %s. (%dx%dx%d vs %dx%dx%d). Aborting.",featFName.c_str(), mWidth, mHeight, mDepth,
                       roi->rawImage.width(), roi->rawImage.height(), roi->rawImage.depth());
        }


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

        unsigned int mComp = img->GetNumberOfComponentsPerPixel();

        // reset spacing so that we read pixels directly
        ItkVectorImageType::SpacingType spacing = img->GetSpacing();
        spacing[0] = spacing[1] = spacing[2] = 1.0;
        img->SetSpacing(spacing);


        for (unsigned q=0; q < mComp; q++)
        {
            Matrix3D<float> auxImg;
            auxImg.reallocSizeLike( roi->rawImage );

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

            roi->
                    addII( std::move(ii), chanType );
        }
    }
}

//#include "CcboostAdapter.in"

//void CcboostAdapter::removeborders(const ConfigData< itkVolumeType > &cfgData, itkVolumeType::Pointer& outputSegmentation);
//void CcboostAdapter::removeborders(const ConfigData< FloatTypeImage > &cfgData, FloatTypeImage::Pointer& outputSegmentation);

//void CcboostAdapter::removeborders(itkVolumeType::Pointer& outputSegmentation, bool saveIntermediateVolumes, std::string cacheDir);
//void CcboostAdapter::removeborders(FloatTypeImage::Pointer& outputSegmentation, bool saveIntermediateVolumes, std::string cacheDir);
