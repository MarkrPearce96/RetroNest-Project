#include "retro_log.h"
#include <QDebug>
#include <cstdarg>
#include <cstdio>

static void retroLogPrintf(enum retro_log_level level, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    QString line = QString::fromUtf8(buf).trimmed();
    if (line.isEmpty()) return;
    switch (level) {
        case RETRO_LOG_DEBUG: qDebug().noquote()    << "[core]" << line; break;
        case RETRO_LOG_INFO:  qInfo().noquote()     << "[core]" << line; break;
        case RETRO_LOG_WARN:  qWarning().noquote()  << "[core]" << line; break;
        case RETRO_LOG_ERROR: qCritical().noquote() << "[core]" << line; break;
        default: qInfo().noquote() << "[core]" << line; break;
    }
}

retro_log_callback retroLogCallback() {
    retro_log_callback cb;
    cb.log = retroLogPrintf;
    return cb;
}
