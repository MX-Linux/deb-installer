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
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QTextEdit>

#include "cmd.h"

Installer::Installer(const QCommandLineParser &arg_parser, QObject *parent)
    : QObject(parent)
{
    QStringList file_names = canonicalize(arg_parser.positionalArguments());
    if (confirmAction(file_names)) {
        install(file_names);
    }
}

QStringList Installer::canonicalize(const QStringList &file_names)
{
    QStringList new_list;
    new_list.reserve(file_names.size());
    for (auto const &name : file_names) {
        new_list << "\"" + QFileInfo(name).canonicalFilePath() + "\"";
    }
    return new_list;
}

bool Installer::confirmAction(const QStringList &names)
{
    const QString names_str = names.join(" ");
    const QString frontend {
        "DEBIAN_FRONTEND=$(dpkg -l debconf-kde-helper 2>/dev/null | grep -sq ^i && echo kde || echo gnome) "};
    const QString aptget {"apt-get -s -V -o=Dpkg::Use-Pty=0 "};

    const QString detailed_names = cmd.getCmdOut(
        frontend + aptget + "install " + names_str
        + R"lit(|grep 'Inst\|Remv'| awk '{V=""; P="";}; $3 ~ /^\[/ { V=$3 }; $3 ~ /^\(/ { P=$3 ")"}; $4 ~ /^\(/ {P=" => " $4 ")"}; {print $2 ";" V  P ";" $1}')lit");

    QStringList detailed_installed_names;
    if (!detailed_names.isEmpty()) {
        detailed_installed_names = detailed_names.split("\n");
    }
    detailed_installed_names.sort();
    QString detailed_to_install;
    QString detailed_removed_names;
    QStringListIterator iterator(detailed_installed_names);
    while (iterator.hasNext()) {
        auto value = iterator.next();
        if (value.contains(QLatin1String("Remv"))) {
            value = value.section(";", 0, 0) + " " + value.section(";", 1, 1);
            detailed_removed_names += value + "\n";
        }
        if (value.contains(QLatin1String("Inst"))) {
            value = value.section(";", 0, 0) + " " + value.section(";", 1, 1);
            detailed_to_install += value + "\n";
        }
    }
    if (!detailed_removed_names.isEmpty()) {
        detailed_removed_names.prepend(tr("Remove") + "\n");
    }
    if (!detailed_to_install.isEmpty()) {
        detailed_to_install.prepend(tr("Install") + "\n");
    }

    const QString msg {
        "<b>"
        + tr("The following packages will be installed. Click 'Show Details...' for information about the packages.")
        + "</b>"};

    QMessageBox msgBox;
    msgBox.setText(msg);

    QString detailed_text;
    for (const auto &file_name : names) {
        detailed_text += tr("File: %1").arg(file_name) + "\n\n";
        detailed_text += cmd.getCmdOut("dpkg -I " + file_name + "| sed -n '/Package:/,$p'");
        detailed_text += "\n\n";
    }
    msgBox.setDetailedText(detailed_text);

    // find Detailed Info box and set heigth, set box height between 100 - 400 depending on length of content
    const auto min = 100;
    const auto max = 400;
    auto *const detailedInfo = msgBox.findChild<QTextEdit *>();
    const auto recommended = qMax(msgBox.detailedText().length() / 2, min); // half of length is just guesswork
    const auto height = qMin(recommended, max);
    detailedInfo->setFixedHeight(height);

    if (!detailed_installed_names.isEmpty() || !detailed_removed_names.isEmpty()) {
        msgBox.setInformativeText(detailed_to_install + "\n" + detailed_removed_names);
    } else {
        msgBox.setInformativeText(tr("Will install the following:") + "\n" + names.join("\n"));
    }

    msgBox.addButton(tr("Install"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);

    const auto width = 600;
    auto *horizontalSpacer = new QSpacerItem(width, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    auto *layout = qobject_cast<QGridLayout *>(msgBox.layout());
    layout->addItem(horizontalSpacer, 0, 1);
    return msgBox.exec() == QMessageBox::AcceptRole;
}

void Installer::install(const QStringList &file_names)
{
    const QString msg {tr("Installing selected package please authenticate")};
    const QString admincommand = QFile::exists("/usr/bin/pkexec") ? "pkexec" : QString("sudo -p '%1: '").arg(msg);
    cmd.run("x-terminal-emulator -e " + admincommand + " bash -c ' LANG=" + qEnvironmentVariable("LANG")
            + " DISPLAY=" + qEnvironmentVariable("DISPLAY") + " XAUTHORITY=" + qEnvironmentVariable("XAUTHORITY")
            + " apt -o Acquire::AllowUnsizedPackages=true -o APT::Sandbox::User=root" + " reinstall "
            + file_names.join(" ") + "; echo; read -n1 -srp \"" + tr("Press any key to close") + "\"'");
}
