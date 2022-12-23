/**********************************************************************
 *  mainwindow.h
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

#ifndef INSTALLER_H
#define INSTALLER_H

#include <QCommandLineParser>

#include <cmd.h>

class Installer : QObject
{
    Q_OBJECT

public:
    Installer(const QCommandLineParser &arg_parser = {});
    ~Installer();

public slots:

private slots:
    bool confirmAction(const QString &file_name);
    void install(const QString &file_name);

private:
    Cmd cmd;
    QProcess proc;
    QString file_name;
};

#endif
