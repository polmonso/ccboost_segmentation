/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2014  <copyright holder> <email>
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

#ifndef ESPINA_RAS_TRAINER_TASK_H
#define ESPINA_RAS_TRAINER_TASK_H

#include <Core/MultiTasking/Task.h>
#include <GUI/Model/ChannelAdapter.h>


namespace ESPINA {
  namespace CCB {

    class CcboostTask
    : public Task
    {
    public:
        explicit CcboostTask(ChannelAdapterPtr channel,
                             SchedulerSPtr     scheduler);

      ChannelAdapterPtr channel() const
      { return m_channel; }

    protected:
      virtual void run();

    private:
      int numberOfLabels() const;

    private:
      ChannelAdapterPtr m_channel;

      bool m_abort;
    };
    using CcboostTaskPtr  = CcboostTask *;
    using CcboostTaskSPtr = std::shared_ptr<CcboostTask>;

  } // namespace CCB
} // namespace ESPINA

Q_DECLARE_METATYPE(ESPINA::CCB::CcboostTaskSPtr);
#endif // ESPINA_RAS_TRAINER_TASK_H
