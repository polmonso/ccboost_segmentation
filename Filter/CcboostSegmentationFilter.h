/*
 * CcboostSegmentationFilter.h
 *
 *  Created on: Jan 18, 2013
 *      Author: Félix de las Pozas Álvarez
 */

#ifndef CCBOOSTSEGMENTATIONFILTER_H_
#define CCBOOSTSEGMENTATIONFILTER_H_

#include "CcboostSegmentationPlugin_Export.h"

//ccboost
#include <QDebug>
#include "BoosterInputData.h"

// EspINA
#include <Core/EspinaTypes.h>
#include <Filters/BasicSegmentationFilter.h>
#include <GUI/Representations/MeshRepresentation.h>

#include <Core/Analysis/Data/Mesh/MarchingCubesMesh.hxx>
#include <Filter/ConfigData.h>

// ITK
#include <itkImageFileWriter.h>

#include <itkConstantPadImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>

// VTK
#include <vtkGridTransform.h>
#include <vtkOBBTree.h>
#include <vtkPlaneSource.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTransformPolyDataFilter.h>

// STL
#include <list>

class QString;

namespace EspINA
{
  class SASFetchBehaviour;
  //FIXME do I have to put this here?
  //typedef itk::Image<unsigned char, 3> itkVolumeType;

  const Filter::Type AS_FILTER = "CcboostSegmentation::CcboostSegmentationFilter";

  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationFilter
  : public Filter
  {
    Q_OBJECT
    static constexpr double   THRESHOLDFACTOR           = 0.005; // Percentage of a single step
    static const unsigned int MAXSAVEDSTATUSES          = 10;
    static const int          MAXITERATIONSFACTOR       = 100;
    static constexpr float    DISPLACEMENTSCALE         = 1;
    static constexpr float    CLIPPINGTHRESHOLD         = 0.5;
    static constexpr float    DISTANCESMOOTHSIGMAFACTOR = 0.67448; // probit(0.25)

    typedef itk::ImageFileWriter< itkVolumeType > WriterType;
    using bigVolumeType = itk::Image<unsigned short, 3>;
    using BigWriterType = itk::ImageFileWriter< bigVolumeType >;

    static const unsigned int FREEMEMORYREQUIREDPROPORTIONTRAIN = 170;
    static const unsigned int FREEMEMORYREQUIREDPROPORTIONPREDICT = 180;
    static const unsigned int MINTRUTHSYNAPSES = 6;
    static const unsigned int MINTRUTHMITOCHONDRIA = 2;
    static const double       MINNUMBGPX = 100000;
    static const unsigned int CCBOOSTBACKGROUNDLABEL = 128;
    static const unsigned int ANNOTATEDPADDING = 30;

    using DistanceType = float;
    using Points = vtkSmartPointer<vtkPoints>;
    using PolyData = vtkSmartPointer<vtkPolyData>;
    using OBBTreeType = vtkSmartPointer<vtkOBBTree>;

    using itkVolumeIterator = itk::ImageRegionIterator<itkVolumeType>;
    using ItkToVtkFilterType = itk::ImageToVTKImageFilter<itkVolumeType>;
    using ExtractFilterType = itk::ExtractImageFilter<itkVolumeType, itkVolumeType>;
    using PadFilterType = itk::ConstantPadImageFilter<itkVolumeType, itkVolumeType>;

    using GridTransform = vtkSmartPointer<vtkGridTransform>;
    using TransformPolyDataFilter = vtkSmartPointer<vtkTransformPolyDataFilter>;

    using DistanceMapType = itk::Image<DistanceType,3>;
    using DistanceIterator = itk::ImageRegionConstIterator<DistanceMapType>;
    using SMDistanceMapFilterType = itk::SignedMaurerDistanceMapImageFilter<itkVolumeType, DistanceMapType>;
    using SmoothingFilterType = itk::SmoothingRecursiveGaussianImageFilter<DistanceMapType, DistanceMapType>;
    using GradientFilterType = itk::GradientImageFilter<DistanceMapType, float>;

    using PlaneSourceType = vtkSmartPointer<vtkPlaneSource>;
    using CovariantVectorType = itk::CovariantVector<float, 3>;
    using CovariantVectorImageType = itk::Image<CovariantVectorType,3>;
    using PointsListType = std::list< vtkSmartPointer<vtkPoints> >;

  public:

    static const char * MESH_ORIGIN;
    static const char * MESH_NORMAL;

    static const QString MITOCHONDRIA;
    static const QString SYNAPSE;
    static QString ELEMENT;
    static const QString POSITIVETAG;
    static const QString NEGATIVETAG;

    ConfigData<itkVolumeType> ccboostconfig;

  public:
    explicit CcboostSegmentationFilter(InputSList inputs, Type type, SchedulerSPtr scheduler);
    virtual ~CcboostSegmentationFilter();

  protected:

    /* \brief Implements Persistent::restoreState().
     *
     */
    virtual void restoreState(const State& state)
    {};

    /* \brief Implements Persistent::state().
     *
     */
    virtual State state() const
    { return State(); }

    /* \brief Implements Filter::safeFilterSnapshot().
     *
     */
    virtual Snapshot saveFilterSnapshot() const
    { return Snapshot(); }

    /* \brief Implements Filter::needUpdate().
     *
     */
    virtual bool needUpdate() const;

    /* \brief Implements Filter::needUpdate(oid).
     *
     */
    virtual bool needUpdate(Output::Id oid) const;

    /* \brief Implements Filter::execute().
     *
     */
    virtual void execute();

    /** \brief Imlements Filter::execute(oid).
     *
     */
    virtual void execute(Output::Id id);

    /* \brief Implements Filter::ignoreStorageContent()
     *
     */
    virtual bool ignoreStorageContent() const
    { return this->m_alreadyFetchedData; }

    /* \brief Implements Filter::invalidateEditedRegions().
     *
     */
    virtual bool invalidateEditedRegions()
    { return false; }

  protected slots:
    virtual void inputModified();

  private:

    bool enoughGroundTruth();
    bool enoughMemory(const itkVolumeType::Pointer channelItk, const itkVolumeType::RegionType annotatedRegion, unsigned int & numPredictRegions);

    itkVolumeType::Pointer mergeSegmentations(const itkVolumeType::Pointer channelItk,
                                                const SegmentationAdapterList segList,
                                                const SegmentationAdapterList backgroundSegList);
    static void applyEspinaSettings(ConfigData<itkVolumeType> cfgdata);


  public:
    //TODO espina2. this is a hack to get the segmentations in the filter.
    //Find out how to do it properly
    SegmentationAdapterList m_groundTruthSegList;
    SegmentationAdapterList m_backgroundGroundTruthSegList;

  private:
    int m_resolution;
    int m_iterations;
    bool m_converge;
    mutable PolyData m_ap;

    unsigned int memoryAvailableMB;

    itkVolumeType::Pointer m_inputSegmentation;
    itkVolumeType::Pointer m_inputChannel;

    bool m_alreadyFetchedData;
    TimeStamp m_lastModifiedMesh;

    friend class SASFetchBehaviour;
  };

  using CcboostSegmentationFilterPtr  = CcboostSegmentationFilter *;
  using CcboostSegmentationFilterSPtr = std::shared_ptr<CcboostSegmentationFilter>;

} /* namespace EspINA */
#endif /* CCBOOSTSEGMENTATIONFILTER_H_ */
