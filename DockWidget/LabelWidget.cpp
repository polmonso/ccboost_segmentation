/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2014  Jorge Peña Pastor <jpena@cesvima.upm.es>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "LabelWidget.h"
#include <GUI/Selectors/CircularBrushSelector.h>
#include <Core/Analysis/Data/Volumetric/SparseVolume.hxx>
#include <QUndoStack>

using namespace ESPINA;
using namespace ESPINA::RAS;

//----------------------------------------------------------------------------
class LabelWidget::DrawCommand
: public QUndoCommand
{
public:
  explicit DrawCommand(SparseVolumeSPtr volume, LabelWidget *label)
  : m_volume(volume)
  , m_label(label)
  {
  }

  virtual void redo()
  {
    m_label->addTrainingVolume(m_volume);
  }

  virtual void undo()
  {
    m_label->removeTrainingVolume(m_volume);
  }

private:
  SparseVolumeSPtr m_volume;
  LabelWidget*     m_label;
};
//----------------------------------------------------------------------------
LabelWidget::LabelWidget(Label             label,
                         ChannelAdapterPtr channel,
                         ViewManagerSPtr   viewManager,
                         QUndoStack*       undoStack,
                         QWidget*          parent,
                         Qt::WindowFlags   f)
: QWidget(parent, f)
, m_label(label)
, m_undoStack(undoStack)
, m_viewManager(viewManager)
, m_widget(new TrainingVolumePreviewWidget())
{
  setupUi(this);

  if (label == Label::Background())
  {
    m_removeLabel      ->setVisible(false);
    m_minVoxelSize     ->setVisible(false);
    m_minVoxelSizeLabel->setVisible(false);
  } else
  {
    m_minVoxelSize->setValue(label.minVoxelSize());
  }

  m_voxelCount->setText(QString("%1").arg(label.trainingVoxelsCount()));

  connect(m_removeLabel, SIGNAL(clicked(bool)),
          this,          SLOT(onLabelRemoved()));

  connect(m_showTrainingVolumes, SIGNAL(toggled(bool)),
          this,                  SLOT(onVisibilityChanged(bool)));

  connect(m_drawTrainingVolume, SIGNAL(toggled(bool)),
          this,                 SLOT(onBrushEnabled(bool)));

  QColor c = m_label.color();

  QString style = "background-color : rgb(%1, %2, %3);";
  m_color->setStyleSheet(style.arg(c.red()).arg(c.green()).arg(c.blue()));

  Q_ASSERT(channel);
  m_brush = BrushSelectorSPtr{new CircularBrushSelector()};
  m_brush->setReferenceItem(channel);
  m_brush->setBrushColor(c);

  connect(m_brush.get(), SIGNAL(itemsSelected(Selector::Selection)),
          this,          SLOT(onStroke(Selector::Selection)));

  connect(m_brush.get(), SIGNAL(eventHandlerInUse(bool)),
          this,          SLOT(onBrushUseChanged(bool)));

  connect(m_discardTrainingVolumes, SIGNAL(clicked(bool)),
          this,                     SLOT(discardTrainingVolumes()));

  connect(m_minVoxelSize, SIGNAL(valueChanged(int)),
          this,           SLOT(onMinVoxelSizeChanged(int)));

  m_widget->setColor(m_label.color());

  m_viewManager->addWidget(m_widget);

  for (auto volume : m_label.trainingVolumes())
  {
    m_widget->addTrainingVolume(volume);
  }
}

//----------------------------------------------------------------------------
LabelWidget::~LabelWidget()
{
  m_viewManager->unsetEventHandler(m_brush);
  m_viewManager->removeWidget(m_widget);
}


//----------------------------------------------------------------------------
void LabelWidget::addTrainingVolume(Label::TrainingVolume volume)
{
  m_label.addTrainingVolume(volume);
  m_widget->addTrainingVolume(volume);
  m_voxelCount->setText(QString("%1").arg(m_label.trainingVoxelsCount()));

  emit trainingVolumeChanged();
}

//----------------------------------------------------------------------------
void LabelWidget::removeTrainingVolume(Label::TrainingVolume volume)
{
  m_label.removeTrainingVolume(volume);
  m_widget->removeTrainingVolume(volume);
  m_voxelCount->setText(QString("%1").arg(m_label.trainingVoxelsCount()));

  emit trainingVolumeChanged();
}

//----------------------------------------------------------------------------
void LabelWidget::stopDrawing()
{
  m_drawTrainingVolume->setChecked(false);
}

//----------------------------------------------------------------------------
void LabelWidget::onBrushEnabled(bool checked)
{
  if (checked)
  {
    m_viewManager->setEventHandler(m_brush);
  } else
  {
    m_viewManager->unsetEventHandler(m_brush);
  }
}

//----------------------------------------------------------------------------
void LabelWidget::onBrushUseChanged(bool used)
{
  m_drawTrainingVolume->setChecked(used);
}

//----------------------------------------------------------------------------
void LabelWidget::onStroke(Selector::Selection selection)
{
  auto mask = selection.first().first; // m_brush->voxelSelectionMask();

  Bounds    bounds  = mask->bounds().bounds();
  NmVector3 spacing = mask->spacing();

  SparseVolumeSPtr volume{new SparseVolume<itkVolumeType>(bounds, spacing)};
  volume->draw(mask, mask->foregroundValue());

  m_undoStack->beginMacro(tr("Add Training Volume"));
  m_undoStack->push(new DrawCommand(volume, this));
  m_undoStack->endMacro();
}

//----------------------------------------------------------------------------
void LabelWidget::discardTrainingVolumes()
{
  m_label.discardTrainingVolumes();
  m_widget->discardTrainingVolumes();
  m_voxelCount->setText(QString("%1").arg(m_label.trainingVoxelsCount()));

  m_viewManager->updateViews();

  trainingVolumeChanged();
}

//----------------------------------------------------------------------------
void LabelWidget::onLabelRemoved()
{

  emit labelRemoved(m_label);
}

//----------------------------------------------------------------------------
void LabelWidget::onVisibilityChanged(bool visible)
{
  m_widget->setVisibility(visible);

  if (visible) {
    m_showTrainingVolumes->setIcon(QIcon(":/espina/show_all.svg"));
  } else
  {
    m_showTrainingVolumes->setIcon(QIcon(":/espina/hide_all.svg"));
  }

  m_viewManager->updateViews();
}

//----------------------------------------------------------------------------
void LabelWidget::onMinVoxelSizeChanged(int size)
{
  m_label.setMinVoxelSize(size);
}
