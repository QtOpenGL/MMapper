/************************************************************************
**
** Authors:   Kalev Lember <kalev@smartlink.ee>
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

#include "aboutdialog.h"

#include <QString>
#include <QtConfig>
#include <QtGui>
#include <QtWidgets>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowIcon(QIcon(":/icons/m.png"));
    setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    aboutTabLayout->setAlignment(Qt::AlignHCenter);

    /* About tab */
    pixmapLabel->setPixmap(QPixmap(":/pixmaps/splash20.png"));
    pixmapLabel->setFixedSize(
        QSize(pixmapLabel->pixmap()->width(), pixmapLabel->pixmap()->height()));
    pixmapLabel->setAlignment(Qt::AlignCenter);

    aboutText->setAlignment(Qt::AlignCenter);
    aboutText->setTextFormat(Qt::RichText);
    aboutText->setOpenExternalLinks(true);
    aboutText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutText->setText(
        "<p align=\"center\"><b>" + tr("MMapper Version %1").arg(MMAPPER_VERSION) + "</b></p>"
        + "<p align=\"center\">"
#ifdef GIT_BRANCH
        + tr("Built on branch %1 and revision %2 ").arg(GIT_BRANCH).arg(GIT_COMMIT_HASH)
#ifdef __clang__
        + tr("using Clang %1.%2.%3").arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__)
#elif __GNUC__
        + tr("using GCC %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__)
#endif
        + "<br>"
#endif
        + tr("Based on Qt %1 (%2 bit)")
              .arg(QT_VERSION_STR)
              .arg(static_cast<size_t>(QSysInfo::WordSize))

        // REVISIT: warning: expansion of date or time macro is not reproducible [-Wdate-time]
        + "<br>" + tr("Built on %1 at %2").arg(QString(__DATE__)).arg(QString(__TIME__)) + "</p>");

    /* Authors tab */
    authorsView->setOpenExternalLinks(true);
    authorsView->setHtml(tr(
        "<p>Maintainer: Jahara (please report bugs <a href=\"https://github.com/MUME/MMapper/issues\">here</a>)</p>"
        "<p><u>Special thanks to:</u><br>"
        "Alve for his great map engine<br>"
        "Caligor for starting the mmapper project<br>"
        "Azazello for creating the group manager</p>"
        "<p><u>Contributors:</u><br>"
        "Arfang, Ethorondil, Kalev, Korir, Kovis, Krush, Midoc, Teoli, and Waba"
        "</p>"));

    /* License tab */
    QFile f(":/COPYING");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        licenseView->setText(tr("Unable to open file 'COPYING'."));
    } else {
        QTextStream ts(&f);
        licenseView->setText(ts.readAll());
    }
    setFixedFont(licenseView);
    licenseView->setMinimumWidth(700);

    setMaximumSize(sizeHint());
    adjustSize();
}

void AboutDialog::setFixedFont(QTextBrowser *browser)
{
    QFont fixed;
    fixed.setStyleHint(QFont::TypeWriter);
    fixed.setFamily("Courier");
    browser->setFont(fixed);
}
