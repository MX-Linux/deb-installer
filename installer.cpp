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
#include <QGridLayout>
#include <QMessageBox>

#include "cmd.h"

Installer::Installer(const QCommandLineParser &arg_parser)
{
    file_name = QFileInfo(arg_parser.positionalArguments().at(0)).canonicalFilePath();
    if (confirmAction(file_name))
        install(file_name);
}

Installer::~Installer() = default;

bool Installer::confirmAction(const QString &name)
{
    QString detailed_names;
    QString detailed_removed_names;
    QString detailed_to_install;
    QString msg;
    QStringList detailed_installed_names;

    const QString frontend = QStringLiteral("DEBIAN_FRONTEND=$(dpkg -l debconf-kde-helper 2>/dev/null | grep -sq ^i "
                                            "&& echo kde || echo gnome) LANG=C ");
    const QString aptget = QStringLiteral("apt-get -s -V -o=Dpkg::Use-Pty=0 ");

    detailed_names = cmd.getCmdOut(
        frontend + aptget + "install \"" + name + "\""
        + R"lit(|grep 'Inst\|Remv'| awk '{V=""; P="";}; $3 ~ /^\[/ { V=$3 }; $3 ~ /^\(/ { P=$3 ")"}; $4 ~ /^\(/ {P=" => " $4 ")"}; {print $2 ";" V  P ";" $1}')lit");
    if (!detailed_names.isEmpty())
        detailed_installed_names = detailed_names.split(QStringLiteral("\n"));
    detailed_installed_names.sort();
    QStringListIterator iterator(detailed_installed_names);
    QString value;
    while (iterator.hasNext()) {
        value = iterator.next();
        if (value.contains(QLatin1String("Remv"))) {
            value = value.section(QStringLiteral(";"), 0, 0) + " " + value.section(QStringLiteral(";"), 1, 1);
            detailed_removed_names = detailed_removed_names + value + "\n";
        }
        if (value.contains(QLatin1String("Inst"))) {
            value = value.section(QStringLiteral(";"), 0, 0) + " " + value.section(QStringLiteral(";"), 1, 1);
            detailed_to_install = detailed_to_install + value + "\n";
        }
    }
    if (!detailed_removed_names.isEmpty())
        detailed_removed_names.prepend(tr("Remove") + "\n");
    if (!detailed_to_install.isEmpty())
        detailed_to_install.prepend(tr("Install") + "\n");

    msg = "<b>" + tr("The following package will be installed. Click Show Details for list of changes.") + "</b>";

    QMessageBox msgBox;
    msgBox.setText(msg);
    msgBox.setInformativeText("\n" + tr("File: ") + name + "\n\n"
                              + cmd.getCmdOut("dpkg -I \"" + file_name + "\"| sed -n '/Package:/,$p'"));

    if (!detailed_installed_names.isEmpty() || !detailed_removed_names.isEmpty())
        msgBox.setDetailedText(detailed_to_install + "\n" + detailed_removed_names);

    msgBox.addButton(tr("Install"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);

    auto *horizontalSpacer = new QSpacerItem(600, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    auto *layout = qobject_cast<QGridLayout *>(msgBox.layout());
    layout->addItem(horizontalSpacer, 0, 1);
    return msgBox.exec() == QMessageBox::AcceptRole;
}

void Installer::install(const QString &file_name)
{
    cmd.run("x-terminal-emulator -e bash -c \"echo 'Installing selected package, please authenticate';echo;sudo apt "
            "reinstall '"
            + file_name + "'; echo; read -n1 -srp '" + tr("Press any key to close") + "'\"");
}
