/**********************************************************************
 *  installer.cpp
 **********************************************************************
 * Copyright (C) 2022 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#include "installer.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QTextEdit>

#include <algorithm>
#include <iterator>

namespace {

QString shellQuote(QString value)
{
    value.replace('\'', QStringLiteral("'\\''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

}

Installer::Installer(const QCommandLineParser &arg_parser, QObject *parent)
    : QObject(parent)
{
    QStringList file_names = canonicalize(arg_parser.positionalArguments());
    file_arguments = file_names;
    qDebug() << "file arguments is " << file_arguments;
    if (file_names.isEmpty() || !confirmAction(file_names)) {
        QMetaObject::invokeMethod(QApplication::instance(), "quit", Qt::QueuedConnection);
        return;
    }

    install(file_names);
    QMetaObject::invokeMethod(QApplication::instance(), "quit", Qt::QueuedConnection);
}

QStringList Installer::canonicalize(const QStringList &file_names)
{
    QStringList new_list;
    new_list.reserve(file_names.size());

    std::transform(file_names.cbegin(), file_names.cend(), std::back_inserter(new_list),
                   [](const QString &name) { return shellQuote(QFileInfo(name).canonicalFilePath()); });
    return new_list;
}

bool Installer::confirmAction(const QStringList &names)
{
    const QString names_str = names.join(' ');

    // Detect debconf frontend without shell pipe/command-substitution
    const QString dpkgStatus = cmd.getCmdOut("dpkg -l debconf-kde-helper 2>/dev/null", true);
    bool kdeFrontend = false;
    for (const auto &line : dpkgStatus.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("ii")) {
            kdeFrontend = true;
            break;
        }
    }
    const QString frontend = QStringLiteral("DEBIAN_FRONTEND=%1 ").arg(kdeFrontend ? "kde" : "gnome");
    const QString aptget {"apt-get -s -V -o=Dpkg::Use-Pty=0 "};

    // Run apt-get simulation, then parse Inst/Remv lines in Qt (no shell grep/awk)
    const QString aptOutput = cmd.getCmdOut(frontend + aptget + "install " + names_str);

    QString detailed_to_install;
    QString detailed_removed_names;
    QStringList detailed_installed_names;

    const QStringList aptLines = aptOutput.split('\n', Qt::SkipEmptyParts);
    for (const auto &line : aptLines) {
        const QStringList fields = line.split(' ', Qt::SkipEmptyParts);
        if (fields.size() < 2) {
            continue;
        }
        const QString &action = fields.at(0);
        if (action != "Inst" && action != "Remv") {
            continue;
        }
        const QString &pkg = fields.at(1);
        QString oldVer;
        QString newVer;
        if (fields.size() >= 3 && fields.at(2).startsWith('[')) {
            oldVer = fields.at(2);
        }
        if (fields.size() >= 3 && fields.at(2).startsWith('(')) {
            newVer = fields.at(2) + ')';
        }
        if (fields.size() >= 4 && fields.at(3).startsWith('(')) {
            newVer = " => " + fields.at(3) + ')';
        }
        detailed_installed_names << pkg + ';' + oldVer + newVer + ';' + action;
    }
    detailed_installed_names.sort();

    for (const auto &value : detailed_installed_names) {
        if (value.contains("Remv")) {
            detailed_removed_names += value.section(';', 0, 0) + ' ' + value.section(';', 1, 1) + '\n';
        } else if (value.contains("Inst")) {
            detailed_to_install += value.section(';', 0, 0) + ' ' + value.section(';', 1, 1) + '\n';
        }
    }

    if (!detailed_removed_names.isEmpty()) {
        detailed_removed_names.prepend(tr("Remove") + '\n');
    }
    if (!detailed_to_install.isEmpty()) {
        detailed_to_install.prepend(tr("Install") + '\n');
    }

    QMessageBox msgBox;
    msgBox.setText(
        "<b>"
        + tr("The following packages will be installed. Click 'Show Details...' for information about the packages.")
        + "</b>");

    QString detailed_text;
    for (const auto &file_name : names) {
        detailed_text += tr("File: %1").arg(file_name) + "\n\n";
        const QString dpkgInfo = cmd.getCmdOut("dpkg -I " + file_name);
        const int pkgIdx = dpkgInfo.indexOf(QStringLiteral("Package:"));
        detailed_text += (pkgIdx >= 0 ? dpkgInfo.mid(pkgIdx) : dpkgInfo) + "\n\n";
    }
    detailed_text += detailed_to_install;
    detailed_text += "\n" + detailed_removed_names;
    msgBox.setDetailedText(detailed_text);

    // Set height of detailed info box
    auto *const detailedInfo = msgBox.findChild<QTextEdit *>();
    if (detailedInfo) {
        const int height = qBound(100, msgBox.detailedText().length() / 2, 400);
        detailedInfo->setFixedHeight(height);
    }

    msgBox.setInformativeText(!detailed_installed_names.isEmpty() || !detailed_removed_names.isEmpty()
                                  ? file_arguments.join("\n") + '\n'
                                  : tr("Will install the following:") + '\n' + file_arguments.join("\n"));

    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    msgBox.button(QMessageBox::Ok)->setText(tr("Install"));
    auto *layout = qobject_cast<QGridLayout *>(msgBox.layout());
    if (layout) {
        layout->addItem(new QSpacerItem(600, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), 0, 1);
    }
    msgBox.exec();
    return msgBox.clickedButton() == msgBox.button(QMessageBox::Ok);
}

void Installer::install(const QStringList &file_names)
{
    const QString msg {tr("Installing selected package, please authenticate")};
    const QString admincommand = QFile::exists("/usr/bin/pkexec") ? QStringLiteral("pkexec")
                                                                  : QStringLiteral("sudo -p ")
                                                                        + shellQuote(msg + QStringLiteral(": "));
    const QString script = QStringLiteral("LANG=") + shellQuote(qEnvironmentVariable("LANG"))
                           + QStringLiteral(" DISPLAY=") + shellQuote(qEnvironmentVariable("DISPLAY"))
                           + QStringLiteral(" XAUTHORITY=") + shellQuote(qEnvironmentVariable("XAUTHORITY"))
                           + QStringLiteral(" apt -o Acquire::AllowUnsizedPackages=true "
                                            "-o APT::Sandbox::User=root reinstall ")
                           + file_names.join(' ') + QStringLiteral("; echo; read -n1 -srp ")
                           + shellQuote(tr("Press any key to close"));
    cmd.run(QStringLiteral("x-terminal-emulator -e ") + admincommand + QStringLiteral(" bash -c ")
            + shellQuote(script));
}
