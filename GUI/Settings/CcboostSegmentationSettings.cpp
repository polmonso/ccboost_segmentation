/*
 * CcboostSegmentationSettings.cpp
 *
 *  Created on: Jan 16, 2013
 *      Author: Felix de las Pozas Alvarez
 */

// ESPINA
#include "CcboostSegmentationSettings.h"
#include <Support/Settings/EspinaSettings.h>

// Qt
#include <QSettings>
#include <QColorDialog>

namespace ESPINA
{
  
  //-----------------------------------------------------------------------------
  CcboostSegmentationSettings::CcboostSegmentationSettings()
  {
    setupUi(this);

    QSettings settings("CeSViMa", "ESPINA");
    settings.beginGroup("ccboost segmentation");

    if (settings.contains("Automatic Computation For Synapses"))
      m_automaticComputation = settings.value("Automatic Computation For Synapses").toBool();
    else
    {
      m_automaticComputation = false;
      settings.setValue("Automatic Computation For Synapses", m_automaticComputation);
    }
    settings.sync();

    m_modified = false;
    defaultComputation->setChecked(m_automaticComputation);
    connect(defaultComputation, SIGNAL(stateChanged(int)),
            this, SLOT(changeDefaultComputation(int)));
  }

  //-----------------------------------------------------------------------------
  void CcboostSegmentationSettings::changeDefaultComputation(int value)
  {
    m_automaticComputation = (Qt::Checked == value ? true : false);
    m_modified = true;
  }

  //-----------------------------------------------------------------------------
  void CcboostSegmentationSettings::acceptChanges()
  {
    if (!m_modified)
      return;

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    settings.setValue("Automatic Computation For Synapses", m_automaticComputation);
    settings.sync();
  }

  //-----------------------------------------------------------------------------
  void CcboostSegmentationSettings::rejectChanges()
  {
  }

  //-----------------------------------------------------------------------------
  bool CcboostSegmentationSettings::modified() const
  {
    return m_modified;
  }

  //-----------------------------------------------------------------------------
  SettingsPanelPtr CcboostSegmentationSettings::clone()
  {
    return new CcboostSegmentationSettings();
  }

} /* namespace ESPINA */
