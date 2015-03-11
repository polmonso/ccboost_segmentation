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
                       SchedulerSPtr     scheduler,
                       std::string file)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
, filename(file)
{

    loadFromDisk = true;

}

//------------------------------------------------------------------------
ImportTask::ImportTask(ChannelAdapterPtr channel,
                       SchedulerSPtr     scheduler,
                       itkVolumeType::Pointer inputSegmentation)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
, m_inputSegmentation(inputSegmentation)
{

    loadFromDisk = false;

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

  //TODO add const-correctness
  std::vector<itkVolumeType::Pointer> outSegList;

  try {

      if(loadFromDisk) {
          typedef itk::ImageFileReader<itkVolumeType> ReaderType;
          ReaderType::Pointer reader = ReaderType::New();
          reader->SetFileName(filename);
          reader->Update();

          m_inputSegmentation = reader->GetOutput();
      }

      CcboostAdapter::splitSegmentations(m_inputSegmentation, outSegList);

  } catch( itk::ExceptionObject & err ) {
      qDebug() << tr("Itk exception on plugin caught at %1:%2. Error: %3.").arg(__FILE__).arg(__LINE__).arg(err.what());
      qDebug("Returning focus to ESPina.");
      return;
  }

  if(!canExecute())
      return;


  qDebug() << outSegList.size();

  predictedSegmentationsList = outSegList;

  if (canExecute())
  {
    emit progress(100);
    qDebug() << "Ccboost Segmentation Finished";
  }
}
