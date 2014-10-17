#ifndef CCBOOSTADAPTER_H
#define CCBOOSTADAPTER_H

//ccboost
#include <QDebug>
#include "BoosterInputData.h"

#include "ConfigData.h"
#include "MessageQueue.h"

//ITK
#include <itkImageFileWriter.h>

// EspINA
#include <Core/EspinaTypes.h>

namespace EspINA
{
class CcboostAdapter
{

    typedef itk::ImageFileWriter< itkVolumeType > WriterType;
    using bigVolumeType = itk::Image<unsigned short, 3>;
    using BigWriterType = itk::ImageFileWriter< bigVolumeType >;

public:
    CcboostAdapter();

    static void splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                            std::vector<itkVolumeType::Pointer>& outSegList);

    static MultipleROIData preprocess(std::vector<itkVolumeType::Pointer> channels,
                               std::vector<itkVolumeType::Pointer> groundTruths,
                               std::string cacheDir,
                               std::vector<std::string> featuresList);

    static void postprocessing(itkVolumeType::Pointer& outputSegmentation,
                        double anisotropyFactor, std::string cacheDir,
                        int minComponentSize = 1000,
                        bool saveIntermediateVolumes = true);

    static void removeSmallComponents(itk::Image<unsigned char, 3>::Pointer & segmentation,
                               int minCCSize = 1000,
                               int threshold = 128);

    static void computeAllFeatures(const ConfigData<itkVolumeType> cfgData, ThreadStateDescription *stateDescription = NULL);

    static void computeFeatures(const ConfigData<itkVolumeType> cfgData, const SetConfigData<itkVolumeType> cfgDataROI, ThreadStateDescription *stateDescription = NULL);

    static void addAllFeatures(const ConfigData<itkVolumeType> cfgData, ROIData &roi);

    static void addFeatures(const SetConfigData<itkVolumeType> cfgDataROI, ROIData &roi);

    //TODO add const-correctness
    static bool core(const ConfigData<itkVolumeType>& cfg,
                     itkVolumeType::Pointer& probabilisticOutSeg,
                     std::vector<itkVolumeType::Pointer>& outSegList);


};
} /* namespace EspINA */

#endif // CCBOOSTADAPTER_H
