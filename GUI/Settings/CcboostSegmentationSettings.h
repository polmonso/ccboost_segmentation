/*
 * CcboostSegmentationSettings.h
 *
 *  Created on: Jan 16, 2013
 *      Author: Felix de las Pozas Alvarez
 */

#ifndef CCBOOSTSEGMENTATIONSETTINGS_H_
#define CCBOOSTSEGMENTATIONSETTINGS_H_

#include "CcboostSegmentationPlugin_Export.h"

// ESPINA
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

    virtual const QString shortDescription() { return tr("Synaptic ccboost segmentation"); }
    virtual const QString longDescription()  { return tr("Synaptic ccboost segmentation"); }
    virtual const QIcon icon()               { return QIcon(":/AppSurface.svg"); }

    virtual void acceptChanges();
    virtual void rejectChanges();
    virtual bool modified() const;

    virtual SettingsPanelPtr clone();

  public slots:
    void changeDefaultComputation(int);

  private:
    bool m_automaticComputation;
    bool m_modified;
  };
} /* namespace ESPINA */

#endif /* CCBOOSTSEGMENTATIONSETTINGS_H_ */
