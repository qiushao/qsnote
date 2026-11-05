#include "core/DesktopSession.h"
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <cstdio>
#include <cstdlib>

namespace {
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}
}// namespace

int main(int argc, char **argv) {
    const QCoreApplication app(argc, argv);
    if (app.arguments().contains("--child")) {
        const auto address = qgetenv("DBUS_SESSION_BUS_ADDRESS");
        std::fwrite(address.constData(), 1, address.size(), stdout);
        return 0;
    }
    qunsetenv("DBUS_SESSION_BUS_ADDRESS");
    DesktopSession::normalizeBusAddress();
    check(!qEnvironmentVariableIsSet("DBUS_SESSION_BUS_ADDRESS"), "unset session bus address was overridden");
    for (const auto &address: {"", "unix:path=/run/user/1000/bus", "unix:abstract=/tmp/session-bus", "autolaunch:",
                              "unix:path=/tmp/first;unix:path=/tmp/second", "tcp:host=localhost,port=1234"}) {
        qputenv("DBUS_SESSION_BUS_ADDRESS", address);
        DesktopSession::normalizeBusAddress();
        check(qgetenv("DBUS_SESSION_BUS_ADDRESS") == address, "existing D-Bus address was changed");
    }
    qputenv("DBUS_SESSION_BUS_ADDRESS", "/run/user/1000/bus");
    DesktopSession::normalizeBusAddress();
    check(qgetenv("DBUS_SESSION_BUS_ADDRESS") == "unix:path=/run/user/1000/bus", "bare socket path was not normalized");
    DesktopSession::normalizeBusAddress();
    QProcess child;
    child.start(app.applicationFilePath(), {"--child"});
    check(child.waitForFinished(5000) && child.exitCode() == 0, "environment probe failed");
    check(child.readAllStandardOutput() == "unix:path=/run/user/1000/bus", "child process did not inherit normalized session bus address");
    qputenv("DBUS_SESSION_BUS_ADDRESS", "/tmp/bus ,;%~");
    DesktopSession::normalizeBusAddress();
    check(qgetenv("DBUS_SESSION_BUS_ADDRESS") == "unix:path=/tmp/bus%20%2C%3B%25%7E", "socket path delimiters were not escaped");
    qInfo() << "Desktop session checks passed";
}
