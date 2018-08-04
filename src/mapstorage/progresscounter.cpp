/************************************************************************
**
** Authors:   Thomas Equeter <waba@waba.be>
**
** This file is part of the MMapper project.
** Maintained by Nils Schimmelmann <nschimme@gmail.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the:
** Free Software Foundation, Inc.
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************************************************************************/

#include "progresscounter.h"

#include <QObject>

ProgressCounter::ProgressCounter(QObject *parent)
    : QObject(parent)
{}

void ProgressCounter::increaseTotalStepsBy(quint32 steps)
{
    m_totalSteps += steps;
    step(0u);
}

void ProgressCounter::step(const quint32 steps)
{
    m_steps += steps;
    const quint32 percentage = (m_totalSteps == 0u) ? 0u : (100u * m_steps / m_totalSteps);
    if (percentage != m_percentage) {
        m_percentage = percentage;
        emit onPercentageChanged(percentage);
    }
}

void ProgressCounter::reset()
{
    m_totalSteps = m_steps = m_percentage = 0u;
}
