#pragma once

#include <QProcess>

class QString;
class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    [[nodiscard]] QString getCmdOut(const QString &cmd, bool quiet = false);
    [[nodiscard]] QString getCmdOut(const QString &program, const QStringList &args, bool quiet = false);
    bool run(const QString &cmd, QString &output, bool quiet = false);
    bool run(const QString &cmd, bool quiet = false);
    bool run(const QString &program, const QStringList &args, QString &output, bool quiet = false,
             const QProcessEnvironment &extraEnv = QProcessEnvironment());

signals:
    void done();
};
