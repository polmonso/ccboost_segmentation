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
//#include <Core/EspinaTypes.h>

//for standalone version we input itkVolumeType ourselves
typedef itk::Image<unsigned char, 3> itkVolumeType;

class CcboostAdapter : public QObject
{
    Q_OBJECT

public:
    typedef itk::Image<float, 3> FloatTypeImage;

    //TODO this goes somewhere else
    static const unsigned int FREEMEMORYREQUIREDPROPORTIONPREDICT = 180;

private:
    typedef itk::ImageFileWriter< itkVolumeType > WriterType;
    using bigVolumeType = itk::Image<unsigned short, 3>;
    using BigWriterType = itk::ImageFileWriter< bigVolumeType >;


signals:
    void updatePrediction(FloatTypeImage::Pointer itkVolumeType);

public:
    explicit CcboostAdapter() {};

    static void splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                                   std::vector<itkVolumeType::Pointer>& outSegList,
                                   bool saveIntermediateVolumes = false,
                                   std::string cacheDir = std::string(".") );

    static void postprocessing(const ConfigData<itkVolumeType> &cfgData, itkVolumeType::Pointer& outputSegmentation);

    static void postprocessing(itkVolumeType::Pointer& outputSegmentation, double zAnisotropyFactor, bool saveIntermediateVolumes = false, std::string cacheDir = ".");

    static void removeSmallComponents(itk::Image<unsigned char, 3>::Pointer & segmentation,
                               int minCCSize = 1000,
                               int threshold = 128);

    static void computeAllFeatures(const ConfigData<itkVolumeType> cfgData);

    static void computeFeatures(const ConfigData<itkVolumeType> cfgData, const SetConfigData<itkVolumeType> cfgDataROI);

    static void computeFeature(const SetConfigData<itkVolumeType> cfgDataROI,
                                        const std::string cacheDir,
                                        const std::string featureFile);

    static void addAllFeatures(const ConfigData<itkVolumeType>& cfgData, MultipleROIData::ROIDataPtr roi);

    static void addFeatures(const std::string& cacheDir, const SetConfigData<itkVolumeType>& cfgDataROI, MultipleROIData::ROIDataPtr roi);

    //TODO add const-correctness
    static bool core(const ConfigData<itkVolumeType>& cfg,
                     FloatTypeImage::Pointer &probabilisticOutSeg,
                     std::vector<itkVolumeType::Pointer>& outputSplittedSegList,
                     itkVolumeType::Pointer &outputSegmentation);

    static bool testcore(const ConfigData<itkVolumeType>& cfg,
                         std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs,
                         std::vector<itkVolumeType::Pointer>& outSegList,
                         std::vector<itkVolumeType::Pointer>& outputSegmentations);

    /**
     * @brief core interface with ccboost with automatic postprocessing
     * @param cfg configuration data
     * @param probabilisticOutSegs unsplitted, unbinarized probabilistic chunk list
     * @param outSegList connected component list
     * @param outputSegmentations unsplitted segmented binarized chunk list
     * @return true on success false on error
     */
    static bool core(const ConfigData<itkVolumeType>& cfg,
                         std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs,
                         std::vector<itkVolumeType::Pointer>& outputSplittedSegList,
                         std::vector<itkVolumeType::Pointer>& outputSegmentations);

    static bool automaticCore(const ConfigData<itkVolumeType>& cfgdata,
                              FloatTypeImage::Pointer& probabilisticOutSeg,
                              std::vector<itkVolumeType::Pointer>& outSegList);

};

#endif // CCBOOSTADAPTER_H
