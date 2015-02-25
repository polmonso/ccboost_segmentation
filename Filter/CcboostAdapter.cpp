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

#include <itkConstantPadImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include "itkImageDuplicator.h"

//using namespace ESPINA;

#ifndef ESPINA_SETTINGS
#define ESPINA_SETTINGS(settings) QSettings settings("CeSViMa", "ESPINA");
#endif

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
#define WORK
#ifdef WORK

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

        //nofeatures add
        //#define NOFEATURESTEST
        #ifdef NOFEATURESTEST
        {
            MultipleROIData allROIssimple;

            // set anisotropy factor
            allROIs.init( cfgdata.train.at(0).zAnisotropyFactor );

            allROIssimple.add( &roi );

            qDebug() << "running core plugin";

            BoosterInputData bdatasimple;
            bdatasimple.init( &allROIssimple );
            bdatasimple.showInfo();

            Booster adaboostsimple;
            qDebug() << "training";
            adaboostsimple.train( bdatasimple, 500 );
            qDebug() << "predict";
            Matrix3D<float> predImgsimple;
            adaboostsimple.predictDoublePolarity( &allROIssimple, &predImgsimple );

            qDebug() << "output image without features";

            typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
            fWriterType::Pointer writerf = fWriterType::New();
            writerf->SetFileName(cfgdata.cacheDir + "simplepredicted.tif");
            writerf->SetInput(predImgsimple.asItkImage());
            writerf->Update();
            qDebug() << "simplepredicted.tif created";
        }
        #endif
        //no features end

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

    adaboost.train( bdata, 500 );

    qDebug("train Elapsed: %f", timerF.elapsed());

    // save JSON model
      if (!adaboost.saveModelToFile( "/tmp/model.json" ))
          std::cout << "Error saving JSON model" << std::endl;

      // predict

    qDebug() << "predict";

    for(SetConfigData<itkVolumeType> testData: cfgdata.test) {

        Matrix3D<ImagePixelType> img, gt;

        img.loadItkImage(testData.rawVolumeImage, true);
        //FIXME why do we need ground truth on predict?
//#warning why do we need ground truth on predict?
//        gt.loadItkImage(testData.groundTruthImage, true);

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

        probabilisticOutSeg->DisconnectPipeline();

        probabilisticOutSegs.push_back(probabilisticOutSeg);

    }

#else
    for(int roiidx = 0; roiidx < cfgdata.test.size(); roiidx++) {
        typedef itk::ImageFileReader< FloatTypeImage > fReaderType;
        fReaderType::Pointer freader = fReaderType::New();
        freader->SetFileName(cfgdata.cacheDir + std::to_string(roiidx) + "-1-" + "predicted.tif");
        freader->Update();
        FloatTypeImage::Pointer probabilisticOutSeg = freader->GetOutput();
        probabilisticOutSeg->DisconnectPipeline();
        probabilisticOutSegs.push_back(probabilisticOutSeg);
    }
#endif
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
        } else {
            //TODO what to do when region fails Verify? is return false enough?
            //return false;
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

        postprocessing(cfgdata, outputSegmentation);

        outputSegmentation->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

        splitSegmentations(outputSegmentation, outputSplittedSegList, cfgdata.saveIntermediateVolumes, cfgdata.cacheDir);

        outputSegmentation->DisconnectPipeline();

        outputSegmentations.push_back(outputSegmentation);

    }

     ESPINA_SETTINGS(settings);
     settings.beginGroup("ccboost segmentation");
     settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));

     return true;
}

bool CcboostAdapter::testcore(const ConfigData<itkVolumeType>& cfgdata,
                          std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs,
                          std::vector<itkVolumeType::Pointer>& outSegList,
                          std::vector<itkVolumeType::Pointer>& outputSegmentations) {

    BoosterInputData::MultipleROIDataPtr allTrainROIs = std::make_shared<MultipleROIData>();

    SetConfigData<itkVolumeType> trainData = cfgdata.train.at(0);

    Matrix3D<ImagePixelType> trainimg, traingt;

    //itkVolumeType::Pointer annotatedImage = Splitter::crop<itkVolumeType>(cfgData.train.groundTruthImage,cfgData.train.annotatedRegion);

    trainimg.loadItkImage(trainData.rawVolumeImage, true);
    traingt.loadItkImage(trainData.groundTruthImage, true);

    MultipleROIData::ROIDataPtr trainroi = std::make_shared<ROIData>();;
    trainroi->init( trainimg.data(), traingt.data(), 0, 0, trainimg.width(), trainimg.height(), trainimg.depth(), cfgdata.train.at(0).zAnisotropyFactor );

    // raw image integral image
    {
        ROIData::IntegralImageType ii;
        ii.compute( trainimg );
        trainroi->addII( std::move(ii), ROIData::ChannelType::GAUSS );
    }

    TimerRT timerF; timerF.reset();

    computeFeatures(cfgdata, trainData);

    qDebug("Compute all features Elapsed: %f", timerF.elapsed());

    timerF.reset();

    addFeatures(cfgdata.cacheDir, trainData, trainroi);

    qDebug("Add all features Elapsed: %f", timerF.elapsed());

    allTrainROIs->add( trainroi );

    qDebug() << "running core plugin";

    BoosterInputData bdata;
    bdata.init( allTrainROIs );
    bdata.showInfo();

    Booster adaboost;

    qDebug() << "training";

    timerF.reset();

    adaboost.train( bdata, 500 );

    qDebug("train Elapsed: %f", timerF.elapsed());

    // save JSON model
      if (!adaboost.saveModelToFile( "/tmp/model.json" ))
          std::cout << "Error saving JSON model" << std::endl;

      // predict

      qDebug() << "predict";

      SetConfigData<itkVolumeType> testData = cfgdata.test.at(0);

      Matrix3D<ImagePixelType> img, gt;

      img.loadItkImage(testData.rawVolumeImage, true);
      gt.loadItkImage(testData.groundTruthImage, true);

      MultipleROIData::ROIDataPtr roi = std::make_shared<ROIData>();
      roi->init( img.data(), gt.data(), 0, 0, img.width(), img.height(), img.depth(), cfgdata.train.at(0).zAnisotropyFactor );

      // raw image integral image
      {
          ROIData::IntegralImageType ii;
          ii.compute( img );
          roi->addII( std::move(ii), ROIData::ChannelType::GAUSS );
      }

      //features
      timerF; timerF.reset();

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
      adaboost.predictDoublePolarity( testROI, &predImg );
      //predImg.save("/tmp/test.nrrd");

      qDebug("Elapsed: %f", timer.elapsed());

      typedef itk::ImageDuplicator< FloatTypeImage > DuplicatorType;
      DuplicatorType::Pointer duplicator = DuplicatorType::New();
      duplicator->SetInputImage(predImg.asItkImage());
      duplicator->Update();
      FloatTypeImage::Pointer probabilisticOutSeg = duplicator->GetModifiableOutput();

      probabilisticOutSeg->DisconnectPipeline();

      probabilisticOutSegs.push_back(probabilisticOutSeg);


    //provisory test, assume that we have the full volume and paste it into one volume

    //TODO handle multiple output ROIs

    qDebug() << "output image";

    typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
    if(cfgdata.saveIntermediateVolumes && probabilisticOutSegs[0]->VerifyRequestedRegion()) {
        fWriterType::Pointer writerf = fWriterType::New();
        writerf->SetFileName(cfgdata.cacheDir + "predicted.tif");
        writerf->SetInput(probabilisticOutSegs[0]);
        writerf->Update();
    } else {
        //TODO what to do when region fails Verify? is return false enough?
        //return false;
    }
    qDebug() << "predicted.tif created";


    typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
            fBinaryThresholdImageFilterType;

    //float lowerThreshold = 0.0;
    float lowerThreshold = 0.0;

    fBinaryThresholdImageFilterType::Pointer thresholdFilter
            = fBinaryThresholdImageFilterType::New();
    thresholdFilter->SetInput(probabilisticOutSegs[0]);
    thresholdFilter->SetLowerThreshold(lowerThreshold);
    thresholdFilter->SetInsideValue(255);
    thresholdFilter->SetOutsideValue(0);
    thresholdFilter->Update();

    WriterType::Pointer writer = WriterType::New();
    if(cfgdata.saveIntermediateVolumes && thresholdFilter->GetOutput()->VerifyRequestedRegion()) {
        writer->SetFileName(cfgdata.cacheDir + "1" + "predicted-thresholded.tif");
        writer->SetInput(thresholdFilter->GetOutput());
        writer->Update();
    }

    itkVolumeType::Pointer outputSegmentation = thresholdFilter->GetOutput();

     if(cfgdata.saveIntermediateVolumes && outputSegmentation->VerifyRequestedRegion()){
         writer->SetFileName(cfgdata.cacheDir + "2" + "thread-outputSegmentation.tif");
         writer->SetInput(outputSegmentation);
         writer->Update();
     }


     outputSegmentation->DisconnectPipeline();

     postprocessing(cfgdata, outputSegmentation);

     outputSegmentation->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

     splitSegmentations(outputSegmentation, outSegList, cfgdata.saveIntermediateVolumes, cfgdata.cacheDir);

     outputSegmentation->DisconnectPipeline();

     outputSegmentations.push_back(outputSegmentation);

     ESPINA_SETTINGS(settings);
     settings.beginGroup("ccboost segmentation");
     settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));

     return true;
}

bool CcboostAdapter::automaticCore(const ConfigData<itkVolumeType>& cfgdata,
                          FloatTypeImage::Pointer& probabilisticOutSeg,
                          std::vector<itkVolumeType::Pointer>& outSegList) {
#define WORK
#ifdef WORK2
    MultipleROIData allROIs;

    // set anisotropy factor
    allROIs.init( cfgdata.train.at(0).zAnisotropyFactor );

    //    for(auto imgItk: channels) {
    //        for(auto gtItk: groundTruths) {
    Matrix3D<ImagePixelType> img, gt;

    //itkVolumeType::Pointer annotatedImage = Splitter::crop<itkVolumeType>(cfgData.train.groundTruthImage,cfgData.train.annotatedRegion);

    img.loadItkImage(cfgdata.train.at(0).rawVolumeImage, true);

    gt.loadItkImage(cfgdata.train.at(0).groundTruthImage, true);

    ROIData roi;
    roi.init( img.data(), gt.data(), 0, 0, img.width(), img.height(), img.depth(), cfgdata.train.at(0).zAnisotropyFactor );

    // raw image integral image
    ROIData::IntegralImageType ii;
    ii.compute( img );
    roi.addII( ii.internalImage().data() );

    //features
    TimerRT timerF; timerF.reset();

    computeAllFeatures(cfgdata);

    qDebug("Compute all features Elapsed: %f", timerF.elapsed());

    timerF.reset();

    addAllFeatures(cfgdata, roi);

    qDebug("Add all features Elapsed: %f", timerF.elapsed());

    allROIs.add( &roi );

    // }
    // }

    qDebug() << "running core plugin";

    BoosterInputData bdata;
    bdata.init( &allROIs );
    bdata.showInfo();

    Booster adaboost;

    qDebug() << "training";

    timerF.reset();

    // predict
    Matrix3D<float> predImg;

    //TODO provide a stumped train
    if(!cfgdata.automaticComputation) {

        adaboost.train( bdata, 500 );

        qDebug("train Elapsed: %f", timerF.elapsed());

        qDebug() << "predict";

        TimerRT timer; timer.reset();
        adaboost.predictDoublePolarity( &allROIs, &predImg );
        qDebug("Elapsed: %f", timer.elapsed());

    } else {

        int stumpindex = 0;
        do {

            int sizetrain = cfgdata.train.size();
            int pendingTrain = cfgdata.pendingTrain.size();

            cfgdata.train.reserve(sizetrain + pendingTrain);
            if(!cfgdata.pendingTrain.empty()){
                std::move(std::begin(cfgdata.train), std::end(cfgdata.train), std::back_inserter(cfgdata.pendingTrain));
                cfgdata.pendingTrain.clear();
            }
            //adaboost.trainStump( bdata, 100, stumpindex);

            Matrix3D<float> predImg;
            TimerRT timer; timer.reset();
            adaboost.predictDoublePolarity( &allROIs, &predImg );
            qDebug("Elapsed: %f", timer.elapsed());

            //emit(updatePrediction(predImg.asItkImage()));

            stumpindex++;
        } while(stumpindex < cfgdata.numStumps);
    }

    predImg.save("/tmp/test.nrrd");

    // save JSON model
    if (!adaboost.saveModelToFile( "/tmp/model.json" ))
        std::cout << "Error saving JSON model" << std::endl;

    probabilisticOutSeg = predImg.asItkImage();
//    probabilisticOutSeg->DisconnectPipeline();

#else
    typedef itk::ImageFileReader< FloatTypeImage > ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(cfgdata.cacheDir + "predicted.tif");
    reader->Update();
    probabilisticOutSeg = reader->GetOutput();
    probabilisticOutSeg->DisconnectPipeline();
    return true;
#endif
    qDebug() << "output image";

    typedef itk::ImageFileWriter< FloatTypeImage > fWriterType;
    if(cfgdata.saveIntermediateVolumes && probabilisticOutSeg->VerifyRequestedRegion()) {
        fWriterType::Pointer writerf = fWriterType::New();
        writerf->SetFileName(cfgdata.cacheDir + "predicted.tif");
        writerf->SetInput(probabilisticOutSeg);
        writerf->Update();
    } else {
        //TODO what to do when region fails Verify? is return false enough?
        //return false;
    }
    qDebug() << "predicted.tif created";

    typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
            fBinaryThresholdImageFilterType;

    float lowerThreshold = 0.0;

    fBinaryThresholdImageFilterType::Pointer thresholdFilter
            = fBinaryThresholdImageFilterType::New();
    thresholdFilter->SetInput(probabilisticOutSeg);
    thresholdFilter->SetLowerThreshold(lowerThreshold);
    thresholdFilter->SetInsideValue(255);
    thresholdFilter->SetOutsideValue(0);
    thresholdFilter->Update();

    WriterType::Pointer writer = WriterType::New();
    if(cfgdata.saveIntermediateVolumes && thresholdFilter->GetOutput()->VerifyRequestedRegion()) {
        writer->SetFileName(cfgdata.cacheDir + "1" + "predicted-thresholded.tif");
        writer->SetInput(thresholdFilter->GetOutput());
        writer->Update();
    }

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    settings.setValue("Channel Hash", QString(cfgdata.train.at(0).featuresRawVolumeImageHash.c_str()));

    if(!cfgdata.usePreview) {
        itkVolumeType::Pointer outputSegmentation = thresholdFilter->GetOutput();

        if(cfgdata.saveIntermediateVolumes && outputSegmentation->VerifyRequestedRegion()){
            writer->SetFileName(cfgdata.cacheDir + "2" + "thread-outputSegmentation.tif");
            writer->SetInput(outputSegmentation);
            writer->Update();
        }

        outputSegmentation->DisconnectPipeline();

        postprocessing(cfgdata, outputSegmentation);

        outputSegmentation->SetSpacing(cfgdata.train.at(0).rawVolumeImage->GetSpacing());

        splitSegmentations(outputSegmentation, outSegList, cfgdata.saveIntermediateVolumes, cfgdata.cacheDir);

    }
    return true;
}

void CcboostAdapter::splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                                        std::vector<itkVolumeType::Pointer>& outSegList,
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
        qDebug("Save relabeled segmentation");
        bigwriter->SetFileName(cacheDir + "6" + "relabeled-segmentation.tif");
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
//    for(int i=1; i < connected->GetObjectCount() && i < ccboostconfig.maxNumObjects; i++){
    for(int i=1; i < connected->GetObjectCount(); i++){
        labelThresholdFilter->SetInput(relabelFilter->GetOutput());
        labelThresholdFilter->SetLowerThreshold(i);
        labelThresholdFilter->SetUpperThreshold(i);
        labelThresholdFilter->Update();

        itkVolumeType::Pointer outSeg = labelThresholdFilter->GetOutput();
        outSeg->DisconnectPipeline();
        outSegList.push_back(outSeg);

    }

}

void CcboostAdapter::postprocessing(const ConfigData<itkVolumeType>& cfgData,
                                    itkVolumeType::Pointer& outputSegmentation) {


postprocessing(outputSegmentation, cfgData.train.at(0).zAnisotropyFactor,
               cfgData.saveIntermediateVolumes, cfgData.cacheDir);

}

void CcboostAdapter::postprocessing(itkVolumeType::Pointer& outputSegmentation,
                                    double zAnisotropyFactor,
                                    bool saveIntermediateVolumes,
                                    std::string cacheDir) {



    qDebug("remove borders");

    itkVolumeType::SizeType outputSize = outputSegmentation->GetLargestPossibleRegion().GetSize();

    const int borderToRemove = 2;
    const int borderZ = std::max((int)1, (int)(borderToRemove/zAnisotropyFactor + 0.5));
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
        writer->SetFileName(cacheDir + "3" + "ccOutputSegmentation-noborders.tif");
        writer->SetInput(outputSegmentation);
        writer->Update();
        qDebug("cc boost segmentation saved");
    }

    qDebug("Remove small components");

    removeSmallComponents(outputSegmentation);

    if(saveIntermediateVolumes) {
        writer->SetFileName(cacheDir + "4" + "outputSegmentation-nosmallcomponents.tif");
        writer->SetInput(outputSegmentation);
        writer->Update();
    }

    qDebug("Removed");

    outputSegmentation->DisconnectPipeline();

}

void CcboostAdapter::removeSmallComponents(itk::Image<unsigned char, 3>::Pointer & segmentation, int minCCSize, int threshold) {

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
