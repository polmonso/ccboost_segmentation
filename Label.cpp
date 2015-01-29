
#include "Label.h"
#include <Core/Analysis/Category.h>
#include <itkImageRegionIterator.h>

#include <QDebug>

using namespace ESPINA;
using namespace ESPINA::CCB;


//----------------------------------------------------------------------------
Label::Label(CategoryAdapterSPtr category)
: m_category(category)
, m_visibleInPreview(category != nullptr)
, m_apriori(0.0)
, m_minVoxelSize(1)
, m_trainingVoxelsCount(0)
{
}

//----------------------------------------------------------------------------
QString Label::name() const
{
  return m_category?m_category->classificationName():"Background";
}

//----------------------------------------------------------------------------
QColor Label::color() const
{
  return m_category?m_category->color():QColor(Qt::black);
}

//----------------------------------------------------------------------------
void Label::setVisibleInPreview(bool visible)
{
  m_visibleInPreview = visible;
}

//----------------------------------------------------------------------------
void Label::setAPriori(double value)
{
  m_apriori = value;
}

//----------------------------------------------------------------------------
void Label::addTrainingVolume(TrainingVolume volume)
{
  m_trainingVolumes << volume;

  m_trainingVoxelsCount += numberOfVoxels(volume);
}

//----------------------------------------------------------------------------
void Label::removeTrainingVolume(Label::TrainingVolume volume)
{
  if (m_trainingVolumes.contains(volume))
  {
    m_trainingVolumes.removeOne(volume);

    m_trainingVoxelsCount -= numberOfVoxels(volume);
  }
}

//----------------------------------------------------------------------------
void Label::discardTrainingVolumes()
{
  m_trainingVolumes.clear();
  m_trainingVoxelsCount = 0;
}

// TODO Move to DefaultVolumetricAPI?
//----------------------------------------------------------------------------
unsigned int Label::numberOfVoxels(Label::TrainingVolume volume)
{
  unsigned int total = 0;

  // TODO: Use iterators
  auto image = volume->itkImage();
  itk::ImageRegionIterator<itkVolumeType> it(image, image->GetLargestPossibleRegion());
  it.Begin();
  while (!it.IsAtEnd())
  {
    total += it.Get() != volume->backgroundValue();
    ++it;
  }

  return total;
}
