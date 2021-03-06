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

#include "roomfactory.h"

#include <QString>

#include "../expandoracommon/AbstractRoomFactory.h"
#include "../expandoracommon/coordinate.h"
#include "../expandoracommon/exit.h"
#include "../expandoracommon/parseevent.h"
#include "../expandoracommon/room.h"
#include "../global/EnumIndexedArray.h"
#include "../global/Flags.h"
#include "../global/StringView.h"
#include "../parser/CommandId.h"
#include "../parser/ConnectedRoomFlags.h"
#include "../parser/ExitsFlags.h"
#include "../parser/PromptFlags.h"
#include "DoorFlags.h"
#include "ExitDirection.h"
#include "ExitFlags.h"
#include "mmapper2room.h"

Room *RoomFactory::createRoom() const
{
    return new Room(Room::tagValid);
}

Room *RoomFactory::createRoom(const ParseEvent &ev) const
{
    if (auto *const room = createRoom()) {
        update(*room, ev);
        return room;
    }

    /* This will never happen, because createRoom would throw std::bad_alloc(). */
    return nullptr;
}

SharedParseEvent RoomFactory::getEvent(const Room *const room) const
{
    ExitsFlagsType exitFlags{};
    for (auto dir : ALL_EXITS_NESWUD) {
        const Exit &e = room->exit(dir);
        const ExitFlags eFlags = e.getExitFlags();
        exitFlags.set(dir, eFlags);
    }
    exitFlags.setValid();

    return ParseEvent::createEvent(CommandIdType::UNKNOWN,
                                   room->getName(),
                                   room->getDynamicDescription(),
                                   room->getStaticDescription(),
                                   exitFlags,
                                   PromptFlagsType::fromRoomTerrainType(room->getTerrainType()),
                                   ConnectedRoomFlagsType{});
}

static int wordDifference(StringView a, StringView b)
{
    int diff = 0;
    while (!a.isEmpty() && !b.isEmpty())
        if (a.takeFirstLetter() != b.takeFirstLetter())
            ++diff;
    return diff + a.size() + b.size();
}

ComparisonResult RoomFactory::compareStrings(const QString &room,
                                             const QString &event,
                                             int prevTolerance,
                                             const bool updated)
{
    assert(prevTolerance >= 0);
    prevTolerance = std::max(0, prevTolerance);
    prevTolerance *= room.size();
    prevTolerance /= 100;
    int tolerance = prevTolerance;

    auto descWords = StringView{room}.trim();
    auto eventWords = StringView{event}.trim();

    if (!eventWords.isEmpty()) { // if event is empty we don't compare
        while (tolerance >= 0) {
            if (descWords.isEmpty()) {
                if (updated) { // if notUpdated the desc is allowed to be shorter than the event
                    tolerance -= eventWords.countNonSpaceChars();
                }
                break;
            }
            if (eventWords.isEmpty()) { // if we get here the event isn't empty
                tolerance -= descWords.countNonSpaceChars();
                break;
            }

            tolerance -= wordDifference(eventWords.takeFirstWord(), descWords.takeFirstWord());
        }
    }

    if (tolerance < 0) {
        return ComparisonResult::DIFFERENT;
    } else if (static_cast<int>(prevTolerance) != tolerance) {
        return ComparisonResult::TOLERANCE;
    } else if (event.size() != room.size()) { // differences in amount of whitespace
        return ComparisonResult::TOLERANCE;
    }
    return ComparisonResult::EQUAL;
}

ComparisonResult RoomFactory::compare(const Room *const room,
                                      const ParseEvent &event,
                                      const int tolerance) const
{
    const QString name = room->getName();
    const QString staticDesc = room->getStaticDescription();
    const RoomTerrainType terrainType = room->getTerrainType();
    bool updated = room->isUpToDate();

    //    if (event == nullptr) {
    //        return ComparisonResult::EQUAL;
    //    }

    if (name.isEmpty() && staticDesc.isEmpty() && (!updated)) {
        // user-created
        return ComparisonResult::TOLERANCE;
    }

    const PromptFlagsType pf = event.getPromptFlags();
    if (pf.isValid()) {
        if (pf.getTerrainType() != terrainType) {
            if (room->isUpToDate()) {
                return ComparisonResult::DIFFERENT;
            }
        }
    }

    switch (compareStrings(name, event.getRoomName(), tolerance)) {
    case ComparisonResult::TOLERANCE:
        updated = false;
        break;
    case ComparisonResult::DIFFERENT:
        return ComparisonResult::DIFFERENT;
    case ComparisonResult::EQUAL:
        break;
    }

    switch (compareStrings(staticDesc, event.getStaticDesc(), tolerance, updated)) {
    case ComparisonResult::TOLERANCE:
        updated = false;
        break;
    case ComparisonResult::DIFFERENT:
        return ComparisonResult::DIFFERENT;
    case ComparisonResult::EQUAL:
        break;
    }

    switch (compareWeakProps(room, event, 0)) {
    case ComparisonResult::DIFFERENT:
        return ComparisonResult::DIFFERENT;
    case ComparisonResult::TOLERANCE:
        updated = false;
        break;
    case ComparisonResult::EQUAL:
        break;
    }

    if (updated) {
        return ComparisonResult::EQUAL;
    }
    return ComparisonResult::TOLERANCE;
}

ComparisonResult RoomFactory::compareWeakProps(const Room *const room,
                                               const ParseEvent &event,
                                               int /*tolerance*/) const
{
    bool exitsValid = room->isUpToDate();
    // REVISIT: Should tolerance be an integer given known 'weak' params like hidden
    // exits or undefined flags?
    bool tolerance = false;

    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags();
    const PromptFlagsType pFlags = event.getPromptFlags();
    if (pFlags.isValid()) {
        const RoomLightType lightType = room->getLightType();
        const RoomSundeathType sunType = room->getSundeathType();
        if (pFlags.isLit() && lightType != RoomLightType::LIT
            && sunType == RoomSundeathType::NO_SUNDEATH) {
            // Allow prompt sunlight to override rooms without LIT flag if we know the room
            // is troll safe and obviously not in permanent darkness
            qDebug() << "Updating room to be LIT";
            tolerance = true;

        } else if (pFlags.isDark() && lightType != RoomLightType::DARK
                   && sunType == RoomSundeathType::NO_SUNDEATH && connectedRoomFlags.isValid()
                   && connectedRoomFlags.hasAnyDirectSunlight()) {
            // Allow prompt sunlight to override rooms without DARK flag if we know the room
            // has at least one sunlit exit and the room is troll safe
            qDebug() << "Updating room to be DARK";
            tolerance = true;
        }
    }

    const ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    if (eventExitsFlags.isValid()) {
        bool previousDifference = false;
        for (auto dir : ALL_EXITS_NESWUD) {
            const Exit &roomExit = room->exit(dir);
            const ExitFlags roomExitFlags = roomExit.getExitFlags();
            if (roomExitFlags) {
                // exits are considered valid as soon as one exit is found (or if the room is updated)
                exitsValid = true;
                if (previousDifference) {
                    return ComparisonResult::DIFFERENT;
                }
            }
            if (roomExitFlags.isNoMatch()) {
                continue;
            }
            const auto eventExitFlags = eventExitsFlags.get(dir);
            const auto diff = eventExitFlags ^ roomExitFlags;
            if (diff.isExit() || diff.isDoor()) {
                if (!exitsValid) {
                    // Room was not isUpToDate or the exits/doors do not match
                    previousDifference = true;
                } else {
                    if (tolerance) {
                        // Do not be tolerant for multiple differences
                        qDebug() << "Found too many differences" << event;
                        return ComparisonResult::DIFFERENT;

                    } else if (!roomExitFlags.isExit() && eventExitFlags.isDoor()) {
                        // No exit exists on the map so we probably found a secret door
                        qDebug() << "Secret door likely found to the" << lowercaseDirection(dir)
                                 << event;
                        tolerance = true;

                    } else if (roomExit.isHiddenExit() && !eventExitFlags.isDoor()) {
                        qDebug() << "Secret exit hidden to the" << lowercaseDirection(dir);

                    } else {
                        qWarning() << "Unknown exit/door tolerance condition to the"
                                   << lowercaseDirection(dir) << event;
                        return ComparisonResult::DIFFERENT;
                    }
                }
            } else if (diff.isRoad()) {
                if (roomExitFlags.isRoad() && connectedRoomFlags.isValid()
                    && connectedRoomFlags.hasDirectionalSunlight(static_cast<DirectionType>(dir))) {
                    // Orcs/trolls can only see trails/roads if it is dark (but can see climbs)
                    qDebug() << "Orc/troll could not see trail to the" << lowercaseDirection(dir);

                } else if (roomExitFlags.isRoad() && !eventExitFlags.isRoad()
                           && roomExitFlags.isDoor() && eventExitFlags.isDoor()) {
                    // A closed door is hiding the road that we know is there
                    qDebug() << "Closed door masking road/trail to the" << lowercaseDirection(dir);

                } else if (!roomExitFlags.isRoad() && eventExitFlags.isRoad()
                           && roomExitFlags.isDoor() && eventExitFlags.isDoor()) {
                    // A known door was previously mapped closed and a new road exit flag was found
                    qDebug() << "Previously closed door was hiding road to the"
                             << lowercaseDirection(dir);
                    tolerance = true;

                } else {
                    qWarning() << "Unknown road tolerance condition to the"
                               << lowercaseDirection(dir) << event;
                    tolerance = true;
                }
            } else if (diff.isClimb()) {
                if (roomExitFlags.isClimb() && !eventExitFlags.isClimb() && roomExitFlags.isDoor()
                    && eventExitFlags.isDoor()) {
                    // A closed door is hiding the climb that we know is there
                    qDebug() << "Closed door masking climb to the" << lowercaseDirection(dir);

                } else if (!roomExitFlags.isClimb() && eventExitFlags.isClimb()
                           && roomExitFlags.isDoor() && eventExitFlags.isDoor()) {
                    // A known door was previously mapped closed and a new climb exit flag was found
                    qDebug() << "Previously closed door was hiding climb to the"
                             << lowercaseDirection(dir);
                    tolerance = true;

                } else {
                    qWarning() << "Unknown climb tolerance condition to the"
                               << lowercaseDirection(dir) << event;
                    tolerance = true;
                }
            }
        }
    }
    if (tolerance || !exitsValid) {
        return ComparisonResult::TOLERANCE;
    }
    return ComparisonResult::EQUAL;
}

void RoomFactory::update(Room &room, const ParseEvent &event) const
{
    room.setDynamicDescription(event.getDynamicDesc());

    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags();
    ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    if (eventExitsFlags.isValid()) {
        eventExitsFlags.removeValid();
        for (const auto dir : ALL_EXITS_NESWUD) {
            ExitFlags eventExitFlags = eventExitsFlags.get(dir);
            Exit &roomExit = room.exit(dir);

            if (!room.isUpToDate()) {
                if (roomExit.isDoor() && !eventExitFlags.isDoor()) {
                    // Prevent room hidden exits from being overridden
                    eventExitFlags |= ExitFlag::DOOR | ExitFlag::EXIT;
                }
                if (roomExit.exitIsRoad() && !eventExitFlags.isRoad()
                    && connectedRoomFlags.isValid()
                    && connectedRoomFlags.hasDirectionalSunlight(static_cast<DirectionType>(dir))) {
                    // Prevent orcs/trolls from removing roads/trails if they're sunlit
                    eventExitFlags |= ExitFlag::ROAD;
                }
                // Replace exits if target room is not up to date
                roomExit.setExitFlags(eventExitFlags);

            } else {
                // Update exits if target room is up to date
                roomExit.updateExit(eventExitFlags);
            }
        }
        room.setUpToDate();
    } else {
        room.setOutDated();
    }

    const PromptFlagsType pFlags = event.getPromptFlags();
    if (pFlags.isValid()) {
        room.setTerrainType(pFlags.getTerrainType());
        const RoomSundeathType sunType = room.getSundeathType();
        if (pFlags.isLit() && sunType == RoomSundeathType::NO_SUNDEATH) {
            room.setLightType(RoomLightType::LIT);
        } else if (pFlags.isDark() && sunType == RoomSundeathType::NO_SUNDEATH
                   && connectedRoomFlags.isValid() && connectedRoomFlags.hasAnyDirectSunlight()) {
            room.setLightType(RoomLightType::DARK);
        }
    } else {
        room.setOutDated();
    }

    const QString &desc = event.getStaticDesc();
    if (!desc.isEmpty()) {
        room.setStaticDescription(desc);
    } else {
        room.setOutDated();
    }

    const QString &name = event.getRoomName();
    if (!name.isEmpty()) {
        room.setName(name);
    } else {
        room.setOutDated();
    }
}

void RoomFactory::update(Room *const target, const Room *const source) const
{
    const QString name = source->getName();
    if (!name.isEmpty()) {
        target->setName(name);
    }
    const QString desc = source->getStaticDescription();
    if (!desc.isEmpty()) {
        target->setStaticDescription(desc);
    }
    const QString dynamic = source->getDynamicDescription();
    if (!dynamic.isEmpty()) {
        target->setDynamicDescription(dynamic);
    }

    if (target->getAlignType() == RoomAlignType::UNDEFINED) {
        target->setAlignType(source->getAlignType());
    }
    if (target->getLightType() == RoomLightType::UNDEFINED) {
        target->setLightType(source->getLightType());
    }
    if (target->getSundeathType() == RoomSundeathType::UNDEFINED) {
        target->setSundeathType(source->getSundeathType());
    }
    if (target->getPortableType() == RoomPortableType::UNDEFINED) {
        target->setPortableType(source->getPortableType());
    }
    if (target->getRidableType() == RoomRidableType::UNDEFINED) {
        target->setRidableType(source->getRidableType());
    }
    if (source->getTerrainType() != RoomTerrainType::UNDEFINED) {
        target->setTerrainType(source->getTerrainType());
    }

    /* REVISIT: why are these append operations, while the others replace? */
    target->setNote(target->getNote().append(source->getNote()));
    target->setMobFlags(target->getMobFlags() | source->getMobFlags());
    target->setLoadFlags(target->getLoadFlags() | source->getLoadFlags());

    if (!target->isUpToDate()) {
        // Replace data if target room is not up to date
        for (const auto dir : ALL_EXITS_NESWUD) {
            const Exit &sourceExit = source->exit(dir);
            Exit &targetExit = target->exit(dir);
            ExitFlags sourceExitFlags = sourceExit.getExitFlags();
            if (targetExit.isDoor()) {
                if (!sourceExitFlags.isDoor()) {
                    // Prevent target hidden exits from being overridden
                    sourceExitFlags |= ExitFlag::DOOR | ExitFlag::EXIT;
                } else {
                    targetExit.setDoorName(sourceExit.getDoorName());
                    targetExit.setDoorFlags(sourceExit.getDoorFlags());
                }
            }
            targetExit.setExitFlags(sourceExitFlags);
        }
    } else {
        // Combine data if target room is up to date
        for (const auto dir : ALL_EXITS_NESWUD) {
            const Exit &soureExit = source->exit(dir);
            Exit &targetExit = target->exit(dir);
            const ExitFlags sourceExitFlags = soureExit.getExitFlags();
            const ExitFlags targetExitFlags = targetExit.getExitFlags();
            if (targetExitFlags != sourceExitFlags) {
                targetExit.setExitFlags(targetExitFlags | sourceExitFlags);
            }
            const QString &sourceDoorName = soureExit.getDoorName();
            if (!sourceDoorName.isEmpty()) {
                targetExit.setDoorName(sourceDoorName);
            }
            const DoorFlags doorFlags = soureExit.getDoorFlags() | targetExit.getDoorFlags();
            targetExit.setDoorFlags(doorFlags);
        }
    }
    if (source->isUpToDate()) {
        target->setUpToDate();
    }
}

using ExitCoordinates = EnumIndexedArray<Coordinate, ExitDirection, NUM_EXITS_INCLUDING_NONE>;
static ExitCoordinates initExitCoordinates()
{
    // CAUTION: This choice of coordinate system will probably
    // come back to bite us if we ever try to go 3d.
    const Coordinate north(0, -1, 0);
    const Coordinate south(0, 1, 0); /* South is increasing Y */
    const Coordinate east(1, 0, 0);
    const Coordinate west(-1, 0, 0);
    const Coordinate up(0, 0, 1);
    const Coordinate down(0, 0, -1);

    ExitCoordinates exitDirs{};
    exitDirs[ExitDirection::NORTH] = north;
    exitDirs[ExitDirection::SOUTH] = south;
    exitDirs[ExitDirection::EAST] = east;
    exitDirs[ExitDirection::WEST] = west;
    exitDirs[ExitDirection::UP] = up;
    exitDirs[ExitDirection::DOWN] = down;
    return exitDirs;
}

/* TODO: move this to another namespace */
const Coordinate &RoomFactory::exitDir(ExitDirection dir)
{
    static const auto exitDirs = initExitCoordinates();
    return exitDirs[dir];
}

RoomFactory::RoomFactory() = default;
