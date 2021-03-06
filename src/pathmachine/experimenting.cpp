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

#include "experimenting.h"

#include "../expandoracommon/room.h"
#include "../mapdata/roomfactory.h"
#include "path.h"
#include "pathparameters.h"

Experimenting::Experimenting(std::list<Path *> *const pat,
                             const ExitDirection in_dirCode,
                             PathParameters &in_params,
                             AbstractRoomFactory *const in_factory)
    : direction(RoomFactory::exitDir(in_dirCode))
    , dirCode(in_dirCode)
    , shortPaths(pat)
    , paths(new std::list<Path *>)
    , params(in_params)
    , factory(in_factory)
{}

Experimenting::~Experimenting() = default;

void Experimenting::augmentPath(Path *const path, RoomAdmin *const map, const Room *const room)
{
    const Coordinate c = path->getRoom()->getPosition() + direction;
    Path *const working = path->fork(room, c, map, params, this, dirCode);
    if (best == nullptr) {
        best = working;
    } else if (working->getProb() > best->getProb()) {
        paths->push_back(best);
        second = best;
        best = working;
    } else {
        if (second == nullptr || working->getProb() > second->getProb()) {
            second = working;
        }
        paths->push_back(working);
    }
    numPaths++;
}

std::list<Path *> *Experimenting::evaluate()
{
    for (Path *working = nullptr; !shortPaths->empty();) {
        working = shortPaths->front();
        shortPaths->pop_front();
        if (!(working->hasChildren())) {
            working->deny();
        }
    }

    if (best != nullptr) {
        if (second == nullptr || best->getProb() > second->getProb() * params.acceptBestRelative
            || best->getProb() > second->getProb() + params.acceptBestAbsolute) {
            for (auto &path : *paths) {
                path->deny();
            }
            paths->clear();
            paths->push_front(best);
        } else {
            paths->push_back(best);

            for (Path *working = paths->front(); working != best;) {
                paths->pop_front();
                // throw away if the probability is very low or not
                // distinguishable from best. Don't keep paths with equal
                // probability at the front, for we need to find a unique
                // best path eventually.
                if (best->getProb() > working->getProb() * params.maxPaths / numPaths
                    || (best->getProb() <= working->getProb()
                        && best->getRoom() == working->getRoom())) {
                    working->deny();
                } else {
                    paths->push_back(working);
                }
                working = paths->front();
            }
        }
    }
    second = nullptr;
    delete shortPaths;
    shortPaths = nullptr;
    best = nullptr;
    return paths;
}
