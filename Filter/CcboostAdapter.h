#ifndef CCBOOSTADAPTER_H
#define CCBOOSTADAPTER_H

//ccboost
#include <QDebug>
#include "BoosterInputData.h"

#include "ConfigData.h"
//ITK
#include <itkImageFileWriter.h>

// ESPINA
#include "CCBTypes.h"

class CcboostAdapter : public QObject
{
    Q_OBJECT

public:
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

    /**
     * @brief splitSegmentations
     * @param outputSegmentation
     * @param labelMap itk::ShapeLabelMap
     * @param saveIntermediateVolumes
     * @param cacheDir
     */
    static void splitSegmentations(const itkVolumeType::Pointer& outputSegmentation,
                                   ESPINA::CCB::LabelMapType::Pointer& labelMap,
                                   const int minCCSize = 1000);

    static void splitSegmentations(const itkVolumeType::Pointer outputSegmentation,
                                   std::vector<itkVolumeType::Pointer>& outSegList,
                                   bool skipBiggest = false,
                                   bool saveIntermediateVolumes = false,
                                   std::string cacheDir = std::string(""));

    template< typename TImageType = itkVolumeType >
    static void removeborders(const ConfigData< TImageType > &cfgData, typename TImageType::Pointer& outputSegmentation);

    template< typename TImageType = itkVolumeType >
    static void removeborders(typename TImageType::Pointer& outputSegmentation, bool saveIntermediateVolumes = false, std::string cacheDir = ".");

    //Note that splitSegmentations already discards small components
    //throws ItkException
    template< typename TImageType = itkVolumeType >
    static void removeSmallComponents(typename TImageType::Pointer & segmentation, int minCCSize = 1000);

    static void computeAllFeatures(const ConfigData<itkVolumeType> cfgData);

    static void computeFeatures(const ConfigData<itkVolumeType> cfgData, const SetConfigData<itkVolumeType> cfgDataROI);

    static void computeFeature(const SetConfigData<itkVolumeType> cfgDataROI,
                                        const std::string cacheDir,
                                        const std::string featureFile);

    static void addAllFeatures(const ConfigData<itkVolumeType>& cfgData, MultipleROIData::ROIDataPtr roi);

    static void addFeatures(const std::string& cacheDir, const SetConfigData<itkVolumeType>& cfgDataROI, MultipleROIData::ROIDataPtr roi);

    //TODO add const-correctness
    static bool core(const ConfigData<itkVolumeType>& cfgdata,
                              std::vector<FloatTypeImage::Pointer>& probabilisticOutSegs);

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

//#define CVL_EXPLICIT_INSTANTIATION
#ifndef CVL_EXPLICIT_INSTANTIATION
//add for implicit instantiation (and remove explicit instantiaton on CcboostAdapter.cpp
#include "CcboostAdapter.tpp"
#endif

#endif // CCBOOSTADAPTER_H
