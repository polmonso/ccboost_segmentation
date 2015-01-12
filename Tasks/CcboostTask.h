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

#ifndef ESPINA_CCBOOST_TASK_H
#define ESPINA_CCBOOST_TASK_H

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

//Qt
#include <QMessageBox>

namespace ESPINA {
  namespace CCB {

    class CcboostTask
    : public Task
    {

        Q_OBJECT
    private:
        typedef itk::ImageFileWriter< itkVolumeType > WriterType;
        using bigVolumeType = itk::Image<unsigned short, 3>;
        using BigWriterType = itk::ImageFileWriter< bigVolumeType >;

        static const unsigned int FREEMEMORYREQUIREDPROPORTIONTRAIN = 170;
        static const unsigned int FREEMEMORYREQUIREDPROPORTIONPREDICT = 180;
        static const unsigned int MINTRUTHSYNAPSES = 6;
        static const unsigned int MINTRUTHMITOCHONDRIA = 2;
        static const double       MINNUMBGPX = 100000;
        static const unsigned int CCBOOSTBACKGROUNDLABEL = 128;
    public:
        static const unsigned int ANNOTATEDPADDING = 30;
    public:
       static const QString MITOCHONDRIA;
       static const QString SYNAPSE;
       static const QString BACKGROUND;
       static QString ELEMENT;
       static const QString POSITIVETAG;
       static const QString NEGATIVETAG;

       ConfigData<itkVolumeType> ccboostconfig;
    public:
        explicit CcboostTask(ChannelAdapterPtr channel,
                             SchedulerSPtr     scheduler);

      ChannelAdapterPtr channel() const
      { return m_channel; }

      std::vector<itkVolumeType::Pointer>  predictedSegmentationsList;

      static itkVolumeType::Pointer mergeSegmentations(const itkVolumeType::Pointer channelItk,
                                                            const SegmentationAdapterSList& segList,
                                                            const SegmentationAdapterSList& backgroundSegList);

    signals:
      void message(QString);
      void questionContinue(QString);

    protected:
      virtual void run();

    private:
      int numberOfLabels() const;

      bool enoughGroundTruth();
      bool enoughMemory(const itkVolumeType::Pointer channelItk, const itkVolumeType::RegionType annotatedRegion, unsigned int & numPredictRegions);


      static void applyEspinaSettings(ConfigData<itkVolumeType> cfgdata);

      void runCore(const ConfigData<itkVolumeType>& ccboostconfig,
                   std::vector<itkVolumeType::Pointer>& outputSplittedSegList);
    public:
      //TODO espina2. this is a hack to get the segmentations in the filter.
      //Find out how to do it properly
      SegmentationAdapterSList m_groundTruthSegList;
      SegmentationAdapterSList m_backgroundGroundTruthSegList;

    private:
      ChannelAdapterPtr m_channel;

      bool m_abort;

      unsigned int memoryAvailableMB;

      itkVolumeType::Pointer m_inputSegmentation;
      itkVolumeType::Pointer m_inputChannel;

    };
    using CcboostTaskPtr  = CcboostTask *;
    using CcboostTaskSPtr = std::shared_ptr<CcboostTask>;

  } // namespace CCB
} // namespace ESPINA

Q_DECLARE_METATYPE(ESPINA::CCB::CcboostTaskSPtr);
#endif // ESPINA_RAS_TRAINER_TASK_H
