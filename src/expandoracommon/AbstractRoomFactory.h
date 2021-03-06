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

#ifndef ABSTRACTROOMFACTORY_H
#define ABSTRACTROOMFACTORY_H

#include "../mapdata/mmapper2exit.h"
#include "parseevent.h"
#include <QtGlobal>

class Room;
class Coordinate;

enum class ComparisonResult { DIFFERENT = 0, EQUAL, TOLERANCE };

class AbstractRoomFactory
{
public:
    virtual ~AbstractRoomFactory();
    virtual Room *createRoom() const = 0;
    virtual Room *createRoom(const ParseEvent &event) const = 0;
    virtual ComparisonResult compare(const Room *,
                                     const ParseEvent &props,
                                     int tolerance = 0) const = 0;
    virtual ComparisonResult compareWeakProps(const Room *,
                                              const ParseEvent &props,
                                              int tolerance = 0) const = 0;
    virtual SharedParseEvent getEvent(const Room *) const = 0;
    virtual void update(Room &, const ParseEvent &event) const = 0;
    virtual void update(Room *target, const Room *source) const = 0;
};

#endif
