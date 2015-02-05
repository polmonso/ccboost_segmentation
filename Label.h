
#ifndef ESPINA_CCB_LABEL_H
#define ESPINA_CCB_LABEL_H

#include <QColor>
#include <GUI/Model/CategoryAdapter.h>
#include <Core/Analysis/Data/VolumetricData.hxx>

namespace ESPINA
{
  namespace CCB
  {

    class Label
    {
    public:
      using TrainingVolume     = DefaultVolumetricDataSPtr;
      using TrainingVolumeList = QList<TrainingVolume>;

      static Label Background()
      { return Label(); }

    public:
      Label(CategoryAdapterSPtr category = CategoryAdapterSPtr());

      QString name() const;

      QColor color() const;

      CategoryAdapterSPtr category() const
      { return m_category; }

      void setVisibleInPreview(bool visible);

      bool isVisibleInPreview() const
      {return m_visibleInPreview;}

      void setAPriori(double value);

      double aPriori() const
      {return m_apriori;}

      void setMinVoxelSize(int size)
      { m_minVoxelSize = size; }

      unsigned int minVoxelSize() const
      { return m_minVoxelSize; }

      void addTrainingVolume(TrainingVolume volume);

      void removeTrainingVolume(TrainingVolume volume);

      void discardTrainingVolumes();

      TrainingVolumeList trainingVolumes() const
      { return m_trainingVolumes; }

      unsigned int  trainingVoxelsCount() const
      { return m_trainingVoxelsCount; }

      bool operator<(const Label& rhs) const
      {
        QString leftName  =     m_category?    m_category->classificationName():"";
        QString rightName = rhs.m_category?rhs.m_category->classificationName():"";

        return leftName < rightName;
      }

      bool operator==(const Label& rhs) const
      { return m_category == rhs.m_category; }

      bool operator!=(const Label& rhs) const
      { return !(m_category == rhs.m_category); }

    private:
      unsigned int numberOfVoxels(TrainingVolume volume);

    private:
      CategoryAdapterSPtr m_category;
      bool     m_visibleInPreview;
      double   m_apriori;

      unsigned int m_minVoxelSize;
      unsigned int m_trainingVoxelsCount;

      QList<DefaultVolumetricDataSPtr> m_trainingVolumes;
    };

    inline bool isBackground(const Label& label)
    { return label.category().get() == nullptr; }

    using LabelList = QList<Label>;
  }
}

#endif // ESPINA_RAS_LABEL_H
