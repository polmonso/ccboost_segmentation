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

#include "CcboostTask.h"

#include <QElapsedTimer>
#include <omp.h>

using namespace ESPINA;
using namespace CCB;

//------------------------------------------------------------------------
CcboostTask::CcboostTask(ChannelAdapterPtr channel,
                         SchedulerSPtr     scheduler)
: Task(scheduler)
, m_channel(channel)
, m_abort{false}
{
}

//------------------------------------------------------------------------
void CcboostTask::run()
{
  QElapsedTimer timer;
  timer.start();


  // Determine the number of threads to use.
  const int num_threads = omp_get_num_procs() - 1;
  qDebug() << "Number of threads: " << num_threads;



  qDebug() << "Ccboost Segment Time: " << timer.elapsed();
  if (canExecute())
  {
    emit progress(100);
    qDebug() << "AutoSegment Finished";
  }
}
