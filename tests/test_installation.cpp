#include <QApplication>
#include <QCommandLineParser>
#include <QAbstractButton>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QMessageBox>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

#include "installer.h"

namespace {

bool writeExecutable(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
           && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

int testSnapshot(QTemporaryDir &commands)
{
    const QString first = commands.filePath("first/sample.deb");
    const QString second = commands.filePath("second/sample.deb");
    if (!QDir(commands.path()).mkdir("first") || !QDir(commands.path()).mkdir("second")
        || !writeExecutable(first, "first") || !writeExecutable(second, "second")) {
        return 1;
    }

    const QByteArray simulate = R"SH(#!/bin/bash
printf '%s\n' "${@: -2}" > "$REVIEW_PATHS"
printf changed > "$REVIEW_ORIGINAL"
)SH";
    const QByteArray inspect = R"SH(#!/bin/sh
printf 'Package: '
cat "$2"
)SH";
    const QByteArray install = R"SH(#!/bin/bash
mapfile -t staged < "$REVIEW_PATHS"
[[ $# == 3 && "$2" == "${staged[0]}" && "$3" == "${staged[1]}" ]] || exit 1
[[ "$(<"$2")" == first && "$(<"$3")" == second ]] || exit 1
touch "$REVIEW_SUCCESS"
)SH";
    const QString paths = commands.filePath("paths");
    const QString success = commands.filePath("success");
    qputenv("REVIEW_ORIGINAL", first.toLocal8Bit());
    qputenv("REVIEW_PATHS", paths.toLocal8Bit());
    qputenv("REVIEW_SUCCESS", success.toLocal8Bit());
    if (!writeExecutable(commands.filePath("apt-get"), simulate)
        || !writeExecutable(commands.filePath("dpkg-deb"), inspect)
        || !writeExecutable(commands.filePath("pkexec"), install)) {
        return 1;
    }

    bool previewOk = false;
    QTimer confirm;
    QObject::connect(&confirm, &QTimer::timeout, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box) {
            if (auto *ok = box->button(QMessageBox::Ok); ok && box->button(QMessageBox::Cancel)) {
                const QString details = box->detailedText();
                previewOk = details.contains(first) && details.contains(second)
                            && details.contains("Package: first") && details.contains("Package: second");
                ok->click();
            } else {
                box->reject();
            }
        }
    });
    confirm.start(10);
    QCommandLineParser parser;
    parser.process(QStringList {"deb-installer", first, second});
    Installer installer(parser);
    QFile recorded(paths);
    if (!previewOk || !QFile::exists(success) || !recorded.open(QIODevice::ReadOnly)) {
        return 1;
    }
    const QStringList staged = QString::fromUtf8(recorded.readAll()).split('\n', Qt::SkipEmptyParts);
    return staged.size() == 2 && staged[0] != staged[1] && !QFile::exists(staged[0])
           && !QFile::exists(staged[1]) ? 0 : 1;
}


}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QTemporaryDir commands;
    if (!commands.isValid()) {
        return 1;
    }

    // Exercise the actual generated Bash script without installing packages or
    // opening a terminal. Detached launchers close their pipes just like a
    // terminal server; waiting launchers return only after the script exits.
    const QByteArray terminal = R"SH(#!/bin/bash
case "$DEB_INSTALLER_TEST_CASE" in
    launch-failure) exit 1 ;;
    no-start) exit 0 ;;
esac
[ "$1" = -e ] || exit 2
shift
if [ "$DEB_INSTALLER_TEST_CASE" = waiting ]; then
    exec "$@" </dev/null
fi
(
    if [ "$DEB_INSTALLER_TEST_CASE" = delayed ]; then sleep 0.7; fi
    case "$DEB_INSTALLER_TEST_CASE" in
        held-success|held-package-failure) exec "$@" < <(sleep 3) ;;
    esac
    exec "$@"
) </dev/null >/dev/null 2>&1 &
)SH";
    const QByteArray pkexec = R"SH(#!/bin/bash
# A daemon spawned from a maintainer script must not inherit the Bash lock.
if [ -e /proc/$$/fd/9 ]; then exit 97; fi
case "$DEB_INSTALLER_TEST_CASE" in
    package-failure|held-package-failure) exit 42 ;;
    slow) sleep 1 ;;
    interrupted)
        kill -HUP "$PPID"
        ;;
    surviving-child)
        sleep 3 </dev/null >/dev/null 2>&1 &
        kill -HUP "$PPID"
        ;;
esac
exit 0
)SH";
    if (!writeExecutable(commands.filePath("x-terminal-emulator"), terminal)
        || !writeExecutable(commands.filePath("pkexec"), pkexec)) {
        return 1;
    }
    qputenv("PATH", commands.path().toLocal8Bit() + ":" + qgetenv("PATH"));

    if (app.arguments().contains("--snapshot")) {
        return testSnapshot(commands);
    }

    QCommandLineParser parser;
    parser.process(QStringList {QStringLiteral("deb-installer-test")});
    Installer installer(parser);
    // The empty constructor queues quit; consume it before testing the slot.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);

    QStringList titles;
    QStringList messages;
    QTimer dismissMessages;
    QObject::connect(&dismissMessages, &QTimer::timeout, [&] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            if (box && box->isVisible()) {
                titles << box->windowTitle();
                messages << box->text();
                box->accept();
            }
        }
    });
    dismissMessages.start(10);

    int failures = 0;
    const auto run = [&](const char *scenario, const QString &expectedTitle,
                         qint64 minMs, qint64 maxMs) {
        titles.clear();
        messages.clear();
        qputenv("DEB_INSTALLER_TEST_CASE", scenario);
        QElapsedTimer elapsed;
        elapsed.start();
        const bool invoked = QMetaObject::invokeMethod(
            &installer, "install", Qt::DirectConnection,
            Q_ARG(QStringList, QStringList {QStringLiteral("/tmp/test package's.deb")}));
        const qint64 duration = elapsed.elapsed();
        const bool titleOk = expectedTitle.isEmpty() ? titles.isEmpty()
                                                   : titles == QStringList {expectedTitle};
        const bool interruptionOk = expectedTitle != "Installation interrupted"
                                    || (messages.size() == 1
                                        && messages.front().contains("result is unknown")
                                        && messages.front().contains("may still be running"));
        const bool passed = invoked && titleOk && interruptionOk
                            && duration >= minMs && duration < maxMs;
        std::printf("%s: %s (%lld ms)\n", scenario, passed ? "OK" : "FAILED",
                    static_cast<long long>(duration));
        if (!passed) {
            std::fprintf(stderr, "Unexpected dialogs: %s\n", qPrintable(titles.join("; ")));
            ++failures;
        }
    };

    run("waiting", {}, 0, 3000);
    // No TTY means the close prompt immediately reaches EOF, so the script can
    // exit between the initial result check and the liveness check.
    for (int i = 0; i < 10; ++i) {
        run("fast", {}, 0, 3000);
    }
    run("delayed", {}, 600, 4000);
    run("slow", {}, 900, 4000);
    // The terminal normally stays at the close prompt with its lock held.
    // A completed result must take priority over that still-live script.
    run("held-success", {}, 0, 2000);
    run("held-package-failure", "Package installation failed", 0, 2000);
    run("package-failure", "Package installation failed", 0, 3000);
    run("interrupted", "Installation interrupted", 0, 3000);
    run("surviving-child", "Installation interrupted", 0, 2000);
    run("launch-failure", "Installation result unknown", 500, 3000);
    run("no-start", "Installation result unknown", 30000, 34000);
    return failures == 0 ? 0 : 1;
}
