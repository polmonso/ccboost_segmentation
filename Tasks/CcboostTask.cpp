/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2014  <copyright holder> <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "CcboostTask.h"

#include <QElapsedTimer>
#include <omp.h>

#include <sys/sysinfo.h> //memory available

//ccboost
#include <Filter/CcboostAdapter.h>

// ESPINA
#include <Core/Analysis/Segmentation.h>
#include <Core/Analysis/Data/MeshData.h>
#include <Core/Analysis/Data/Mesh/RawMesh.h>
#include <Core/Analysis/Data/Volumetric/RasterizedVolume.hxx>
#include <GUI/Representations/SliceRepresentation.h>
#include <GUI/Representations/MeshRepresentation.h>

// Qt
#include <QtGlobal>
#include <QSettings>
#include <Support/Settings/EspinaSettings.h>

// ITK
#include <itkImageDuplicator.h>
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
#include <itkTestingHashImageFilter.h>

#include <itkConstantPadImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>

using namespace ESPINA;
using namespace CCB;
using namespace std;

const QString CVL = "CVL";


const QString CcboostTask::BACKGROUND = "background";
const QString CcboostTask::MITOCHONDRIA = "mitochondria";
const QString CcboostTask::SYNAPSE = "synapse";
QString CcboostTask::ELEMENT = "element";
const QString CcboostTask::POSITIVETAG = "yes"; //Tags are always lowercase
const QString CcboostTask::NEGATIVETAG = "no";

//------------------------------------------------------------------------
CcboostTask::CcboostTask(ChannelAdapterPtr channel,
                         SchedulerSPtr     scheduler)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
{
#ifdef __unix
  struct sysinfo info;
  sysinfo(&info);
  memoryAvailableMB = info.totalram/1024/1024;

  //TODO FIXME quick test of limited memory resources.
 // memoryAvailableMB = 6000;

  if(memoryAvailableMB == 0){
      memoryAvailableMB = numeric_limits<unsigned int>::max();
     //FIXME we're not on the main thread, dialog creation will crash
//      QMessageBox::warning(NULL,"RAM Memory check", QString("The amount of RAM memory couldn't be detected or very low (info.totalram = %1 Bytes). "
//                                                            "Running the Synapse Segmentation Plugin might lead to crash").arg(info.totalram));

      emit message(QString("The amount of RAM memory couldn't be detected or very low (info.totalram = %1 Bytes). "
                   "Running the Synapse Segmentation Plugin might lead to crash").arg(info.totalram));


  }
#else
  memoryAvailableMB = numeric_limits<unsigned int>::max();
//  QMessageBox::warning(NULL,"RAM Memory check", "The amount of RAM memory couldn't be detected."
//                       "Running the Synapse Segmentation Plugin might lead to crash");
  emit message(QString("The amount of RAM memory couldn't be detected or very low (info.totalram = %1 Bytes). "
               "Running the Synapse Segmentation Plugin might lead to crash").arg(info.totalram));

#endif



}

//------------------------------------------------------------------------
void CcboostTask::run()
{
  QElapsedTimer timer;
  timer.start();


  // Determine the number of threads to use.
  const int num_threads = omp_get_num_procs() - 1;
  qDebug() << "Number of threads: " << num_threads;

  emit progress(0);
  if (!canExecute()) return;

  //TODO do I have to put back the original locale?
  std::setlocale(LC_NUMERIC, "C");

  m_inputChannel = volumetricData(m_channel->asInput()->output())->itkImage();
  itkVolumeType::Pointer channelItk = m_inputChannel;

  //m_inputSegmentation = volumetricData(m_inputs[1]->output())->itkImage();

  if(!enoughGroundTruth())
      return;

  typedef itk::ChangeInformationImageFilter< itkVolumeType > ChangeInformationFilterType;
  ChangeInformationFilterType::Pointer normalizeFilter = ChangeInformationFilterType::New();
  normalizeFilter->SetInput(channelItk);
  itkVolumeType::SpacingType spacing = channelItk->GetSpacing();
  spacing[2] = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
               + channelItk->GetSpacing()[1]);
  spacing[0] = 1;
  spacing[1] = 1;
  normalizeFilter->SetOutputSpacing( spacing );
  normalizeFilter->ChangeSpacingOn();
  normalizeFilter->Update();
  std::cout << "Using spacing: " << spacing << std::endl;
  itkVolumeType::Pointer normalizedChannelItk = normalizeFilter->GetOutput();

  emit progress(20);
  if (!canExecute()) return;

  itkVolumeType::Pointer segmentedGroundTruth = mergeSegmentations(normalizedChannelItk,
                                                                   m_groundTruthSegList,
                                                                   m_backgroundGroundTruthSegList);
  //save itk image (volume) as binary/classed volume here
     WriterType::Pointer writer = WriterType::New();
     if(ccboostconfig.saveIntermediateVolumes){
       writer->SetFileName(ccboostconfig.cacheDir + "labelmap.tif");
       writer->SetInput(segmentedGroundTruth);
       writer->Update();
       qDebug() << "labelmap.tif created";
     }

  //Get bounding box of annotated data
  typedef itk::ImageMaskSpatialObject< 3 > ImageMaskSpatialObjectType;
  ImageMaskSpatialObjectType::Pointer
          imageMaskSpatialObject  = ImageMaskSpatialObjectType::New();
  imageMaskSpatialObject->SetImage ( segmentedGroundTruth );
  itkVolumeType::RegionType annotatedRegion = imageMaskSpatialObject->GetAxisAlignedBoundingBoxRegion();
  itkVolumeType::OffsetValueType offset(CcboostTask::ANNOTATEDPADDING);
  annotatedRegion.PadByRadius(offset);

  annotatedRegion.Crop(channelItk->GetLargestPossibleRegion());

  unsigned int numPredictRegions;
  if(!enoughMemory(channelItk, annotatedRegion, numPredictRegions))
      return;

  //Create config object and retrieve settings from espina
  ConfigData<itkVolumeType>::setDefault(ccboostconfig);

  ccboostconfig.numPredictRegions = numPredictRegions;

  applyEspinaSettings(ccboostconfig);

  if(CcboostTask::ELEMENT == CcboostTask::MITOCHONDRIA)
      ccboostconfig.preset = ConfigData<itkVolumeType>::MITOCHONDRIA;
  else if(CcboostTask::ELEMENT == CcboostTask::SYNAPSE)
      ccboostconfig.preset = ConfigData<itkVolumeType>::SYNAPSE;
  else {
      qWarning() << "Error! Preset is not set. Using default: " << CcboostTask::SYNAPSE;
      ccboostconfig.preset = ConfigData<itkVolumeType>::SYNAPSE;
  }

  ccboostconfig.originalVolumeImage = normalizedChannelItk;

  //FIXME computing the hash on cfgData.train.rawVolumeImage provokes a segFault
  //TODO each volume (and feature) should have its hash, but we are just using the first throught cachedir
  typedef itk::ImageDuplicator< itkVolumeType > DuplicatorType;
  DuplicatorType::Pointer duplicator = DuplicatorType::New();
  duplicator->SetInputImage(ccboostconfig.originalVolumeImage);
  duplicator->Update();
  itkVolumeType::Pointer inpImg = duplicator->GetOutput();
  inpImg->DisconnectPipeline();
  typedef itk::Testing::HashImageFilter<itkVolumeType> HashImageFilterType;
  HashImageFilterType::Pointer hashImageFilter = HashImageFilterType::New();
  hashImageFilter->SetInput(inpImg);
  hashImageFilter->Update();
  string hash = hashImageFilter->GetHash();

  ccboostconfig.rawVolume = "supervoxelcache-";
  ccboostconfig.cacheDir = ccboostconfig.cacheDir + ccboostconfig.rawVolume + hash
          + QString::number(spacing[2]).toStdString() + "/";
  //FIXME use channel filename instead of supervoxelcache
  qDebug("Output directory: %s", ccboostconfig.cacheDir.c_str());
  QDir dir(QString::fromStdString(ccboostconfig.cacheDir));
  dir.mkpath(QString::fromStdString(ccboostconfig.cacheDir));

  //volume spliting
  //FIXME decide whether or not to divide the volume depending on the memory available and size of it
  bool divideVolume = true;

  if(!divideVolume) {


      SetConfigData<itkVolumeType> trainData;
      SetConfigData<itkVolumeType>::setDefaultSet(trainData);
      trainData.rawVolumeImage = ccboostconfig.originalVolumeImage;
      trainData.groundTruthImage = segmentedGroundTruth;
      trainData.zAnisotropyFactor = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
              + channelItk->GetSpacing()[1]);
      trainData.annotatedRegion = annotatedRegion;

      ccboostconfig.train.push_back(trainData);

      // Test
      //FIXME test should get the whole volume or pieces, it is not necessarily a copy of train.
      SetConfigData<itkVolumeType> testData = trainData;
      testData.rawVolumeImage = ccboostconfig.originalVolumeImage;
      testData.zAnisotropyFactor = trainData.zAnisotropyFactor;

      ccboostconfig.test.push_back(testData);

      //      CcboostAdapter::core(cfgdata,
      //                           probabilisticOutSegs,
      //                           outSegList,
      //                           outputSegmentations);

      //      outputSegmentation = outputSegmentations.at(0);

  } else {

      itkVolumeType::OffsetType overlap{30,30.0};

      typedef itk::ImageSplitter< itkVolumeType > SplitterType;


//      cfgdata.numPredictRegions = SplitterType::numRegionsFittingInMemory( region.GetSize(),
//                                                                           CcboostAdapter::FREEMEMORYREQUIREDPROPORTIONPREDICT);

      SplitterType trainSplitter(annotatedRegion, ccboostconfig.numPredictRegions, overlap);

      trainSize = trainSplitter.getCropRegions().size();

      for(int i=0; i < trainSize; i++){

          std::string id = QString::number(i).toStdString();

          SetConfigData<itkVolumeType> trainData;
          SetConfigData<itkVolumeType>::setDefaultSet(trainData, id);
          trainData.zAnisotropyFactor = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
                  + channelItk->GetSpacing()[1]);
          trainData.rawVolumeImage = trainSplitter.crop(ccboostconfig.originalVolumeImage,
                                                        trainSplitter.getCropRegions().at(i));
          trainData.groundTruthImage = trainSplitter.crop(inputVolumeGTITK,
                                                          trainSplitter.getCropRegions().at(i));

          ccboostconfig.train.push_back(trainData);

      }

      //TODO we could reuse if regions match
      SplitterType testSplitter(annotatedRegion, ccboostconfig.numPredictRegions, overlap);

      testSize = testSplitter.getCropRegions().size();

      for(int i=trainSize; i < trainSize+testSize; i++){

          std::string id = QString::number(i).toStdString();

          SetConfigData<itkVolumeType> testData(trainData);
          SetConfigData<itkVolumeType>::setDefaultSet(testData, id);
          trainData.zAnisotropyFactor = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
                  + channelItk->GetSpacing()[1]);
          testData.rawVolumeImage = trainData.rawVolumeImage;
          testData.orientEstimate = trainData.orientEstimate;
          testData.rawVolumeImage = testSplitter.crop(cfgdata.originalVolumeImage,
                                                      testSplitter.getCropRegions().at(i));
          testData.groundTruthImage = testSplitter.crop(inputVolumeGTITK,
                                                        testSplitter.getCropRegions().at(i));
          ccboostconfig.test.push_back(testData);
      }
  }

  qDebug("%s", "cc boost segmentation");

  emit progress(30);
  if (!canExecute()) return;

  //CCBOOST here
    //TODO add const-correctness
  std::vector<itkVolumeType::Pointer> outSegList;
  ccboostconfig.saveIntermediateVolumes = true;
  runCore(ccboostconfig, outSegList);

  if(!canExecute())
      return;

  //CCBOOST finished

  if(ccboostconfig.saveIntermediateVolumes) {

      std::cout << "Core finished" << std::endl;

      //repaste
      splitter.pasteCroppedRegions(outputSegmentations, outputSegmentation);

      typedef CcboostAdapter::FloatTypeImage FloatTypeImage;
      typedef itk::ImageSplitter< FloatTypeImage > FloatSplitterType;

      FloatTypeImage::RegionType fregion(region);

      FloatSplitterType fsplitter(fregion,
                                  cfgdata.numPredictRegions,
                                  FloatTypeImage::OffsetType{30,30.0});

      fsplitter.pasteCroppedRegions(probabilisticOutSegs, probOutputSegmentation);

      typedef itk::ImageFileWriter< CcboostAdapter::FloatTypeImage > fWriterType;
      fWriterType::Pointer fwriter = fWriterType::New();
      fwriter->SetFileName(arguments.outfile);
      fwriter->SetInput(probOutputSegmentation);

      typedef itk::ImageFileWriter< itkVolumeType > WriterType;
      WriterType::Pointer writer = WriterType::New();
      writer->SetFileName(std::string("binary") + arguments.outfile);
      writer->SetInput(outputSegmentation);

      try {

          fwriter->Update();
          writer->Update();

      } catch(itk::ExceptionObject &exp){
          std::cout << "Warning: Error writing output. what(): " << exp << std::endl;
      } catch(...) {
          std::cout << "Warning: Error writing output" << std::endl;
      }
  }

  qDebug() << outSegList.size();

  predictedSegmentationsList = outSegList;

  qDebug() << "Ccboost Segment Time: " << timer.elapsed();
  if (canExecute())
  {
    emit progress(100);
    qDebug() << "Ccboost Segmentation Finished";
  }
}

bool CcboostTask::enoughMemory(const itkVolumeType::Pointer channelItk, const itkVolumeType::RegionType annotatedRegion, unsigned int & numPredictRegions){
    itkVolumeType::SizeType channelItkSize = channelItk->GetLargestPossibleRegion().GetSize();
    double channelSize = channelItkSize[0] * channelItkSize[1] * channelItkSize[2] /1024/1024;

    itkVolumeType::SizeType gtItkSize = annotatedRegion.GetSize();
    double gtSize = gtItkSize[0] * gtItkSize[1] * gtItkSize[2] /1024/1024;

    unsigned int memoryNeededMB = FREEMEMORYREQUIREDPROPORTIONTRAIN*gtSize;

    qDebug() << QString("The estimated amount of memory required has an upper bound of %1 Mb (%2 * %3 (ground truth size))"
                        "and you have %4 Mb").arg(memoryNeededMB).arg(FREEMEMORYREQUIREDPROPORTIONTRAIN).arg(gtSize).arg(memoryAvailableMB);

    unsigned int memoryNeededMBpredict = FREEMEMORYREQUIREDPROPORTIONPREDICT*channelSize;
    numPredictRegions = memoryNeededMBpredict/memoryAvailableMB + 1;
    unsigned int pieceSize = channelSize/numPredictRegions;

    qDebug() << QString("The volume will be splitted in %1 / %2 = %3 regions of size %4 MB on prediction").arg(memoryNeededMBpredict).arg(memoryAvailableMB).arg(numPredictRegions).arg(pieceSize);

    if(memoryAvailableMB < memoryNeededMB){
//       if(QMessageBox::critical(NULL, "Synapse Segmentation Memory check",
//                         QString("The estimated amount of memory required has an upper bound of %1 Mb"
//                                 "and you have %2 Mb. We will process by pieces and ESPina might crash."
//                                 "(You should reduce it by constraining the Ground truth to a smaller 3d Cube)."
//                                 "Continue anyway?").arg(memoryNeededMB).arg(memoryAvailableMB), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
//           return false;

        QString msg = QString("The estimated amount of memory required has an upper bound of %1 Mb"
                            "and you have %2 Mb. We will process by pieces and ESPina might crash."
                            "(You should reduce it by constraining the Ground truth to a smaller 3d Cube)."
                            "Continue anyway?").arg(memoryNeededMB).arg(memoryAvailableMB);
        qDebug() << msg;
        emit questionContinue(msg);
    }

    return true;
}

//FIXME Pass necessary parameters more cleanly
bool CcboostTask::enoughGroundTruth(){
    int numBgPx = 0;

    for(auto bgSeg: m_backgroundGroundTruthSegList){

      itkVolumeType::Pointer bgSegItk = volumetricData(bgSeg->output())->itkImage();

      itk::ImageRegionConstIterator<itkVolumeType> imageIterator(bgSegItk, bgSegItk->GetLargestPossibleRegion());
      while(!imageIterator.IsAtEnd() && numBgPx < MINNUMBGPX) {
        if(imageIterator.Get() != 0)
            numBgPx++;
        ++imageIterator;
      }
      if(numBgPx >= MINNUMBGPX)
         break;
    }

    int mintruth;
    if(CcboostTask::ELEMENT == CcboostTask::MITOCHONDRIA){
        mintruth = MINTRUTHMITOCHONDRIA;
    }else{
        mintruth = MINTRUTHSYNAPSES;
    }

    if(m_groundTruthSegList.size() == 0){

        emit message(QString("Insufficient positive examples provided. Found none."
                          " Make sure the %1 category exists in the model taxonomy"
                          " or that you are using the tags 'positive' and 'negative'."
                          " Aborting.").arg(CcboostTask::ELEMENT));
        return false;
    }else if(numBgPx == 0){

        emit message(QString("Insufficient background examples provided. Found none."
                          " Make sure the Background category exists in the model taxonomy."
                          " or that you are using the tags 'yes%1' and 'no%1'."
                          " Aborting.").arg(CcboostTask::ELEMENT));
        return false;
    }else if(m_groundTruthSegList.size() < mintruth || numBgPx < MINNUMBGPX){

        //FIXME we can no longer create dialogs, so we have to adhere to the async calls
        emit questionContinue(QString("Not enough annotations were provided "
                          "(%1 positive elements %2 background pixels while %3 and %4 are recommended)."
                          " Increase the background or positive annotation provided."
                          " Continue anyway?").arg(m_groundTruthSegList.size()).arg(numBgPx).arg(mintruth).arg(MINNUMBGPX));
    }

    qDebug() << QString("Annotation provided: "
                      "%1 elements > %2 background pixels (%3 and %4 are required). "
                      ).arg(m_groundTruthSegList.size()).arg(numBgPx).arg(mintruth).arg(MINNUMBGPX);
    return true;
}

itkVolumeType::Pointer CcboostTask::mergeSegmentations(const itkVolumeType::Pointer channelItk,
                                                       const SegmentationAdapterSList& segList,
                                                       const SegmentationAdapterSList& backgroundSegList){

    itkVolumeType::IndexType start;
    start[0] = start[1] = start[2] = 0;

    itkVolumeType::Pointer segmentedGroundTruth = itkVolumeType::New();
    itkVolumeType::RegionType regionVolume;
    regionVolume.SetSize(channelItk->GetLargestPossibleRegion().GetSize());
    regionVolume.SetIndex(start);

    segmentedGroundTruth->SetSpacing(channelItk->GetSpacing());
    segmentedGroundTruth->SetRegions(regionVolume);
    segmentedGroundTruth->Allocate();
    segmentedGroundTruth->FillBuffer(0);

    /*saving the segmentations*/
    for(auto seg: segList){

      DefaultVolumetricDataSPtr volume = volumetricData(seg->output());
      itkVolumeType::Pointer segVolume = volume->itkImage();

      typedef itk::PasteImageFilter <itkVolumeType > PasteImageFilterType;

      itkVolumeType::IndexType destinationIndex = segVolume->GetLargestPossibleRegion().GetIndex();

      //FIXME instead of pasting the image, pad it.
      PasteImageFilterType::Pointer pasteFilter  = PasteImageFilterType::New ();
      pasteFilter->SetDestinationImage(segmentedGroundTruth);
      pasteFilter->SetSourceImage(segVolume);
      pasteFilter->SetSourceRegion(segVolume->GetLargestPossibleRegion());
      pasteFilter->SetDestinationIndex(destinationIndex);
      pasteFilter->Update();

      typedef itk::OrImageFilter< itkVolumeType >  ProcessImageFilterType;

      ProcessImageFilterType::Pointer processFilter = ProcessImageFilterType::New ();
      processFilter->SetInput(1,pasteFilter->GetOutput());
      processFilter->SetInput(0,segmentedGroundTruth);
      processFilter->Update();

      segmentedGroundTruth = processFilter->GetOutput();

      //TODO instead of providing a binary image, use the segmentations map.
   }

    typedef itk::MultiplyImageFilter<itkVolumeType, itkVolumeType, itkVolumeType> MultiplyImageFilterType;
    MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();

    for(auto seg: backgroundSegList){

        DefaultVolumetricDataSPtr volume = volumetricData(seg->output());
        itkVolumeType::Pointer segVolume = volume->itkImage();

         typedef itk::PasteImageFilter <itkVolumeType > PasteImageFilterType;

         itkVolumeType::IndexType destinationIndex = segVolume->GetLargestPossibleRegion().GetIndex();

         multiplyImageFilter->SetInput(segVolume);
         //multiplyImageFilter->SetConstant(CCBOOSTBACKGROUNDLABEL);
         multiplyImageFilter->SetConstant(128);
         multiplyImageFilter->UpdateLargestPossibleRegion();

         //FIXME instead of pasting the image, pad it.
         PasteImageFilterType::Pointer pasteFilter = PasteImageFilterType::New ();
         pasteFilter->SetDestinationImage(segmentedGroundTruth);
         pasteFilter->SetSourceImage(multiplyImageFilter->GetOutput());
         pasteFilter->SetSourceRegion(multiplyImageFilter->GetOutput()->GetLargestPossibleRegion());
         pasteFilter->SetDestinationIndex(destinationIndex);
         pasteFilter->Update();

         typedef itk::OrImageFilter< itkVolumeType >  ProcessImageFilterType;

         ProcessImageFilterType::Pointer processFilter  = ProcessImageFilterType::New ();
         processFilter->SetInput(1,pasteFilter->GetOutput());
         processFilter->SetInput(0,segmentedGroundTruth);
         processFilter->Update();

         segmentedGroundTruth = processFilter->GetOutput();

         //TODO instead of providing a binary image, use the segmentations map.
     }

    return segmentedGroundTruth;

}


void CcboostTask::applyEspinaSettings(ConfigData<itkVolumeType> cfgdata){

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");

    //FIXME the stored hash in settings is not used, instead the cacheDir path contains the hash,
    //so when it changes the directory changes and the features are not found and therefore recomputed (correctly)
    //When using ROIs this has to be changed and use a vector of stored hashes.
    if (settings.contains("Channel Hash")){
        for(auto setcfg: cfgdata.train)
            setcfg.featuresRawVolumeImageHash = settings.value("Channel Hash").toString().toStdString();
    }
    if (settings.contains("Number of Stumps"))
        cfgdata.numStumps = settings.value("Number of Stumps").toInt();

    if (settings.contains("Super Voxel Seed"))
        cfgdata.svoxSeed = settings.value("Super Voxel Seed").toInt();

    if (settings.contains("Super Voxel Cubeness"))
        cfgdata.svoxCubeness = settings.value("Super Voxel Cubeness").toInt();

    if (settings.contains("Features Directory")) {
            cfgdata.cacheDir = settings.value("Features Directory").toString().toStdString();
        //FIXME config data setdefault is overwritten and ignored
    }

    if (settings.contains("TP Quantile"))
        cfgdata.TPQuantile = settings.value("TP Quantile").toFloat();

    if (settings.contains("FP Quantile"))
        cfgdata.FPQuantile = settings.value("FP Quantile").toFloat();

    if (settings.contains("Force Recompute Features"))
        cfgdata.forceRecomputeFeatures = settings.value("Force Recompute Features").toBool();

    if (settings.contains("Save Intermediate Volumes"))
        cfgdata.saveIntermediateVolumes = settings.value("Save Intermediate Volumes").toBool();

    if (settings.contains("Minimum synapse size (voxels)"))
        cfgdata.minComponentSize = settings.value("Minimum synapse size (voxels)").toInt();

    if (settings.contains("Number of objects limit"))
        cfgdata.maxNumObjects = settings.value("Number of objects limit").toInt();

    if (settings.contains("Automatic Computation"))
        cfgdata.automaticComputation = settings.value("Automatic Computation").toBool();

}


void CcboostTask::runCore(const ConfigData<itkVolumeType>& ccboostconfig,
             std::vector<itkVolumeType::Pointer>& outputSplittedSegList){

    std::vector<itkVolumeType::Pointer> outputSegmentations;
    CcboostAdapter::FloatTypeImage::Pointer probabilisticOutputSegmentation = CcboostAdapter::FloatTypeImage::New();
    std::vector<CcboostAdapter::FloatTypeImage::Pointer> probabilisticOutputSegmentations;

    // modify ITK number of  s for speed up
    const int prevITKNumberOfThreads = itk::MultiThreader::GetGlobalDefaultNumberOfThreads();
    if (prevITKNumberOfThreads <= 1)
        itk::MultiThreader::SetGlobalDefaultNumberOfThreads(8);

    try {

        // MultipleROIData allROIs = preprocess(channels, groundTruths, cacheDir, featuresList);
        if(!CcboostAdapter::core(ccboostconfig, probabilisticOutputSegmentations, outputSplittedSegList, outputSegmentations)) {
            itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );
            return;
        }

    } catch( itk::ExceptionObject & err ) {
        // restore default number of threads
        itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );

        qDebug() << QObject::tr("Itk exception on ccboost caught. Error: %1.").arg(err.what());

        emit message(QString("Itk exception when running ccboost. Message: %1").arg(err.what()));
        return;
    }

    // restore default number of threads
    itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );

    try {
        if(ccboostconfig.saveIntermediateVolumes){
            typedef itk::ImageFileWriter<CcboostAdapter::FloatTypeImage> FloatWriterType;
            FloatWriterType::Pointer fwriter = FloatWriterType::New();
            fwriter->SetFileName(ccboostconfig.cacheDir + "ccboost-probabilistic-output.tif");
            fwriter->SetInput(probabilisticOutputSegmentation);
            fwriter->Update();
        }

    } catch( itk::ExceptionObject & err ) {
        // restore default number of threads
        itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );

        qDebug() << QObject::tr("Itk exception on ccboost caught. Error: %1. From %2:%3").arg(err.what()).arg(__FILE__).arg(__LINE__);

        emit message(QString("Itk exception when running ccboost. Message: %1").arg(err.what()));
        return;
    }


}

