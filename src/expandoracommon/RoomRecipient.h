#pragma once
/************************************************************************
**
** Authors:   Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve),
**            Marek Krejza <krejza@gmail.com> (Caligor),
**            Nils Schimmelmann <nschimme@gmail.com> (Jahara)
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

#ifndef ROOMRECIPIENT_H
#define ROOMRECIPIENT_H

class Room;
class RoomAdmin;

/*! \brief Interface giving briefly access to a mutex-protected room.
 *
 * See MapFrontend::lookingForRooms().
 */
class RoomRecipient
{
public:
    explicit RoomRecipient();
    virtual ~RoomRecipient();
    virtual void receiveRoom(RoomAdmin *admin, const Room *room) = 0;

public:
    RoomRecipient(RoomRecipient &&) = delete;
    RoomRecipient(const RoomRecipient &) = delete;
    RoomRecipient &operator=(RoomRecipient &&) = delete;
    RoomRecipient &operator=(const RoomRecipient &) = delete;
};

#endif
