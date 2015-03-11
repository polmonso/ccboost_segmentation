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

#ifndef ESPINA_IMPORT_TASK_H
#define ESPINA_IMPORT_TASK_H

#include <Core/MultiTasking/Task.h>
#include <GUI/Model/ChannelAdapter.h>
#include <Core/EspinaTypes.h>

//ccboost
#include <QDebug>
#include "BoosterInputData.h"

// ESPINA
#include <Core/EspinaTypes.h>
#include <Core/Analysis/Filter.h>
#include <GUI/Representations/MeshRepresentation.h>

#include <Core/Analysis/Data/Mesh/MarchingCubesMesh.hxx>
#include <Filter/ConfigData.h>

// ITK
#include <itkImageFileWriter.h>

#include <itkConstantPadImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>


namespace ESPINA {
  namespace CCB {

    class ImportTask
    : public Task
    {

        Q_OBJECT

    private:
        typedef itk::ImageFileWriter< itkVolumeType > WriterType;

        static const unsigned int CCBOOSTBACKGROUNDLABEL = 128;

    public:
        explicit ImportTask(ChannelAdapterPtr channel,
                            SchedulerSPtr     scheduler,
                            std::string file);

        explicit ImportTask(ChannelAdapterPtr channel,
                            SchedulerSPtr     scheduler,
                            itkVolumeType::Pointer inputSegmentation);

      ChannelAdapterPtr channel() const
      { return m_channel; }

      std::vector<itkVolumeType::Pointer>  predictedSegmentationsList;

    protected:
      virtual void run();

    private:
      void runCore(std::vector<itkVolumeType::Pointer>& outSegList);
      void splitSegs(const itkVolumeType::Pointer outputSegmentation,
                                 std::vector<itkVolumeType::Pointer>& outSegList);
     public:
      static const QString IMPORTED;
      static const QString SUPPORTED_FILES;

    private:
      ChannelAdapterPtr m_channel;

      bool m_abort;

      itkVolumeType::Pointer m_inputChannel;

      bool loadFromDisk;
      std::string filename;
      itkVolumeType::Pointer m_inputSegmentation;

    };
    typedef ImportTask* ImportTaskPtr;
    typedef std::shared_ptr<ImportTask> ImportTaskSPtr;
  } // namespace CCB
} // namespace ESPINA

Q_DECLARE_METATYPE(ESPINA::CCB::ImportTaskSPtr);
#endif // ESPINA_IMPORT_TASK_H
