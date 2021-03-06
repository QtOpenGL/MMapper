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

#include "proxy.h"

#include <stdexcept>
#include <QByteArray>
#include <QMessageLogContext>
#include <QObject>
#include <QScopedPointer>
#include <QTcpSocket>

#include "../configuration/configuration.h"
#include "../display/prespammedpath.h"
#include "../global/io.h"
#include "../mpi/mpifilter.h"
#include "../mpi/remoteedit.h"
#include "../pandoragroup/mmapper2group.h"
#include "../parser/abstractparser.h"
#include "../parser/mumexmlparser.h"
#include "../pathmachine/mmapper2pathmachine.h"
#include "MudTelnet.h"
#include "UserTelnet.h"
#include "connectionlistener.h"
#include "mumesocket.h"
#include "telnetfilter.h"

class SigParseEvent;

Proxy::Proxy(MapData *const md,
             Mmapper2PathMachine *const pm,
             PrespammedPath *const pp,
             Mmapper2Group *const gm,
             MumeClock *mc,
             qintptr &socketDescriptor,
             ConnectionListener *const listener)
    : QObject(nullptr)
    , m_socketDescriptor(socketDescriptor)
    , m_mapData(md)
    , m_pathMachine(pm)
    , m_prespammedPath(pp)
    , m_groupManager(gm)
    , m_mumeClock(mc)
    , m_listener(listener)
{
    // REVISIT: Move instantiation to MainWindow
    m_remoteEdit = new RemoteEdit(m_listener->parent());

#ifdef PROXY_STREAM_DEBUG_INPUT_TO_FILE
    QString fileName = "proxy_debug.dat";

    file = new QFile(fileName);

    if (!file->open(QFile::WriteOnly))
        return;

    debugStream = new QDataStream(file);
#endif
}

Proxy::~Proxy()
{
#ifdef PROXY_STREAM_DEBUG_INPUT_TO_FILE
    file->close();
#endif
    if (m_userSocket != nullptr) {
        m_userSocket->flush();
        m_userSocket->disconnectFromHost();
        m_userSocket->deleteLater();
    }
    if (m_mudSocket != nullptr) {
        m_mudSocket->disconnectFromHost();
        m_mudSocket->deleteLater();
    }
    m_remoteEdit->deleteLater(); // Owned by MainWindow
    delete m_userTelnet;
    delete m_mudTelnet;
    delete m_telnetFilter;
    delete m_mpiFilter;
    delete m_parserXml;
}

void Proxy::start()
{
    connect(this,
            SIGNAL(log(const QString &, const QString &)),
            m_listener->parent(),
            SLOT(log(const QString &, const QString &)));

    m_userSocket.reset(new QTcpSocket(this));
    if (!m_userSocket->setSocketDescriptor(m_socketDescriptor)) {
        m_userSocket.reset(nullptr);
        deleteLater();
        return;
    }
    m_userSocket->setSocketOption(QAbstractSocket::LowDelayOption, true);
    m_userSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);

    connect(m_userSocket.data(),
            &QAbstractSocket::disconnected,
            this,
            &Proxy::userTerminatedConnection);
    connect(m_userSocket.data(), &QIODevice::readyRead, this, &Proxy::processUserStream);

    m_userTelnet = new UserTelnet(this);
    m_mudTelnet = new MudTelnet(this);
    m_telnetFilter = new TelnetFilter(this);

    connect(m_userTelnet,
            &UserTelnet::analyzeUserStream,
            m_telnetFilter,
            &TelnetFilter::onAnalyzeUserStream);
    connect(m_userTelnet, &UserTelnet::sendToSocket, this, &Proxy::sendToUser);
    connect(m_userTelnet, &UserTelnet::relayNaws, m_mudTelnet, &MudTelnet::onRelayNaws);
    connect(m_userTelnet, &UserTelnet::relayTermType, m_mudTelnet, &MudTelnet::onRelayTermType);

    connect(m_mudTelnet,
            &MudTelnet::analyzeMudStream,
            m_telnetFilter,
            &TelnetFilter::onAnalyzeMudStream);
    connect(m_mudTelnet, &MudTelnet::sendToSocket, this, &Proxy::sendToMud);
    connect(m_mudTelnet, &MudTelnet::relayEchoMode, m_userTelnet, &UserTelnet::onRelayEchoMode);

    connect(this, &Proxy::analyzeUserStream, m_userTelnet, &UserTelnet::onAnalyzeUserStream);

    m_mpiFilter = new MpiFilter(this);
    connect(m_telnetFilter,
            &TelnetFilter::parseNewMudInput,
            m_mpiFilter,
            &MpiFilter::analyzeNewMudInput);
    connect(m_mpiFilter, &MpiFilter::sendToMud, m_mudTelnet, &MudTelnet::onSendToMud);
    connect(m_mpiFilter, &MpiFilter::editMessage, m_remoteEdit, &RemoteEdit::remoteEdit);
    connect(m_mpiFilter, &MpiFilter::viewMessage, m_remoteEdit, &RemoteEdit::remoteView);
    connect(m_remoteEdit, &RemoteEdit::sendToSocket, m_mudTelnet, &MudTelnet::onSendToMud);

    m_parserXml = new MumeXmlParser(m_mapData, m_mumeClock, this);
    connect(m_mpiFilter,
            &MpiFilter::parseNewMudInput,
            m_parserXml,
            &MumeXmlParser::parseNewMudInput);
    connect(m_telnetFilter,
            &TelnetFilter::parseNewUserInput,
            m_parserXml,
            &MumeXmlParser::parseNewUserInput);
    connect(m_parserXml, &MumeXmlParser::sendToMud, m_mudTelnet, &MudTelnet::onSendToMud);
    connect(m_parserXml, &MumeXmlParser::sig_sendToUser, m_userTelnet, &UserTelnet::onSendToUser);

    connect(m_parserXml,
            static_cast<void (MumeXmlParser::*)(const SigParseEvent &)>(&MumeXmlParser::event),
            m_pathMachine,
            &Mmapper2PathMachine::event);
    connect(m_parserXml,
            &AbstractParser::releaseAllPaths,
            m_pathMachine,
            &PathMachine::releaseAllPaths);
    connect(m_parserXml, &AbstractParser::showPath, m_prespammedPath, &PrespammedPath::setPath);
    connect(m_parserXml,
            SIGNAL(log(const QString &, const QString &)),
            m_listener->parent(),
            SLOT(log(const QString &, const QString &)));
    connect(m_userSocket.data(),
            &QAbstractSocket::disconnected,
            m_parserXml,
            &AbstractParser::reset);

    // Group Manager Support
    connect(m_parserXml,
            &MumeXmlParser::sendScoreLineEvent,
            m_groupManager,
            &Mmapper2Group::parseScoreInformation);
    connect(m_parserXml,
            &MumeXmlParser::sendPromptLineEvent,
            m_groupManager,
            &Mmapper2Group::parsePromptInformation);
    connect(m_parserXml,
            &AbstractParser::sendGroupTellEvent,
            m_groupManager,
            &Mmapper2Group::sendGroupTell);
    connect(m_parserXml,
            &AbstractParser::sendGroupKickEvent,
            m_groupManager,
            &Mmapper2Group::kickCharacter);
    // Group Tell
    connect(m_groupManager,
            &Mmapper2Group::displayGroupTellEvent,
            m_parserXml,
            &AbstractParser::sendGTellToUser);

    emit log("Proxy", "Connection to client established ...");

    QByteArray ba
        = QString(
              "\033[1;37;46mWelcome to MMapper!\033[0;37;46m"
              "   Type \033[1m%1help\033[0m\033[37;46m for help or \033[1m%1vote\033[0m\033[37;46m to vote!\033[0m\r\n")
              .arg(getConfig().parser.prefixChar)
              .toLatin1();
    m_userSocket->write(ba);

    m_mudSocket = (NO_OPEN_SSL || !getConfig().connection.tlsEncryption)
                      ? dynamic_cast<MumeSocket *>(new MumeTcpSocket(this))
                      : dynamic_cast<MumeSocket *>(new MumeSslSocket(this));

    connect(m_mudSocket, &MumeSocket::connected, this, &Proxy::onMudConnected);
    connect(m_mudSocket, &MumeSocket::connected, m_userTelnet, &UserTelnet::onConnected);
    connect(m_mudSocket, &MumeSocket::connected, m_mudTelnet, &MudTelnet::onConnected);
    connect(m_mudSocket, &MumeSocket::connected, this, &Proxy::onMudConnected);
    connect(m_mudSocket, &MumeSocket::socketError, this, &Proxy::onMudError);
    connect(m_mudSocket, &MumeSocket::disconnected, this, &Proxy::mudTerminatedConnection);
    connect(m_mudSocket, &MumeSocket::disconnected, m_parserXml, &AbstractParser::reset);
    connect(m_mudSocket, &MumeSocket::processMudStream, m_mudTelnet, &MudTelnet::onAnalyzeMudStream);
    connect(m_mudSocket,
            SIGNAL(log(const QString &, const QString &)),
            m_listener->parent(),
            SLOT(log(const QString &, const QString &)));
    if (getConfig().general.mapMode != MapMode::OFFLINE)
        m_mudSocket->connectToHost();
    else {
        sendToUser(
            "\r\n"
            "\033[1;37;46mMMapper is running in offline mode. Switch modes and reconnect to play MUME.\033[0m\r\n"
            "\r\n"
            "Welcome to the land of Middle-earth. May your visit here be... interesting.\r\n"
            "Never forget! Try to role-play...\r\n");
        m_parserXml->doMove(CommandIdType::LOOK);
    }
}

void Proxy::onMudConnected()
{
    const auto &settings = getConfig().mumeClientProtocol;

    m_serverConnected = true;

    emit log("Proxy", "Connection to server established ...");

    // send IAC-GA prompt request
    QByteArray idPrompt("~$#EP2\nG\n");
    emit log("Proxy", "Sent MUME Protocol Initiator IAC-GA prompt request");
    sendToMud(idPrompt);

    if (settings.remoteEditing) {
        QByteArray idRemoteEditing("~$#EI\n");
        emit log("Proxy", "Sent MUME Protocol Initiator remote editing request");
        sendToMud(idRemoteEditing);
    }

    sendToMud(QByteArray("~$#EX2\n3G\n"));
    emit log("Proxy", "Sent MUME Protocol Initiator XML request");
}

void Proxy::onMudError(const QString &errorStr)
{
    m_serverConnected = false;

    qWarning() << "Mud socket error" << errorStr;
    emit log("Proxy", errorStr);

    sendToUser("\r\n\033[1;37;46m" + errorStr.toLocal8Bit() + "\033[0m\r\n");

    if (getConfig().connection.proxyConnectionStatus) {
        if (getConfig().general.mapMode == MapMode::OFFLINE) {
            sendToUser("\r\n"
                       "\033[1;37;46mYou are now exploring the world map offline.\033[0m\r\n");
            m_parserXml->sendPromptToUser();
        } else {
            // Terminate connection
            deleteLater();
        }
    } else {
        sendToUser(
            "\r\n"
            "\033[1;37;46mYou can explore world map offline or reconnect again...\033[0m\r\n");
        m_parserXml->sendPromptToUser();
    }
}

void Proxy::userTerminatedConnection()
{
    emit log("Proxy", "User terminated connection ...");
    deleteLater();
}

void Proxy::mudTerminatedConnection()
{
    if (!m_serverConnected) {
        return;
    }

    m_serverConnected = false;

    emit log("Proxy", "Mud terminated connection ...");

    sendToUser("\r\n\033[1;37;46mMUME closed the connection.\033[0m\r\n");

    if (getConfig().connection.proxyConnectionStatus) {
        if (getConfig().general.mapMode == MapMode::OFFLINE) {
            sendToUser("\r\n"
                       "\033[1;37;46mYou are now exploring the world map offline.\033[0m\r\n");
            m_parserXml->sendPromptToUser();
        } else {
            // Terminate connection
            deleteLater();
        }
    } else {
        sendToUser(
            "\r\n"
            "\033[1;37;46mYou can explore world map offline or reconnect again...\033[0m\r\n");
        m_parserXml->sendPromptToUser();
    }
}

void Proxy::processUserStream()
{
    if (m_userSocket != nullptr) {
        io::readAllAvailable(*m_userSocket, m_buffer, [this](const QByteArray &byteArray) {
            if (byteArray.size() != 0)
                emit analyzeUserStream(byteArray);
        });
    }
}

void Proxy::sendToMud(const QByteArray &ba)
{
    if (m_mudSocket != nullptr) {
        if (m_mudSocket->state() != QAbstractSocket::ConnectedState) {
            sendToUser(
                "\033[1;37;46mMMapper is not connected to MUME. Please reconnect to play.\033[0m\r\n");
            m_parserXml->sendPromptToUser();
        } else {
            m_mudSocket->sendToMud(ba);
        }
    } else {
        qWarning() << "Mud socket not available";
    }
}

void Proxy::sendToUser(const QByteArray &ba)
{
    if (m_userSocket != nullptr) {
        m_userSocket->write(ba);
    } else {
        qWarning() << "User socket not available";
    }
}
