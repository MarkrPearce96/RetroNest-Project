#include "libretro_adapter.h"
#include "core/paths.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>

QString LibretroAdapter::coreDylibPath(const EmulatorManifest& manifest) {
    return Paths::emulatorsDir("libretro") + "/cores/" + manifest.core_dylib;
}

QString LibretroAdapter::optionsJsonPath() const {
    return Paths::emulatorsDir("libretro") + "/" + coreId() + "/options.json";
}

QString LibretroAdapter::controlsJsonPath() const {
    return Paths::emulatorsDir("libretro") + "/" + coreId() + "/controls.json";
}

bool LibretroAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                   const QString& /*biosPath*/,
                                   const QString& savesPath) {
    QDir().mkpath(savesPath);
    QDir().mkpath(QFileInfo(optionsJsonPath()).absolutePath());
    return true;
}

QString LibretroAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                           const QString& /*installPath*/) {
    return coreDylibPath(manifest);
}

bool LibretroAdapter::isInstalled(const EmulatorManifest& manifest) {
    return QFileInfo::exists(coreDylibPath(manifest));
}

EmulatorAdapter::DirectDownloadInfo
LibretroAdapter::resolveDirectDownload(const EmulatorManifest& manifest) const {
    DirectDownloadInfo info;
    if (manifest.core_buildbot_path.isEmpty()) return info;
    const QString arch =
#if defined(Q_PROCESSOR_ARM_64)
        "arm64";
#else
        "x86_64";
#endif
    const QString url = QString("https://buildbot.libretro.com/nightly/apple/osx/%1/latest/%2")
                        .arg(arch, manifest.core_buildbot_path);

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    auto* reply = nam.head(req);
    QEventLoop loop;
    QTimer t; t.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(8000);
    loop.exec();

    QString lastMod = reply->rawHeader("Last-Modified");
    reply->deleteLater();
    if (lastMod.isEmpty()) lastMod = "unknown";
    info.version = "nightly-" + lastMod.left(16);
    info.publishedAt = lastMod;
    info.assetName = manifest.core_buildbot_path;
    info.downloadUrl = url;
    return info;
}

QString LibretroAdapter::findResumeFile(const QString& /*serial*/) const {
    // Concrete adapters override; libretro resume uses ROM-base-name + ".resume"
    // and is resolved at start time via the StartConfig.resumeStatePath.
    return {};
}

OptionsStore* LibretroAdapter::libretroOptionsStore() {
    return m_runtime ? &m_runtime->options() : nullptr;
}

void LibretroAdapter::prepareRuntime() {
    if (!m_runtime) m_runtime = std::make_unique<CoreRuntime>();
}

void LibretroAdapter::releaseRuntime() {
    m_runtime.reset();
}
