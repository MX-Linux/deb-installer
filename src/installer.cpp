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
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QStandardPaths>
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
                   [](const QString &name) { return QFileInfo(name).canonicalFilePath(); });
    return new_list;
}

bool Installer::confirmAction(const QStringList &names)
{
    // Detect debconf frontend — prefer desktop environment detection over dpkg
    QString de = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    bool kdeFrontend = de.contains(QStringLiteral("KDE"), Qt::CaseInsensitive);
    if (!kdeFrontend) {
        de = qEnvironmentVariable("DESKTOP_SESSION");
        kdeFrontend = de.contains(QStringLiteral("kde"), Qt::CaseInsensitive)
                      || de.contains(QStringLiteral("plasma"), Qt::CaseInsensitive);
    }
    if (!kdeFrontend) {
        // Fall back to dpkg check
        const QString dpkgStatus = cmd.getCmdOut("dpkg", {"-l", "debconf-kde-helper"}, true);
        for (const auto &line : dpkgStatus.split('\n', Qt::SkipEmptyParts)) {
            if (line.startsWith("ii")) {
                kdeFrontend = true;
                break;
            }
        }
    }

    // Build the apt-get simulation with the correct debconf frontend
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DEBIAN_FRONTEND", kdeFrontend ? "kde" : "gnome");

    QStringList aptArgs = {
        "-s", "-V", "-o=Dpkg::Use-Pty=0", "install"
    };
    aptArgs.append(names);

    QString aptOutput;
    if (!cmd.run("apt-get", aptArgs, aptOutput, false, env)) {
        QApplication::beep();
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle(tr("Error"));
        msgBox.setText(tr("Could not simulate the package installation."));
        msgBox.setDetailedText(aptOutput);
        msgBox.exec();
        return false;
    }

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
        const QString dpkgInfo = cmd.getCmdOut("dpkg-deb", {"--info", file_name});
        const int pkgIdx = dpkgInfo.indexOf(QStringLiteral("Package:"));
        detailed_text += (pkgIdx >= 0 ? dpkgInfo.mid(pkgIdx) : dpkgInfo) + "\n\n";
    }
    detailed_text += detailed_to_install;
    detailed_text += "\n" + detailed_removed_names;
    msgBox.setDetailedText(detailed_text);

    // Limit detailed info box height to a reasonable maximum
    auto *const detailedInfo = msgBox.findChild<QTextEdit *>();
    if (detailedInfo) {
        const int lineCount = msgBox.detailedText().count('\n') + 1;
        const int lineHeight = detailedInfo->fontMetrics().lineSpacing();
        const int maxHeight = qMin(lineCount * lineHeight + 20, 400);
        detailedInfo->setMaximumHeight(maxHeight);
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
    // file names still need shell quoting for the x-terminal-emulator / sh -c invocation
    QStringList quotedNames;
    quotedNames.reserve(file_names.size());
    std::transform(file_names.cbegin(), file_names.cend(), std::back_inserter(quotedNames), shellQuote);

    const QString pkexecPath = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString aptPath = QStandardPaths::findExecutable(QStringLiteral("apt"));
    const QString sudoPath = QStandardPaths::findExecutable(QStringLiteral("sudo"));
    const QString terminalPath = QStandardPaths::findExecutable(QStringLiteral("x-terminal-emulator"));

    QString adminCommand;
    if (!pkexecPath.isEmpty()) {
        adminCommand = pkexecPath + QStringLiteral(" /usr/lib/deb-installer/apt-install ");
    } else if (!sudoPath.isEmpty() && !aptPath.isEmpty()) {
        adminCommand = sudoPath + QStringLiteral(" -p ") + shellQuote(msg + QStringLiteral(": "))
                       + QStringLiteral(" ") + aptPath
                       + QStringLiteral(" -o Acquire::AllowUnsizedPackages=true "
                                        "-o APT::Sandbox::User=root reinstall ");
    } else {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("No privilege escalation tool found.\n"
                                 "Please install pkexec or sudo."));
        return;
    }

    if (terminalPath.isEmpty()) {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("No terminal emulator found.\n"
                                 "Please install x-terminal-emulator."));
        return;
    }

    const QString script = adminCommand + quotedNames.join(' ')
                           + QStringLiteral("; echo; read -n1 -srp ")
                           + shellQuote(tr("Press any key to close"));
    QString terminalOutput;
    // Use shell-based invocation for x-terminal-emulator — the -e flag's
    // behaviour (single string vs. rest-of-args) varies across terminals.
    cmd.run(terminalPath + " -e sh -c " + shellQuote(script), terminalOutput);
    // Only treat a genuine launch failure as an error. Do NOT use the run()
    // return value here: the terminal stays open until the user presses a key,
    // so a non-zero exit (window closed, Ctrl-C/Ctrl-D at the prompt) is normal
    // and must not raise a spurious "failed to launch" dialog.
    if (cmd.error() == QProcess::FailedToStart) {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("Failed to launch the terminal emulator.\n"
                                 "Please check that x-terminal-emulator is installed."));
    }
}
