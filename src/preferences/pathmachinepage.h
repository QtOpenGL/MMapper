#pragma once
/************************************************************************
**
** Authors:   Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve),
**            Marek Krejza <krejza@gmail.com> (Caligor)
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

#ifndef PATHMACHINEPAGE_H
#define PATHMACHINEPAGE_H

#include <QString>
#include <QWidget>
#include <QtCore>

#include "ui_pathmachinepage.h"

class QObject;

class PathmachinePage : public QWidget, private Ui::PathmachinePage
{
    Q_OBJECT

public:
    explicit PathmachinePage(QWidget *parent = nullptr);

public slots:
    void acceptBestRelativeDoubleSpinBoxValueChanged(double);
    void acceptBestAbsoluteDoubleSpinBoxValueChanged(double);
    void newRoomPenaltyDoubleSpinBoxValueChanged(double);
    void correctPositionBonusDoubleSpinBoxValueChanged(double);
    void multipleConnectionsPenaltyDoubleSpinBoxValueChanged(double);
    void maxPathsValueChanged(int);
    void matchingToleranceSpinBoxValueChanged(int);
};

#endif
