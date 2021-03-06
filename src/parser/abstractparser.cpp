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

#include "abstractparser.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <QDesktopServices>
#include <QMessageLogContext>
#include <QObject>
#include <QVariant>
#include <QtCore>

#include "../clock/mumeclock.h"
#include "../clock/mumemoment.h"
#include "../configuration/configuration.h"
#include "../expandoracommon/coordinate.h"
#include "../expandoracommon/exit.h"
#include "../expandoracommon/parseevent.h"
#include "../expandoracommon/room.h"
#include "../global/CharBuffer.h"
#include "../global/DirectionType.h"
#include "../global/StringView.h"
#include "../global/utils.h"
#include "../mapdata/DoorFlags.h"
#include "../mapdata/ExitDirection.h"
#include "../mapdata/ExitFieldVariant.h"
#include "../mapdata/ExitFlags.h"
#include "../mapdata/enums.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/mmapper2room.h"
#include "../mapdata/roomfactory.h"
#include "../mapdata/roomfilter.h"
#include "../mapdata/roomselection.h"
#include "../mapdata/shortestpath.h"
#include "../proxy/telnetfilter.h"
#include "Abbrev.h"
#include "AbstractParser-Commands.h"
#include "AbstractParser-Utils.h"
#include "CommandId.h"
#include "ConnectedRoomFlags.h"
#include "DoorAction.h"
#include "ExitsFlags.h"
#include "PromptFlags.h"
#include "parserutils.h"

class RoomAdmin;

const QString AbstractParser::nullString{};
const QString AbstractParser::emptyString("");
const QByteArray AbstractParser::emptyByteArray("");

static char getTerrainSymbol(const RoomTerrainType type)
{
    switch (type) {
    case RoomTerrainType::UNDEFINED:
        return ' ';
    case RoomTerrainType::INDOORS:
        return '['; // [  // indoors
    case RoomTerrainType::CITY:
        return '#'; // #  // city
    case RoomTerrainType::FIELD:
        return '.'; // .  // field
    case RoomTerrainType::FOREST:
        return 'f'; // f  // forest
    case RoomTerrainType::HILLS:
        return '('; // (  // hills
    case RoomTerrainType::MOUNTAINS:
        return '<'; // <  // mountains
    case RoomTerrainType::SHALLOW:
        return '%'; // %  // shallow
    case RoomTerrainType::WATER:
        return '~'; // ~  // water
    case RoomTerrainType::RAPIDS:
        return 'W'; // W  // rapids
    case RoomTerrainType::UNDERWATER:
        return 'U'; // U  // underwater
    case RoomTerrainType::ROAD:
        return '+'; // +  // road
    case RoomTerrainType::TUNNEL:
        return '='; // =  // tunnel
    case RoomTerrainType::CAVERN:
        return 'O'; // O  // cavern
    case RoomTerrainType::BRUSH:
        return ':'; // :  // brush
    case RoomTerrainType::DEATHTRAP:
        return 'X';
    };

    return ' ';
}

static char getLightSymbol(const RoomLightType lightType)
{
    switch (lightType) {
    case RoomLightType::DARK:
        return 'o';
    case RoomLightType::LIT:
    case RoomLightType::UNDEFINED:
        return '*';
    }

    return '?';
}

static const char *getFlagName(const ExitFlag flag)
{
#define CASE(UPPER, s) \
    do { \
    case ExitFlag::UPPER: \
        return s; \
    } while (false)
    switch (flag) {
        CASE(EXIT, "Possible");
        CASE(DOOR, "Door");
        CASE(ROAD, "Road");
        CASE(CLIMB, "Climbable");
        CASE(RANDOM, "Random");
        CASE(SPECIAL, "Special");
        CASE(NO_MATCH, "No match");
        CASE(FLOW, "Water flow");
        CASE(NO_FLEE, "No flee");
        CASE(DAMAGE, "Damage");
        CASE(FALL, "Fall");
        CASE(GUARDED, "Guarded");
    }
    return "Unknown";
#undef CASE
}

static const char *getFlagName(const DoorFlag flag)
{
#define CASE(UPPER, s) \
    do { \
    case DoorFlag::UPPER: \
        return s; \
    } while (false)
    switch (flag) {
        CASE(HIDDEN, "Hidden");
        CASE(NEED_KEY, "Need key");
        CASE(NO_BLOCK, "No block");
        CASE(NO_BREAK, "No break");
        CASE(NO_PICK, "No pick");
        CASE(DELAYED, "Delayed");
        CASE(CALLABLE, "Callable");
        CASE(KNOCKABLE, "Knockable");
        CASE(MAGIC, "Magic");
        CASE(ACTION, "Action");
        CASE(NO_BASH, "No bash");
    }
    return "Unknown";
#undef CASE
}

AbstractParser::AbstractParser(MapData *md, MumeClock *mc, QObject *parent)
    : QObject(parent)
    , m_mumeClock(mc)
    , m_mapData(md)
    , m_offlineCommandTimer(this)
{
    connect(&m_offlineCommandTimer, &QTimer::timeout, this, &AbstractParser::doOfflineCharacterMove);
    m_offlineCommandTimer.setInterval(250); // MUME enforces 4 commands per seconds (i.e. 250ms)
    m_offlineCommandTimer.setSingleShot(true);

    initSpecialCommandMap();
}

AbstractParser::~AbstractParser() = default;

void AbstractParser::reset()
{
    if (m_trollExitMapping) {
        emit log("Parser", "Disabling troll exit mapping");
        m_trollExitMapping = false;
    }
    queue.clear();
}

void AbstractParser::parsePrompt(const QString &prompt)
{
    m_promptFlags.reset();
    int index = 0;

    switch (static_cast<int>(prompt[index].toLatin1())) {
    case 42:
        index++;
        m_promptFlags.setLit();
        break; // *  indoor/sun (direct and indirect)
    case 33:
        index++;
        break; // !  artifical light
    case 41:
        index++;
        m_promptFlags.setLit();
        break; // )  moon (direct and indirect)
    case 111:
        index++; // o  darkness
        m_promptFlags.setDark();
        break;
    default:;
    }

    switch (static_cast<int>(prompt[index].toLatin1())) {
    case 91:
        m_promptFlags.setTerrainType(RoomTerrainType::INDOORS);
        break; // [  // indoors
    case 35:
        m_promptFlags.setTerrainType(RoomTerrainType::CITY);
        break; // #  // city
    case 46:
        m_promptFlags.setTerrainType(RoomTerrainType::FIELD);
        break; // .  // field
    case 102:
        m_promptFlags.setTerrainType(RoomTerrainType::FOREST);
        break; // f  // forest
    case 40:
        m_promptFlags.setTerrainType(RoomTerrainType::HILLS);
        break; // (  // hills
    case 60:
        m_promptFlags.setTerrainType(RoomTerrainType::MOUNTAINS);
        break; // <  // mountains
    case 37:
        m_promptFlags.setTerrainType(RoomTerrainType::SHALLOW);
        break; // %  // shallow
    case 126:
        m_promptFlags.setTerrainType(RoomTerrainType::WATER);
        break; // ~  // water
    case 87:
        m_promptFlags.setTerrainType(RoomTerrainType::RAPIDS);
        break; // W  // rapids
    case 85:
        m_promptFlags.setTerrainType(RoomTerrainType::UNDERWATER);
        break; // U  // underwater
    case 43:
        m_promptFlags.setTerrainType(RoomTerrainType::ROAD);
        break; // +  // road
    case 61:
        m_promptFlags.setTerrainType(RoomTerrainType::TUNNEL);
        break; // =  // tunnel
    case 79:
        m_promptFlags.setTerrainType(RoomTerrainType::CAVERN);
        break; // O  // cavern
    case 58:
        m_promptFlags.setTerrainType(RoomTerrainType::BRUSH);
        break; // :  // brush
    default:
        break;
    }
    m_promptFlags.setValid();
}

void AbstractParser::parseExits()
{
    QByteArray exitsByteArray = m_exits.toLatin1();

    QString str = normalizeStringCopy(m_exits);
    m_connectedRoomFlags.reset();
    m_exitsFlags.reset();
    ExitsFlagsType closedDoorFlag{};
    bool doors = false;
    bool closed = false;
    bool road = false;
    bool climb = false;
    bool portal = false;
    bool directSun = false;
    auto dir = DirectionType::UNKNOWN;

    if (str.length() > 5 && str.at(5).toLatin1() != ':') {
        // Ainur exits
        sendToUser(exitsByteArray);
        return;
    }
    int length = str.length();
    for (int i = 7; i < length; i++) {
        switch (static_cast<int>(str.at(i).toLatin1())) {
        case 40:
            doors = true;
            break; // (  // open door
        case 91:
            doors = true;
            closed = true;
            break; // [  // closed door
        case 35:
            doors = true;
            break; // #  // broken door
        case 61:
            road = true;
            break; // =  // road
        case 45:
            road = true;
            break; // -  // trail
        case 47:
            climb = true;
            break; // /  // upward climb
        case 92:
            climb = true;
            break; // \  // downward climb
        case 123:
            portal = true;
            break; // {  // portal
        case 42:
            directSun = true;
            break; // *  // sunlit room (troll/orc only)
        case 94:
            directSun = true; // ^  // outdoors room (troll only)
            if (!m_trollExitMapping) {
                emit log("Parser", "Autoenabling troll exit mapping mode.");
            }
            m_trollExitMapping = true;
            break;

        case 32: // empty space means reset for next exit
            doors = false;
            closed = false;
            road = false;
            climb = false;
            portal = false;
            directSun = false;
            dir = DirectionType::UNKNOWN;
            break;

        case 110:                                                        // n
            if ((i + 2) < length && (str.at(i + 2).toLatin1()) == 'r') { // north
                i += 5;
                dir = DirectionType::NORTH;
            } else {
                i += 4; // none
                dir = DirectionType::NONE;
            }
            break;

        case 115: // s
            i += 5;
            dir = DirectionType::SOUTH;
            break;

        case 101: // e
            i += 4;
            dir = DirectionType::EAST;
            break;

        case 119: // w
            i += 4;
            dir = DirectionType::WEST;
            break;

        case 117: // u
            i += 2;
            dir = DirectionType::UP;
            break;

        case 100: // d
            i += 4;
            dir = DirectionType::DOWN;
            break;
        default:;
        }
        if (dir < DirectionType::NONE) {
            ExitFlags exitFlags{ExitFlag::EXIT};
            if (climb) {
                exitFlags |= ExitFlag::CLIMB;
            }
            if (doors) {
                exitFlags |= ExitFlag::DOOR;
                if (closed) {
                    closedDoorFlag.set(static_cast<ExitDirection>(dir), ExitFlag::DOOR);
                }
            }
            if (road) {
                exitFlags |= ExitFlag::ROAD;
            }
            if (directSun) {
                setConnectedRoomFlag(DirectionalLightType::DIRECT_SUN_ROOM, dir);
            }
            setExitFlags(exitFlags, dir);
        }
    }

    // If there is't a portal then we can trust the exits
    if (!portal) {
        m_exitsFlags.setValid();
        m_connectedRoomFlags.setValid();

        // Orcs and trolls can detect exits with direct sunlight
        bool foundDirectSunlight = m_connectedRoomFlags.hasAnyDirectSunlight();
        if (foundDirectSunlight || m_trollExitMapping) {
            for (const auto alt_dir : ALL_DIRECTIONS6) {
                auto eThisExit = getExitFlags(alt_dir);
                auto eThisClosed = closedDoorFlag.get(static_cast<ExitDirection>(alt_dir));
                auto cOtherRoom = getConnectedRoomFlags(alt_dir);

                // Do not flag indirect sunlight if there was a closed door, no exit, or we saw direct sunlight
                if (!eThisExit.isExit() || eThisClosed.isDoor()
                    || IS_SET(cOtherRoom, DirectionalLightType::DIRECT_SUN_ROOM)) {
                    continue;
                }

                // Flag indirect sun
                setConnectedRoomFlag(DirectionalLightType::INDIRECT_SUN_ROOM, alt_dir);
            }
        }
    }

    SigRoomSelection rs = m_mapData->select();
    if (const Room *room = m_mapData->getRoom(getPosition(), rs)) {
        QByteArray cn = enhanceExits(room);

        sendToUser(exitsByteArray.simplified() + cn);

        if (getConfig().mumeNative.showNotes) {
            QString ns = room->getNote();
            if (!ns.isEmpty()) {
                QByteArray note = "Note: " + ns.toLatin1() + "\r\n";
                sendToUser(note);
            }
        }
    }
}

QString AbstractParser::normalizeStringCopy(QString string)
{
    // Remove ANSI first, since we don't want Latin1
    // transliterations to accidentally count as ANSI.
    ParserUtils::removeAnsiMarksInPlace(string);
    ParserUtils::latinToAsciiInPlace(string);
    return string;
}

const Coordinate AbstractParser::getPosition()
{
    Coordinate c;
    CommandQueue tmpqueue;

    if (!queue.isEmpty()) {
        tmpqueue.enqueue(queue.head());
    }

    QList<Coordinate> cl = m_mapData->getPath(tmpqueue);
    if (!cl.isEmpty()) {
        c = cl.at(cl.size() - 1);
    } else {
        c = m_mapData->getPosition();
    }
    return c;
}

void AbstractParser::emulateExits()
{
    Coordinate c = getPosition();
    SigRoomSelection rs = m_mapData->select();
    if (const Room *room = m_mapData->getRoom(c, rs)) {
        sendRoomExitsInfoToUser(room);
    }
}

QByteArray AbstractParser::enhanceExits(const Room *sourceRoom)
{
    QByteArray cn = " -";
    bool enhancedExits = false;

    SigRoomSelection rs = m_mapData->select();
    auto sourceId = sourceRoom->getId();
    for (auto i : ALL_EXITS_NESWUD) {
        const Exit &e = sourceRoom->exit(i);
        const ExitFlags ef = e.getExitFlags();
        if (!ef.isExit()) {
            continue;
        }

        QByteArray etmp{};
        auto add_exit_keyword = [&etmp](const char *word) {
            if (!etmp.isEmpty()) {
                etmp += ",";
            }
            etmp += word;
        };

        // Extract hidden exit flags
        if (getConfig().mumeNative.showHiddenExitFlags) {
            /* TODO: const char* lowercaseName(ExitFlag) */
            if (ef.contains(ExitFlag::NO_FLEE)) {
                add_exit_keyword("noflee");
            }
            if (ef.contains(ExitFlag::RANDOM)) {
                add_exit_keyword("random");
            }
            if (ef.contains(ExitFlag::SPECIAL)) {
                add_exit_keyword("special");
            }
            if (ef.contains(ExitFlag::DAMAGE)) {
                add_exit_keyword("damage");
            }
            if (ef.contains(ExitFlag::FALL)) {
                add_exit_keyword("fall");
            }
            if (ef.contains(ExitFlag::GUARDED)) {
                add_exit_keyword("guarded");
            }

            // Exit modifiers
            if (e.containsOut(sourceId)) {
                add_exit_keyword("loop");

            } else if (!e.outIsEmpty()) {
                // Check target room for exit information
                const auto targetId = e.outFirst();
                uint exitCount = 0;
                bool oneWay = false;
                bool hasNoFlee = false;
                if (const Room *targetRoom = m_mapData->getRoom(targetId, rs)) {
                    if (!targetRoom->exit(opposite(i)).containsOut(sourceId)) {
                        oneWay = true;
                    }
                    for (auto j : ALL_EXITS_NESWUD) {
                        const Exit &targetExit = targetRoom->exit(j);
                        if (!targetExit.getExitFlags().isExit()) {
                            continue;
                        }
                        exitCount++;
                        if (targetExit.containsOut(sourceId)) {
                            // Technically rooms can point back in a different direction
                            oneWay = false;
                        }
                        if (targetExit.getExitFlags().contains(ExitFlag::NO_FLEE)) {
                            hasNoFlee = true;
                        }
                    }
                    if (oneWay) {
                        add_exit_keyword("oneway");
                    }
                    if (hasNoFlee && exitCount == 1) {
                        // If there is only 1 exit out of this room add the 'hasnoflee' flag since its usually a mobtrap
                        add_exit_keyword("hasnoflee");
                    }

                    // Terrain type exit modifiers
                    RoomTerrainType targetTerrain = targetRoom->getTerrainType();
                    if (targetTerrain == RoomTerrainType::UNDERWATER) {
                        add_exit_keyword("underwater");
                    } else if (targetTerrain == RoomTerrainType::DEATHTRAP) {
                        // Override all previous flags
                        etmp = "deathtrap";
                    }
                }
            }
        }

        // Extract door names
        QByteArray dn = e.getDoorName().toLatin1();
        if (!dn.isEmpty() || !etmp.isEmpty()) {
            enhancedExits = true;
            cn += " ";
            cn += Mmapper2Exit::charForDir(i);
            cn += ":";
            if (!dn.isEmpty()) {
                cn += dn;
            }
            if (!etmp.isEmpty()) {
                cn += "(" + etmp + ")";
            }
        }
    }

    if (!enhancedExits) {
        return "\r\n";
    }

    cn += ".\r\n";
    return cn;
}

void AbstractParser::parseNewUserInput(const IncomingData &data)
{
    auto parse_and_send = [this, &data]() {
        auto parse = [this, &data]() -> bool {
            const QString input = QString::fromLatin1(data.line.constData(), data.line.size())
                                      .simplified();
            try {
                return parseUserCommands(input);
            } catch (const std::exception &ex) {
                qWarning() << "Exception: " << ex.what();
                sendToUser(QString::asprintf("An exception occurred: %s\r\n", ex.what()));
                sendPromptToUser();
                return false;
            }
        };

        if (parse()) {
            emit sendToMud(data.line);
        }
    };

    switch (data.type) {
    case TelnetDataType::DELAY:
    case TelnetDataType::PROMPT:
    case TelnetDataType::MENU_PROMPT:
    case TelnetDataType::LOGIN:
    case TelnetDataType::LOGIN_PASSWORD:
    case TelnetDataType::SPLIT:
    case TelnetDataType::UNKNOWN:
        emit sendToMud(data.line);
        break;
    case TelnetDataType::CRLF:
        m_newLineTerminator = "\r\n";
        parse_and_send();
        break;
    case TelnetDataType::LFCR:
        m_newLineTerminator = "\n\r";
        parse_and_send();
        break;
    case TelnetDataType::LF:
        m_newLineTerminator = "\n";
        parse_and_send();
        break;
    }
}

static QString compressDirections(QString original)
{
    QString ans{};
    int curnum = 0;
    QChar curval = '\0';
    Coordinate delta{};
    const auto addDirs = [&ans, &curnum, &curval, &delta]() {
        assert(curnum >= 1);
        assert(curval != '\0');
        if (curnum > 1) {
            ans.append(QString::number(curnum));
        }
        ans.append(curval);

        const auto dir = Mmapper2Exit::dirForChar(curval.toLatin1());
        delta += RoomFactory::exitDir(dir) * curnum;
    };

    for (auto c : original) {
        if (curnum != 0 && curval == c) {
            curnum++;
        } else {
            if (curnum != 0)
                addDirs();
            curnum = 1;
            curval = c;
        }
    }
    if (curnum != 0)
        addDirs();

    bool wantDelta = true;
    if (wantDelta) {
        auto addNumber =
            [&curnum, &curval, &addDirs, &ans](const int n, const char pos, const char neg) {
                if (n == 0)
                    return;
                curnum = std::abs(n);
                curval = (n < 0) ? neg : pos;
                ans += ' ';
                addDirs();
            };

        if (delta.isNull()) {
            ans += " (here)";
        } else {
            ans += " (total:";
            addNumber(delta.x, 'e', 'w');
            addNumber(delta.y, 's', 'n');
            addNumber(delta.z, 'u', 'd');
            ans += ")";
        }
    }

    return ans;
}

class ShortestPathEmitter final : public ShortestPathRecipient
{
    AbstractParser &parser;

public:
    explicit ShortestPathEmitter(AbstractParser &parser)
        : parser(parser)
    {}
    virtual ~ShortestPathEmitter() override;

    void receiveShortestPath(RoomAdmin * /*admin*/, QVector<SPNode> spnodes, int endpoint) override
    {
        const SPNode *spnode = &spnodes[endpoint];
        auto name = spnode->r->getName();
        parser.sendToUser("Distance " + QString::number(spnode->dist) + ": " + name + "\r\n");
        QString dirs;
        while (spnode->parent >= 0) {
            if (&spnodes[spnode->parent] == spnode) {
                parser.sendToUser("ERROR: loop\r\n");
                break;
            }
            dirs.append(Mmapper2Exit::charForDir(spnode->lastdir));
            spnode = &spnodes[spnode->parent];
        }
        std::reverse(dirs.begin(), dirs.end());
        parser.sendToUser("dirs: " + compressDirections(dirs) + "\r\n");
    }
};

ShortestPathEmitter::~ShortestPathEmitter() = default;

void AbstractParser::searchCommand(const RoomFilter &f)
{
    search_rs = m_mapData->select();
    m_mapData->genericSearch(search_rs, f);
    emit m_mapData->updateCanvas();
    sendToUser(QString("%1 room%2 found.\r\n")
                   .arg(search_rs.getShared()->size())
                   .arg((search_rs.getShared()->size() == 1) ? "" : "s"));
}

void AbstractParser::dirsCommand(const RoomFilter &f)
{
    ShortestPathEmitter sp_emitter(*this);

    Coordinate c = m_mapData->getPosition();
    SigRoomSelection rs = m_mapData->select(c);
    if (!rs.getShared()->isEmpty()) {
        const Room *r = rs.getShared()->values().front();

        m_mapData->shortestPathSearch(r, &sp_emitter, f, 10, 0);
    }
}

void AbstractParser::markCurrentCommand()
{
    Coordinate c = getPosition();
    search_rs = m_mapData->select(c);
    emit m_mapData->updateCanvas();
}

DirectionType AbstractParser::tryGetDir(StringView &view)
{
    if (view.isEmpty())
        return DirectionType::UNKNOWN;

    auto word = view.takeFirstWord();
    for (auto dir : ALL_DIRECTIONS6) {
        auto lower = lowercaseDirection(static_cast<ExitDirection>(dir));
        if (lower != nullptr && Abbrev{lower, 1}.matches(word))
            return dir;
    }

    sendToUser(QString("Unexpected direction: \"%1\"\r\n").arg(word.toQString()));
    throw std::runtime_error("bad direction");
}

void AbstractParser::showCommandPrefix()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    const auto quote = static_cast<char>((prefixChar == '\'') ? '"' : '\'');
    sendToUser(QString("The current command prefix is: %1%2%1 (e.g. %2help) \r\n")
                   .arg(quote)
                   .arg(prefixChar));
}

bool AbstractParser::setCommandPrefix(const char prefix)
{
    if (!isValidPrefix(prefix))
        return false;
    setConfig().parser.prefixChar = prefix;
    showCommandPrefix();
    return true;
}

void AbstractParser::showSyntax(const char *rest)
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    sendToUser(QString::asprintf("Usage: %c%s\r\n", prefixChar, rest));
}

void AbstractParser::setNote(const QString &note)
{
    setRoomFieldCommand(note, RoomField::NOTE);
    if (note.isEmpty()) {
        sendToUser("Note cleared!\r\n");
    } else {
        sendToUser("Note set!\r\n");
        showNote();
    }
}

void AbstractParser::showNote()
{
    printRoomInfo(RoomField::NOTE);
}

void AbstractParser::toggleTrollMapping()
{
    m_trollExitMapping = !m_trollExitMapping;
    QString toggleText = enabledString(m_trollExitMapping);
    sendToUser("OK. Troll exit mapping is now " + toggleText + ".\r\n");
}

void AbstractParser::doSearchCommand(StringView view)
{
    QString pattern_str = view.toQString();
    RoomFilter f;
    if (!RoomFilter::parseRoomFilter(pattern_str, f)) {
        sendToUser(RoomFilter::parse_help);
    } else {
        searchCommand(f);
    }
}

void AbstractParser::doGetDirectionsCommand(StringView view)
{
    QString pattern_str = view.toQString();
    RoomFilter f;
    if (!RoomFilter::parseRoomFilter(pattern_str, f)) {
        sendToUser(RoomFilter::parse_help);
    } else {
        dirsCommand(f);
    }
}

void AbstractParser::doMarkCurrentCommand()
{
    markCurrentCommand();
    sendToUser("--->Current room marked temporarily on the map.\r\n");
}

void AbstractParser::doGroupLockCommand()
{
    bool toggle = !getConfig().groupManager.lockGroup;
    setConfig().groupManager.lockGroup = toggle;
    sendToUser(QString("--->Group has been %1.\r\n").arg(toggle ? "locked" : "unlocked"));
}

void AbstractParser::doRemoveDoorNamesCommand()
{
    m_mapData->removeDoorNames();
    sendToUser("OK. Secret exits purged.\r\n");
}

void AbstractParser::doBackCommand()
{
    queue.clear();
    sendToUser("OK.\r\n");
    emit showPath(queue, true);
}

void AbstractParser::openVoteURL()
{
    QDesktopServices::openUrl(QUrl(
        "http://www.mudconnect.com/cgi-bin/vote_rank.cgi?mud=MUME+-+Multi+Users+In+Middle+Earth"));
    sendToUser("--->Thank you kindly for voting!\r\n");
}

void AbstractParser::showHelpCommands(const bool showAbbreviations)
{
    auto &map = this->m_specialCommandMap;
    const auto &prefixChar = getConfig().parser.prefixChar;

    struct record
    {
        std::string from;
        std::string to;
    };

    std::vector<record> records;
    for (auto &kv : map) {
        const auto &from = kv.first;
        const auto &to = kv.second.fullCommand;
        if (from.empty() || to.empty())
            throw std::runtime_error("internal havoc");

        if (showAbbreviations || from == to)
            records.emplace_back(record{from, to});
    }

    if (records.empty())
        return;

    std::sort(std::begin(records), std::end(records), [](const record &a, const record &b) {
        return a.from < b.from;
    });

    char currentLetter = records[0].from[0];
    for (const auto &rec : records) {
        const auto thisLetter = rec.from[0];
        if (thisLetter != currentLetter) {
            currentLetter = thisLetter;
            sendToUser("\r\n");
        }

        if (rec.from == rec.to)
            sendToUser(QString::asprintf("  %c%s\r\n", prefixChar, rec.from.c_str()));
        else
            sendToUser(QString::asprintf("  %c%-20s -> %c%s\r\n",
                                         prefixChar,
                                         rec.from.c_str(),
                                         prefixChar,
                                         rec.to.c_str()));
    }
}

void AbstractParser::showGroupHelp()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("MMapper group manager help");
    showHeader("Group commands");
    sendToUser(QString("  %1GKick [player]      - kick [player] from the group\r\n"
                       "  %1GLock               - Toggle lock on group\r\n"
                       "  %1GTell [message]     - send a grouptell with the [message]\r\n")
                   .arg(prefixChar));
}

void AbstractParser::showHeader(const QString &s)
{
    QString result;
    result += "\r\n";
    result += s;
    result += "\r\n";
    result += QString(s.size(), '-');
    result += "\r\n";
    sendToUser(result);
}

void AbstractParser::showMapHelp()
{
    showHeader("MMapper mapping help");

    showExitHelp();
    sendToUser("\r\n");

    showRoomSimpleFlagsHelp();
    sendToUser("\r\n");

    showRoomMobFlagsHelp();
    sendToUser("\r\n");

    showRoomLoadFlagsHelp();
    showMiscHelp();
}

void AbstractParser::showMiscHelp()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("Miscellaneous commands");
    sendToUser(QString("  %1note [note] - set a note in the room\r\n"
                       "  %1trollexit   - toggle troll-only exit mapping for direct sunlight\r\n")
                   .arg(prefixChar));
}

void AbstractParser::showRoomLoadFlagsHelp()
{
    showHeader("Room load flag commands");

    const auto &prefixChar = getConfig().parser.prefixChar;
    for (auto x : ALL_LOAD_FLAGS) {
        if (const Abbrev cmd = getParserCommandName(x))
            sendToUser(QString("  %1%2 - toggle the \"%3\" load flag in the room\r\n")
                           .arg(prefixChar)
                           .arg(cmd.describe(), -12)
                           .arg(cmd.getCommand()));
    }
}

void AbstractParser::showRoomMobFlagsHelp()
{
    showHeader("Room mob flag commands");

    const auto &prefixChar = getConfig().parser.prefixChar;
    for (auto x : ALL_MOB_FLAGS) {
        if (const Abbrev cmd = getParserCommandName(x))
            sendToUser(QString("  %1%2 - toggle the \"%3\" mob flag in the room\r\n")
                           .arg(prefixChar)
                           .arg(cmd.describe(), -12)
                           .arg(cmd.getCommand()));
    }
}

void AbstractParser::showRoomSimpleFlagsHelp()
{
    showHeader("Basic room flag commands");

    const auto &prefixChar = getConfig().parser.prefixChar;
#define SHOW(X) \
    for (auto x : DEFINED_ROOM_##X##_TYPES) { \
        if (const Abbrev cmd = getParserCommandName(x)) \
            sendToUser(QString("  %1%2 - set the room to \"%3\"\r\n") \
                           .arg(prefixChar) \
                           .arg(cmd.describe(), -12) \
                           .arg(cmd.getCommand())); \
    }

    SHOW(PORTABLE)
    SHOW(LIGHT)
    SHOW(SUNDEATH)
    SHOW(RIDABLE)
    SHOW(ALIGN)
#undef SHOW
}

void AbstractParser::showExitHelp()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("Exit commands");
    sendToUser(QString("  %1name <dir> <name> - name a door in direction <dir> with <name>\r\n")
                   .arg(prefixChar));

    sendToUser("\r\n");

    showDoorFlagHelp();

    sendToUser("\r\n");

    showExitFlagHelp();
}

void AbstractParser::showExitFlagHelp()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("Exit flags");
    for (const ExitFlag flag : ALL_EXIT_FLAGS) {
        if (const Abbrev cmd = getParserCommandName(flag))
            sendToUser(QString("  %1%2 <dir> - toggle \"%3\" exit flag in direction <dir>\r\n")
                           .arg(prefixChar)
                           .arg(cmd.describe(), -7)
                           .arg(cmd.getCommand()));
    }
}

void AbstractParser::showDoorFlagHelp()
{
    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("Door flags (implies exit has door flag)");
    for (const DoorFlag flag : ALL_DOOR_FLAGS) {
        if (const Abbrev cmd = getParserCommandName(flag))
            sendToUser(QString("  %1%2 <dir> - toggle \"%3\" door flag in direction <dir>\r\n")
                           .arg(prefixChar)
                           .arg(cmd.describe(), -9)
                           .arg(cmd.getCommand()));
    }
}

void AbstractParser::showHelp()
{
    auto s = QString("\r\nMMapper help:\r\n-------------\r\n"
                     "\r\nStandard MUD commands:\r\n"
                     "  Move commands: [n,s,...] or [north,south,...]\r\n"
                     "  Sync commands: [exa,l] or [examine,look]\r\n"
                     "\r\nManage prespammed command queue:\r\n"
                     "  %1back        - delete prespammed commands from queue\r\n"
                     "\r\nDescription commands:\r\n"
                     "  %1pdynamic    - prints current room description\r\n"
                     "  %1pstatic     - the same as previous, but without moveable items\r\n"
                     "  %1pnote       - print the note in the current room\r\n"
                     "\r\nHelp commands:\n"
                     "  %1help      - this help text\r\n"
                     "  %1maphelp   - help for mapping console commands\r\n"
                     "  %1doorhelp  - help for door console commands\r\n"
                     "  %1grouphelp - help for group manager console commands\r\n"
                     "\r\nOther commands:\n"
                     "  %1vote                      - vote for MUME on TMC!\r\n"
                     "  %1dirs [-options] pattern   - directions to matching rooms\r\n"
                     "  %1search [-options] pattern - highlight matching rooms\r\n"
                     "  %1markcurrent               - highlight the room you are currently in\r\n"
                     "  %1time                      - display current MUME time\r\n"
                     "  %1set [prefix] character    - change command prefix\r\n");

    const auto &prefixChar = getConfig().parser.prefixChar;
    sendToUser(s.arg(prefixChar));
}

void AbstractParser::showMumeTime()
{
    MumeMoment moment = m_mumeClock->getMumeMoment();
    QByteArray data = m_mumeClock->toMumeTime(moment).toLatin1() + "\r\n";
    MumeClockPrecision precision = m_mumeClock->getPrecision();
    if (precision > MumeClockPrecision::MUMECLOCK_DAY) {
        MumeTime time = moment.toTimeOfDay();
        data += "It is currently ";
        if (time == MumeTime::TIME_DAWN) {
            data += "\033[31mdawn\033[0m";
        } else if (time == MumeTime::TIME_DUSK) {
            data += "\033[34mdusk\033[0m and will be night";
        } else if (time == MumeTime::TIME_NIGHT) {
            data += "\033[34mnight\033[0m";
        } else {
            data += "\033[33mday\033[0m";
        }

        data += " for " + m_mumeClock->toCountdown(moment).toLatin1() + " more ticks.\r\n";
    }
    sendToUser(data);
}

void AbstractParser::showDoorCommandHelp()
{
    showHeader("MMapper door help");

    const auto &prefixChar = getConfig().parser.prefixChar;
    showHeader("Door commands");
    for (auto dat : ALL_DOOR_ACTION_TYPES) {
        const int cmdWidth = 6;
        sendToUser(QString("  %1%2 [dir] - executes \"%3 ... [dir]\"\r\n")
                       .arg(prefixChar)
                       .arg(getParserCommandName(dat).describe(), -cmdWidth)
                       .arg(QString{getCommandName(dat)}));
    }

    showDoorVariableHelp();

    showHeader("Destructive commands");
    sendToUser(
        QString("  %1removedoornames   - removes all secret door names from the current map\r\n")
            .arg(prefixChar));
}

void AbstractParser::showDoorVariableHelp()
{
    showHeader("Door variables");
    for (auto dir : ALL_EXITS_NESWUD) {
        auto lower = lowercaseDirection(dir);
        if (lower == nullptr)
            continue;
        auto upper = static_cast<char>(toupper(lower[0]));
        sendToUser(
            QString("  $$DOOR_%1$$   - secret name of door leading %2\r\n").arg(upper).arg(lower));
    }
    sendToUser("  $$DOOR$$     - the same as 'exit'\r\n");
}

void AbstractParser::doMove(CommandIdType cmd)
{
    // REVISIT: should "look" commands be queued?
    assert(isDirectionNESWUD(cmd) || cmd == CommandIdType::LOOK);
    queue.enqueue(cmd);
    emit showPath(queue, true);
    if (isOffline())
        offlineCharacterMove(cmd);
}

bool AbstractParser::tryParseGenericDoorCommand(const QString &str)
{
    if (!str.contains("$$DOOR"))
        return false;

    const char pattern[] = "$$DOOR_X$$";
    for (auto dir : ALL_DIRECTIONS6) {
        const auto c = static_cast<char>(
            std::toupper(Mmapper2Exit::charForDir(static_cast<ExitDirection>(dir))));
        auto buf = makeCharBuffer(pattern);
        buf.replaceAll('X', c);
        if (str.contains(buf.getBuffer())) {
            genericDoorCommand(str, dir);
            return true;
        }
    }
    return false;
}

void AbstractParser::doOfflineCharacterMove()
{
    if (queue.isEmpty()) {
        return;
    }

    CommandIdType direction = queue.dequeue();
    if (m_mapData->isEmpty()) {
        sendToUser("Alas, you cannot go that way...\r\n");
        m_offlineCommandTimer.start();
        return;
    }

    bool flee = false;
    bool scout = false;
    if (direction == CommandIdType::FLEE) {
        flee = true;
        sendToUser("You flee head over heels.\r\n");
        direction = static_cast<CommandIdType>(rand() % 6); // NOLINT
    } else if (direction == CommandIdType::SCOUT) {
        scout = true;
        direction = queue.dequeue();
    }

    Coordinate c;
    c = m_mapData->getPosition();
    SigRoomSelection rs1 = m_mapData->select(c);
    if (!rs1.getShared()->isEmpty()) {
        const Room *rb = rs1.getShared()->values().front();
        if (direction == CommandIdType::LOOK) {
            sendRoomInfoToUser(rb);
            sendRoomExitsInfoToUser(rb);
            sendPromptToUser(*rb);
        } else {
            /* FIXME: This needs stronger type check on the cast */
            /* NOTE: This is a copy instead of a reference; later e is re-assigned. */
            Exit e = rb->exit(static_cast<ExitDirection>(direction));
            if (e.exitIsRandom()) {
                // Pick an alternative direction to randomly wander into
                std::vector<ExitDirection> exitDirections;
                for (auto i : ALL_EXITS_NESWUD) {
                    const Exit &e2 = rb->exit(i);
                    if (!e2.isExit() || e2.outIsEmpty()) {
                        continue;
                    }
                    exitDirections.emplace_back(i);
                }
                if (exitDirections.size() > 0) {
                    const auto n = static_cast<uint32_t>(rand()); // NOLINT
                    const auto dir = n % exitDirections.size();
                    const auto randomDirection = static_cast<CommandIdType>(exitDirections[dir]);

                    // Update exit and direction with new random one
                    if (direction != randomDirection) {
                        sendToUser("You feel confused and move along randomly...\r\n");
                        qDebug() << "Randomly moving" << getLowercase(randomDirection)
                                 << "instead of" << getLowercase(direction);
                        e = rb->exit(static_cast<ExitDirection>(randomDirection));
                        // REVISIT: Should we update the direction hint to the Path Machine?
                        // direction = randomDirection;
                    }
                }
            }
            if (e.isExit() && !e.outIsEmpty()) {
                SigRoomSelection rs2 = m_mapData->select();
                const auto targetId = e.outFirst();
                if (const Room *r = m_mapData->getRoom(targetId, rs2)) {
                    if (flee) {
                        sendToUser(
                            QByteArray("You flee ").append(getLowercase(direction)).append("."));
                    }
                    if (scout) {
                        sendToUser(QByteArray("You quietly scout ")
                                       .append(getLowercase(direction))
                                       .append("wards...\r\n"));
                    }
                    sendRoomInfoToUser(r);
                    sendRoomExitsInfoToUser(r);
                    if (scout) {
                        sendToUser("\r\nYou stop scouting.\r\n");
                        sendPromptToUser();

                    } else {
                        sendPromptToUser(*r);

                        // Create character move event for main move/search algorithm
                        auto ev = ParseEvent::createEvent(direction,
                                                          r->getName(),
                                                          r->getDynamicDescription(),
                                                          r->getStaticDescription(),
                                                          ExitsFlagsType{},
                                                          PromptFlagsType::fromRoomTerrainType(
                                                              r->getTerrainType()),
                                                          ConnectedRoomFlagsType{});
                        emit event(SigParseEvent{ev});
                        emit showPath(queue, true);
                    }
                }
            } else {
                if (!flee || scout) {
                    sendToUser("Alas, you cannot go that way...\r\n");
                } else {
                    sendToUser("PANIC! You couldn't escape!\r\n");
                }
                sendPromptToUser(*rb);
            }
        }
    }
    m_offlineCommandTimer.start();
}

void AbstractParser::offlineCharacterMove(CommandIdType direction)
{
    if (direction == CommandIdType::FLEE) {
        queue.enqueue(direction);
    }

    if (!m_offlineCommandTimer.isActive()) {
        m_offlineCommandTimer.start();
    }
}

void AbstractParser::sendRoomInfoToUser(const Room *r)
{
    if (r == nullptr) {
        return;
    }

    const auto &settings = getConfig().parser;
    static constexpr const auto ESCAPE = "\033"; // also known as "\x1b"

    QByteArray roomName("\r\n");
    if (!settings.roomNameColor.isEmpty()) {
        roomName += ESCAPE + settings.roomNameColor.toLatin1();
    }
    roomName += r->getName().toLatin1() + ESCAPE + "[0m\r\n";
    sendToUser(roomName);

    QByteArray roomDescription;
    if (!r->getStaticDescription().trimmed().isEmpty()) {
        if (!settings.roomDescColor.isEmpty()) {
            roomDescription += ESCAPE + settings.roomDescColor.toLatin1();
        }
        roomDescription += r->getStaticDescription().trimmed().toLatin1() + ESCAPE + "[0m";
        roomDescription.replace("\n", "\r\n");
        sendToUser(roomDescription + "\r\n");
    }

    QByteArray dynamicDescription = r->getDynamicDescription().trimmed().toLatin1();
    if (!dynamicDescription.isEmpty()) {
        dynamicDescription.replace("\n", "\r\n");
        sendToUser(dynamicDescription + "\r\n");
    }
}

void AbstractParser::sendRoomExitsInfoToUser(const Room *r)
{
    if (r == nullptr) {
        return;
    }
    char sunCharacter = (m_mumeClock->getMumeMoment().toTimeOfDay() <= MumeTime::TIME_DAY) ? '*'
                                                                                           : '^';
    SigRoomSelection rs = m_mapData->select();

    uint exitCount = 0;
    QString etmp = "Exits/emulated:";
    for (int i = 0; i < 6; i++) {
        const auto direction = static_cast<ExitDirection>(i);
        bool door = false;
        bool exit = false;
        bool road = false;
        bool trail = false;
        bool climb = false;
        bool directSun = false;
        bool swim = false;

        const Exit &e = r->exit(direction);
        if (e.isExit()) {
            exitCount++;
            exit = true;
            etmp += " ";

            RoomTerrainType sourceTerrain = r->getTerrainType();
            if (!e.outIsEmpty()) {
                const auto targetId = e.outFirst();
                if (const Room *targetRoom = m_mapData->getRoom(targetId, rs)) {
                    RoomTerrainType targetTerrain = targetRoom->getTerrainType();

                    // Sundeath exit flag modifiers
                    if (targetRoom->getSundeathType() == RoomSundeathType::SUNDEATH) {
                        directSun = true;
                        etmp += sunCharacter;
                    }

                    // Terrain type exit modifiers
                    if (targetTerrain == RoomTerrainType::RAPIDS
                        || targetTerrain == RoomTerrainType::UNDERWATER
                        || targetTerrain == RoomTerrainType::WATER) {
                        swim = true;
                        etmp += "~";

                    } else if (targetTerrain == RoomTerrainType::ROAD
                               && sourceTerrain == RoomTerrainType::ROAD) {
                        road = true;
                        etmp += "=";
                    }
                }
            }

            if (!road && e.getExitFlags().isRoad()) {
                if (sourceTerrain == RoomTerrainType::ROAD) {
                    road = true;
                    etmp += "=";
                } else {
                    trail = true;
                    etmp += "-";
                }
            }

            if (e.isDoor()) {
                door = true;
                etmp += "{";
            } else if (e.getExitFlags().isClimb()) {
                climb = true;
                etmp += "|";
            }

            etmp += lowercaseDirection(direction);
        }

        if (door) {
            etmp += "}";
        } else if (climb) {
            etmp += "|";
        }
        if (swim) {
            etmp += "~";
        } else if (road) {
            etmp += "=";
        } else if (trail) {
            etmp += "-";
        }
        if (directSun) {
            etmp += sunCharacter;
        }
        if (exit) {
            etmp += ",";
        }
    }
    if (exitCount == 0) {
        etmp += " none.";
    } else {
        etmp.replace(etmp.length() - 1, 1, '.');
    }

    QByteArray cn = enhanceExits(r);
    sendToUser(etmp.toLatin1() + cn);

    if (getConfig().mumeNative.showNotes) {
        QString ns = r->getNote();
        if (!ns.isEmpty()) {
            QByteArray note = "Note: " + ns.toLatin1() + "\r\n";
            sendToUser(note);
        }
    }
}

void AbstractParser::sendPromptToUser()
{
    if (m_overrideSendPrompt) {
        m_overrideSendPrompt = false;
        return;
    }

    if (!m_lastPrompt.isEmpty() && isOnline()) {
        if (!m_compactMode) {
            sendToUser("\r\n");
        }
        sendToUser(m_lastPrompt, true);
        return;
    }

    // Emulate prompt mode
    Coordinate c = getPosition();
    SigRoomSelection rs = m_mapData->select();

    if (const Room *r = m_mapData->getRoom(c, rs))
        sendPromptToUser(*r);
    else
        sendPromptToUser('?', '?');
}

void AbstractParser::sendPromptToUser(const Room &r)
{
    const auto lightType = r.getLightType();
    const auto terrainType = r.getTerrainType();
    sendPromptToUser(lightType, terrainType);
}

void AbstractParser::sendPromptToUser(const RoomLightType lightType,
                                      const RoomTerrainType terrainType)
{
    const char light = getLightSymbol(lightType);
    const char terrain = getTerrainSymbol(terrainType);
    sendPromptToUser(light, terrain);
}

void AbstractParser::sendPromptToUser(const char light, const char terrain)
{
    if (!m_compactMode) {
        sendToUser("\r\n");
    }
    QByteArray prompt;
    prompt += light;
    prompt += terrain;
    prompt += ">";
    sendToUser(prompt, true);
}

void AbstractParser::performDoorCommand(const DirectionType direction, const DoorActionType action)
{
    Coordinate c = getPosition();

    auto cn = getCommandName(action) + " ";
    auto dn = m_mapData->getDoorName(c, direction).toLatin1();

    bool needdir = false;

    if (direction == DirectionType::UNKNOWN) {
        // If there is only one secret assume that is what needs opening
        auto secretCount = 0;
        for (int i_ = 0; i_ < 6; i_++) {
            const auto i = static_cast<DirectionType>(i_);
            if (getField(c, i, ExitFieldVariant{DoorFlags{DoorFlag::HIDDEN}})) {
                dn = m_mapData->getDoorName(c, i).toLatin1();
                secretCount++;
            }
        }
        if (secretCount == 1 && !dn.isEmpty()) {
            needdir = false;
        } else {
            // Otherwise open any exit
            needdir = true;
            dn = "exit";
        }
    } else if (dn.isEmpty()) {
        needdir = true;
        dn = "exit";
    } else {
        // Check if we need to add a direction to the door name
        for (int i_ = 0; i_ < 6; i_++) {
            const auto i = static_cast<DirectionType>(i_);
            if ((i != direction) && (m_mapData->getDoorName(c, i).toLatin1() == dn)) {
                needdir = true;
            }
        }
    }

    cn += dn;
    if (needdir && direction < DirectionType::NONE) {
        cn += " ";
        cn += Mmapper2Exit::charForDir(static_cast<ExitDirection>(direction));
    }
    cn += m_newLineTerminator;

    if (isOnline()) { // online mode
        emit sendToMud(cn);
        sendToUser("--->" + cn);
        m_overrideSendPrompt = true;
    } else {
        sendToUser("--->" + cn);
        sendToUser("OK.\r\n");
    }
}

void AbstractParser::genericDoorCommand(QString command, const DirectionType direction)
{
    QByteArray cn = emptyByteArray;
    Coordinate c = getPosition();

    auto dn = m_mapData->getDoorName(c, direction).toLatin1();

    bool needdir = false;
    if (dn.isEmpty()) {
        dn = "exit";
        needdir = true;
    } else {
        for (int i_ = 0; i_ < 6; i_++) {
            static const auto i = static_cast<DirectionType>(i_);
            if ((i != direction) && (m_mapData->getDoorName(c, i).toLatin1() == dn)) {
                needdir = true;
            }
        }
    }
    if (direction < DirectionType::NONE) {
        QChar dirChar = Mmapper2Exit::charForDir(static_cast<ExitDirection>(direction));
        cn += dn;
        if (needdir) {
            cn += " ";
            cn += dirChar.toLatin1();
        }
        cn += m_newLineTerminator;
        command = command.replace(QString("$$DOOR_%1$$").arg(dirChar.toUpper()), cn);
    } else if (direction == DirectionType::UNKNOWN) {
        cn += dn + m_newLineTerminator;
        command = command.replace("$$DOOR$$", cn);
    }

    if (isOnline()) { // online mode
        emit sendToMud(command.toLatin1());
        sendToUser("--->" + command);
        m_overrideSendPrompt = true;
    } else {
        sendToUser("--->" + command);
        sendToUser("OK.\r\n");
    }
}

void AbstractParser::nameDoorCommand(const QString &doorname, DirectionType direction)
{
    Coordinate c = getPosition();

    // if (doorname.isEmpty()) toggleExitFlagCommand(ExitFlag::DOOR, direction);
    m_mapData->setDoorName(c, doorname, static_cast<ExitDirection>(direction));
    sendToUser("--->Doorname set to: " + doorname.toLatin1() + "\r\n");
}

void AbstractParser::toggleExitFlagCommand(const ExitFlag flag, const DirectionType direction)
{
    const Coordinate c = getPosition();

    const ExitFieldVariant var{ExitFlags{flag}};
    m_mapData->toggleExitFlag(c, static_cast<ExitDirection>(direction), var);

    const auto toggle = enabledString(getField(c, direction, var));
    const QByteArray flagname = getFlagName(flag);

    sendToUser("--->" + flagname + " exit " + toggle + "\r\n");
}

bool AbstractParser::getField(const Coordinate &c,
                              const DirectionType &direction,
                              const ExitFieldVariant &var) const
{
    return m_mapData->getExitFlag(c, static_cast<ExitDirection>(direction), var);
}

void AbstractParser::toggleDoorFlagCommand(const DoorFlag flag, const DirectionType direction)
{
    const Coordinate c = getPosition();

    const ExitFieldVariant var{DoorFlags{flag}};
    m_mapData->toggleExitFlag(c, static_cast<ExitDirection>(direction), var);

    const auto toggle = enabledString(getField(c, direction, var));
    const QByteArray flagname = getFlagName(flag);
    sendToUser("--->" + flagname + " door " + toggle + "\r\n");
}

void AbstractParser::setRoomFieldCommand(const QVariant &flag, const RoomField field)
{
    const Coordinate c = getPosition();

    m_mapData->setRoomField(c, flag, field);

    sendToUser("--->Room field set\r\n");
}

ExitFlags AbstractParser::getExitFlags(const DirectionType dir) const
{
    return m_exitsFlags.get(static_cast<ExitDirection>(dir));
}

DirectionalLightType AbstractParser::getConnectedRoomFlags(const DirectionType dir) const
{
    return m_connectedRoomFlags.getDirectionalLight(dir);
}

void AbstractParser::setExitFlags(const ExitFlags ef, const DirectionType dir)
{
    m_exitsFlags.set(static_cast<ExitDirection>(dir), ef);
}

void AbstractParser::setConnectedRoomFlag(const DirectionalLightType light, const DirectionType dir)
{
    m_connectedRoomFlags.setDirectionalLight(dir, light);
}

void AbstractParser::toggleRoomFlagCommand(const uint flag, const RoomField field)
{
    const Coordinate c = getPosition();

    m_mapData->toggleRoomFlag(c, flag, field);

    const QString toggle = enabledString(m_mapData->getRoomFlag(c, flag, field));

    sendToUser("--->Room flag " + toggle.toLatin1() + "\r\n");
}

void AbstractParser::printRoomInfo(const RoomFields fieldset)
{
    if (m_mapData->isEmpty())
        return;

    const Coordinate c = getPosition();

    SigRoomSelection rs = m_mapData->select(c);
    if (!rs.getShared()->isEmpty()) {
        const Room *const r = rs.getShared()->values().front();

        // TODO: use QStringBuilder (or std::ostringstream)?
        QString result;

        if (fieldset.contains(RoomField::NAME)) {
            result += r->getName() + "\r\n";
        }
        if (fieldset.contains(RoomField::DESC)) {
            result += r->getStaticDescription();
        }
        if (fieldset.contains(RoomField::DYNAMIC_DESC)) {
            result += r->getDynamicDescription();
        }
        if (fieldset.contains(RoomField::NOTE)) {
            result += "Note: " + r->getNote() + "\r\n";
        }

        sendToUser(result);
    }
}

void AbstractParser::sendGTellToUser(const QByteArray &ba)
{
    sendToUser("\r\n" + ba + "\r\n");
    sendPromptToUser();
}

void AbstractParser::setRoomFieldCommand(const RoomAlignType rat, const RoomField field)
{
    assert(field == RoomField::ALIGN_TYPE);
    setRoomFieldCommand(static_cast<QVariant>(static_cast<uint>(rat)), field);
}

void AbstractParser::setRoomFieldCommand(const RoomLightType rlt, const RoomField field)
{
    assert(field == RoomField::LIGHT_TYPE);
    setRoomFieldCommand(static_cast<QVariant>(static_cast<uint>(rlt)), field);
}

void AbstractParser::setRoomFieldCommand(const RoomPortableType rpt, const RoomField field)
{
    assert(field == RoomField::PORTABLE_TYPE);
    setRoomFieldCommand(static_cast<QVariant>(static_cast<uint>(rpt)), field);
}

void AbstractParser::setRoomFieldCommand(const RoomRidableType rrt, const RoomField field)
{
    assert(field == RoomField::RIDABLE_TYPE);
    setRoomFieldCommand(static_cast<QVariant>(static_cast<uint>(rrt)), field);
}

void AbstractParser::setRoomFieldCommand(const RoomSundeathType rst, const RoomField field)
{
    assert(field == RoomField::SUNDEATH_TYPE);
    setRoomFieldCommand(static_cast<QVariant>(static_cast<uint>(rst)), field);
}

void AbstractParser::toggleRoomFlagCommand(const RoomMobFlag flag, const RoomField field)
{
    assert(field == RoomField::MOB_FLAGS);
    toggleRoomFlagCommand(RoomMobFlags{flag}.asUint32(), field);
}

void AbstractParser::toggleRoomFlagCommand(const RoomLoadFlag flag, const RoomField field)
{
    assert(field == RoomField::LOAD_FLAGS);
    toggleRoomFlagCommand(RoomLoadFlags{flag}.asUint32(), field);
}

void AbstractParser::printRoomInfo(const RoomField field)
{
    printRoomInfo(RoomFields{field});
}
