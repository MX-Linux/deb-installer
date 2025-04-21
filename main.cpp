/**********************************************************************
 *  main.cpp
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

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileDialog>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>

#include "installer.h"
#include "version.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MX-Linux");
    QApplication::setApplicationDisplayName("Deb Installer");
    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Program for installing Debian binary packages (deb files)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QObject::tr("files..."), QObject::tr("Name of .deb files to install"),
                                 QObject::tr("[file...]"));
    parser.process(app);

    QTranslator qtTran;
    if (qtTran.load("qt_" + QLocale::system().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTran);
    }

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtBaseTran);
    }

    QTranslator appTran;
    if (appTran.load(QApplication::applicationName() + "_" + QLocale::system().name(),
                     "/usr/share/" + QApplication::applicationName() + "/locale")) {
        QApplication::installTranslator(&appTran);
    }

    if (getuid() != 0) {
        if (parser.positionalArguments().isEmpty()) {
            QStringList args = {"deb-installer"};
            args << QFileDialog::getOpenFileNames(nullptr, QObject::tr("Select .deb files to install"),
                                                  QDir::currentPath(), QObject::tr("Deb Files (*.deb)"));
            parser.process(args);
        }
        if (parser.positionalArguments().isEmpty()) {
            qDebug().noquote() << QObject::tr("No .deb files were provided.");
            return EXIT_FAILURE;
        }
        const auto args = parser.positionalArguments();
        for (const auto &file : args) {
            if (!QFile::exists(file)) {
                QApplication::beep();
                QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("File %1 not found").arg(file));
                return EXIT_FAILURE;
            } else if (!file.endsWith(".deb")) {
                QApplication::beep();
                QMessageBox::critical(nullptr, QObject::tr("Error"),
                                      QObject::tr("File %1 is not a .deb file.").arg(file));
                return EXIT_FAILURE;
            }
        }
        Installer installer(parser);
    } else {
        QApplication::beep();
        QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("You must run this program as normal user."));
        return EXIT_FAILURE;
    }
}
