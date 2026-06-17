/**********************************************************************
 *  test_main.cpp
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <cstdio>
#include <cstdlib>

#include "../src/cmd.h"
#include "../src/installer.h"

// ── Mini test framework ──────────────────────────────────────────────────
static int tests_run = 0;
static int asserts_failed = 0;
static const char *current_test = nullptr;

#define TEST(name)                                                          \
    static void test_##name();                                              \
    static void test_##name()

#define RUN_TEST(name)                                                      \
    do {                                                                    \
        current_test = #name;                                               \
        tests_run++;                                                        \
        std::fprintf(stdout, "[%d] %s ... ", tests_run, #name);             \
        std::fflush(stdout);                                                \
        int before = asserts_failed;                                        \
        test_##name();                                                      \
        std::fprintf(stdout, "%s\n", asserts_failed == before ? "OK" : "FAILED"); \
    } while (false)

#define ASSERT_TRUE(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::fprintf(stderr, "  FAIL [%s:%d] %s: %s\n",                 \
                         __FILE__, __LINE__, current_test, #cond);           \
            asserts_failed++;                                               \
        }                                                                   \
    } while (false)

#define ASSERT_EQ_QSTR(a, b)                                                \
    do {                                                                    \
        auto _a = (a);                                                      \
        auto _b = (b);                                                      \
        if (_a != _b) {                                                     \
            std::fprintf(stderr, "  FAIL [%s:%d] %s: '%s' == '%s'\n"        \
                         "    expected: %s\n    actual:   %s\n",            \
                         __FILE__, __LINE__, current_test, #a, #b,           \
                         _b.toUtf8().constData(),                            \
                         _a.toUtf8().constData());                          \
            asserts_failed++;                                               \
        }                                                                   \
    } while (false)

#define ASSERT_EQ_INT(a, b)                                                 \
    do {                                                                    \
        auto _a = (a);                                                      \
        auto _b = (b);                                                      \
        if (_a != _b) {                                                     \
            std::fprintf(stderr, "  FAIL [%s:%d] %s: '%s' == '%s'\n"        \
                         "    expected: %lld\n    actual:   %lld\n",         \
                         __FILE__, __LINE__, current_test, #a, #b,           \
                         static_cast<long long>(_b),                         \
                         static_cast<long long>(_a));                       \
            asserts_failed++;                                               \
        }                                                                   \
    } while (false)

// ── Cmd: shell-based ─────────────────────────────────────────────────────

TEST(cmd_shell_echo)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(cmd.run("echo hello", out));
    ASSERT_EQ_QSTR(out, QStringLiteral("hello"));
}

TEST(cmd_shell_empty_output)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(cmd.run("true", out));
    ASSERT_EQ_QSTR(out, QString());
}

TEST(cmd_shell_false_returns_false)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(!cmd.run("false", out));
}

TEST(cmd_shell_getCmdOut)
{
    Cmd cmd;
    ASSERT_EQ_QSTR(cmd.getCmdOut("echo hello"), QStringLiteral("hello"));
}

TEST(cmd_shell_multiple_words)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(cmd.run("echo hello world", out));
    ASSERT_EQ_QSTR(out, QStringLiteral("hello world"));
}

// ── Cmd: arg-based ───────────────────────────────────────────────────────

TEST(cmd_args_echo)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(cmd.run(QStringLiteral("echo"), {QStringLiteral("hello")}, out));
    ASSERT_EQ_QSTR(out, QStringLiteral("hello"));
}

TEST(cmd_args_false_returns_false)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(!cmd.run(QStringLiteral("false"), QStringList(), out));
}

TEST(cmd_args_getCmdOut)
{
    Cmd cmd;
    ASSERT_EQ_QSTR(cmd.getCmdOut(QStringLiteral("echo"), {QStringLiteral("hello")}),
                   QStringLiteral("hello"));
}

TEST(cmd_args_with_env)
{
    Cmd cmd;
    QProcessEnvironment env;
    env.insert(QStringLiteral("TEST_DEB_VAR"), QStringLiteral("test-value-42"));
    QString out;
    ASSERT_TRUE(cmd.run(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("echo $TEST_DEB_VAR")},
                        out, false, env));
    ASSERT_EQ_QSTR(out, QStringLiteral("test-value-42"));
}

TEST(cmd_args_env_isolation)
{
    Cmd cmd;
    QProcessEnvironment env;
    env.insert(QStringLiteral("MY_SECRET"), QStringLiteral("leaked"));
    QString out;
    cmd.run(QStringLiteral("true"), QStringList(), out, true, env);

    // Run again without the extra env — should NOT see MY_SECRET
    ASSERT_TRUE(cmd.run(QStringLiteral("sh"),
                        {QStringLiteral("-c"), QStringLiteral("echo ${MY_SECRET:-none}")},
                        out, true));
    ASSERT_EQ_QSTR(out, QStringLiteral("none"));
}

TEST(cmd_args_merge_channels)
{
    Cmd cmd;
    QString out;
    ASSERT_TRUE(!cmd.run(QStringLiteral("sh"),
                         {QStringLiteral("-c"), QStringLiteral("echo stdout; echo stderr >&2; false")},
                         out));
    ASSERT_TRUE(!out.isEmpty());
}

// ── Installer::canonicalize ──────────────────────────────────────────────

TEST(canonicalize_empty_list)
{
    ASSERT_TRUE(Installer::canonicalize({}).isEmpty());
}

TEST(canonicalize_nonexistent_file)
{
    QStringList result = Installer::canonicalize({QStringLiteral("/nonexistent/deb-file.deb")});
    ASSERT_EQ_INT(result.size(), 1);
    ASSERT_TRUE(result[0].isEmpty());
}

TEST(canonicalize_existing_file)
{
    QString self = QCoreApplication::applicationFilePath();
    QStringList result = Installer::canonicalize({self});
    ASSERT_EQ_INT(result.size(), 1);
    ASSERT_TRUE(!result[0].isEmpty());
    ASSERT_EQ_QSTR(result[0], QFileInfo(self).canonicalFilePath());
}

// ── Main ─────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    RUN_TEST(cmd_shell_echo);
    RUN_TEST(cmd_shell_empty_output);
    RUN_TEST(cmd_shell_false_returns_false);
    RUN_TEST(cmd_shell_getCmdOut);
    RUN_TEST(cmd_shell_multiple_words);
    RUN_TEST(cmd_args_echo);
    RUN_TEST(cmd_args_false_returns_false);
    RUN_TEST(cmd_args_getCmdOut);
    RUN_TEST(cmd_args_with_env);
    RUN_TEST(cmd_args_env_isolation);
    RUN_TEST(cmd_args_merge_channels);
    RUN_TEST(canonicalize_empty_list);
    RUN_TEST(canonicalize_nonexistent_file);
    RUN_TEST(canonicalize_existing_file);

    std::printf("\nResults: %d tests, %d failed\n", tests_run, asserts_failed);
    return asserts_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
