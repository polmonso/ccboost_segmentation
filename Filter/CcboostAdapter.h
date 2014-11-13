#ifndef CCBOOSTADAPTER_H
#define CCBOOSTADAPTER_H

//ccboost
#include <QDebug>
#include "BoosterInputData.h"

#include "ConfigData.h"
//#include "CcboostSegmentationFilter.h"
//ITK
#include <itkImageFileWriter.h>

// ESPINA
#include <Core/EspinaTypes.h>

namespace ESPINA
{
class CcboostAdapter : public QObject
{
    Q_OBJECT

public:
    typedef itk::Image<float, 3> FloatTypeImage;
private:
    typedef itk::ImageFileWriter< itkVolumeType > WriterType;
    using bigVolumeType = itk::Image<unsigned short, 3>;
    using BigWriterType = itk::ImageFileWriter< bigVolumeType >;

signals:
    updatePrediction(FloatTypeImage::Pointer itkVolumeType);

public:
    explicit CcboostAdapter() {};

    static void splitSegmentations(const ConfigData<itkVolumeType>  &cfgData,
                                   const itkVolumeType::Pointer outputSegmentation,
                                   std::vector<itkVolumeType::Pointer>& outSegList);

    static void postprocessing(const ConfigData<itkVolumeType> &cfgData, itkVolumeType::Pointer& outputSegmentation);

    static void removeSmallComponents(itk::Image<unsigned char, 3>::Pointer & segmentation,
                               int minCCSize = 1000,
                               int threshold = 128);

    static void computeAllFeatures(const ConfigData<itkVolumeType> cfgData);

    static void computeFeatures(const ConfigData<itkVolumeType> cfgData, const SetConfigData<itkVolumeType> cfgDataROI);

    static void addAllFeatures(const ConfigData<itkVolumeType>& cfgData, ROIData &roi);

    static void addFeatures(const std::string& cacheDir, const SetConfigData<itkVolumeType>& cfgDataROI, ROIData& roi);

    //TODO add const-correctness
    static bool core(const ConfigData<itkVolumeType>& cfg,
                     FloatTypeImage::Pointer &probabilisticOutSeg,
                     std::vector<itkVolumeType::Pointer>& outSegList);

    static bool automaticCore(const ConfigData<itkVolumeType>& cfgdata,
                              FloatTypeImage::Pointer& probabilisticOutSeg,
                              std::vector<itkVolumeType::Pointer>& outSegList);

};
} /* namespace ESPINA */

#endif // CCBOOSTADAPTER_H
