/*
 * CcboostSegmentationFilter.cpp
 *
 *  Created on: Jan 18, 2013
 *      Author: Felix de las Pozas Alvarez
 */

// Plugin
#include "CcboostSegmentationFilter.h"

#include <itkTestingHashImageFilter.h>

#include "ImageSplitter.h"

#include "CcboostAdapter.h"

// ESPINA
#include <Core/Analysis/Segmentation.h>
#include <Core/Analysis/Data/MeshData.h>
#include <Core/Analysis/Data/Mesh/RawMesh.h>
#include <Core/Analysis/Data/Volumetric/RasterizedVolume.hxx>
#include <GUI/Representations/SliceRepresentation.h>
#include <GUI/Representations/MeshRepresentation.h>

// Qt
#include <QtGlobal>
#include <QMessageBox>
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

#include <itkConstantPadImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>

// VTK
#include <vtkMath.h>
#include <vtkTransform.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkImplicitVolume.h>
#include <vtkClipPolyData.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkMeshQuality.h>
#include <vtkMutableUndirectedGraph.h>
#include <vtkIdList.h>
#include <vtkBoostConnectedComponents.h>
#include <vtkIdTypeArray.h>
#include <vtkEdgeListIterator.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkPlane.h>
#include <vtkDoubleArray.h>

#include <sys/sysinfo.h>

const double UNDEFINED = -1.;

using namespace ESPINA;
using namespace std;

const QString SAS = "SAS";

const char * CcboostSegmentationFilter::MESH_NORMAL = "Normal";
const char * CcboostSegmentationFilter::MESH_ORIGIN = "Origin";

const QString CcboostSegmentationFilter::MITOCHONDRIA = "mitochondria";
const QString CcboostSegmentationFilter::SYNAPSE = "synapse";
QString CcboostSegmentationFilter::ELEMENT = "element";
const QString CcboostSegmentationFilter::POSITIVETAG = "yes"; //Tags are always lowercase
const QString CcboostSegmentationFilter::NEGATIVETAG = "no";

//----------------------------------------------------------------------------
CcboostSegmentationFilter::CcboostSegmentationFilter(InputSList inputs, Type type, SchedulerSPtr scheduler)
: Filter(inputs, type, scheduler)
, m_resolution        {50}
, m_iterations        {10}
, m_converge          {true}
, m_ap                {nullptr}
, m_alreadyFetchedData{false}
, m_lastModifiedMesh  {0}
{
  setDescription(tr("Compute SAS"));

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
  }
#else
  memoryAvailableMB = numeric_limits<unsigned int>::max();
  QMessageBox::warning(NULL,"RAM Memory check", "The amount of RAM memory couldn't be detected."
                       "Running the Synapse Segmentation Plugin might lead to crash");
#endif
}

//----------------------------------------------------------------------------
CcboostSegmentationFilter::~CcboostSegmentationFilter()
{
  if (m_ap != nullptr)
  {
    disconnect(m_inputs[0]->output().get(), SIGNAL(modified()),
               this, SLOT(inputModified()));
  }
}

//----------------------------------------------------------------------------
bool CcboostSegmentationFilter::needUpdate() const
{
  return needUpdate(0);
}

//----------------------------------------------------------------------------
bool CcboostSegmentationFilter::needUpdate(Output::Id oId) const
{
 // Q_ASSERT(oId == 0);

  bool update = true;

  if (!m_inputs.isEmpty() && m_alreadyFetchedData)
  {
    Q_ASSERT(m_inputs.size() == 1);

    auto inputVolume = volumetricData(m_inputs[0]->output());
    if(inputVolume != nullptr)
    {
      update = (m_lastModifiedMesh < inputVolume->lastModified());
    }
    else
    {
      qWarning() << "SAS input volume es NULL";
    }
  }

  return update;
}

//----------------------------------------------------------------------------
void CcboostSegmentationFilter::execute(Output::Id oId)
{
  Q_ASSERT(oId == 0);
  execute();
}

//----------------------------------------------------------------------------
void CcboostSegmentationFilter::execute()
{
  emit progress(0);
  if (!canExecute()) return;

  //TODO do I have to put back the original locale?
  std::setlocale(LC_NUMERIC, "C");

  m_inputChannel = volumetricData(m_inputs[0]->output())->itkImage();
  itkVolumeType::Pointer channelItk = m_inputChannel;

  m_inputSegmentation = volumetricData(m_inputs[1]->output())->itkImage();

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
 //TODO espina2
     if(ccboostconfig.saveIntermediateVolumes){
       writer->SetFileName(ccboostconfig.cacheDir + "labelmap.tif");
       writer->SetFileName("labelmap.tif");
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
  itkVolumeType::OffsetValueType offset(CcboostSegmentationFilter::ANNOTATEDPADDING);
  annotatedRegion.PadByRadius(offset);

  unsigned int numPredictRegions;
  if(!enoughMemory(channelItk, annotatedRegion, numPredictRegions))
      return;

  //Create config object and retrieve settings from espina
  ConfigData<itkVolumeType>::setDefault(ccboostconfig);

  ccboostconfig.numPredictRegions = numPredictRegions;

  applyEspinaSettings(ccboostconfig);

  if(CcboostSegmentationFilter::ELEMENT == CcboostSegmentationFilter::MITOCHONDRIA)
      ccboostconfig.preset = ConfigData<itkVolumeType>::MITOCHONDRIA;
  else if(CcboostSegmentationFilter::ELEMENT == CcboostSegmentationFilter::SYNAPSE)
      ccboostconfig.preset = ConfigData<itkVolumeType>::SYNAPSE;
  else {
      qWarning() << "Error! Preset is not set. Using default: " << CcboostSegmentationFilter::SYNAPSE;
      ccboostconfig.preset = ConfigData<itkVolumeType>::SYNAPSE;
  }

  ccboostconfig.originalVolumeImage = normalizedChannelItk;

  //FIXME computing the hash on cfgData.train.rawVolumeImage provokes a segFault
  //TODO each volume (and feature) should has its hash, but we are just using the first throught cachedir
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

  qDebug("%s", ccboostconfig.cacheDir.c_str());
  ccboostconfig.rawVolume = "supervoxelcache-";
  ccboostconfig.cacheDir = ccboostconfig.cacheDir + ccboostconfig.rawVolume + hash
          + QString::number(spacing[2]).toStdString() + "/";
  //FIXME use channel filename instead of supervoxelcache
  qDebug("%s", ccboostconfig.cacheDir.c_str());

  SetConfigData<itkVolumeType> trainData;
  SetConfigData<itkVolumeType>::setDefaultSet(trainData);
  trainData.rawVolumeImage = ccboostconfig.originalVolumeImage;
  trainData.groundTruthImage = segmentedGroundTruth;
  trainData.zAnisotropyFactor = 2*channelItk->GetSpacing()[2]/(channelItk->GetSpacing()[0]
          + channelItk->GetSpacing()[1]);


  annotatedRegion.Crop(channelItk->GetLargestPossibleRegion());
  trainData.annotatedRegion = annotatedRegion;

  ccboostconfig.train.push_back(trainData);

  // Test
  //FIXME test should get the whole volume or pieces, it is not a copy of train.
  SetConfigData<itkVolumeType> testData = trainData;
  testData.rawVolumeImage = ccboostconfig.originalVolumeImage;
  testData.zAnisotropyFactor = trainData.zAnisotropyFactor;
  ccboostconfig.test.push_back(testData);

  QDir dir(QString::fromStdString(ccboostconfig.cacheDir));
  dir.mkpath(QString::fromStdString(ccboostconfig.cacheDir));

  qDebug("%s", "cc boost segmentation");

  //CCBOOST here
    //TODO add const-correctness
  std::vector<itkVolumeType::Pointer> outSegList;
  ccboostconfig.saveIntermediateVolumes = true;
  runCore(ccboostconfig, outSegList);

  if(!canExecute())
      return;

  //CCBOOST finished

  qDebug() << outSegList.size();

  //TODO add as many outputs as segmentations
  for(int i = 0; i < outSegList.size() && i < 250; i++){
      if (!m_outputs.contains(i))
      {
          m_outputs[i] = OutputSPtr(new Output(this, i));
      }

      itkVolumeType::Pointer output = outSegList.at(i);

      Bounds bounds = minimalBounds<itkVolumeType>(output, SEG_BG_VALUE);

      //m_input[0] is the channel
      NmVector3 newspacing = m_inputs[0]->output()->spacing();

      DefaultVolumetricDataSPtr volume{new SparseVolume<itkVolumeType>(bounds, newspacing)};
      volume->draw(output, bounds);

      MeshDataSPtr mesh{new MarchingCubesMesh<itkVolumeType>(volume)};

      m_outputs[i]->setData(volume);
      m_outputs[i]->setData(mesh);

      m_outputs[i]->setSpacing(newspacing);
  }
  emit progress(100);
  if (!canExecute()) return;
}

bool CcboostSegmentationFilter::enoughMemory(const itkVolumeType::Pointer channelItk, const itkVolumeType::RegionType annotatedRegion, unsigned int & numPredictRegions){
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
        //FIXME we're not on the main thread, creating a dialog will provoke a crash
//       if(QMessageBox::critical(NULL, "Synapse Segmentation Memory check",
//                         QString("The estimated amount of memory required has an upper bound of %1 Mb"
//                                 "and you have %2 Mb. We will process by pieces and ESPina might crash."
//                                 "(You should reduce it by constraining the Ground truth to a smaller 3d Cube)."
//                                 "Continue anyway?").arg(memoryNeededMB).arg(memoryAvailableMB), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
//           return false;
    }
    return true;
}

//FIXME Pass necessary parameters more cleanly
bool CcboostSegmentationFilter::enoughGroundTruth(){
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
    if(CcboostSegmentationFilter::ELEMENT == CcboostSegmentationFilter::MITOCHONDRIA){
        mintruth = MINTRUTHMITOCHONDRIA;
    }else{
        mintruth = MINTRUTHSYNAPSES;
    }

    //FIXME filter is no longer in the main thread, so it will fail everytime it tries to popup a dialog
//    if(m_groundTruthSegList.size() == 0){

//                QMessageBox::critical(NULL, CcboostSegmentationFilter::ELEMENT + " Segmentation Annotation Check",
//                                      QString("Insufficient positive examples provided. Found none."
//                                              " Make sure the %1 category exists in the model taxonomy"
//                                              " or that you are using the tags 'positive' and 'negative'."
//                                              " Aborting.").arg(CcboostSegmentationFilter::ELEMENT));
//        return false;
//    }else if(numBgPx == 0){

//                QMessageBox::critical(NULL, CcboostSegmentationFilter::ELEMENT +  " Segmentation Annotation Check",
//                                      QString("Insufficient background examples provided. Found none."
//                                              " Make sure the Background category exists in the model taxonomy."
//                                              " or that you are using the tags 'yes%1' and 'no%1'."
//                                              " Aborting.").arg(CcboostSegmentationFilter::ELEMENT));
//        return false;
//    }else if(m_groundTruthSegList.size() < mintruth || numBgPx < MINNUMBGPX){

//        QMessageBox::StandardButton reply
//                = QMessageBox::question(NULL, CcboostSegmentationFilter::ELEMENT +  " Segmentation Annotation Check",
//                                      QString("Not enough annotations were provided "
//                                              "(%1 positive elements %2 background pixels while %3 and %4 are recommended)."
//                                              " Increase the background or positive annotation provided."
//                                              " Continue anyway?").arg(m_groundTruthSegList.size()).arg(numBgPx).arg(mintruth).arg(MINNUMBGPX),
//                                              QMessageBox::Yes|QMessageBox::No);
//        if (reply == QMessageBox::No)
//            return false;
//    }

    qDebug() << QString("Annotation provided: "
                      "%1 elements > %2 background pixels (%3 and %4 are required). "
                      ).arg(m_groundTruthSegList.size()).arg(numBgPx).arg(mintruth).arg(MINNUMBGPX);
    return true;
}

itkVolumeType::Pointer CcboostSegmentationFilter::mergeSegmentations(const itkVolumeType::Pointer channelItk,
                                                                     const SegmentationAdapterList& segList,
                                                                     const SegmentationAdapterList& backgroundSegList){

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


void CcboostSegmentationFilter::applyEspinaSettings(ConfigData<itkVolumeType> cfgdata){

    ESPINA_SETTINGS(settings);
    settings.beginGroup("Synapse Segmentation");

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

    if (settings.contains("Synapse Features Directory")) {
            cfgdata.cacheDir = settings.value("Synapse Features Directory").toString().toStdString();
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

}


void CcboostSegmentationFilter::runCore(const ConfigData<itkVolumeType>& ccboostconfig,
             std::vector<itkVolumeType::Pointer>& outSegList){

    CcboostAdapter::FloatTypeImage::Pointer probabilisticOutputSegmentation;

    // modify ITK number of  s for speed up
    const int prevITKNumberOfThreads = itk::MultiThreader::GetGlobalDefaultNumberOfThreads();
    if (prevITKNumberOfThreads <= 1)
        itk::MultiThreader::SetGlobalDefaultNumberOfThreads(8);

    try {

        // MultipleROIData allROIs = preprocess(channels, groundTruths, cacheDir, featuresList);
        if(!CcboostAdapter::core(ccboostconfig, probabilisticOutputSegmentation, outSegList)) {
            itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );
            return;
        }

    } catch( itk::ExceptionObject & err ) {
        // restore default number of threads
        itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );

        qDebug() << QObject::tr("Itk exception on ccboost caught. Error: %1.").arg(err.what());
        //FIXME we're not on main thread, dialog will crash
//        QMessageBox::warning(NULL, "ESPINA", QString("Itk exception when running ccboost. Message: %1").arg(err.what()));
        return;
    }

    // restore default number of threads
    itk::MultiThreader::SetGlobalDefaultNumberOfThreads( prevITKNumberOfThreads );

    if(ccboostconfig.saveIntermediateVolumes){
        typedef itk::ImageFileWriter<CcboostAdapter::FloatTypeImage> FloatWriterType;
        FloatWriterType::Pointer fwriter = FloatWriterType::New();
        fwriter->SetFileName(ccboostconfig.cacheDir + "ccboost-probabilistic-output.tif");
        fwriter->SetInput(probabilisticOutputSegmentation);
        fwriter->Update();
    }

}




//----------------------------------------------------------------------------
void CcboostSegmentationFilter::inputModified()
{
  run();
}
