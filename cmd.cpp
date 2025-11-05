#include "cmd.h"

#include <QDebug>
#include <QEventLoop>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Cmd::done);
}

bool Cmd::run(const QString &cmd, bool quiet)
{
    QString output;
    return run(cmd, output, quiet);
}

QString Cmd::getCmdOut(const QString &cmd, bool quiet)
{
    QString output;
    run(cmd, output, quiet);
    return output;
}

bool Cmd::run(const QString &cmd, QString &output, bool quiet)
{
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) {
        qDebug().noquote() << cmd;
    }
    QEventLoop loop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    bool encounteredError = false;
    auto errorConnection = connect(this, &QProcess::errorOccurred, &loop,
                                   [&loop, &encounteredError](QProcess::ProcessError) {
                                       encounteredError = true;
                                       loop.quit();
                                   });
    setProcessChannelMode(QProcess::MergedChannels);
    start("/bin/bash", {"-c", cmd});
    if (!waitForStarted()) {
        if (!quiet) {
            qDebug().noquote() << "Failed to start process:" << errorString();
        }
        disconnect(errorConnection);
        return false;
    }
    loop.exec();
    disconnect(errorConnection);
    output = readAll().trimmed();
    return (!encounteredError && exitStatus() == QProcess::NormalExit && exitCode() == 0);
}
