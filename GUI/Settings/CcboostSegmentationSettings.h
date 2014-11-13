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

#ifndef CCBOOSTSEGMENTATIONSETTINGS_H_
#define CCBOOSTSEGMENTATIONSETTINGS_H_

#include "CcboostSegmentationPlugin_Export.h"

#include "Filter/ConfigData.h"

// EspINA
#include <Support/Settings/SettingsPanel.h>
#include "ui_CcboostSegmentationSettings.h"

// Qt
#include <QColor>

namespace ESPINA
{
  class CcboostSegmentationPlugin_EXPORT CcboostSegmentationSettings
  : public SettingsPanel
  , public Ui::CcboostSegmentationSettings
  {
    Q_OBJECT
  public:
    explicit CcboostSegmentationSettings();
    virtual ~CcboostSegmentationSettings() {};

    virtual const QString shortDescription() { return tr("Synapse Detection"); }
    virtual const QString longDescription()  { return tr("Synapse Detection with ccboost"); }
    virtual const QIcon icon()               { return QIcon(":/CcboostSegmentation.svg"); }

    virtual void acceptChanges();
    virtual void rejectChanges();
    virtual bool modified() const;
    virtual SettingsPanelPtr clone();

  public slots:
    void changeMinCCSize(int);
    void changeMaxNumObjects(int value);
    void changeSaveIntermediateVolumes(bool doSave);
    void changeForceRecomputeFeatures(bool doForce);
    void changeNumStumps(int);
    void changeSVoxSeed(int);
    void changeSVoxCubeness(int);
    void changeTPQuantile(double);
    void changeFPQuantile(double);
    void changeFeaturesPath(QString path);
    void changeSynapseFeaturesPath(QString);
    void openDirectoryDialog();
    void changeAutomaticComputation(bool);

  private:
    int m_minCCSize;
    int m_maxNumObjects;
    ConfigData<itkVolumeType> m_settingsConfigData;
    bool m_modified;
  };
} /* namespace EspINA */

#endif /* SYNAPSEDETECTIONSETTINGS_H_ */
