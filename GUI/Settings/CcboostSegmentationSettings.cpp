/********************************************************************************/
// Copyright (c) 2013 Pol Monsó Purtí                                           //
// Ecole Polytechnique Federale de Lausanne                                     //
// Contact <pol.monso@epfl.ch> for comments & bug reports                       //
//                                                                              //
// This program is free software: you can redistribute it and/or modify         //
// it under the terms of the version 3 of the GNU General Public License        //
// as published by the Free Software Foundation.                                //
//                                                                              //
// This program is distributed in the hope that it will be useful, but          //
// WITHOUT ANY WARRANTY; without even the implied warranty of                   //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             //
// General Public License for more details.                                     //
//                                                                              //
// You should have received a copy of the GNU General Public License            //
// along with this program. If not, see <http://www.gnu.org/licenses/>.         //
/********************************************************************************/

// EspINA
#include "CcboostSegmentationSettings.h"
#include <Support/Settings/EspinaSettings.h>

// Qt
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include <QDir>
#include <QDebug>

namespace ESPINA
{
  
  //-----------------------------------------------------------------------------
  CcboostSegmentationSettings::CcboostSegmentationSettings()
  {
    setupUi(this);

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");

    m_settingsConfigData.reset();

    //FIXME hash is per region, but we don't know on load
//    if (settings.contains("Channel Hash"))
//      m_settingsConfigData.featuresRawVolumeImageHash = settings.value("Channel Hash").toString().toStdString();

    if (settings.contains("Number of Stumps"))
      m_settingsConfigData.numStumps = settings.value("Number of Stumps").toInt();
    else
      settings.setValue("Number of Stumps", m_settingsConfigData.numStumps);

    if (settings.contains("Force Recompute Features"))
       m_settingsConfigData.forceRecomputeFeatures = settings.value("Force Recompute Features").toBool();
    else
       settings.setValue("Force Recompute Features", m_settingsConfigData.forceRecomputeFeatures);

    if (settings.contains("Save Intermediate Volumes"))
        m_settingsConfigData.saveIntermediateVolumes = settings.value("Save Intermediate Volumes").toBool();
    else
        settings.setValue("Save Intermediate Volumes", m_settingsConfigData.saveIntermediateVolumes);

    if (settings.contains("Use Preview"))
        m_settingsConfigData.usePreview = settings.value("Use Preview").toBool();
    else
        settings.setValue("Use Preview", m_settingsConfigData.usePreview);

    if (settings.contains("Minimum synapse size (voxels)"))
       m_minCCSize = settings.value("Minimum synapse size (voxels)").toInt();
    else
        settings.setValue("Minimum synapse size (voxels)", m_settingsConfigData.minComponentSize);

    if (settings.contains("Number of objects limit"))
        m_maxNumObjects = settings.value("Number of objects limit").toInt();
    else
        settings.setValue("Number of objects limit", m_settingsConfigData.maxNumObjects);

    if (settings.contains("Features Directory"))
      m_settingsConfigData.cacheDir = settings.value("Features Directory").toString().toStdString();
    else{
        m_settingsConfigData.cacheDir = (QDir("./").absolutePath() + QString("/")).toStdString();
//        m_settingsConfigData.train.orientEstimate = (std::string("hessOrient-s3.5-repolarized.nrrd"));

//        m_settingsConfigData.train.otherFeatures.clear();
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("gradient-magnitude-s1.0.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("gradient-magnitude-s1.6.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("gradient-magnitude-s3.5.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("gradient-magnitude-s5.0.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("stensor-s0.5-r1.0.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("stensor-s0.8-r1.6.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("stensor-s1.8-r3.5.nrrd"));
//        m_settingsConfigData.train.otherFeatures.push_back( std::string("stensor-s2.5-r5.0.nrrd"));
        settings.setValue("Features Directory", QString::fromStdString(m_settingsConfigData.cacheDir));
    }

    settings.sync();

    m_modified = false;
    usePreview->setChecked(m_settingsConfigData.usePreview);
    minCCSize->setValue(m_minCCSize);
    maxNumObj->setValue(m_maxNumObjects);
    numStumps->setValue(m_settingsConfigData.numStumps);
    saveIntermediateVolumes->setChecked(m_settingsConfigData.saveIntermediateVolumes);
    forceRecomputeFeatures->setChecked(m_settingsConfigData.forceRecomputeFeatures);  
    featuresPath->setText(QString::fromStdString(m_settingsConfigData.cacheDir));
    connect(minCCSize,  SIGNAL(valueChanged(int)),
                  this, SLOT(changeMinCCSize(int)));
    connect(maxNumObj,  SIGNAL(valueChanged(int)),
                  this, SLOT(changeMaxNumObjects(int)));
    connect(numStumps,  SIGNAL(valueChanged(int)),
                  this, SLOT(changeNumStumps(int)));
   connect(saveIntermediateVolumes, SIGNAL(toggled(bool)),
                  this, SLOT(changeSaveIntermediateVolumes(bool)));
    connect(forceRecomputeFeatures, SIGNAL(toggled(bool)),
                  this, SLOT(changeForceRecomputeFeatures(bool)));
    connect(featuresPath, SIGNAL(textChanged(QString)),
                  this, SLOT(changeFeaturesPath(QString)));
    connect(chooseFeaturesDirectoryButton, SIGNAL(clicked()),
                  this, SLOT(openDirectoryDialog()));
    connect(usePreview, SIGNAL(toggled(bool)),
                      this, SLOT(changeUsePreview(bool)));
  }

  //-----------------------------------------------------------------------------
  void CcboostSegmentationSettings::changeUsePreview(bool value)
    {
      m_settingsConfigData.usePreview = value;
      m_modified = true;
    }

  void CcboostSegmentationSettings::changeMinCCSize(int value)
  {
    m_minCCSize = value;
    m_modified = true;
  }

  void CcboostSegmentationSettings::changeMaxNumObjects(int value)
    {
      m_maxNumObjects = value;
      m_modified = true;
    }

  void CcboostSegmentationSettings::changeSaveIntermediateVolumes(bool doSave){
    m_settingsConfigData.saveIntermediateVolumes = doSave;
    m_modified = true;
  }

  void CcboostSegmentationSettings::changeForceRecomputeFeatures(bool doForce){
    m_settingsConfigData.forceRecomputeFeatures = doForce;
    m_modified = true;
  }

  void CcboostSegmentationSettings::changeNumStumps(int value)
  {
    m_settingsConfigData.numStumps = value;
    m_modified = true;
  }

  void CcboostSegmentationSettings::changeFeaturesPath(QString path){
    if(!path.endsWith("/"))
        path.append("/");
    m_settingsConfigData.cacheDir = path.toStdString();
    m_modified = true;
  }

  void CcboostSegmentationSettings::openDirectoryDialog(){

      QString path = QFileDialog::getExistingDirectory(this,
                                                       tr("Select directory"), ".", QFileDialog::ShowDirsOnly);
      path.append("/");
      qDebug() << "Selected path " << path;

      m_settingsConfigData.cacheDir = path.toStdString();
      featuresPath->setText(path);
  }

  //-----------------------------------------------------------------------------
  void CcboostSegmentationSettings::acceptChanges()
  {
    if (!m_modified)
      return;

    ESPINA_SETTINGS(settings);
    settings.beginGroup("ccboost segmentation");
    settings.setValue("Minimum synapse size (voxels)", m_minCCSize);
    settings.setValue("Number of Stumps", m_settingsConfigData.numStumps);
    settings.setValue("Features Directory", QString::fromStdString(m_settingsConfigData.cacheDir));
    settings.setValue("Save Intermediate Volumes", m_settingsConfigData.saveIntermediateVolumes);
    settings.setValue("Force Recompute Features", m_settingsConfigData.forceRecomputeFeatures);
    settings.setValue("Use Preview", m_settingsConfigData.usePreview);
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

} /* namespace EspINA */
