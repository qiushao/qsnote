#pragma once

#include <QByteArray>

namespace DesktopSession {
inline void normalizeBusAddress() {
#ifdef Q_OS_LINUX
    // A launcher may pass a bare socket path. D-Bus clients require a transport prefix.
    const auto address = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    if (address.startsWith('/'))
        qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + address.toPercentEncoding("/\\*", "~"));
#endif
}
}// namespace DesktopSession
