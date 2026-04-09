# In-Game Menu & Quit System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow users to pause, quit, and save-state from an in-game menu triggered by Escape or Select+B/Circle, replacing the native emulator pause menu.

**Architecture:** Async `GameSession` replaces the blocking `EmulatorLauncher`. A `PineClient` handles IPC with PCSX2 for save/load state. Hotkey injection handles pause/resume. A new `InGameMenu.qml` provides the UI. PCSX2's native pause menu is suppressed via INI patching.

**Tech Stack:** C++17, Qt6 (QML + QProcess + QLocalSocket), SDL2, PINE protocol (Unix domain sockets on macOS)

---

## File Map

### New Files

| File | Responsibility |
|------|----------------|
| `cpp/src/core/game_session.h` | GameSession class — async QProcess wrapper, game lifecycle signals |
| `cpp/src/core/game_session.cpp` | GameSession implementation — start, kill, output forwarding |
| `cpp/src/core/pine_client.h` | PineClient class — PINE IPC protocol over Unix/TCP sockets |
| `cpp/src/core/pine_client.cpp` | PineClient implementation — connect, sendCommand, message framing |
| `cpp/qml/AppUI/InGameMenu.qml` | In-game pause/quit menu with main menu and quit submenu |
| `cpp/qml/AppUI/ResumeStateDialog.qml` | Save state resume prompt on game launch |
| `cpp/tests/test_pine_client.cpp` | PineClient unit tests (message framing, parsing) |

### Modified Files

| File | Changes |
|------|---------|
| `cpp/src/adapters/emulator_adapter.h` | Add IPC/control virtual methods |
| `cpp/src/adapters/pcsx2_adapter.h` | Override IPC/control methods, declare PineClient member |
| `cpp/src/adapters/pcsx2_adapter.cpp` | Implement PINE IPC, add OpenPauseMenu suppression to INI patching |
| `cpp/src/services/game_service.h` | Replace `launchGame()` with async `startGame()`/`stopGame()`, add `gameRunning` property |
| `cpp/src/services/game_service.cpp` | Own GameSession, wire signals, implement stop flows |
| `cpp/src/ui/app_controller.h` | Expose `gameRunning` Q_PROPERTY, add async game signals |
| `cpp/src/ui/app_controller.cpp` | Wire GameService async signals, forward to QML |
| `cpp/src/ui/theme_context.h` | Expose `gameRunning`, add `stopGame()`, `saveAndStopGame()` |
| `cpp/src/ui/theme_context.cpp` | Implement async launch, stop methods |
| `cpp/src/core/sdl_input_manager.h` | Add `inGameMenuRequested()` signal, combo state tracking |
| `cpp/src/core/sdl_input_manager.cpp` | Detect Select+Circle/B combo, emit signal |
| `cpp/qml/AppUI/AppWindow.qml` | Context-switch Escape, integrate InGameMenu, wire combo signal |
| `cpp/CMakeLists.txt` | Add new source files and QML resources |
| `CLAUDE.md` | Document in-game menu architecture, emulator control abstraction |

---

## Task 1: GameSession — Async Process Wrapper

**Files:**
- Create: `cpp/src/core/game_session.h`
- Create: `cpp/src/core/game_session.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create game_session.h**

```cpp
#pragma once

#include "manifest.h"
#include <QObject>
#include <QProcess>

class EmulatorAdapter;

/**
 * GameSession — manages an async emulator process.
 * Only one session at a time. Owned by GameService.
 */
class GameSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit GameSession(QObject* parent = nullptr);
    ~GameSession() override;

    /** Launch the emulator. Returns false if already running or start fails. */
    bool start(const EmulatorManifest& manifest,
               EmulatorAdapter* adapter,
               const QString& romPath);

    /** Kill the emulator process immediately. */
    void kill();

    bool isRunning() const;
    qint64 pid() const;

    /** The adapter for the currently running emulator. Null if not running. */
    EmulatorAdapter* adapter() const { return m_adapter; }

    /** The manifest for the currently running emulator. */
    const EmulatorManifest* manifest() const { return m_manifest; }

signals:
    void runningChanged();
    void started();
    void finished(int exitCode, bool crashed);
    void errorOccurred(const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyRead();

private:
    QProcess* m_process = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    const EmulatorManifest* m_manifest = nullptr;
    QString m_emuId;
};
```

- [ ] **Step 2: Create game_session.cpp**

```cpp
#include "game_session.h"
#include "paths.h"
#include "adapters/emulator_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

GameSession::GameSession(QObject* parent)
    : QObject(parent) {}

GameSession::~GameSession() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool GameSession::start(const EmulatorManifest& manifest,
                        EmulatorAdapter* adapter,
                        const QString& romPath) {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "[GameSession] Already running, cannot start another";
        return false;
    }

    // Verify ROM exists
    if (!QFileInfo::exists(romPath)) {
        emit errorOccurred("ROM file not found: " + romPath);
        return false;
    }

    // Resolve executable
    const QString installPath = Paths::emulatorsDir(manifest.install_folder);
    const QString execPath = QFileInfo(adapter->resolveExecutable(manifest, installPath)).absoluteFilePath();

    if (!QFileInfo::exists(execPath)) {
        emit errorOccurred(manifest.name + " is not installed. Executable not found: " + execPath);
        return false;
    }

    // Ensure config
    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    const QString savesPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath();
    QDir().mkpath(savesPath);

    if (!adapter->ensureConfig(manifest, biosPath, savesPath)) {
        qWarning() << "[GameSession] Config creation/patching failed for" << manifest.name;
    }

    // Build arguments
    QStringList args = adapter->buildLaunchArgs(manifest, romPath);

    // Resolve working directory
    QString cwd;
#if defined(Q_OS_MACOS)
    static const QRegularExpression appRe("^(.+\\.app)/");
    auto match = appRe.match(execPath);
    if (match.hasMatch()) {
        cwd = QFileInfo(match.captured(1)).absolutePath();
    }
#endif
    if (cwd.isEmpty()) {
        cwd = QFileInfo(execPath).absolutePath();
    }

    // Store state
    m_adapter = adapter;
    m_manifest = &manifest;
    m_emuId = manifest.id;

    // Create and configure process
    delete m_process;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(cwd);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::finished, this, &GameSession::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GameSession::onProcessError);
    connect(m_process, &QProcess::readyRead, this, &GameSession::onReadyRead);

    qInfo().noquote() << "[GameSession]" << manifest.name << ":" << execPath << args.join(" ");
    qInfo().noquote() << "[GameSession] CWD:" << cwd;

    m_process->start(execPath, args);

    if (!m_process->waitForStarted(5000)) {
        emit errorOccurred("Failed to start process: " + m_process->errorString());
        return false;
    }

    qInfo() << "[GameSession] PID:" << m_process->processId();
    emit runningChanged();
    emit started();
    return true;
}

void GameSession::kill() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}

bool GameSession::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 GameSession::pid() const {
    return m_process ? m_process->processId() : -1;
}

void GameSession::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool crashed = (exitStatus == QProcess::CrashExit);
    if (crashed) {
        qWarning() << "[GameSession]" << m_emuId << "crashed.";
    } else {
        qInfo() << "[GameSession]" << m_emuId << "exited with code" << exitCode;
    }
    m_adapter = nullptr;
    m_manifest = nullptr;
    emit runningChanged();
    emit finished(exitCode, crashed);
}

void GameSession::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        emit errorOccurred("Process failed to start: " + m_process->errorString());
        m_adapter = nullptr;
        m_manifest = nullptr;
        emit runningChanged();
    }
}

void GameSession::onReadyRead() {
    QByteArray output = m_process->readAll();
    for (const auto& line : output.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            qInfo().noquote() << "  [" + m_emuId + "]" << line.trimmed();
        }
    }
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In the SOURCES section (after `emulator_launcher.cpp`), add:
```
src/core/game_session.cpp
```

In the HEADERS section (after `emulator_launcher.h`), add:
```
src/core/game_session.h
```

- [ ] **Step 4: Build and verify compilation**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation with no errors.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/game_session.h cpp/src/core/game_session.cpp cpp/CMakeLists.txt
git commit -m "feat: add GameSession async process wrapper"
```

---

## Task 2: PineClient — IPC Protocol Client

**Files:**
- Create: `cpp/src/core/pine_client.h`
- Create: `cpp/src/core/pine_client.cpp`
- Create: `cpp/tests/test_pine_client.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create pine_client.h**

```cpp
#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

/**
 * PineClient — PINE protocol IPC client for communicating with emulators.
 *
 * PINE uses Unix domain sockets on macOS/Linux and TCP on Windows.
 * Default socket: $TMPDIR/pcsx2.sock (macOS) or /tmp/pcsx2.sock (Linux).
 * Default TCP port: 28011 (Windows).
 *
 * Message format:
 *   Request:  [4-byte LE length] [opcode byte] [params...]
 *   Response: [4-byte LE length] [status byte] [data...]
 *   Status: 0x00 = OK, 0xFF = FAIL
 */
class PineClient {
public:
    // PINE opcodes
    enum Opcode : uint8_t {
        MsgRead8       = 0x00,
        MsgRead16      = 0x01,
        MsgRead32      = 0x02,
        MsgRead64      = 0x03,
        MsgWrite8      = 0x04,
        MsgWrite16     = 0x05,
        MsgWrite32     = 0x06,
        MsgWrite64     = 0x07,
        MsgVersion     = 0x08,
        MsgSaveState   = 0x09,
        MsgLoadState   = 0x0A,
        MsgTitle       = 0x0B,
        MsgID          = 0x0C,
        MsgUUID        = 0x0D,
        MsgGameVersion = 0x0E,
        MsgStatus      = 0x0F,
    };

    // PINE result codes
    enum Result : uint8_t {
        IPC_OK   = 0x00,
        IPC_FAIL = 0xFF,
    };

    // Emulator status values
    enum EmuStatus : uint32_t {
        Running  = 0,
        Paused   = 1,
        Shutdown = 2,
    };

    PineClient();
    ~PineClient();

    /** Connect to PINE socket. slot = port/suffix (default 28011). */
    bool connectToEmulator(int slot = 28011);

    /** Disconnect from the emulator. */
    void disconnect();

    /** Whether we have an active connection. */
    bool isConnected() const;

    /** Save state to slot (1-10). Returns true on success. */
    bool saveState(int slot);

    /** Load state from slot (1-10). Returns true on success. */
    bool loadState(int slot);

    /** Get emulator status. Returns EmuStatus or -1 on error. */
    int getStatus();

    /** Get game title. Returns empty string on error. */
    QString getTitle();

    /** Get game serial ID (e.g. "SLUS-12345"). Returns empty on error. */
    QString getGameID();

    /** Get game CRC as hex string. Returns empty on error. */
    QString getGameUUID();

    /** Build a PINE request message with the given opcode and payload. */
    static QByteArray buildMessage(Opcode opcode, const QByteArray& payload = {});

    /** Parse a PINE response. Returns true if status is IPC_OK. data receives payload after status byte. */
    static bool parseResponse(const QByteArray& response, QByteArray* data = nullptr);

private:
    /** Send a message and receive the response. Returns empty on error. */
    QByteArray sendCommand(Opcode opcode, const QByteArray& payload = {});

    int m_socket = -1;
};
```

- [ ] **Step 2: Create pine_client.cpp**

```cpp
#include "pine_client.h"

#include <QDebug>
#include <QtEndian>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

static constexpr int PINE_TIMEOUT_MS = 5000;

PineClient::PineClient() {}

PineClient::~PineClient() {
    disconnect();
}

bool PineClient::connectToEmulator(int slot) {
    if (m_socket >= 0)
        disconnect();

#ifdef _WIN32
    // TCP connection on Windows
    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        qWarning() << "[PineClient] Failed to create TCP socket";
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(slot));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::connect(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        qWarning() << "[PineClient] Failed to connect to TCP port" << slot;
        ::close(m_socket);
        m_socket = -1;
        return false;
    }
#else
    // Unix domain socket on macOS/Linux
    m_socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket < 0) {
        qWarning() << "[PineClient] Failed to create Unix socket";
        return false;
    }

    // Build socket path: $TMPDIR/pcsx2.sock (macOS) or /tmp/pcsx2.sock (Linux)
    std::string socketPath;
    const char* tmpdir = nullptr;
#ifdef __APPLE__
    tmpdir = std::getenv("TMPDIR");
#else
    tmpdir = std::getenv("XDG_RUNTIME_DIR");
#endif
    if (tmpdir)
        socketPath = std::string(tmpdir) + "/pcsx2.sock";
    else
        socketPath = "/tmp/pcsx2.sock";

    if (slot != 28011)
        socketPath += "." + std::to_string(slot);

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        qWarning() << "[PineClient] Failed to connect to" << socketPath.c_str();
        ::close(m_socket);
        m_socket = -1;
        return false;
    }
#endif

    qInfo() << "[PineClient] Connected to PINE";
    return true;
}

void PineClient::disconnect() {
    if (m_socket >= 0) {
#ifdef _WIN32
        ::closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = -1;
        qInfo() << "[PineClient] Disconnected";
    }
}

bool PineClient::isConnected() const {
    return m_socket >= 0;
}

QByteArray PineClient::buildMessage(Opcode opcode, const QByteArray& payload) {
    // Format: [4-byte LE length of everything after length] [opcode] [payload]
    uint32_t contentLen = 1 + static_cast<uint32_t>(payload.size());
    QByteArray msg;
    msg.resize(4 + contentLen);
    qToLittleEndian(contentLen, reinterpret_cast<uchar*>(msg.data()));
    msg[4] = static_cast<char>(opcode);
    if (!payload.isEmpty())
        std::memcpy(msg.data() + 5, payload.constData(), payload.size());
    return msg;
}

bool PineClient::parseResponse(const QByteArray& response, QByteArray* data) {
    // Minimum response: 4 bytes length + 1 byte status = 5 bytes
    if (response.size() < 5)
        return false;

    uint8_t status = static_cast<uint8_t>(response.at(4));
    if (data && response.size() > 5)
        *data = response.mid(5);

    return status == IPC_OK;
}

QByteArray PineClient::sendCommand(Opcode opcode, const QByteArray& payload) {
    if (m_socket < 0)
        return {};

    QByteArray msg = buildMessage(opcode, payload);

    // Send
    ssize_t sent = ::send(m_socket, msg.constData(), msg.size(), 0);
    if (sent != msg.size()) {
        qWarning() << "[PineClient] Send failed";
        return {};
    }

    // Receive: first 4 bytes = length
    char lenBuf[4];
    ssize_t received = 0;

    // Poll for readability with timeout
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;
    int pollResult = ::poll(&pfd, 1, PINE_TIMEOUT_MS);
    if (pollResult <= 0) {
        qWarning() << "[PineClient] Receive timeout or error";
        return {};
    }

    received = ::recv(m_socket, lenBuf, 4, MSG_WAITALL);
    if (received != 4) {
        qWarning() << "[PineClient] Failed to read response length";
        return {};
    }

    uint32_t respLen = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(lenBuf));
    if (respLen == 0 || respLen > 450000) {
        qWarning() << "[PineClient] Invalid response length:" << respLen;
        return {};
    }

    QByteArray response;
    response.resize(4 + respLen);
    std::memcpy(response.data(), lenBuf, 4);

    received = ::recv(m_socket, response.data() + 4, respLen, MSG_WAITALL);
    if (received != static_cast<ssize_t>(respLen)) {
        qWarning() << "[PineClient] Incomplete response";
        return {};
    }

    return response;
}

bool PineClient::saveState(int slot) {
    QByteArray payload;
    payload.append(static_cast<char>(slot));
    QByteArray response = sendCommand(MsgSaveState, payload);
    return parseResponse(response);
}

bool PineClient::loadState(int slot) {
    QByteArray payload;
    payload.append(static_cast<char>(slot));
    QByteArray response = sendCommand(MsgLoadState, payload);
    return parseResponse(response);
}

int PineClient::getStatus() {
    QByteArray data;
    QByteArray response = sendCommand(MsgStatus);
    if (!parseResponse(response, &data) || data.size() < 4)
        return -1;
    return static_cast<int>(qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData())));
}

QString PineClient::getTitle() {
    QByteArray data;
    QByteArray response = sendCommand(MsgTitle);
    if (!parseResponse(response, &data))
        return {};
    // PINE strings: 4-byte LE length + UTF-8 data
    if (data.size() < 4)
        return {};
    uint32_t strLen = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData()));
    if (data.size() < static_cast<int>(4 + strLen))
        return {};
    return QString::fromUtf8(data.mid(4, strLen));
}

QString PineClient::getGameID() {
    QByteArray data;
    QByteArray response = sendCommand(MsgID);
    if (!parseResponse(response, &data))
        return {};
    if (data.size() < 4)
        return {};
    uint32_t strLen = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData()));
    if (data.size() < static_cast<int>(4 + strLen))
        return {};
    return QString::fromUtf8(data.mid(4, strLen));
}

QString PineClient::getGameUUID() {
    QByteArray data;
    QByteArray response = sendCommand(MsgUUID);
    if (!parseResponse(response, &data))
        return {};
    if (data.size() < 4)
        return {};
    uint32_t strLen = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(data.constData()));
    if (data.size() < static_cast<int>(4 + strLen))
        return {};
    return QString::fromUtf8(data.mid(4, strLen));
}
```

- [ ] **Step 3: Create test_pine_client.cpp**

```cpp
#include <QtTest/QtTest>
#include "core/pine_client.h"

class TestPineClient : public QObject {
    Q_OBJECT

private slots:
    void testBuildMessageNoPayload() {
        QByteArray msg = PineClient::buildMessage(PineClient::MsgStatus);
        // Length = 1 (opcode only), stored as 4-byte LE
        QCOMPARE(msg.size(), 5);
        // First 4 bytes = length (1)
        uint32_t len = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(msg.constData()));
        QCOMPARE(len, 1u);
        // 5th byte = opcode
        QCOMPARE(static_cast<uint8_t>(msg.at(4)), static_cast<uint8_t>(PineClient::MsgStatus));
    }

    void testBuildMessageWithPayload() {
        QByteArray payload;
        payload.append(static_cast<char>(1)); // slot 1
        QByteArray msg = PineClient::buildMessage(PineClient::MsgSaveState, payload);
        // Length = 2 (opcode + 1 byte payload)
        QCOMPARE(msg.size(), 6);
        uint32_t len = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(msg.constData()));
        QCOMPARE(len, 2u);
        QCOMPARE(static_cast<uint8_t>(msg.at(4)), static_cast<uint8_t>(PineClient::MsgSaveState));
        QCOMPARE(static_cast<uint8_t>(msg.at(5)), static_cast<uint8_t>(1));
    }

    void testParseResponseOK() {
        // Build a fake OK response: length=1, status=0x00
        QByteArray response;
        response.resize(5);
        uint32_t len = 1;
        qToLittleEndian(len, reinterpret_cast<uchar*>(response.data()));
        response[4] = static_cast<char>(PineClient::IPC_OK);

        QVERIFY(PineClient::parseResponse(response));
    }

    void testParseResponseFail() {
        QByteArray response;
        response.resize(5);
        uint32_t len = 1;
        qToLittleEndian(len, reinterpret_cast<uchar*>(response.data()));
        response[4] = static_cast<char>(PineClient::IPC_FAIL);

        QVERIFY(!PineClient::parseResponse(response));
    }

    void testParseResponseWithData() {
        // Build response: length=5, status=OK, data=[0x01, 0x02, 0x03, 0x04]
        QByteArray response;
        response.resize(9);
        uint32_t len = 5;
        qToLittleEndian(len, reinterpret_cast<uchar*>(response.data()));
        response[4] = static_cast<char>(PineClient::IPC_OK);
        response[5] = 0x01;
        response[6] = 0x02;
        response[7] = 0x03;
        response[8] = 0x04;

        QByteArray data;
        QVERIFY(PineClient::parseResponse(response, &data));
        QCOMPARE(data.size(), 4);
        QCOMPARE(static_cast<uint8_t>(data.at(0)), static_cast<uint8_t>(0x01));
    }

    void testParseResponseTooShort() {
        QByteArray response;
        response.resize(3); // Too short
        QVERIFY(!PineClient::parseResponse(response));
    }
};

QTEST_MAIN(TestPineClient)
#include "test_pine_client.moc"
```

- [ ] **Step 4: Add to CMakeLists.txt**

In the SOURCES section (after `game_session.cpp`):
```
src/core/pine_client.cpp
```

In the HEADERS section (after `game_session.h`):
```
src/core/pine_client.h
```

In the test section (after the existing test targets), add:
```cmake
add_executable(test_pine_client tests/test_pine_client.cpp src/core/pine_client.cpp)
target_link_libraries(test_pine_client PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_pine_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME test_pine_client COMMAND test_pine_client)
```

- [ ] **Step 5: Build and run tests**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
./build/test_pine_client
```
Expected: All 6 tests pass.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/pine_client.h cpp/src/core/pine_client.cpp cpp/tests/test_pine_client.cpp cpp/CMakeLists.txt
git commit -m "feat: add PineClient IPC protocol client with tests"
```

---

## Task 3: Emulator Control Abstraction — Adapter Extensions

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`

- [ ] **Step 1: Add virtual control methods to EmulatorAdapter**

After the existing `hotkeyBindingDefs()` virtual method in `emulator_adapter.h`, add:

```cpp
    // ── Emulator Control (IPC + hotkey injection) ──────────────────────

    /** Whether this emulator supports IPC (e.g. PINE protocol). */
    virtual bool supportsIPC() const { return false; }

    /** Connect to the running emulator via IPC. Called after process starts. */
    virtual bool connectIPC(qint64 pid) { Q_UNUSED(pid); return false; }

    /** Disconnect IPC. Called when process exits. */
    virtual void disconnectIPC() {}

    /** Send pause command. Uses IPC if available, otherwise returns false. */
    virtual bool sendPause() { return false; }

    /** Send resume command. Uses IPC if available, otherwise returns false. */
    virtual bool sendResume() { return false; }

    /** Save state to the given slot (1-10). */
    virtual bool sendSaveState(int slot) { Q_UNUSED(slot); return false; }

    /** Load state from the given slot (1-10). */
    virtual bool sendLoadState(int slot) { Q_UNUSED(slot); return false; }

    /** Fallback: key string to inject for pause/resume toggle. */
    virtual QString pauseHotkeyString() const { return {}; }

    /** Get the running game's serial ID via IPC (e.g. "SLUS-12345"). */
    virtual QString getGameSerial() { return {}; }

    /** Get the running game's CRC via IPC (hex string). */
    virtual QString getGameCRC() { return {}; }

    /** Check if a save state exists for the given serial, CRC, and slot. */
    virtual bool hasSaveState(const QString& gameSerial, const QString& gameCRC, int slot) const {
        Q_UNUSED(gameSerial); Q_UNUSED(gameCRC); Q_UNUSED(slot);
        return false;
    }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation. All existing adapters still compile (they inherit defaults).

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "feat: add IPC and control virtual methods to EmulatorAdapter"
```

---

## Task 4: PCSX2 Adapter — PINE IPC + Menu Suppression

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.h`
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

- [ ] **Step 1: Add PINE overrides and member to pcsx2_adapter.h**

Add `#include "core/pine_client.h"` at the top.

Add the following overrides in the public section (after `hotkeyBindingDefs`):

```cpp
    // ── Emulator Control ──
    bool supportsIPC() const override { return true; }
    bool connectIPC(qint64 pid) override;
    void disconnectIPC() override;
    bool sendPause() override;
    bool sendResume() override;
    bool sendSaveState(int slot) override;
    bool sendLoadState(int slot) override;
    QString pauseHotkeyString() const override;
    QString getGameSerial() override;
    QString getGameCRC() override;
    bool hasSaveState(const QString& gameSerial, const QString& gameCRC, int slot) const override;
```

Add a private member:
```cpp
    PineClient m_pine;
```

- [ ] **Step 2: Implement PINE control methods in pcsx2_adapter.cpp**

Add at the end of the file (before the closing):

```cpp
// ============================================================================
// Emulator Control — PINE IPC
// ============================================================================

bool PCSX2Adapter::connectIPC(qint64 /*pid*/) {
    return m_pine.connectToEmulator();
}

void PCSX2Adapter::disconnectIPC() {
    m_pine.disconnect();
}

bool PCSX2Adapter::sendPause() {
    // PINE doesn't support pause directly — handled by hotkey injection
    return false;
}

bool PCSX2Adapter::sendResume() {
    // PINE doesn't support resume directly — handled by hotkey injection
    return false;
}

bool PCSX2Adapter::sendSaveState(int slot) {
    if (!m_pine.isConnected()) {
        qWarning() << "[PCSX2] Cannot save state: PINE not connected";
        return false;
    }
    return m_pine.saveState(slot);
}

bool PCSX2Adapter::sendLoadState(int slot) {
    if (!m_pine.isConnected()) {
        qWarning() << "[PCSX2] Cannot load state: PINE not connected";
        return false;
    }
    return m_pine.loadState(slot);
}

QString PCSX2Adapter::pauseHotkeyString() const {
    return QStringLiteral("Keyboard/Escape");
}

QString PCSX2Adapter::getGameSerial() {
    if (!m_pine.isConnected())
        return {};
    return m_pine.getGameID();
}

QString PCSX2Adapter::getGameCRC() {
    if (!m_pine.isConnected())
        return {};
    return m_pine.getGameUUID();
}

bool PCSX2Adapter::hasSaveState(const QString& gameSerial, const QString& gameCRC, int slot) const {
    // PCSX2 save state path: {savestates}/{serial} ({crc}).{slot:02d}.p2s
    // The savestates dir is under the saves path configured in ensureConfig
    const QString systemId = "ps2";
    const QString savesPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath();
    const QString savestatesDir = savesPath + "/savestates";
    const QString filename = QString("%1 (%2).%3.p2s")
        .arg(gameSerial, gameCRC)
        .arg(slot, 2, 10, QChar('0'));
    const QString fullPath = savestatesDir + "/" + filename;
    return QFileInfo::exists(fullPath);
}
```

- [ ] **Step 3: Add OpenPauseMenu suppression to createDefaultConfig**

In `createDefaultConfig()`, add a `[Hotkeys]` section after the `[Pad1]` section:

```cpp
        "[Pad1]",
        "",
        "[Hotkeys]",
        "OpenPauseMenu =",
        "",
```

- [ ] **Step 4: Add OpenPauseMenu suppression to patchExistingConfig**

In `patchExistingConfig()`, add to the `IniKeyPatch` vector (alongside the existing folder patches):

```cpp
    // Suppress native pause menu — our in-game menu replaces it
    QVector<IniKeyPatch> hotkeyPatches = {
        {"Hotkeys", "OpenPauseMenu", ""},
    };
    if (patchIniKeys(content, hotkeyPatches))
        changed = true;
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "feat: implement PINE IPC control and suppress native pause menu for PCSX2"
```

---

## Task 5: Wire GameService for Async Launch

**Files:**
- Modify: `cpp/src/services/game_service.h`
- Modify: `cpp/src/services/game_service.cpp`

- [ ] **Step 1: Update game_service.h**

Add `#include "core/game_session.h"` at the top.

Replace the `LaunchResult launchGame(...)` method and add new methods/properties:

```cpp
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)

    /** Start a game asynchronously. Returns false if start fails immediately. */
    bool startGame(const QString& romPath, const QString& emuId);

    /** Stop the running game (kill process). */
    void stopGame();

    /** Save state to slot and then stop. */
    void saveAndStopGame(int slot);

    /** Connect IPC to the running emulator (call after process starts). */
    bool connectIPC();

    /** Check if a save state exists for the current game at the given slot. */
    bool hasSaveStateForCurrentGame(int slot) const;

    /** Load a save state for the running game. */
    bool loadSaveState(int slot);

    bool isGameRunning() const;

    /** Access the game session (for adapter control). */
    GameSession* session() { return &m_session; }

signals:
    void statusMessage(const QString& msg);
    void gameRunningChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
    void gameError(const QString& error);
```

Add private members:
```cpp
    GameSession m_session;
    QString m_gameSerial;
    QString m_gameCRC;
```

Keep the existing `LaunchResult` struct and `launchGame()` declaration for now — they will be removed once all callers are updated. Mark with a comment: `// DEPRECATED — use startGame()`

- [ ] **Step 2: Update game_service.cpp**

Add the async methods. Keep the existing `launchGame()` temporarily:

```cpp
bool GameService::isGameRunning() const {
    return m_session.isRunning();
}

bool GameService::startGame(const QString& romPath, const QString& emuId) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) {
        emit gameError("No manifest for: " + emuId);
        return false;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) {
        emit gameError("No adapter for: " + emuId);
        return false;
    }

    if (!adapter->isInstalled(*manifest)) {
        emit gameError(manifest->name + " not installed. Go to Settings > Emulators to install.");
        return false;
    }

    if (!QFileInfo::exists(romPath)) {
        emit gameError("ROM not found: " + romPath);
        return false;
    }

    emit statusMessage("Launching: " + QFileInfo(romPath).completeBaseName());

    if (!m_session.start(*manifest, adapter, romPath)) {
        return false;
    }

    return true;
}

void GameService::stopGame() {
    m_session.kill();
}

void GameService::saveAndStopGame(int slot) {
    auto* adapter = m_session.adapter();
    if (adapter && adapter->supportsIPC()) {
        if (adapter->sendSaveState(slot)) {
            qInfo() << "[GameService] Save state to slot" << slot << "succeeded, stopping";
        } else {
            qWarning() << "[GameService] Save state to slot" << slot << "failed, stopping anyway";
        }
    }
    m_session.kill();
}

bool GameService::connectIPC() {
    auto* adapter = m_session.adapter();
    if (!adapter || !adapter->supportsIPC())
        return false;
    if (!adapter->connectIPC(m_session.pid()))
        return false;

    // Query and store game serial + CRC for save state detection
    m_gameSerial = adapter->getGameSerial();
    m_gameCRC = adapter->getGameCRC();
    qInfo() << "[GameService] Game serial:" << m_gameSerial << "CRC:" << m_gameCRC;

    return true;
}

bool GameService::hasSaveStateForCurrentGame(int slot) const {
    auto* adapter = m_session.adapter();
    if (!adapter)
        return false;
    return adapter->hasSaveState(m_gameSerial, m_gameCRC, slot);
}

bool GameService::loadSaveState(int slot) {
    auto* adapter = m_session.adapter();
    if (!adapter)
        return false;
    return adapter->sendLoadState(slot);
}
```

- [ ] **Step 3: Wire GameSession signals in GameService constructor**

In the constructor, after `m_db(db)`:

```cpp
    connect(&m_session, &GameSession::started, this, [this]() {
        emit gameRunningChanged();
        emit gameStarted();
    });

    connect(&m_session, &GameSession::finished, this, [this](int exitCode, bool crashed) {
        auto* adapter = m_session.adapter();
        if (adapter)
            adapter->disconnectIPC();
        emit gameRunningChanged();
        emit gameFinished(exitCode, crashed);
        if (crashed)
            emit statusMessage("Emulator crashed");
        else
            emit statusMessage("Game exited (code " + QString::number(exitCode) + ")");
    });

    connect(&m_session, &GameSession::errorOccurred, this, [this](const QString& error) {
        emit gameError(error);
        emit statusMessage("Launch failed: " + error);
    });
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/game_service.h cpp/src/services/game_service.cpp
git commit -m "feat: wire GameService for async game launch via GameSession"
```

---

## Task 6: Wire AppController and ThemeContext for Async Launch

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/ui/theme_context.h`
- Modify: `cpp/src/ui/theme_context.cpp`

- [ ] **Step 1: Update app_controller.h**

Add a `gameRunning` Q_PROPERTY:
```cpp
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)
```

Add methods:
```cpp
    bool isGameRunning() const;
    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void saveAndStopGame(int slot);
    Q_INVOKABLE bool connectGameIPC();
    Q_INVOKABLE bool loadSaveState(int slot);
    Q_INVOKABLE bool hasSaveStateForCurrentGame(int slot);
```

Add signals:
```cpp
    void gameRunningChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
```

- [ ] **Step 2: Update app_controller.cpp**

Change `launchGame()` to use async start:
```cpp
void AppController::launchGame(int /*gameId*/, const QString& romPath, const QString& emuId) {
    if (!m_gameService.startGame(romPath, emuId)) {
        setStatus("Launch failed");
    }
}
```

Add new methods:
```cpp
bool AppController::isGameRunning() const {
    return m_gameService.isGameRunning();
}

void AppController::stopGame() {
    m_gameService.stopGame();
}

void AppController::saveAndStopGame(int slot) {
    m_gameService.saveAndStopGame(slot);
}

bool AppController::connectGameIPC() {
    return m_gameService.connectIPC();
}

bool AppController::loadSaveState(int slot) {
    return m_gameService.loadSaveState(slot);
}

bool AppController::hasSaveStateForCurrentGame(int slot) {
    return m_gameService.hasSaveStateForCurrentGame(slot);
}
```

Wire GameService signals in constructor (or init):
```cpp
    connect(&m_gameService, &GameService::gameRunningChanged, this, &AppController::gameRunningChanged);
    connect(&m_gameService, &GameService::gameStarted, this, &AppController::gameStarted);
    connect(&m_gameService, &GameService::gameFinished, this, &AppController::gameFinished);
```

- [ ] **Step 3: Update theme_context.h**

Add property and methods:
```cpp
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)

    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void saveAndStopGame(int slot);
    Q_INVOKABLE bool connectGameIPC();
    Q_INVOKABLE bool loadSaveState(int slot);
    Q_INVOKABLE bool hasSaveStateForCurrentGame(int slot);

signals:
    void gameRunningChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
```

- [ ] **Step 4: Update theme_context.cpp**

Change `launchGame()`:
```cpp
void ThemeContext::launchGame(int gameId, const QString& romPath, const QString& emuId) {
    m_db->recordGameLaunch(gameId);
    m_app->launchGame(gameId, romPath, emuId);
    // Don't reload here — game is now async, reload on finish
}
```

Add methods:
```cpp
bool ThemeContext::isGameRunning() const {
    return m_app->isGameRunning();
}

void ThemeContext::stopGame() {
    m_app->stopGame();
}

void ThemeContext::saveAndStopGame(int slot) {
    m_app->saveAndStopGame(slot);
}

bool ThemeContext::connectGameIPC() {
    return m_app->connectGameIPC();
}

bool ThemeContext::loadSaveState(int slot) {
    return m_app->loadSaveState(slot);
}

bool ThemeContext::hasSaveStateForCurrentGame(int slot) {
    return m_app->hasSaveStateForCurrentGame(slot);
}
```

Wire signals in constructor:
```cpp
    connect(m_app, &AppController::gameRunningChanged, this, &ThemeContext::gameRunningChanged);
    connect(m_app, &AppController::gameStarted, this, &ThemeContext::gameStarted);
    connect(m_app, &AppController::gameFinished, this, [this](int, bool) {
        m_gameModel->reload();
        emit gameFinished(0, false);
    });
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp
git commit -m "feat: wire AppController and ThemeContext for async game launch"
```

---

## Task 7: Select+B/Circle Combo Detection in SdlInputManager

**Files:**
- Modify: `cpp/src/core/sdl_input_manager.h`
- Modify: `cpp/src/core/sdl_input_manager.cpp`

- [ ] **Step 1: Add signal and state to sdl_input_manager.h**

Add signal:
```cpp
    void inGameMenuRequested();
```

Add private members:
```cpp
    bool m_selectHeld = false;
```

- [ ] **Step 2: Implement combo detection in sdl_input_manager.cpp**

In the controller button handling section (where `navigateStart()` is emitted), add combo detection. Find the button press handler and add:

```cpp
// Select + B/Circle combo detection for in-game menu
if (button == SDL_CONTROLLER_BUTTON_BACK) {
    m_selectHeld = pressed;
}

if (pressed && m_selectHeld && button == SDL_CONTROLLER_BUTTON_B) {
    emit inGameMenuRequested();
    return; // Don't inject B as Key_Back when used in combo
}
```

The exact location depends on the button dispatch code. This should be added in the `processControllerButton()` or equivalent method, before the normal key injection logic, so the combo takes priority.

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/sdl_input_manager.h cpp/src/core/sdl_input_manager.cpp
git commit -m "feat: detect Select+B/Circle combo for in-game menu trigger"
```

---

## Task 8: InGameMenu QML Component

**Files:**
- Create: `cpp/qml/AppUI/InGameMenu.qml`
- Modify: `cpp/CMakeLists.txt` (QML resources)

- [ ] **Step 1: Create InGameMenu.qml**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15

/**
 * InGameMenu — in-game pause/quit overlay.
 * Two states: "main" (Resume / Quit Game) and "quit" (submenu).
 * Emulator is paused while this menu is open.
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    // State: "main" or "quit"
    property string menuState: "main"
    property int focusIndex: 0

    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function open() {
        menuState = "main";
        focusIndex = 0;
        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    // ── Scrim ──
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.visible ? 0.7 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        MouseArea { anchors.fill: parent } // Block clicks through
    }

    // ── Card ──
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 400
        height: contentCol.implicitHeight + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1

        Column {
            id: contentCol
            anchors {
                left: parent.left; right: parent.right
                top: parent.top
                margins: 24
            }
            spacing: 4

            // ── Title ──
            Text {
                text: menuState === "main" ? "Paused" : "Quit Game"
                font.pixelSize: 20
                font.bold: true
                color: "#ffffff"
                bottomPadding: 12
            }

            // ── Main Menu Items ──
            Repeater {
                model: menuState === "main" ? mainMenuModel : quitMenuModel

                delegate: Rectangle {
                    width: contentCol.width
                    height: 44
                    radius: 6
                    color: root.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: modelData.label
                        font.pixelSize: 15
                        font.bold: root.focusIndex === index
                        color: modelData.destructive
                               ? (root.focusIndex === index ? "#ff6666" : "#ff4444")
                               : (root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6))
                    }
                }
            }
        }
    }

    // ── Menu Models ──
    property var mainMenuModel: [
        { label: "Resume Game", action: "resume", destructive: false },
        { label: "Quit Game", action: "quit", destructive: false }
    ]

    property var quitMenuModel: [
        { label: "Back to Pause Menu", action: "back", destructive: false },
        { label: "Exit & Save State", action: "exitSave", destructive: false },
        { label: "Exit Without Saving", action: "exitNoSave", destructive: true }
    ]

    property var currentModel: menuState === "main" ? mainMenuModel : quitMenuModel

    // ── Input Handling ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(currentModel.length - 1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return) {
            executeAction(currentModel[focusIndex].action);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (menuState === "quit") {
                menuState = "main";
                focusIndex = 0;
            } else {
                resumeRequested();
            }
            event.accepted = true;
        }
    }

    function executeAction(action) {
        switch (action) {
        case "resume":
            resumeRequested();
            break;
        case "quit":
            menuState = "quit";
            focusIndex = 0;
            break;
        case "back":
            menuState = "main";
            focusIndex = 0;
            break;
        case "exitSave":
            exitWithSaveRequested();
            break;
        case "exitNoSave":
            exitWithoutSaveRequested();
            break;
        }
    }
}
```

- [ ] **Step 2: Add to CMakeLists.txt QML resources**

In the `qt_add_qml_module` or QML resource section for `AppUI`, add:
```
AppUI/InGameMenu.qml
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/InGameMenu.qml cpp/CMakeLists.txt
git commit -m "feat: add InGameMenu QML component with main and quit submenus"
```

---

## Task 9: ResumeStateDialog QML Component

**Files:**
- Create: `cpp/qml/AppUI/ResumeStateDialog.qml`
- Modify: `cpp/CMakeLists.txt` (QML resources)

- [ ] **Step 1: Create ResumeStateDialog.qml**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15

/**
 * ResumeStateDialog — shown on game launch if a save state exists.
 * Asks user whether to resume from save state or start fresh.
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    property int focusIndex: 0

    signal resumeChosen()
    signal startFreshChosen()

    function open() {
        focusIndex = 0;
        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    // ── Scrim ──
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.visible ? 0.7 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        MouseArea { anchors.fill: parent }
    }

    // ── Card ──
    Rectangle {
        anchors.centerIn: parent
        width: 420
        height: contentCol.implicitHeight + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1

        Column {
            id: contentCol
            anchors {
                left: parent.left; right: parent.right
                top: parent.top
                margins: 24
            }
            spacing: 8

            Text {
                text: "Save State Found"
                font.pixelSize: 20
                font.bold: true
                color: "#ffffff"
            }

            Text {
                text: "A save state was found. Resume from where you left off?"
                font.pixelSize: 14
                color: Qt.rgba(1, 1, 1, 0.6)
                wrapMode: Text.WordWrap
                width: parent.width
                bottomPadding: 8
            }

            // ── Buttons ──
            Repeater {
                model: [
                    { label: "Resume", action: "resume" },
                    { label: "Start Fresh", action: "fresh" }
                ]

                delegate: Rectangle {
                    width: contentCol.width
                    height: 44
                    radius: 6
                    color: root.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: modelData.label
                        font.pixelSize: 15
                        font.bold: root.focusIndex === index
                        color: root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6)
                    }
                }
            }
        }
    }

    // ── Input Handling ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return) {
            if (focusIndex === 0) {
                resumeChosen();
            } else {
                startFreshChosen();
            }
            event.accepted = true;
        }
    }
}
```

- [ ] **Step 2: Add to CMakeLists.txt QML resources**

In the QML resource section for `AppUI`, add:
```
AppUI/ResumeStateDialog.qml
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ResumeStateDialog.qml cpp/CMakeLists.txt
git commit -m "feat: add ResumeStateDialog QML component for save state prompt"
```

---

## Task 10: Wire Everything in AppWindow.qml

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`

- [ ] **Step 1: Add InGameMenu and ResumeStateDialog instances**

After the `GameActionPopup` declaration, add:

```qml
    InGameMenu {
        id: inGameMenu

        onResumeRequested: {
            inGameMenu.close();
            // Unpause is handled by injecting the pause toggle hotkey
            // into the emulator process (same key that paused it)
        }

        onExitWithSaveRequested: {
            inGameMenu.close();
            themeContext.saveAndStopGame(1);
        }

        onExitWithoutSaveRequested: {
            inGameMenu.close();
            themeContext.stopGame();
        }
    }

    ResumeStateDialog {
        id: resumeStateDialog

        onResumeChosen: {
            resumeStateDialog.close();
            themeContext.loadSaveState(1);
        }

        onStartFreshChosen: {
            resumeStateDialog.close();
        }
    }
```

- [ ] **Step 2: Context-switch Escape key**

Find the existing Escape key handler (the `Shortcut` for settings overlay). Modify it to check game state:

```qml
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (app.gameRunning) {
                if (inGameMenu.visible) {
                    inGameMenu.close();
                } else {
                    inGameMenu.open();
                }
            } else {
                settingsOverlay.toggle();
            }
        }
    }
```

- [ ] **Step 3: Wire controller combo signal**

Add a `Connections` block for the `inGameMenuRequested` signal:

```qml
    Connections {
        target: inputManager
        function onInGameMenuRequested() {
            if (!app.gameRunning) return;
            if (inGameMenu.visible) {
                inGameMenu.close();
            } else {
                inGameMenu.open();
            }
        }
    }
```

- [ ] **Step 4: Wire game lifecycle signals**

Add connections for game start/finish:

```qml
    Connections {
        target: themeContext
        function onGameStarted() {
            // Give emulator time to start its PINE server before connecting
            ipcConnectTimer.start();
        }
        function onGameFinished(exitCode, crashed) {
            inGameMenu.close();
            resumeStateDialog.close();
        }
    }

    Timer {
        id: ipcConnectTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (themeContext.connectGameIPC()) {
                // IPC connected — check for save state in slot 1
                // hasSaveState checks the file path on disk via the adapter
                if (themeContext.hasSaveStateForCurrentGame(1)) {
                    resumeStateDialog.open();
                }
            }
        }
    }
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation.

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "feat: integrate InGameMenu and ResumeStateDialog into AppWindow"
```

---

## Task 11: Remove EmulatorLauncher

**Files:**
- Delete: `cpp/src/core/emulator_launcher.h`
- Delete: `cpp/src/core/emulator_launcher.cpp`
- Modify: `cpp/CMakeLists.txt`
- Modify: `cpp/src/services/game_service.h` (remove old LaunchResult and launchGame)
- Modify: `cpp/src/services/game_service.cpp` (remove old launchGame method)

- [ ] **Step 1: Remove EmulatorLauncher include from game_service.cpp**

Remove the line:
```cpp
#include "core/emulator_launcher.h"
```

- [ ] **Step 2: Remove old launchGame method from game_service.h and .cpp**

Remove the `LaunchResult` struct and `launchGame()` declaration from the header.
Remove the `launchGame()` implementation from the .cpp file.

- [ ] **Step 3: Remove from CMakeLists.txt**

Remove `src/core/emulator_launcher.cpp` from SOURCES and `src/core/emulator_launcher.h` from HEADERS.

- [ ] **Step 4: Delete the files**

```bash
rm cpp/src/core/emulator_launcher.h cpp/src/core/emulator_launcher.cpp
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Clean compilation. If there are other references to EmulatorLauncher, fix them (they should all go through GameSession now).

- [ ] **Step 6: Commit**

```bash
git add -A cpp/src/core/emulator_launcher.h cpp/src/core/emulator_launcher.cpp cpp/CMakeLists.txt cpp/src/services/game_service.h cpp/src/services/game_service.cpp
git commit -m "refactor: remove EmulatorLauncher, fully replaced by GameSession"
```

---

## Task 12: Update CLAUDE.md and Memory

**Files:**
- Modify: `CLAUDE.md`
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-EmuFront-Project/memory/reference_emulator_control.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-EmuFront-Project/memory/MEMORY.md`

- [ ] **Step 1: Update CLAUDE.md — Add In-Game Menu section**

After the "## Input System" section, add:

```markdown
## In-Game Menu & Emulator Control

### In-Game Menu
- **Trigger:** Escape (keyboard) or Select+B/Circle (controller)
- **When game running:** Escape opens InGameMenu instead of SettingsOverlay
- **Menu flow:** Paused → Resume / Quit Game → Back / Exit & Save State / Exit Without Saving
- **Emulator paused** while menu is open (via hotkey injection)

### Emulator Control Abstraction
Each adapter can implement control methods for communicating with a running emulator:

| Method | Purpose | PCSX2 | DuckStation |
|--------|---------|-------|-------------|
| `supportsIPC()` | Has native IPC protocol | Yes (PINE) | No |
| `connectIPC(pid)` | Connect after process starts | PINE socket | — |
| `sendSaveState(slot)` | Save state to slot | PINE opcode 0x09 | — |
| `sendLoadState(slot)` | Load state from slot | PINE opcode 0x0A | — |
| `sendPause()` / `sendResume()` | Pause/resume emulation | Hotkey injection | — |
| `pauseHotkeyString()` | Key to inject for pause toggle | `Keyboard/Escape` | — |
| `hasSaveState(serial, crc, slot)` | Check if save state file exists | Checks .p2s file | — |

### PINE Protocol (PCSX2)
- IPC over Unix domain sockets (macOS: `$TMPDIR/pcsx2.sock`, Linux: `/tmp/pcsx2.sock`)
- TCP on Windows (port 28011)
- `PineClient` class in `core/pine_client.h` handles connection and messaging
- Message format: `[4-byte LE length][opcode][params]` → `[4-byte LE length][status][data]`

### GameSession (Async Launcher)
- `GameSession` replaces the old blocking `EmulatorLauncher`
- Owned by `GameService`, exposes `gameRunning` Q_PROPERTY up through AppController and ThemeContext
- QProcess runs asynchronously — Qt event loop stays responsive during gameplay
- Signals: `started()`, `finished(exitCode, crashed)`, `errorOccurred(error)`

### Adding Emulator Control to a New Adapter
1. Override `supportsIPC()` → `true` if the emulator has an IPC protocol
2. Implement `connectIPC()` / `disconnectIPC()` for the protocol
3. Implement `sendSaveState()` / `sendLoadState()` via IPC
4. Override `pauseHotkeyString()` for hotkey injection fallback
5. Implement `hasSaveState()` to check save state file paths
6. Add `OpenPauseMenu` suppression to INI patching (if emulator has a native pause menu)
```

- [ ] **Step 2: Update CLAUDE.md — Add new files to Repository Layout**

In the `src/core/` section, add:
```
      game_session.*        — async QProcess wrapper, game lifecycle
      pine_client.*         — PINE IPC protocol client (save/load state, status)
```

In the `qml/AppUI/` section, add:
```
      InGameMenu.qml        — in-game pause/quit menu overlay
      ResumeStateDialog.qml — save state resume prompt on launch
```

- [ ] **Step 3: Update CLAUDE.md — Add to "Adding a new emulator adapter" checklist**

After the existing checklist item about system mappings, add:

```markdown
11. **Emulator control** (if applicable) — override control methods in the adapter:
    - `supportsIPC()`, `connectIPC()`, `disconnectIPC()` — for IPC-capable emulators
    - `sendSaveState()`, `sendLoadState()` — save state support
    - `pauseHotkeyString()` — hotkey injection fallback for pause/resume
    - `hasSaveState()` — check save state file existence
    - Suppress native pause menu in `createDefaultConfig()` and `patchExistingConfig()`
```

- [ ] **Step 4: Create memory file**

Create `/Users/mark/.claude/projects/-Users-mark-Documents-EmuFront-Project/memory/reference_emulator_control.md`:

```markdown
---
name: Emulator control architecture
description: How to control running emulators — IPC (PINE for PCSX2), hotkey injection fallback, GameSession async process management
type: reference
---

Emulator control uses a hybrid approach: IPC where available, hotkey injection as fallback.

**PINE Protocol (PCSX2):**
- Unix socket at $TMPDIR/pcsx2.sock (macOS) or /tmp/pcsx2.sock (Linux), TCP port 28011 (Windows)
- Opcodes: SaveState (0x09), LoadState (0x0A), Status (0x0F), Title (0x0B), ID (0x0C), UUID (0x0D)
- No native pause/resume — use hotkey injection instead
- Save state file format: `{serial} ({crc}).{slot:02d}.p2s` in savestates dir

**Hotkey Injection:**
- Universal fallback for emulators without IPC
- Adapter provides `pauseHotkeyString()`, GameSession injects the key into the emulator window
- PCSX2 pause key: `Keyboard/Escape` (but we suppress OpenPauseMenu in INI, so Escape only toggles pause)

**GameSession:**
- Replaced the old blocking EmulatorLauncher
- Async QProcess, signals for lifecycle, owned by GameService
- `gameRunning` property exposed through GameService → AppController → ThemeContext → QML

**PCSX2 native pause menu** is suppressed by setting `OpenPauseMenu =` (empty) in [Hotkeys] INI section.
```

- [ ] **Step 5: Update MEMORY.md**

Add entry:
```markdown
- [Emulator control architecture](reference_emulator_control.md) — PINE IPC, hotkey injection fallback, GameSession async process, save state file paths
```

- [ ] **Step 6: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update CLAUDE.md with in-game menu architecture and emulator control"
```

Memory files are not committed (they live outside the repo).

---

## Task 13: Final Build Verification and Cleanup

- [ ] **Step 1: Full clean build**

```bash
cd cpp && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -30
```
Expected: Clean compilation with no errors or warnings related to the new code.

- [ ] **Step 2: Run all tests**

```bash
cd cpp && ./build/test_ini_file && ./build/test_rom_scanner && ./build/test_pine_client
```
Expected: All tests pass.

- [ ] **Step 3: Verify app launches**

```bash
cd cpp && ./build/EmulatorFrontend &
```
Expected: App launches without crashes. Settings overlay still works (Escape when no game running).

- [ ] **Step 4: Commit any final fixes**

If any issues found, fix and commit individually.
