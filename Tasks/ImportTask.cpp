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

#include "ImportTask.h"

#include "Filter/CcboostAdapter.h"

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

const QString ImportTask::IMPORTED = "IMPORTED";
const QString ImportTask::SUPPORTED_FILES = tr("Binary Images (*.tiff *.tif *.jpeg)");


//------------------------------------------------------------------------
ImportTask::ImportTask(ChannelAdapterPtr channel,
                       SchedulerSPtr     scheduler)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
{


}

//------------------------------------------------------------------------
void ImportTask::run()
{

  m_inputChannel = volumetricData(m_channel->asInput()->output())->itkImage();
  itkVolumeType::Pointer channelItk = m_inputChannel;

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

  //CCBOOST here
    //TODO add const-correctness
  std::vector<itkVolumeType::Pointer> outSegList;
  runCore(outSegList);

  if(!canExecute())
      return;

  //CCBOOST finished

  qDebug() << outSegList.size();

  predictedSegmentationsList = outSegList;

  if (canExecute())
  {
    emit progress(100);
    qDebug() << "Ccboost Segmentation Finished";
  }
}

void ImportTask::runCore(std::vector<itkVolumeType::Pointer>& outSegList){

    try {

         typedef itk::ImageFileReader<itkVolumeType> ReaderType;
           ReaderType::Pointer reader = ReaderType::New();
           reader->SetFileName(filename);
           reader->Update();

           //either do this:
           CcboostAdapter::splitSegmentations(reader->GetOutput(), outSegList);
           //or all this:
           //splitSegs(outputSegmentation, outSegList);
    } catch( itk::ExceptionObject & err ) {
        qDebug() << tr("Itk exception on plugin caught at %1:%2. Error: %3.").arg(__FILE__).arg(__LINE__).arg(err.what());
        qDebug("Returning focus to ESPina.");
        return;
    }
}

//void ImportTask::splitSegs(const itkVolumeType::Pointer outputSegmentation,
//                           std::vector<itkVolumeType::Pointer>& outSegList){

//    try {

//        //filter background into one segmentation object
//      typedef itk::BinaryThresholdImageFilter <itkVolumeType, itkVolumeType>
//          BinaryThresholdImageFilterType;

//        BinaryThresholdImageFilterType::Pointer backgroundThresholdFilter
//            = BinaryThresholdImageFilterType::New();

//        backgroundThresholdFilter->SetInput(outputSegmentation);
//        backgroundThresholdFilter->SetLowerThreshold(BackgroundImportLabel);
//        backgroundThresholdFilter->SetUpperThreshold(BackgroundImportLabel);
//        backgroundThresholdFilter->SetInsideValue(255);
//        backgroundThresholdFilter->SetOutsideValue(0);
//        backgroundThresholdFilter->Update();

//        RawSegmentationVolumeSPtr volRepresentation(new RawSegmentationVolume(backgroundThresholdFilter->GetOutput()));
//        volRepresentation->fitToContent();

//        SegmentationRepresentationSList representationList;
//        representationList << volRepresentation;
//        representationList << MeshRepresentationSPtr(new MarchingCubesMesh(volRepresentation));

//        addOutputRepresentations(0, representationList);

//        m_outputs[0]->updateModificationTime();

//        if(saveDebugImages) {
//            writer->SetFileName("segmentation_background.tif");
//            writer->SetInput(backgroundThresholdFilter->GetOutput());
//            writer->Update();
//        }

//        BinaryThresholdImageFilterType::Pointer backgroundSuppressionThresholdFilter
//                   = BinaryThresholdImageFilterType::New();

//        //set unknown to background value (0)
//        backgroundSuppressionThresholdFilter->SetInput(reader->GetOutput());
//        backgroundSuppressionThresholdFilter->SetLowerThreshold(SegmentationImportLabel);
//        backgroundSuppressionThresholdFilter->SetUpperThreshold(SegmentationImportLabel);
//        backgroundSuppressionThresholdFilter->SetInsideValue(255);
//        backgroundSuppressionThresholdFilter->SetOutsideValue(0);
//        backgroundSuppressionThresholdFilter->Update();

//        if(saveDebugImages){
//          writer->SetFileName("no-background-segmentation.tif");
//          writer->SetInput(backgroundSuppressionThresholdFilter->GetOutput());
//          writer->Update();
//        }

//        //connected components

//        typedef itk::ConnectedComponentImageFilter <itkVolumeType, itkFloatVolumeType >
//           ConnectedComponentImageFilterType;

//        ConnectedComponentImageFilterType::Pointer connected = ConnectedComponentImageFilterType::New ();
//        connected->SetInput(backgroundSuppressionThresholdFilter->GetOutput());

//         try {

//            connected->Update();

//         } catch( itk::ExceptionObject & err ) {
//             qDebug() << tr("Itk exception on plugin caught at %1:%2. Error: %3.").arg(__FILE__).arg(__LINE__).arg(err.what());
//             qDebug("Returning focus to ESPina.");
//             return;
//         }
//         std::cout << "Number of objects: " << connected->GetObjectCount() << std::endl;

//         qDebug("Connected components segmentation");

//           //tiff does not support int as pixel value and Connected components does not support float as pixel value
////         if(saveDebugImages){
////             fwriter->SetFileName("connected-segmentation.tif");
////             fwriter->SetInput(connected->GetOutput());
////             fwriter->Update();
////         }

//         qDebug("Relabeling Connected components segmentation");
//         typedef itk::RelabelComponentImageFilter<itkFloatVolumeType, itkFloatVolumeType> FilterType;
//         FilterType::Pointer relabelFilter = FilterType::New();
//         relabelFilter->SetInput(connected->GetOutput());
//         relabelFilter->Update();

//         typedef itk::BinaryThresholdImageFilter <itkFloatVolumeType, itkVolumeType>
//                   FloatBinaryThresholdImageFilterType;

//         qDebug("Creating segmentations");

//         FloatBinaryThresholdImageFilterType::Pointer labelThresholdFilter
//                            = FloatBinaryThresholdImageFilterType::New();

//         labelThresholdFilter->SetInsideValue(255);
//         labelThresholdFilter->SetOutsideValue(0);
//         labelThresholdFilter->SetInput(relabelFilter->GetOutput());

//        for(int i=1; i < connected->GetObjectCount() + 1; i++){
//            labelThresholdFilter->SetLowerThreshold(i);
//            labelThresholdFilter->SetUpperThreshold(i);
//            labelThresholdFilter->Update();

//            QString filename = QString("segmentation%1.tif").arg(i);
//            qDebug(filename.toUtf8().constData());

//            if(saveDebugImages) {
//                writer->SetFileName(filename.toUtf8().constData());
//                writer->SetInput(labelThresholdFilter->GetOutput());
//                writer->Update();
//            }

//            RawSegmentationVolumeSPtr volumeRepresentation(new RawSegmentationVolume(labelThresholdFilter->GetOutput()));
//            volumeRepresentation->fitToContent();

//            SegmentationRepresentationSList repList;
//            repList << volumeRepresentation;
//            repList << MeshRepresentationSPtr(new MarchingCubesMesh(volumeRepresentation));

//            addOutputRepresentations(i, repList);

//            m_outputs[i]->updateModificationTime();

//        }
//    } catch( itk::ExceptionObject & err ) {
//        qDebug() << tr("Itk exception on plugin caught at %1:%2. Error: %3.").arg(__FILE__).arg(__LINE__).arg(err.what());
//        qDebug("Returning focus to ESPina.");
//        return;
//    }

//}
