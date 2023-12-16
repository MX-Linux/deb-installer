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
#pragma once

#include <QCommandLineParser>

#include <cmd.h>

class Installer : QObject
{
    Q_OBJECT

public:
    explicit Installer(const QCommandLineParser &arg_parser = {}, QObject *parent = nullptr);

public slots:

private slots:
    static QStringList canonicalize(const QStringList &file_names);
    bool confirmAction(const QStringList &file_names);
    void install(const QStringList &file_names);

private:
    Cmd cmd;
};
