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

#include "configuration.h"

#include <cassert>
#include <QByteArray>
#include <QChar>
#include <QHostInfo>
#include <QString>
#include <QStringList>

#include "../global/utils.h"
#include "../pandoragroup/mmapper2group.h"

static const char *getPlatformEditor()
{
    switch (CURRENT_PLATFORM) {
    case Platform::Win32:
        return "notepad";

    case Platform::Mac:
        return "open -W -n -t";

    case Platform::Linux:
        // add .txt extension and use xdg-open instead?
        // or if xdg-open doesn't exist, then you can
        // look for gnome-open, mate-open, etc.
        return "gedit";

    case Platform::Unknown:
    default:
        return "";
    }
}

// REVISIT: Why not just check the path of the binary?
// Windows: GetModuleFileName(NULL, ...).
// Linux: readlink /proc/self/exe
// Mac: _NSGetExecutablePath
// All: argv[0] may also work as a last resort.
// OR maybe: github.com/gpakosz/whereami
static QByteArray getPlatformLoadDir()
{
    switch (CURRENT_PLATFORM) {
    case Platform::Win32:
        return "C:/Program Files (x86)/MMapper";

    case Platform::Linux:
        return qgetenv("SNAP").append("/usr/share/games/mmapper");

    case Platform::Mac:
    case Platform::Unknown:
    default:
        return "";
    }
}

Configuration::Configuration()
{
    read(); // read the settings or set them to the default values
}

/*
 * TODO: Make a dialog asking if the user wants to import settings
 * from an older version of MMapper, and then change the organization name
 * to reflect that it's an open source project that's not Caligor's
 * personal project anymore.
 *
 * Also, don't use space, because it will be a file name on disk.
 */
#define ConstString static constexpr const char *const
ConstString SETTINGS_ORGANIZATION = "Caligor soft";
ConstString SETTINGS_APPLICATION = "MMapper2";

/* QSettings has a deleted copy constructor, so we can't write
 * `auto conf = getSettings();` until C++17's guaranteed RVO
 * elides the copy/move constructors. */
/* Also note: Avoiding using global QSettings& getSettings()
 * because the QSettings object maintains some state. */
#define SETTINGS(conf) QSettings conf(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)

ConstString GRP_AUTO_LOAD_WORLD = "Auto load world";
ConstString GRP_CANVAS = "Canvas";
ConstString GRP_CONNECTION = "Connection";
ConstString GRP_GENERAL = "General";
ConstString GRP_GROUP_MANAGER = "Group Manager";
ConstString GRP_INFOMARKS_DIALOG = "InfoMarks Dialog";
ConstString GRP_INTEGRATED_MUD_CLIENT = "Integrated Mud Client";
ConstString GRP_MUME_CLIENT_PROTOCOL = "Mume client protocol";
ConstString GRP_MUME_CLOCK = "Mume Clock";
ConstString GRP_MUME_NATIVE = "Mume native";
ConstString GRP_PARSER = "Parser";
ConstString GRP_PATH_MACHINE = "Path Machine";
ConstString GRP_ROOMEDIT_DIALOG = "RoomEdit Dialog";

ConstString KEY_ABSOLUTE_PATH_ACCEPTANCE = "absolute path acceptance";
ConstString KEY_ALWAYS_ON_TOP = "Always On Top";
ConstString KEY_AUTHORIZATION_REQUIRED = "Authorization required";
ConstString KEY_AUTHORIZED_SECRETS = "Authorized secrets";
ConstString KEY_AUTO_LOAD = "Auto load";
ConstString KEY_AUTO_RESIZE_TERMINAL = "Auto resize terminal";
ConstString KEY_BACKGROUND_COLOR = "Background color";
ConstString KEY_RSA_X509_CERTIFICATE = "RSA X509 certificate";
ConstString KEY_CHARACTER_ENCODING = "Character encoding";
ConstString KEY_CHARACTER_NAME = "character name";
ConstString KEY_CLEAR_INPUT_ON_ENTER = "Clear input on enter";
ConstString KEY_COLOR = "color";
ConstString KEY_COLUMNS = "Columns";
ConstString KEY_COMMAND_PREFIX_CHAR = "Command prefix character";
ConstString KEY_CORRECT_POSITION_BONUS = "correct position bonus";
ConstString KEY_DISPLAY_CLOCK = "Display clock";
ConstString KEY_DRAW_DOOR_NAMES = "Draw door names";
ConstString KEY_DRAW_NO_MATCH_EXITS = "Draw no match exits";
ConstString KEY_DRAW_NOT_MAPPED_EXITS = "Draw not mapped exits";
ConstString KEY_DRAW_UPPER_LAYERS_TEXTURED = "Draw upper layers textured";
ConstString KEY_EMULATED_EXITS = "Emulated Exits";
ConstString KEY_EXTERNAL_EDITOR_COMMAND = "External editor command";
ConstString KEY_FILE_NAME = "File name";
ConstString KEY_FONT = "Font";
ConstString KEY_FOREGROUND_COLOR = "Foreground color";
ConstString KEY_GROUP_TELL_ANSI_COLOR = "Group tell ansi color";
ConstString KEY_HOST = "host";
ConstString KEY_LAST_MAP_LOAD_DIRECTORY = "Last map load directory";
ConstString KEY_LINES_OF_INPUT_HISTORY = "Lines of input history";
ConstString KEY_LINES_OF_SCROLLBACK = "Lines of scrollback";
ConstString KEY_LOCAL_PORT = "local port";
ConstString KEY_LOCAL_PORT_NUMBER = "Local port number";
ConstString KEY_LOCK_GROUP = "Lock current group members";
ConstString KEY_MAP_MODE = "Map Mode";
ConstString KEY_MAXIMUM_NUMBER_OF_PATHS = "maximum number of paths";
ConstString KEY_MOVE_FORCE_PATTERNS_FOR_XML = "Move force patterns for XML";
ConstString KEY_MULTIPLE_CONNECTIONS_PENALTY = "multiple connections penalty";
ConstString KEY_MUME_START_EPOCH = "Mume start epoch";
ConstString KEY_NO_LAUNCH_PANEL = "No launch panel";
ConstString KEY_NO_ROOM_DESCRIPTION_PATTERNS = "No room description patterns";
ConstString KEY_NO_SPLASH = "No splash screen";
ConstString KEY_NUMBER_OF_ANTI_ALIASING_SAMPLES = "Number of anti-aliasing samples";
ConstString KEY_PROXY_THREADED = "Proxy Threaded";
ConstString KEY_PROXY_CONNECTION_STATUS = "Proxy connection status";
ConstString KEY_RELATIVE_PATH_ACCEPTANCE = "relative path acceptance";
ConstString KEY_REMOTE_EDITING_AND_VIEWING = "Remote editing and viewing";
ConstString KEY_REMOTE_PORT_NUMBER = "Remote port number";
ConstString KEY_REMOTE_PORT = "remote port";
ConstString KEY_REMOVE_XML_TAGS = "Remove XML tags";
ConstString KEY_ROOM_CREATION_PENALTY = "room creation penalty";
ConstString KEY_ROOM_DESC_ANSI_COLOR = "Room desc ansi color";
ConstString KEY_ROOM_MATCHING_TOLERANCE = "room matching tolerance";
ConstString KEY_ROOM_NAME_ANSI_COLOR = "Room name ansi color";
ConstString KEY_ROWS = "Rows";
ConstString KEY_RSA_PRIVATE_KEY = "RSA private key";
ConstString KEY_RULES_WARNING = "rules warning";
ConstString KEY_RUN_FIRST_TIME = "Run first time";
ConstString KEY_SECRET_METADATA = "Secret metadata";
ConstString KEY_SERVER_NAME = "Server name";
ConstString KEY_SHARE_SELF = "share self";
ConstString KEY_SHOW_HIDDEN_EXIT_FLAGS = "Show hidden exit flags";
ConstString KEY_SHOW_NOTES = "Show notes";
ConstString KEY_SHOW_UPDATED_ROOMS = "Show updated rooms";
ConstString KEY_STATE = "state";
ConstString KEY_TAB_COMPLETION_DICTIONARY_SIZE = "Tab completion dictionary size";
ConstString KEY_TLS_ENCRYPTION = "TLS encryption";
ConstString KEY_USE_INTERNAL_EDITOR = "Use internal editor";
ConstString KEY_USE_SOFTWARE_OPENGL = "Use software OpenGL";
ConstString KEY_USE_TRILINEAR_FILTERING = "Use trilinear filtering";
ConstString KEY_WINDOW_GEOMETRY = "Window Geometry";
ConstString KEY_WINDOW_STATE = "Window State";

static bool isValidAnsi(const QString &input)
{
    static constexpr const auto MAX = static_cast<uint32_t>(std::numeric_limits<uint8_t>::max());

    if (!input.startsWith("[") || !input.endsWith("m")) {
        return false;
    }

    for (const auto &part : input.mid(1, input.length() - 2).split(";")) {
        for (const QChar &c : part)
            if (!c.isDigit())
                return false;
        bool ok = false;
        const auto n = part.toUInt(&ok, 10);
        if (!ok || n > MAX)
            return false;
    }

    return true;
}

static bool isValidGroupManagerState(const GroupManagerState state)
{
    switch (state) {
    case GroupManagerState::Off:
    case GroupManagerState::Client:
    case GroupManagerState::Server:
        return true;
    }
    return false;
}

static bool isValidMapMode(const MapMode mode)
{
    switch (mode) {
    case MapMode::PLAY:
    case MapMode::MAP:
    case MapMode::OFFLINE:
        return true;
    }
    return false;
}

static bool isValidCharacterEncoding(const CharacterEncoding encoding)
{
    switch (encoding) {
    case CharacterEncoding::ASCII:
    case CharacterEncoding::LATIN1:
    case CharacterEncoding::UTF8:
        return true;
    }
    return false;
}

static QString sanitizeAnsi(const QString &input, const QString &defaultValue)
{
    assert(isValidAnsi(defaultValue));
    if (isValidAnsi(input))
        return input;

    if (!input.isEmpty())
        qWarning() << "invalid ansi code: " << input;

    return defaultValue;
}

static GroupManagerState sanitizeGroupManagerState(const int input)
{
    const auto state = static_cast<GroupManagerState>(input);
    if (isValidGroupManagerState(state))
        return state;

    qWarning() << "invalid GroupManagerState:" << input;
    return GroupManagerState::Off;
}

static MapMode sanitizeMapMode(const uint32_t input)
{
    const auto mode = static_cast<MapMode>(input);
    if (isValidMapMode(mode))
        return mode;

    qWarning() << "invalid MapMode:" << input;
    return MapMode::PLAY;
}

static CharacterEncoding sanitizeCharacterEncoding(const uint32_t input)
{
    const auto encoding = static_cast<CharacterEncoding>(input);
    if (isValidCharacterEncoding(encoding))
        return encoding;

    qWarning() << "invalid CharacterEncoding:" << input;
    return CharacterEncoding::LATIN1;
}

static uint16_t sanitizeUint16(const int input, const uint16_t defaultValue)
{
    static constexpr const auto MIN = static_cast<int>(std::numeric_limits<uint16_t>::min());
    static constexpr const auto MAX = static_cast<int>(std::numeric_limits<uint16_t>::max());

    if (isClamped(input, MIN, MAX))
        return static_cast<uint16_t>(input);

    qWarning() << "invalid uint16: " << input;
    return defaultValue;
}

#define GROUP_CALLBACK(callback, name, ref) \
    do { \
        conf.beginGroup(name); \
        ref.callback(conf); \
        conf.endGroup(); \
    } while (false)

#define FOREACH_CONFIG_GROUP(callback) \
    do { \
        GROUP_CALLBACK(callback, GRP_GENERAL, general); \
        GROUP_CALLBACK(callback, GRP_CONNECTION, connection); \
        GROUP_CALLBACK(callback, GRP_CANVAS, canvas); \
        GROUP_CALLBACK(callback, GRP_AUTO_LOAD_WORLD, autoLoad); \
        GROUP_CALLBACK(callback, GRP_PARSER, parser); \
        GROUP_CALLBACK(callback, GRP_MUME_CLIENT_PROTOCOL, mumeClientProtocol); \
        GROUP_CALLBACK(callback, GRP_MUME_NATIVE, mumeNative); \
        GROUP_CALLBACK(callback, GRP_PATH_MACHINE, pathMachine); \
        GROUP_CALLBACK(callback, GRP_GROUP_MANAGER, groupManager); \
        GROUP_CALLBACK(callback, GRP_MUME_CLOCK, mumeClock); \
        GROUP_CALLBACK(callback, GRP_INTEGRATED_MUD_CLIENT, integratedClient); \
        GROUP_CALLBACK(callback, GRP_INFOMARKS_DIALOG, infoMarksDialog); \
        GROUP_CALLBACK(callback, GRP_ROOMEDIT_DIALOG, roomEditDialog); \
    } while (false)

void Configuration::read()
{
    SETTINGS(conf);
    FOREACH_CONFIG_GROUP(read);
}

void Configuration::write() const
{
    SETTINGS(conf);
    FOREACH_CONFIG_GROUP(write);
}

void Configuration::reset()
{
    SETTINGS(conf);
    conf.clear();
    FOREACH_CONFIG_GROUP(read);
}

#undef FOREACH_CONFIG_GROUP
#undef GROUP_CALLBACK

void Configuration::GeneralSettings::read(QSettings &conf)
{
    firstRun = conf.value(KEY_RUN_FIRST_TIME, true).toBool();
    /*
     * REVISIT: It's basically impossible to verify that this state is valid,
     * because we have no idea what it contains!
     *
     * This setting is inherently non-portable between OSes
     * (and possibly even window managers), so it doesn't belong here!
     *
     * If we're going to save it, then we should probably least checksum it
     * (or better yet sign it), and record the OS config, so that we won't
     * try to apply Windows settings to Mac, or Gnome settings to KDE, etc?
     */
    windowGeometry = conf.value(KEY_WINDOW_GEOMETRY).toByteArray();
    windowState = conf.value(KEY_WINDOW_STATE).toByteArray();
    alwaysOnTop = conf.value(KEY_ALWAYS_ON_TOP, false).toBool();
    mapMode = sanitizeMapMode(
        conf.value(KEY_MAP_MODE, static_cast<uint32_t>(MapMode::PLAY)).toUInt());
    noSplash = conf.value(KEY_NO_SPLASH, false).toBool();
    noLaunchPanel = conf.value(KEY_NO_LAUNCH_PANEL, false).toBool();
    characterEncoding = sanitizeCharacterEncoding(
        conf.value(KEY_CHARACTER_ENCODING, static_cast<uint32_t>(CharacterEncoding::LATIN1))
            .toUInt());
}

void Configuration::ConnectionSettings::read(QSettings &conf)
{
    static constexpr const int DEFAULT_PORT = 4242;

    remoteServerName = conf.value(KEY_SERVER_NAME, "mume.org").toString();
    remotePort = sanitizeUint16(conf.value(KEY_REMOTE_PORT_NUMBER, DEFAULT_PORT).toInt(),
                                static_cast<uint16_t>(DEFAULT_PORT));
    localPort = sanitizeUint16(conf.value(KEY_LOCAL_PORT_NUMBER, DEFAULT_PORT).toInt(),
                               static_cast<uint16_t>(DEFAULT_PORT));
    tlsEncryption = NO_OPEN_SSL ? false : conf.value(KEY_TLS_ENCRYPTION, true).toBool();
    proxyThreaded = conf.value(KEY_PROXY_THREADED, false).toBool();
    proxyConnectionStatus = conf.value(KEY_PROXY_CONNECTION_STATUS, true).toBool();

    // News 2340, changing domain from fire.pvv.org to mume.org:
    auto &remote = remoteServerName;
    if (remote.contains("pvv.org")) {
        remote = "mume.org";
    }
}

void Configuration::CanvasSettings::read(QSettings &conf)
{
    showUpdated = conf.value(KEY_SHOW_UPDATED_ROOMS, false).toBool();
    drawNotMappedExits = conf.value(KEY_DRAW_NOT_MAPPED_EXITS, true).toBool();
    drawNoMatchExits = conf.value(KEY_DRAW_NO_MATCH_EXITS, false).toBool();
    drawUpperLayersTextured = conf.value(KEY_DRAW_UPPER_LAYERS_TEXTURED, false).toBool();
    drawDoorNames = conf.value(KEY_DRAW_DOOR_NAMES, true).toBool();
    backgroundColor = QColor(conf.value(KEY_BACKGROUND_COLOR, QColor("#2e3436").name()).toString());
    antialiasingSamples = conf.value(KEY_NUMBER_OF_ANTI_ALIASING_SAMPLES, 0).toInt();
    trilinearFiltering = conf.value(KEY_USE_TRILINEAR_FILTERING, false).toBool();
    softwareOpenGL = conf.value(KEY_USE_SOFTWARE_OPENGL, getCurrentPlatform() == Platform::Win32)
                         .toBool();
}

void Configuration::AutoLoadSettings::read(QSettings &conf)
{
    autoLoadMap = conf.value(KEY_AUTO_LOAD, false).toBool();
    fileName = conf.value(KEY_FILE_NAME, "arda.mm2").toString();
    lastMapDirectory = conf.value(KEY_LAST_MAP_LOAD_DIRECTORY, getPlatformLoadDir()).toString();
}

void Configuration::ParserSettings::read(QSettings &conf)
{
    static constexpr const char *const ANSI_GREEN = "[32m";
    static constexpr const char *const ANSI_RESET = "[0m";

    roomNameColor = sanitizeAnsi(conf.value(KEY_ROOM_NAME_ANSI_COLOR, ANSI_GREEN).toString(),
                                 QString(ANSI_GREEN));
    roomDescColor = sanitizeAnsi(conf.value(KEY_ROOM_DESC_ANSI_COLOR, ANSI_RESET).toString(),
                                 QString(ANSI_RESET));
    prefixChar = conf.value(KEY_COMMAND_PREFIX_CHAR, QChar::fromLatin1('_')).toChar().toLatin1();
    removeXmlTags = conf.value(KEY_REMOVE_XML_TAGS, true).toBool();
    moveForcePatternsList = conf.value(KEY_MOVE_FORCE_PATTERNS_FOR_XML).toStringList();
    noDescriptionPatternsList = conf.value(KEY_NO_ROOM_DESCRIPTION_PATTERNS).toStringList();

    auto &forced = moveForcePatternsList;
    if (forced.isEmpty()) {
        forced.append("#?leads you out");
        forced.append("#<Suddenly an explosion of ancient rhymes");
        forced.append("#=You board the ferry."); // Grey Havens
        forced.append("#=You leave the ferry.");
    }

    auto &nodesc = noDescriptionPatternsList;
    if (nodesc.isEmpty()) {
        nodesc.append("#=It is pitch black...");
        nodesc.append("#=You just see a dense fog around you...");
    }
}

void Configuration::MumeClientProtocolSettings::read(QSettings &conf)
{
    remoteEditing = conf.value(KEY_REMOTE_EDITING_AND_VIEWING, true).toBool();
    internalRemoteEditor = conf.value(KEY_USE_INTERNAL_EDITOR, true).toBool();
    externalRemoteEditorCommand = conf.value(KEY_EXTERNAL_EDITOR_COMMAND, getPlatformEditor())
                                      .toString();
}

void Configuration::MumeNativeSettings::read(QSettings &conf)
{
    emulatedExits = conf.value(KEY_EMULATED_EXITS, true).toBool();
    showHiddenExitFlags = conf.value(KEY_SHOW_HIDDEN_EXIT_FLAGS, true).toBool();
    showNotes = conf.value(KEY_SHOW_NOTES, true).toBool();
}

void Configuration::PathMachineSettings::read(QSettings &conf)
{
    acceptBestRelative = conf.value(KEY_RELATIVE_PATH_ACCEPTANCE, 25).toDouble();
    acceptBestAbsolute = conf.value(KEY_ABSOLUTE_PATH_ACCEPTANCE, 6).toDouble();
    newRoomPenalty = conf.value(KEY_ROOM_CREATION_PENALTY, 5).toDouble();
    correctPositionBonus = conf.value(KEY_CORRECT_POSITION_BONUS, 5).toDouble();
    multipleConnectionsPenalty = conf.value(KEY_MULTIPLE_CONNECTIONS_PENALTY, 2.0).toDouble();
    maxPaths = std::max(0, conf.value(KEY_MAXIMUM_NUMBER_OF_PATHS, 1000).toInt());
    matchingTolerance = std::max(0, conf.value(KEY_ROOM_MATCHING_TOLERANCE, 8).toInt());
}

void Configuration::GroupManagerSettings::read(QSettings &conf)
{
    static constexpr const int DEFAULT_PORT = 4243;
    static constexpr const char *const ANSI_GREEN = "[32m";

    state = sanitizeGroupManagerState(
        conf.value(KEY_STATE, static_cast<int>(GroupManagerState::Off)).toInt());
    localPort = sanitizeUint16(conf.value(KEY_LOCAL_PORT, DEFAULT_PORT).toInt(), DEFAULT_PORT);
    remotePort = sanitizeUint16(conf.value(KEY_REMOTE_PORT, DEFAULT_PORT).toInt(), DEFAULT_PORT);
    host = conf.value(KEY_HOST, "localhost").toByteArray();
    charName = conf.value(KEY_CHARACTER_NAME, QHostInfo::localHostName()).toByteArray();
    shareSelf = conf.value(KEY_SHARE_SELF, true).toBool();
    color = QColor(conf.value(KEY_COLOR, "#ffff00").toString());
    rulesWarning = conf.value(KEY_RULES_WARNING, true).toBool();
    certificate = conf.value(KEY_RSA_X509_CERTIFICATE, "").toByteArray();
    privateKey = conf.value(KEY_RSA_PRIVATE_KEY, "").toByteArray();
    authorizedSecrets = conf.value(KEY_AUTHORIZED_SECRETS, QStringList()).toStringList();
    requireAuth = NO_OPEN_SSL ? false : conf.value(KEY_AUTHORIZATION_REQUIRED, false).toBool();
    geometry = conf.value(KEY_WINDOW_GEOMETRY).toByteArray();
    secretMetadata = conf.value(KEY_SECRET_METADATA).toMap();
    groupTellColor = conf.value(KEY_GROUP_TELL_ANSI_COLOR, QString(ANSI_GREEN)).toString();
    lockGroup = conf.value(KEY_LOCK_GROUP, false).toBool();
}

void Configuration::MumeClockSettings::read(QSettings &conf)
{
    // NOTE: old values might be stored as int32
    startEpoch = conf.value(KEY_MUME_START_EPOCH, 1517443173).toLongLong();
    display = conf.value(KEY_DISPLAY_CLOCK, true).toBool();
}

void Configuration::IntegratedMudClientSettings::read(QSettings &conf)
{
    font = conf.value(KEY_FONT, "").toString();
    backgroundColor = conf.value(KEY_BACKGROUND_COLOR, QColor(Qt::black).name()).toString();
    foregroundColor = conf.value(KEY_FOREGROUND_COLOR, QColor(Qt::lightGray).name()).toString();
    columns = conf.value(KEY_COLUMNS, 80).toInt();
    rows = conf.value(KEY_ROWS, 24).toInt();
    linesOfScrollback = conf.value(KEY_LINES_OF_SCROLLBACK, 10000).toInt();
    linesOfInputHistory = conf.value(KEY_LINES_OF_INPUT_HISTORY, 100).toInt();
    tabCompletionDictionarySize = conf.value(KEY_TAB_COMPLETION_DICTIONARY_SIZE, 100).toInt();
    clearInputOnEnter = conf.value(KEY_CLEAR_INPUT_ON_ENTER, true).toBool();
    autoResizeTerminal = conf.value(KEY_AUTO_RESIZE_TERMINAL, false).toBool();
    geometry = conf.value(KEY_WINDOW_GEOMETRY).toByteArray();
}

void Configuration::InfoMarksDialog::read(QSettings &conf)
{
    geometry = conf.value(KEY_WINDOW_GEOMETRY).toByteArray();
}

void Configuration::RoomEditDialog::read(QSettings &conf)
{
    geometry = conf.value(KEY_WINDOW_GEOMETRY).toByteArray();
}

void Configuration::GeneralSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_RUN_FIRST_TIME, false);
    conf.setValue(KEY_WINDOW_GEOMETRY, windowGeometry);
    conf.setValue(KEY_WINDOW_STATE, windowState);
    conf.setValue(KEY_ALWAYS_ON_TOP, alwaysOnTop);
    conf.setValue(KEY_MAP_MODE, static_cast<uint32_t>(mapMode));
    conf.setValue(KEY_NO_SPLASH, noSplash);
    conf.setValue(KEY_NO_LAUNCH_PANEL, noLaunchPanel);
    conf.setValue(KEY_CHARACTER_ENCODING, static_cast<uint32_t>(characterEncoding));
}

void Configuration::ConnectionSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_SERVER_NAME, remoteServerName);
    conf.setValue(KEY_REMOTE_PORT_NUMBER, static_cast<int>(remotePort));
    conf.setValue(KEY_LOCAL_PORT_NUMBER, static_cast<int>(localPort));
    conf.setValue(KEY_TLS_ENCRYPTION, tlsEncryption);
    conf.setValue(KEY_PROXY_THREADED, proxyThreaded);
    conf.setValue(KEY_PROXY_CONNECTION_STATUS, proxyConnectionStatus);
}

void Configuration::CanvasSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_SHOW_UPDATED_ROOMS, showUpdated);
    conf.setValue(KEY_DRAW_NOT_MAPPED_EXITS, drawNotMappedExits);
    conf.setValue(KEY_DRAW_NO_MATCH_EXITS, drawNoMatchExits);
    conf.setValue(KEY_DRAW_UPPER_LAYERS_TEXTURED, drawUpperLayersTextured);
    conf.setValue(KEY_DRAW_DOOR_NAMES, drawDoorNames);
    conf.setValue(KEY_BACKGROUND_COLOR, backgroundColor.name());
    conf.setValue(KEY_NUMBER_OF_ANTI_ALIASING_SAMPLES, antialiasingSamples);
    conf.setValue(KEY_USE_TRILINEAR_FILTERING, trilinearFiltering);
    conf.setValue(KEY_USE_SOFTWARE_OPENGL, softwareOpenGL);
}

void Configuration::AutoLoadSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_AUTO_LOAD, autoLoadMap);
    conf.setValue(KEY_FILE_NAME, fileName);
    conf.setValue(KEY_LAST_MAP_LOAD_DIRECTORY, lastMapDirectory);
}

void Configuration::ParserSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_ROOM_NAME_ANSI_COLOR, roomNameColor);
    conf.setValue(KEY_ROOM_DESC_ANSI_COLOR, roomDescColor);
    conf.setValue(KEY_REMOVE_XML_TAGS, removeXmlTags);
    conf.setValue(KEY_COMMAND_PREFIX_CHAR, QChar::fromLatin1(prefixChar));
    conf.setValue(KEY_MOVE_FORCE_PATTERNS_FOR_XML, moveForcePatternsList);
    conf.setValue(KEY_NO_ROOM_DESCRIPTION_PATTERNS, noDescriptionPatternsList);
}

void Configuration::MumeNativeSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_EMULATED_EXITS, emulatedExits);
    conf.setValue(KEY_SHOW_HIDDEN_EXIT_FLAGS, showHiddenExitFlags);
    conf.setValue(KEY_SHOW_NOTES, showNotes);
}

void Configuration::MumeClientProtocolSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_REMOTE_EDITING_AND_VIEWING, remoteEditing);
    conf.setValue(KEY_USE_INTERNAL_EDITOR, internalRemoteEditor);
    conf.setValue(KEY_EXTERNAL_EDITOR_COMMAND, externalRemoteEditorCommand);
}

void Configuration::PathMachineSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_RELATIVE_PATH_ACCEPTANCE, acceptBestRelative);
    conf.setValue(KEY_ABSOLUTE_PATH_ACCEPTANCE, acceptBestAbsolute);
    conf.setValue(KEY_ROOM_CREATION_PENALTY, newRoomPenalty);
    conf.setValue(KEY_CORRECT_POSITION_BONUS, correctPositionBonus);
    conf.setValue(KEY_MAXIMUM_NUMBER_OF_PATHS, maxPaths);
    conf.setValue(KEY_ROOM_MATCHING_TOLERANCE, matchingTolerance);
    conf.setValue(KEY_MULTIPLE_CONNECTIONS_PENALTY, multipleConnectionsPenalty);
}

void Configuration::GroupManagerSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_STATE, static_cast<int>(state));
    // Note: There's no QVariant(quint16) constructor.
    conf.setValue(KEY_LOCAL_PORT, static_cast<int>(localPort));
    conf.setValue(KEY_REMOTE_PORT, static_cast<int>(remotePort));
    conf.setValue(KEY_HOST, host);
    conf.setValue(KEY_CHARACTER_NAME, charName);
    conf.setValue(KEY_SHARE_SELF, shareSelf);
    conf.setValue(KEY_COLOR, color.name());
    conf.setValue(KEY_RULES_WARNING, rulesWarning);
    conf.setValue(KEY_RSA_X509_CERTIFICATE, certificate);
    conf.setValue(KEY_RSA_PRIVATE_KEY, privateKey);
    conf.setValue(KEY_AUTHORIZED_SECRETS, authorizedSecrets);
    conf.setValue(KEY_AUTHORIZATION_REQUIRED, requireAuth);
    conf.setValue(KEY_WINDOW_GEOMETRY, geometry);
    conf.setValue(KEY_SECRET_METADATA, secretMetadata);
    conf.setValue(KEY_GROUP_TELL_ANSI_COLOR, groupTellColor);
    conf.setValue(KEY_LOCK_GROUP, lockGroup);
}

void Configuration::MumeClockSettings::write(QSettings &conf) const
{
    // Note: There's no QVariant(int64_t) constructor.
    conf.setValue(KEY_MUME_START_EPOCH, static_cast<qlonglong>(startEpoch));
    conf.setValue(KEY_DISPLAY_CLOCK, display);
}

void Configuration::IntegratedMudClientSettings::write(QSettings &conf) const
{
    conf.setValue(KEY_FONT, font);
    conf.setValue(KEY_BACKGROUND_COLOR, backgroundColor.name());
    conf.setValue(KEY_FOREGROUND_COLOR, foregroundColor.name());
    conf.setValue(KEY_COLUMNS, columns);
    conf.setValue(KEY_ROWS, rows);
    conf.setValue(KEY_LINES_OF_SCROLLBACK, linesOfScrollback);
    conf.setValue(KEY_LINES_OF_INPUT_HISTORY, linesOfInputHistory);
    conf.setValue(KEY_TAB_COMPLETION_DICTIONARY_SIZE, tabCompletionDictionarySize);
    conf.setValue(KEY_CLEAR_INPUT_ON_ENTER, clearInputOnEnter);
    conf.setValue(KEY_AUTO_RESIZE_TERMINAL, autoResizeTerminal);
    conf.setValue(KEY_WINDOW_GEOMETRY, geometry);
}

void Configuration::InfoMarksDialog::write(QSettings &conf) const
{
    conf.setValue(KEY_WINDOW_GEOMETRY, geometry);
}

void Configuration::RoomEditDialog::write(QSettings &conf) const
{
    conf.setValue(KEY_WINDOW_GEOMETRY, geometry);
}

Configuration &setConfig()
{
    static Configuration conf{};
    return conf;
}

const Configuration &getConfig()
{
    return setConfig();
}
