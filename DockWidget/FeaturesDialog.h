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

#ifndef ESPINA_RAS_FEATURES_DIALOG_H
#define ESPINA_RAS_FEATURES_DIALOG_H

// ESPINA
#include "ui_FeaturesDialog.h"

// Qt
#include <QDialog>
#include <GUI/Model/ModelAdapter.h>
#include <GUI/Model/Proxies/ChannelProxy.h>

namespace ESPINA
{
  namespace RAS
  {
    class FeaturesDialog
    : public QDialog
    , private Ui::FeaturesDialog
    {
      Q_OBJECT

    public:
      enum FeaturesType { LAST_USED, EXTRACT, LOAD };

    public:
      FeaturesDialog(ModelAdapterSPtr model,
                     QWidget         *parent=0);

      virtual ~FeaturesDialog() {};

      ChannelAdapterPtr channel()
      { return m_channel; }

      FeaturesType features() const;

      unsigned int baseScale() const;

      unsigned int numberOfScales() const;

      float samplingArea() const;

      unsigned int samplingFactor() const;

    private slots:
      void channelSelected();

      void radioChanged();

    private:
      void enableLastUsedFeatures(bool value);

      void enableExtractFeaturesSettings(bool value);

    private:
      QDir m_featuresDir;

      std::shared_ptr<ChannelProxy> m_proxy;

      QModelIndex       m_channelIndex;
      ChannelAdapterPtr m_channel;
    };

  } // namespace RAS
} // namespace ESPINA

#endif // ESPINA_RAS_FEATURES_DIALOG_H
