#ifndef __itkImageSplitter_h
#define __itkImageSplitter_h

#include "itkExtractImageFilter.h"

#include <sys/sysinfo.h>
#include <limits.h>

namespace itk
{
    template< class TImage>
    class ImageSplitter
    {
        //FIXME either use it as an object with all the data
        //  or with a static library (which is horrible or not possible in templates)
    public:
        ImageSplitter(const typename TImage::RegionType volumeLargestRegion,
                      const int numRegions2,
                      typename TImage::OffsetType overlap) : volumeLargestRegion(volumeLargestRegion),
            numRegions(numRegions2),
            overlap(overlap)
        {
            computeNumSplits(volumeLargestRegion.GetSize(), numRegions, numSplits);
            computeCropRegions(volumeLargestRegion.GetSize(), overlap, numSplits);
        }

    public:

        static int numRegionsFittingInMemory(const typename TImage::SizeType volumeItkSize,
                                             int proportion = 1) {
            //installed ram data
          #ifdef __unix
            struct sysinfo info;
            sysinfo(&info);
            int memoryAvailableMB = info.totalram/1024/1024;

            if(memoryAvailableMB == 0){
                memoryAvailableMB = std::numeric_limits<unsigned int>::max();
                std::cerr << "The amount of RAM memory couldn't be detected or very low "
                          << "(info.totalram = " << std::to_string(info.totalram) << " Bytes). "
                          << "Running the Synapse Segmentation Plugin might lead to crash." << std::endl;
                return 1;
            }else{
                std::cout << "Available memory: " << std::to_string(info.totalram) << " Bytes." << std::endl;
            }
          #else
            int memoryAvailableMB = numeric_limits<unsigned int>::max();
            std::cerr << "The amount of RAM memory couldn't be detected or very low "
                      << "(info.totalram = " << std::to_string(info.totalram) << " Bytes). "
                      << "Running the Synapse Segmentation Plugin might lead to crash." << std::endl;
            return 1;
          #endif

            double volumeSize = volumeItkSize[0]*volumeItkSize[1]*volumeItkSize[2];
            unsigned int memoryNeededMBpredict = proportion*volumeSize/1024/1024;
            int numPredictRegions = memoryNeededMBpredict/memoryAvailableMB + 1;

            std::cout << "numregions:" << numPredictRegions << " volumeSize " << volumeSize << std::endl;

            return numPredictRegions;
        }


        void computeNumSplits(const typename TImage::SizeType originalSize,
                              const unsigned int numPredictRegions,
                              typename TImage::SizeType & numSplits);

        std::vector<typename TImage::RegionType> getCropRegions(){
            return cropRegions;
        }

        void saveCroppedRegions(const typename TImage::ConstPointer rawVolume,
                                const std::vector<typename TImage::RegionType> cropRegions);

        void SetNumSplits(const typename TImage::SizeType numSplits);

        void SetOverlap(const typename TImage::OffsetType overlap);

        void pasteCroppedRegions(std::vector<typename TImage::Pointer>& probabilisticOutSegs,
                                 typename TImage::Pointer& outputImage);

        void pasteCroppedRegions(const typename TImage::RegionType volumeRegion,
                                 const typename TImage::SizeType numSplits,
                                 std::vector<typename TImage::Pointer>& probabilisticOutSegs,
                                 std::vector<typename TImage::RegionType>& cropRegions,
                                 std::vector<typename TImage::RegionType>& pasteDRegions,
                                 std::vector<typename TImage::RegionType>& pasteORegions,
                                 typename TImage::Pointer& outputImage);

        void pasteCroppedRegions(const typename TImage::ConstPointer rawVolume,
                                 const typename TImage::SizeType originalSize,
                                 const typename TImage::OffsetType overlap,
                                 const typename TImage::SizeType numSplits,
                                 std::vector<typename TImage::RegionType>& cropRegions,
                                 std::vector<typename TImage::RegionType>& pasteDRegions,
                                 std::vector<typename TImage::RegionType>& pasteORegions);

        void pasteCroppedRegionsFromDisk(const typename TImage::ConstPointer rawVolume,
                                         const typename TImage::SizeType originalSize,
                                         const typename TImage::OffsetType overlap,
                                         const typename TImage::SizeType numSplits,
                                         std::vector<typename TImage::RegionType>& cropRegions,
                                         std::vector<typename TImage::RegionType>& pasteDRegions,
                                         std::vector<typename TImage::RegionType>& pasteORegions);

        typename TImage::Pointer crop(const typename TImage::Pointer image,
                                      typename TImage::RegionType region)
        {

            typedef ExtractImageFilter<TImage, TImage> ExtractImageFilterType;
            typename ExtractImageFilterType::Pointer filter = ExtractImageFilterType::New();

            filter->SetExtractionRegion(region);
            filter->SetInput(image);
            filter->SetDirectionCollapseToIdentity();
            filter->Update();

            typename TImage::Pointer croppedImage = TImage::New();
            croppedImage = filter->GetOutput();
            croppedImage->DisconnectPipeline();

            return croppedImage;
        }

    protected:
        void computeCropRegions(const typename TImage::SizeType originalSize,
                                const typename TImage::OffsetType overlap,
                                const typename TImage::SizeType numSplits);

    private:
        typename TImage::RegionType volumeLargestRegion;
        typename TImage::OffsetType overlap;
        typename TImage::SizeType numSplits;
        const int numRegions;
        std::vector<typename TImage::RegionType> pasteDRegions, pasteORegions, cropRegions;


    };
} //namespace ITK


#ifndef ITK_MANUAL_INSTANTIATION
#include "ImageSplitter.txx"
#endif


#endif // __itkImageSplitter_h
