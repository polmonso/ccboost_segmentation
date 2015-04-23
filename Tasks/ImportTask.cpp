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
//#include <GUI/Representations/SliceRepresentation.h>
//#include <GUI/Representations/MeshRepresentation.h>

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
                       SchedulerSPtr     scheduler,
                       std::string file,
                       float threshold)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
, m_filename(file)
, m_loadFromDisk{true}
, m_doThreshold{true}
, m_threshold{threshold}
{

}

//------------------------------------------------------------------------
ImportTask::ImportTask(ChannelAdapterPtr channel,
                       SchedulerSPtr     scheduler,
                       FloatTypeImage::Pointer inputSegmentation,
                       float threshold)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
, m_floatInputSegmentation(inputSegmentation)
, m_loadFromDisk{false}
, m_doThreshold{true}
, m_threshold{threshold}
{

}

//------------------------------------------------------------------------
ImportTask::ImportTask(ChannelAdapterPtr channel,
                       SchedulerSPtr     scheduler,
                       itkVolumeType::Pointer inputSegmentation)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
, m_binarySegmentation(inputSegmentation)
, m_doThreshold{false}
, m_loadFromDisk{false}
, m_threshold{0.0} //irrellevant
{


}

//------------------------------------------------------------------------
void ImportTask::run()
{

  emit progress(20);
  if (!canExecute()) return;


  try {

      //we have to load m_inputSegmentation
      if(m_loadFromDisk) {
          typedef itk::ImageFileReader<itkVolumeType> ReaderType;
          ReaderType::Pointer reader = ReaderType::New();
          reader->SetFileName(m_filename);
          reader->Update();

          m_binarySegmentation = reader->GetOutput();
      }

      if(m_doThreshold) {

          typedef itk::BinaryThresholdImageFilter <Matrix3D<float>::ItkImageType, itkVolumeType>
                  fBinaryThresholdImageFilterType;

          fBinaryThresholdImageFilterType::Pointer thresholdFilter
                  = fBinaryThresholdImageFilterType::New();
          thresholdFilter->SetInput(m_floatInputSegmentation);
          thresholdFilter->SetLowerThreshold(m_threshold);
          thresholdFilter->SetInsideValue(255);
          thresholdFilter->SetOutsideValue(0);
          thresholdFilter->Update();

          m_binarySegmentation = thresholdFilter->GetOutput();

          ESPINA_SETTINGS(settings);
          settings.beginGroup("ccboost segmentation");

          if (settings.contains("Save Intermediate Volumes")) {

              //warning, cache dir will be different than the one
              //used for storing features and other data since that one uses the hash
              std::string cacheDir = "./";
              if (settings.contains("Features Directory"))
                  cacheDir = settings.value("Features Directory").toString().toStdString();

              typedef itk::ImageFileWriter< itkVolumeType > WriterType;
              WriterType::Pointer writer = WriterType::New();
              writer->SetFileName(cacheDir + "binarized.tif");
              writer->SetInput(m_binarySegmentation);
              writer->Update();
          }

          int minCCSize = 0;
          if (settings.contains("Minimum synapse size (voxels)"))
                minCCSize = settings.value("Minimum synapse size (voxels)").toInt();

          if(minCCSize > 0)
          {
              qDebug() << "Removing components smaller than " << minCCSize << " voxels.";
              CcboostAdapter::removeSmallComponentsOld(m_binarySegmentation, minCCSize);
          }

      }

      //CcboostAdapter::splitSegmentations(m_inputSegmentation, outSegList, true, true);
      LabelMapType::Pointer labelmap = LabelMapType::New();
      CcboostAdapter::splitSegmentations(m_binarySegmentation, labelmap);

      if(!canExecute())
            return;

      predictedSegmentationsList = labelmap;


  } catch( itk::ExceptionObject & err ) {
      qDebug() << tr("Itk exception on plugin caught at %1:%2. Error: %3.").arg(__FILE__).arg(__LINE__).arg(err.what());
      qDebug("Returning focus to ESPina.");
      return;
  }


  if (canExecute()) {
    emit progress(100);
    qDebug() << "Ccboost Segmentation Finished";
  }

}
