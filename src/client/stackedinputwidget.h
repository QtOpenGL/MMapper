#pragma once
/************************************************************************
**
** Authors:   Nils Schimmelmann <nschimme@gmail.com>
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

#ifndef STACKEDINPUTWIDGET_H
#define STACKEDINPUTWIDGET_H

#include <QStackedWidget>
#include <QString>
#include <QtCore>

class InputWidget;
class QEvent;
class QLineEdit;
class QObject;
class QWidget;

class StackedInputWidget final : public QStackedWidget
{
private:
    Q_OBJECT

public:
    explicit StackedInputWidget(QWidget *parent = nullptr);
    ~StackedInputWidget() override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void toggleEchoMode(bool);
    void gotPasswordInput();
    void gotMultiLineInput(const QString &);
    void cut();
    void copy();
    void paste();

signals:
    void sendUserInput(const QString &);
    void displayMessage(const QString &);
    void showMessage(const QString &, int);

private:
    bool m_localEcho = false;
    InputWidget *m_inputWidget = nullptr;
    QLineEdit *m_passwordWidget = nullptr;
};

#endif /* STACKEDINPUTWIDGET_H */
