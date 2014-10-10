/*
 * CcboostSegmentationFilter.cpp
 *
 *  Created on: Jan 18, 2013
 *      Author: Felix de las Pozas Alvarez
 */

// Plugin
#include "CcboostSegmentationFilter.h"

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

  itkVolumeType::Pointer segmentedGroundTruth = mergeSegmentations(normalizedChannelItk, m_groundTruthSegList, m_groundTruthSegList);
  //save itk image (volume) as binary/classed volume here
  WriterType::Pointer writer = WriterType::New();
//TODO espina2
//    if(ccboostconfig.saveIntermediateVolumes){
//      writer->SetFileName(ccboostconfig.train.cacheDir + "labelmap.tif");
    writer->SetFileName("labelmap2.tif");
    writer->SetInput(segmentedGroundTruth);
    writer->Update();

    //CCBOOST here
    //TODO add const-correctness
   std::vector<itkVolumeType::Pointer> segmentedGroundTruthVector;
   segmentedGroundTruthVector.push_back(segmentedGroundTruth);
   itkVolumeType::Pointer coreOutputSegmentation = core(normalizedChannelItk, segmentedGroundTruthVector);
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
    segmentedGroundTruth->FillBuffer(100);

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
        multiplyImageFilter->SetConstant(CCBOOSTBACKGROUNDLABEL);
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

itkVolumeType::Pointer CcboostSegmentationFilter::core(const itkVolumeType::Pointer normalizedChannelItk,
                                                       std::vector<itkVolumeType::Pointer> segmentedGroundTruthVector){



    itkVolumeType::Pointer coreOutputSegmentation = segmentedGroundTruthVector.at(0);
    return coreOutputSegmentation;

}


//----------------------------------------------------------------------------
void CcboostSegmentationFilter::inputModified()
{
  run();
}
