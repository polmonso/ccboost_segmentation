#ifndef __itkImageSplitter_h
#define __itkImageSplitter_h
  
namespace itk
{
    template< class TImage>
    class ImageSplitter
    {
    public:
      void computeNumSplits(const typename TImage::SizeType originalSize,
                            const unsigned int numPredictRegions,
                            typename TImage::SizeType & numSplits);

      void getCroppedRegions(const typename TImage::SizeType originalSize,
                             const typename TImage::OffsetType overlap,
                             const typename TImage::SizeType numSplits,
                             std::vector<typename TImage::RegionType>& cropRegions,
                             std::vector<typename TImage::RegionType>& pasteDRegions,
                             std::vector<typename TImage::RegionType>& pasteORegions);

      void saveCroppedRegions(const typename TImage::ConstPointer rawVolume,
                              const std::vector<typename TImage::RegionType> cropRegions);

      void SetWorkAsSplitter(bool split);

      void SetNumSplits(const typename TImage::SizeType numSplits);

      void SetOverlap(const typename TImage::OffsetType overlap);

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

    protected:
      ImageSplitter();
      ~ImageSplitter(){}

    private:
      typename TImage::OffsetType overlap;
      typename TImage::SizeType numSplits;
      std::vector<typename TImage::RegionType> pasteDRegions, pasteORegions, cropRegions;


    };
} //namespace ITK
 
 
#ifndef ITK_MANUAL_INSTANTIATION
#include "ImageSplitter.txx"
#endif
 
 
#endif // __itkImageSplitter_h
