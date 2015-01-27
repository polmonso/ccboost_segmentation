/*
 *    <one line to give the program's name and a brief idea of what it does.>
 *    Copyright (C) 2014  Jorge Peña Pastor <jpena@cesvima.upm.es>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// ESPINA
#include "FeaturesDialog.h"
#include <Settings/FeatureExtractorSettings.h>
#include <AutoSegmentManager.h>

#include <GUI/Model/ModelAdapter.h>
#include <Extensions/ExtensionUtils.h>
#include <Extensions/EdgeDistances/ChannelEdges.h>

// Qt
#include <QDialog>
#include <QRadioButton>

using namespace ESPINA;
using namespace ESPINA::RAS;

//-----------------------------------------------------------------------------
FeaturesDialog::FeaturesDialog(ModelAdapterSPtr         model,
                               QWidget                  *parent)
: QDialog(parent)
, m_proxy(new ChannelProxy(model))
, m_channel(nullptr)
{
  setupUi(this);

  m_featuresDir = FeatureExtractorSettings::feauresPath();

  if (!m_featuresDir.exists()) {
    QDir::root().mkpath(m_featuresDir.absolutePath());
  }

  m_channelSelector->setModel(m_proxy.get());

  connect(m_channelSelector,  SIGNAL(activated(QModelIndex)),
          this,               SLOT(channelSelected()));

  connect(m_channelSelector,  SIGNAL(activated(int)),
          this,               SLOT(channelSelected()));

  connect(m_lastUsedFeatures, SIGNAL(toggled(bool)),
          this,               SLOT(radioChanged()));

  connect(m_extractFeatures,  SIGNAL(toggled(bool)),
          this,               SLOT(radioChanged()));

  connect(m_loadFeatures,     SIGNAL(toggled(bool)),
          this,               SLOT(radioChanged()));

  m_channelIndex = m_proxy->index(0, 0);
  m_channelIndex = m_channelIndex.child(0,0);

  m_channelSelector->setCurrentModelIndex(m_channelIndex);
}

//------------------------------------------------------------------------
FeaturesDialog::FeaturesType FeaturesDialog::features() const
{
  FeaturesType result = LAST_USED;

  if (m_extractFeatures->isChecked())
  {
    result = EXTRACT;
  } else if (m_loadFeatures->isChecked())
  {
    result = LOAD;
  }

  return result;
}
//------------------------------------------------------------------------
unsigned int FeaturesDialog::baseScale() const
{
  return m_baseScale->value();
}


//------------------------------------------------------------------------
unsigned int FeaturesDialog::numberOfScales() const
{
  return m_numberScales->value();
}

//------------------------------------------------------------------------
float FeaturesDialog::samplingArea() const
{
  return m_samplingArea->value()/100.0;
}

//------------------------------------------------------------------------
unsigned int FeaturesDialog::samplingFactor() const
{
  return m_samplingFactor->value();
}

//------------------------------------------------------------------------
void FeaturesDialog::channelSelected()
{
  auto currentIndex = m_channelSelector->currentModelIndex();

  auto item = itemAdapter(currentIndex);

  if (!item || isSample(item))
  {
    currentIndex = currentIndex.child(0, 0);
    item = itemAdapter(currentIndex);
  }

  if (item && isChannel(item))
  {
    m_channelIndex = currentIndex;

    m_channel = channelPtr(item);
  }

  QFileInfo features = AutoSegmentManager::featuresFile(m_channel);
  m_lastUsedFeaturesPath->setText(features.absoluteFilePath());

  if (features.exists())
  {
    m_lastUsedFeaturesPathStatus->setText(tr("Found:"));
  }
  else
  {
    m_lastUsedFeaturesPathStatus->setText(tr("Not Found:"));
  }
  enableLastUsedFeatures(features.exists());
}

//------------------------------------------------------------------------
void FeaturesDialog::radioChanged()
{
  bool extractFeatures = sender() == m_extractFeatures;

  enableExtractFeaturesSettings(extractFeatures);
}

//------------------------------------------------------------------------
void FeaturesDialog::enableLastUsedFeatures(bool value)
{
  m_lastUsedFeatures          ->setChecked(value);
  m_lastUsedFeatures          ->setEnabled(value);
  m_lastUsedFeaturesMessage   ->setEnabled(value);
  m_lastUsedFeaturesPathStatus->setEnabled(value);
  m_lastUsedFeaturesPath      ->setEnabled(value);

  m_extractFeatures           ->setChecked(!value);
}

//------------------------------------------------------------------------
void FeaturesDialog::enableExtractFeaturesSettings(bool value)
{
  m_baseScale          ->setEnabled(value);
  m_baseScaleLabel     ->setEnabled(value);
  m_numberScales       ->setEnabled(value);
  m_numberScalesLabel  ->setEnabled(value);
  m_samplingFactor     ->setEnabled(value);
  m_samplingFactorLabel->setEnabled(value);
  m_samplingArea       ->setEnabled(value);
  m_samplingAreaLabel  ->setEnabled(value);
}
