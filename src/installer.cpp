/**********************************************************************
 *  installer.cpp
 **********************************************************************
 * Copyright (C) 2022-2026 MX Authors
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
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTemporaryFile>
#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <iterator>
#include <sys/file.h>

namespace {

QString shellQuote(QString value)
{
    value.replace('\'', QStringLiteral("'\\''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

bool readInstallationStatus(QTemporaryFile &file, int &status)
{
    if (!file.open()) {
        return false;
    }
    const QByteArray contents = file.readAll();
    file.close();
    // A newline marks a complete write; don't accept a partially written code.
    if (!contents.endsWith('\n')) {
        return false;
    }
    bool ok = false;
    const int value = contents.trimmed().toInt(&ok);
    if (ok && value >= 0 && value <= 255) {
        status = value;
        return true;
    }
    return false;
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
        "-s", "-V", "-o=Dpkg::Use-Pty=0", "-o=Acquire::AllowUnsizedPackages=true",
        "-o=APT::Sandbox::User=root", "reinstall", "--"
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
    static const QRegularExpression oldVerRe(R"(\[([^\]]+)\])");
    static const QRegularExpression newVerRe(R"(\(([^)]+)\))");
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
        const auto oldMatch = oldVerRe.match(line);
        const auto newMatch = newVerRe.match(line);
        if (oldMatch.hasMatch()) {
            // Only accept bracket that appears before the version parens,
            // to avoid matching arch tags (e.g. [all]) nested inside parens
            if (!newMatch.hasMatch() || oldMatch.capturedStart() < newMatch.capturedStart()) {
                oldVer = '[' + oldMatch.captured(1) + ']';
            }
        }
        if (newMatch.hasMatch()) {
            newVer = '(' + newMatch.captured(1) + ')';
            if (!oldVer.isEmpty()) {
                newVer = " => " + newVer;
            }
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
        adminCommand = shellQuote(pkexecPath) + QStringLiteral(" ")
                       + shellQuote(QStringLiteral("/usr/lib/deb-installer/apt-install")) + QStringLiteral(" ");
    } else if (!sudoPath.isEmpty() && !aptPath.isEmpty()) {
        adminCommand = shellQuote(sudoPath) + QStringLiteral(" -p ") + shellQuote(msg + QStringLiteral(": "))
                       + QStringLiteral(" ") + shellQuote(aptPath)
                       + QStringLiteral(" -o Acquire::AllowUnsizedPackages=true "
                                        "-o APT::Sandbox::User=root reinstall -- ");
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

    QTemporaryFile statusFile;
    if (!statusFile.open()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Could not create an installation status file."));
        return;
    }
    const QString statusFilePath = statusFile.fileName();
    statusFile.close();

    QTemporaryFile startupFile;
    if (!startupFile.open()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Could not create an installation status file."));
        return;
    }

    // Bash holds the lock until it exits, even when the terminal launcher forks.
    // Close fd 9 for the privileged command so services started by maintainer
    // scripts cannot inherit the lock and keep the GUI waiting indefinitely.
    const QString script = QStringLiteral("exec 9>>") + shellQuote(startupFile.fileName())
                           + QStringLiteral(" || exit 1; flock -x 9 || exit 1; "
                                            "printf 'started\\n' >&9 || exit 1; ")
                           + adminCommand + quotedNames.join(' ') + QStringLiteral(" 9>&-")
                           + QStringLiteral("; aptStatus=$?; printf '%s\\n' \"$aptStatus\" > ")
                           + shellQuote(statusFilePath) + QStringLiteral("; echo; read -n1 -srp ")
                           + shellQuote(tr("Press any key to close"))
                           + QStringLiteral(" || true; exit $aptStatus");
    QString terminalOutput;
    // Preserve the argument vector expected by terminal implementations while
    // avoiding an outer shell that parses paths or command text. The inner
    // Bash is intentional: it executes the close prompt below.
    const bool terminalOk = cmd.run(
            QStringLiteral("/bin/bash"),
            {QStringLiteral("-c"), QStringLiteral("exec \"$@\""), QStringLiteral("deb-installer"), terminalPath,
             QStringLiteral("-e"), QStringLiteral("/bin/bash"), QStringLiteral("-c"), script},
            terminalOutput);
    if (cmd.error() == QProcess::FailedToStart) {
        QMessageBox::critical(nullptr, tr("Error"),
                              tr("Failed to launch the terminal emulator.\n"
                                 "Please check that x-terminal-emulator is installed."));
        return;
    }

    // Only startup has a deadline. Authentication, downloads and configuration
    // may take arbitrarily long once the terminal script holds the lock.
    bool statusOk = false;
    bool interrupted = false;
    int aptStatus = -1;
    QElapsedTimer elapsed;
    elapsed.start();
    const qint64 startupTimeoutMs = terminalOk ? 30 * 1000 : 500;
    constexpr int pollIntervalMs = 200;
    while (!(statusOk = readInstallationStatus(statusFile, aptStatus))) {
        if (!startupFile.seek(0)) {
            break;
        }
        const bool started = startupFile.readAll() == "started\n";
        if (started) {
            int lockResult;
            do {
                lockResult = ::flock(startupFile.handle(), LOCK_EX | LOCK_NB);
            } while (lockResult != 0 && errno == EINTR);
            if (lockResult == 0) {
                // The script may have written its result between our first
                // read and its exit (especially without a TTY for read -n1).
                statusOk = readInstallationStatus(statusFile, aptStatus);
                ::flock(startupFile.handle(), LOCK_UN);
                interrupted = !statusOk;
                break;
            }
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                break;
            }
        } else if (elapsed.elapsed() >= startupTimeoutMs) {
            statusOk = readInstallationStatus(statusFile, aptStatus);
            break;
        }

        QEventLoop waitLoop;
        QTimer::singleShot(pollIntervalMs, &waitLoop, &QEventLoop::quit);
        waitLoop.exec();
    }

    if (statusOk) {
        if (aptStatus != 0) {
            QMessageBox::critical(nullptr, tr("Package installation failed"),
                                  tr("The package manager reported an error."));
        }
    } else if (interrupted) {
        QMessageBox::warning(nullptr, tr("Installation interrupted"),
                             tr("The installation was interrupted and its result is unknown.\n"
                                "The package manager may still be running."));
    } else {
        QMessageBox::warning(nullptr, tr("Installation result unknown"),
                             tr("Could not determine whether the installation succeeded."));
    }
}
