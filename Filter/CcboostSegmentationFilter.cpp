/*
 * CcboostSegmentationFilter.cpp
 *
 *  Created on: Jan 18, 2013
 *      Author: Felix de las Pozas Alvarez
 */

// Plugin
#include "CcboostSegmentationFilter.h"

//ccboost
#include "ROIData.h"
#include "BoosterInputData.h"
#include "ContextFeatures/ContextRelativePoses.h"

#include "Booster.h"
#include "utils/TimerRT.h"

#include "EigenOfStructureTensorImageFilter2.h"
#include "GradientMagnitudeImageFilter2.h"
#include "SingleEigenVectorOfHessian2.h"
#include "RepolarizeYVersorWithGradient2.h"
#include "AllEigenVectorsOfHessian2.h"
#include <itkTestingHashImageFilter.h>

#include "SplitterImageFilter.h"

// EspINA
#include <Core/Analysis/Segmentation.h>
#include <Core/Analysis/Data/MeshData.h>
#include <Core/Analysis/Data/Mesh/RawMesh.h>
#include <Core/Analysis/Data/Volumetric/RasterizedVolume.h>
#include <GUI/Representations/SliceRepresentation.h>
#include <GUI/Representations/MeshRepresentation.h>

// Qt
#include <QtGlobal>
#include <QMessageBox>

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

const double UNDEFINED = -1.;

using namespace EspINA;
using namespace std;

const QString SAS = "SAS";

const char * CcboostSegmentationFilter::MESH_NORMAL = "Normal";
const char * CcboostSegmentationFilter::MESH_ORIGIN = "Origin";

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

  itkVolumeType::Pointer segmentedGroundTruth = mergeSegmentations(normalizedChannelItk, m_groundTruthSegList, m_backgroundGroundTruthSegList);

  WriterType::Pointer writer = WriterType::New();

    //CCBOOST here
    //TODO add const-correctness

    std::vector<itkVolumeType::Pointer> channels;
    channels.push_back(normalizedChannelItk);
   std::vector<itkVolumeType::Pointer> groundTruths;
   groundTruths.push_back(segmentedGroundTruth);
   itkVolumeType::Pointer coreOutputSegmentation;
   try {
       std::string cacheDir("./");
       std::vector<std::string> featuresList{"hessOrient-s3.5-repolarized.nrrd",
                                             "gradient-magnitude-s1.0.nrrd",
                                             "gradient-magnitude-s1.6.nrrd",
                                             "gradient-magnitude-s3.5.nrrd",
                                             "gradient-magnitude-s5.0.nrrd",
                                             "stensor-s0.5-r1.0.nrrd",
                                             "stensor-s0.8-r1.6.nrrd",
                                             "stensor-s1.8-r3.5.nrrd",
                                             "stensor-s2.5-r5.0.nrrd"
                                            };

       MultipleROIData allROIs = preprocess(channels, groundTruths, cacheDir, featuresList);
        coreOutputSegmentation = core(allROIs);
   } catch( itk::ExceptionObject & err ) {
       std::cerr << "ExceptionObject caught !" << std::endl;
       std::cerr << err << std::endl;
       return; // Since the goal of the example is to catch the exception, we declare this a success.
   }
    //CCBOOST finished

  emit progress(50);
  if (!canExecute()) return;

  typedef itk::BinaryThresholdImageFilter <itkVolumeType, itkVolumeType>
         BinaryThresholdImageFilterType;

  BinaryThresholdImageFilterType::Pointer thresholdFilter
    = BinaryThresholdImageFilterType::New();
  thresholdFilter->SetInput(coreOutputSegmentation);
  thresholdFilter->SetLowerThreshold(128);
  thresholdFilter->SetUpperThreshold(255);
  thresholdFilter->SetInsideValue(255);
  thresholdFilter->SetOutsideValue(0);
  thresholdFilter->Update();

  if(true) {
      writer->SetFileName( "thresholded-segmentation.tif");
      writer->SetInput(thresholdFilter->GetOutput());
      writer->Update();
  }

  emit progress(70);
  if (!canExecute()) return;

  std::vector<itkVolumeType::Pointer> outSegList;
  splitSegmentations(thresholdFilter->GetOutput(), outSegList);

  qDebug() << outSegList.size();

  //TODO add as many outputs as segmentations
  for(int i = 0; i < outSegList.size(); i++){
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

itkVolumeType::Pointer CcboostSegmentationFilter::mergeSegmentations(const itkVolumeType::Pointer channelItk,
                                                                    const SegmentationAdapterList segList,
                                                                     const SegmentationAdapterList backgroundSegList){

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

    //save itk image (volume) as binary/classed volume here
    WriterType::Pointer writer = WriterType::New();
//TODO espina2
//    if(ccboostconfig.saveIntermediateVolumes){
//      writer->SetFileName(ccboostconfig.train.cacheDir + "labelmap.tif");
      writer->SetFileName("labelmap.tif");
      writer->SetInput(segmentedGroundTruth);
      writer->Update();
      qDebug() << "labelmap.tif created";
//    }

    return segmentedGroundTruth;

}

void CcboostSegmentationFilter::splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
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
      qDebug() << "labelmap.tif created";

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

void CcboostSegmentationFilter::computeFeatures(const itkVolumeType::Pointer volume,
                                                const std::string cacheDir,
                                                std::vector<std::string> featuresList,
                                                float zAnisotropyFactor,
                                                bool forceRecomputeFeatures){


    //TODO can we give message with?
    //setDescription();
    emit progress(10);
    if(!canExecute())
        return;

    stringstream strstream;

    int featureNum = 0;
    for( std::string featureFile: featuresList ) {

       strstream.str(std::string());
       featureNum++;
       strstream << "Computing " << featureNum << "/" << featuresList.size()+1 << " features";
       std::cout << strstream.str() << std::endl;
       //stateDescription->setTime(strstream.str());

       emit progress(10 + featureNum*50/(featuresList.size()+1));
       if(!canExecute())
           return;

       std::string featureFilepath(cacheDir + featureFile);
       ifstream featureStream(featureFilepath.c_str());
       //TODO check hash to prevent recomputing
       if(!featureStream.good() || forceRecomputeFeatures){
           //TODO do this in a more flexible way
           int stringPos;
           if( (stringPos = featureFile.find(string("hessOrient-"))) != std::string::npos){
               float sigma;
               string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"hessOrient-s%f-repolarized.nrrd",&sigma);
              //FIXME break if error
               if(result < 0)
                   qDebug() << "Error scanning feature name " << featureFile.c_str();

               qDebug() << QString("Either you requested recomputing the features, "
                                   "the current channel is new or has changed or "
                                   "Hessian Orient Estimate feature not found at "
                                   "path: %2. Creating with sigma %1").arg(sigma).arg(QString::fromStdString(featureFilepath));

                                                                                                                                                                                            AllEigenVectorsOfHessian2Execute(sigma, volume, featureFilepath, EByMagnitude, zAnisotropyFactor);

               RepolarizeYVersorWithGradient2Execute(sigma, volume, featureFilepath, featureFilepath, zAnisotropyFactor);

          } else if ( (stringPos = featureFile.find(std::string("gradient-magnitude"))) != std::string::npos ) {
               float sigma;
               string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"gradient-magnitude-s%f.nrrd",&sigma);
               if(result < 0)
                   qDebug() << "Error scanning feature name " << featureFile.c_str();

               qDebug() << QString("Current  or channel is new or has changed or gradient magnitude feature not found at path: %2. Creating with sigma %1").arg(sigma).arg(QString::fromStdString(featureFilepath));

               GradientMagnitudeImageFilter2Execute(sigma, volume, featureFilepath, zAnisotropyFactor);

           } else if( (stringPos = featureFile.find(std::string("stensor"))) != std::string::npos ) {
               float rho,sigma;
               string filename(featureFile.substr(stringPos));
               int result = sscanf(filename.c_str(),"stensor-s%f-r%f.nrrd",&sigma,&rho);
               if(result < 0)
                   qDebug() << "Error scanning feature name" << featureFile.c_str();

               qDebug() << QString("Current channel is new or has changed or tensor feature not found at path %3. Creating with rho %1 and sigma %2").arg(rho).arg(sigma).arg(QString::fromStdString(featureFilepath));

               bool sortByMagnitude = true;
               EigenOfStructureTensorImageFilter2Execute(sigma, rho, volume, featureFilepath, sortByMagnitude, zAnisotropyFactor );

           } else {
               qDebug() << QString("feature %1 is not recognized.").arg(featureFile.c_str()).toUtf8();
           }
       }
    }

//    stateDescription->setMessage("Features computed");
//    stateDescription->setTime("");
}

MultipleROIData CcboostSegmentationFilter::preprocess(std::vector<itkVolumeType::Pointer> channels,
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
            int zAnisotropyFactor = 1;
            computeFeatures(channels.at(0), cacheDir, featuresList, zAnisotropyFactor, false);
            for(std::string featureItk: featuresList){
                Matrix3D<ImagePixelType> feature;
                feature.load(cacheDir + featureItk);

                // raw image integral image
                ROIData::IntegralImageType ii;
                ii.compute( feature );
                roi.addII( ii.internalImage().data() );
            }

            allROIs.add( &roi );

    }

//        }
//    }



            return allROIs;
}

itkVolumeType::Pointer CcboostSegmentationFilter::core(MultipleROIData allROIs) {
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

    auto outputSegmentation = predImg.asItkImage();

    //TODO not sure why the types are different
        typedef itk::ImageToImageFilter < Matrix3D<float>::ItkImageType, itkVolumeType > ConvertFilterType;

        ConvertFilterType::Pointer convertFilter = (ConvertFilterType::New()).GetPointer();
        convertFilter->SetInput(outputSegmentation);
        convertFilter->Update();

    itkVolumeType::Pointer coreOutputSegmentation = convertFilter->GetOutput();
    return coreOutputSegmentation;

}


//----------------------------------------------------------------------------
void CcboostSegmentationFilter::inputModified()
{
  run();
}
