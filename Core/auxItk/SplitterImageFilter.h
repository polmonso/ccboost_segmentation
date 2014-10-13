#ifndef __itkSplitterImageFilter_h
#define __itkSplitterImageFilter_h
 
#include "itkImageToImageFilter.h"
 
namespace itk
{
    template< class TImage>
    class SplitterImageFilter:public ImageToImageFilter< TImage, TImage >
    {
    public:
      /** Standard class typedefs. */
      typedef SplitterImageFilter             Self;
      typedef ImageToImageFilter< TImage, TImage > Superclass;
      typedef SmartPointer< Self >        Pointer;

      /** Method for creation through the object factory. */
      itkNewMacro(Self);

      /** Run-time type information (and related methods). */
      itkTypeMacro(SplitterImageFilter, ImageToImageFilter);


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

      void SetSelectedRegion(int regionNum) {
          this->selectedRegion = regionNum;
          this->Modified();
      }

    protected:
      SplitterImageFilter();
      ~SplitterImageFilter(){}

      /** Does the real work. */
      virtual void GenerateData();
      virtual void GenerateOutputInformation();


    private:
      SplitterImageFilter(const Self &); //purposely not implemented
      void operator=(const Self &);  //purposely not implemented

      typename TImage::OffsetType overlap;
      typename TImage::SizeType numSplits;
      int selectedRegion;
      std::vector<typename TImage::RegionType> pasteDRegions, pasteORegions, cropRegions;

      bool WorkAsSplitter; //TODO inheritance instead
      bool needUpdate;

    };
} //namespace ITK
 
 
#ifndef ITK_MANUAL_INSTANTIATION
#include "SplitterImageFilter.txx"
#endif
 
 
#endif // __itkSplitterImageFilter_h
