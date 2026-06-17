#include "cmd.h"

#include <QDebug>
#include <QEventLoop>
#include <QProcessEnvironment>

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
    QByteArray outputBuffer;
    auto outputConnection = connect(this, &QProcess::readyRead, this, [this, &outputBuffer] {
        outputBuffer += readAll();
    });
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
        disconnect(outputConnection);
        disconnect(errorConnection);
        return false;
    }
    loop.exec();
    outputBuffer += readAll();
    disconnect(outputConnection);
    disconnect(errorConnection);
    output = QString::fromLocal8Bit(outputBuffer).trimmed();
    return (!encounteredError && exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

QString Cmd::getCmdOut(const QString &program, const QStringList &args, bool quiet)
{
    QString output;
    run(program, args, output, quiet);
    return output;
}

bool Cmd::run(const QString &program, const QStringList &args, QString &output, bool quiet,
              const QProcessEnvironment &extraEnv)
{
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) {
        qDebug().noquote() << program << args;
    }
    QEventLoop loop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    QByteArray outputBuffer;
    auto outputConnection = connect(this, &QProcess::readyRead, this, [this, &outputBuffer] {
        outputBuffer += readAll();
    });
    bool encounteredError = false;
    auto errorConnection = connect(this, &QProcess::errorOccurred, &loop,
                                   [&loop, &encounteredError](QProcess::ProcessError) {
                                       encounteredError = true;
                                       loop.quit();
                                   });
    setProcessChannelMode(QProcess::MergedChannels);
    // Always set a clean environment so overrides from a previous run on this
    // reused object don't leak into this one.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(extraEnv);
    setProcessEnvironment(env);
    start(program, args);
    if (!waitForStarted()) {
        if (!quiet) {
            qDebug().noquote() << "Failed to start process:" << errorString();
        }
        disconnect(outputConnection);
        disconnect(errorConnection);
        return false;
    }
    loop.exec();
    outputBuffer += readAll();
    disconnect(outputConnection);
    disconnect(errorConnection);
    output = QString::fromLocal8Bit(outputBuffer).trimmed();
    return (!encounteredError && exitStatus() == QProcess::NormalExit && exitCode() == 0);
}
