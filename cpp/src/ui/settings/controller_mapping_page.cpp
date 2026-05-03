#include "controller_mapping_page.h"
#include "ds2_bindings_widget.h"
#include "guitar_bindings_widget.h"
#include "jogcon_bindings_widget.h"
#include "negcon_bindings_widget.h"
#include "digital_bindings_widget.h"
#include "analog_bindings_widget.h"
#include "analog_joystick_bindings_widget.h"
#include "ds_negcon_bindings_widget.h"
#include "ds_negcon_rumble_bindings_widget.h"
#include "ds_jogcon_bindings_widget.h"
#include "popn_bindings_widget.h"
#include "psp_bindings_widget.h"
#include "controller_settings_widget.h"
#include "binding_widget_common.h"
#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QLineEdit>
#include <QDialogButtonBox>

static const QString kToolBtnStyle = QStringLiteral(
    "QToolButton { background: %1; color: %2; border: 1px solid %3;"
    "  border-radius: 6px; font-size: 13px; padding: 6px 14px; }"
    "QToolButton:hover { background: %4; }"
    "QToolButton:checked { background: %5; color: #ffffff; border-color: %5; }"
).arg(kBtnDefault, kTextPrimary, kBoxBorder, kBtnHover, kAccent);

static const QString kProfileBtnStyle = QStringLiteral(
    "QPushButton { background: %1; color: %2; border: 1px solid %3;"
    "  border-radius: 6px; font-size: 12px; padding: 6px 12px; }"
    "QPushButton:hover { background: %4; }"
    "QPushButton:disabled { background: #2a2a48; color: #606080; border-color: #2a2a48; }"
).arg(kBtnDefault, kTextPrimary, kBoxBorder, kBtnHover);

static const QString kComboStyle = QStringLiteral(
    "QComboBox { background: %1; color: %2; border: 1px solid %3;"
    "  border-radius: 6px; font-size: 13px; padding: 6px 10px; min-width: 160px; }"
    "QComboBox:hover { background: %4; }"
    "QComboBox::drop-down { border: none; width: 24px; }"
    "QComboBox QAbstractItemView { background: #1e1e3a; color: %2;"
    "  border: 1px solid %3; selection-background-color: %5; }"
).arg(kBtnDefault, kTextPrimary, kBoxBorder, kBtnHover, kAccent);

// ── Dark-themed text input dialog ────────────────────────────

static QString darkInputDialog(QWidget* parent, const QString& title,
                                const QString& prompt, const QString& initial = "") {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setFixedSize(360, 160);
    dlg.setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel { color: %2; font-size: 13px; background: transparent; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 4px; padding: 6px 10px; font-size: 13px; }"
        "QPushButton { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 6px; padding: 6px 18px; font-size: 13px; }"
        "QPushButton:hover { background: %5; }"
        "QPushButton:default { background: %6; color: #ffffff; border: none; }"
        "QPushButton:default:hover { background: #7c6cf7; }"
    ).arg("#242440", "#e8e8ff", "#353558", "#3a3a60", "#404070", "#6c5ce7"));

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* label = new QLabel(prompt);
    layout->addWidget(label);

    auto* edit = new QLineEdit(initial);
    layout->addWidget(edit);
    edit->setFocus();

    auto* buttons = new QDialogButtonBox();
    auto* cancelBtn = buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    auto* okBtn = buttons->addButton("OK", QDialogButtonBox::AcceptRole);
    okBtn->setDefault(true);
    Q_UNUSED(cancelBtn);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted && !edit->text().trimmed().isEmpty())
        return edit->text().trimmed();
    return {};
}

static bool darkConfirmDialog(QWidget* parent, const QString& title, const QString& message) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setFixedSize(340, 140);
    dlg.setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel { color: %2; font-size: 13px; background: transparent; }"
        "QPushButton { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 6px; padding: 6px 18px; font-size: 13px; }"
        "QPushButton:hover { background: %5; }"
        "QPushButton:default { background: #d63031; color: #ffffff; border: none; }"
        "QPushButton:default:hover { background: #e04040; }"
    ).arg("#242440", "#e8e8ff", "#353558", "#3a3a60", "#404070"));

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* label = new QLabel(message);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto* buttons = new QDialogButtonBox();
    buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    auto* delBtn = buttons->addButton("Delete", QDialogButtonBox::AcceptRole);
    delBtn->setDefault(true);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    return dlg.exec() == QDialog::Accepted;
}

// ─────────────────────────────────────────────────────────────

ControllerMappingPage::ControllerMappingPage(SdlInputManager* inputManager,
                                             AppController* appController,
                                             const QString& emuId,
                                             QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
{
    setWindowTitle("Controller Settings");
    setMinimumSize(1400, 750);
    resize(1400, 750);
    setStyleSheet(QString("QDialog { background: %1; }").arg(kBg));
    buildUI();
    loadPort(1);
}

void ControllerMappingPage::buildUI() {
    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Sidebar ──
    m_portList = new QListWidget();
    m_portList->setFixedWidth(180);
    m_portList->setStyleSheet(QString(
        "QListWidget { background: #1e1e3a; border: none; border-right: 1px solid %1; }"
        "QListWidget::item { padding: 10px 14px; color: %2; font-size: 12px; }"
        "QListWidget::item:selected { background: %3; color: #ffffff; }"
    ).arg(kBoxBorder, kTextSecondary, kAccent));

    m_portList->addItem("Controller Port 1\nNot Connected");
    m_portList->addItem("Controller Port 2\nNot Connected");
    m_portList->setCurrentRow(0);

    connect(m_portList, &QListWidget::currentRowChanged, this, &ControllerMappingPage::onPortChanged);

    outerLayout->addWidget(m_portList);

    // ── Right side ──
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // ── Toolbar ──
    auto* toolbar = new QWidget();
    toolbar->setFixedHeight(52);
    toolbar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(kBoxColor, kBoxBorder));

    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 0, 12, 0);
    toolbarLayout->setSpacing(8);

    m_typeCombo = new QComboBox();
    m_typeCombo->setStyleSheet(kComboStyle);
    toolbarLayout->addWidget(m_typeCombo);

    m_bindingsBtn = new QToolButton();
    m_bindingsBtn->setText("Bindings");
    m_bindingsBtn->setCheckable(true);
    m_bindingsBtn->setChecked(true);
    m_bindingsBtn->setStyleSheet(kToolBtnStyle);
    m_bindingsBtn->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(m_bindingsBtn);

    m_settingsBtn = new QToolButton();
    m_settingsBtn->setText("Settings");
    m_settingsBtn->setCheckable(true);
    m_settingsBtn->setStyleSheet(kToolBtnStyle);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(m_settingsBtn);

    toolbarLayout->addStretch();

    m_autoMapBtn = new QToolButton();
    m_autoMapBtn->setText("Automatic Mapping");
    m_autoMapBtn->setStyleSheet(kToolBtnStyle);
    m_autoMapBtn->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(m_autoMapBtn);

    m_clearMapBtn = new QToolButton();
    m_clearMapBtn->setText("Clear Mapping");
    m_clearMapBtn->setStyleSheet(kToolBtnStyle);
    m_clearMapBtn->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(m_clearMapBtn);

    connect(m_bindingsBtn, &QToolButton::clicked, this, &ControllerMappingPage::onBindingsClicked);
    connect(m_settingsBtn, &QToolButton::clicked, this, &ControllerMappingPage::onSettingsClicked);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControllerMappingPage::onTypeChanged);
    connect(m_autoMapBtn, &QToolButton::clicked, this, &ControllerMappingPage::onAutoMap);
    connect(m_clearMapBtn, &QToolButton::clicked, this, &ControllerMappingPage::onClearMapping);

    rightLayout->addWidget(toolbar);

    // ── Content stack ──
    m_contentStack = new QStackedWidget();
    m_contentStack->setStyleSheet(QString("background: %1;").arg(kBg));
    rightLayout->addWidget(m_contentStack, 1);

    // ── Profile bar ──
    auto* profileBar = new QWidget();
    profileBar->setFixedHeight(52);
    profileBar->setStyleSheet(QString("background: %1; border-top: 1px solid %2;").arg(kBoxColor, kBoxBorder));

    auto* profileLayout = new QHBoxLayout(profileBar);
    profileLayout->setContentsMargins(12, 0, 12, 0);
    profileLayout->setSpacing(8);

    auto* profileLabel = new QLabel("Editing Profile:");
    profileLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(kTextSecondary));
    profileLayout->addWidget(profileLabel);

    m_profileCombo = new QComboBox();
    m_profileCombo->setStyleSheet(kComboStyle);
    m_profileCombo->setMinimumWidth(140);
    profileLayout->addWidget(m_profileCombo);

    auto* newProfileBtn = new QPushButton("+ New Profile");
    newProfileBtn->setStyleSheet(kProfileBtnStyle);
    newProfileBtn->setCursor(Qt::PointingHandCursor);
    connect(newProfileBtn, &QPushButton::clicked, this, &ControllerMappingPage::onNewProfile);
    profileLayout->addWidget(newProfileBtn);

    m_applyProfileBtn = new QPushButton("Apply Profile");
    m_applyProfileBtn->setStyleSheet(kProfileBtnStyle);
    m_applyProfileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_applyProfileBtn, &QPushButton::clicked, this, &ControllerMappingPage::onApplyProfile);
    profileLayout->addWidget(m_applyProfileBtn);

    m_renameProfileBtn = new QPushButton("Rename Profile");
    m_renameProfileBtn->setStyleSheet(kProfileBtnStyle);
    m_renameProfileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_renameProfileBtn, &QPushButton::clicked, this, &ControllerMappingPage::onRenameProfile);
    profileLayout->addWidget(m_renameProfileBtn);

    m_deleteProfileBtn = new QPushButton("Delete Profile");
    m_deleteProfileBtn->setStyleSheet(kProfileBtnStyle);
    m_deleteProfileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &ControllerMappingPage::onDeleteProfile);
    profileLayout->addWidget(m_deleteProfileBtn);

    profileLayout->addStretch();

    auto* restoreBtn = new QPushButton("Restore Defaults");
    restoreBtn->setStyleSheet(kProfileBtnStyle);
    restoreBtn->setCursor(Qt::PointingHandCursor);
    connect(restoreBtn, &QPushButton::clicked, this, &ControllerMappingPage::onRestoreDefaults);
    profileLayout->addWidget(restoreBtn);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(kProfileBtnStyle);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    profileLayout->addWidget(closeBtn);

    rightLayout->addWidget(profileBar);

    outerLayout->addLayout(rightLayout);

    // Enable/disable profile buttons based on selection
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        bool isShared = (index == 0);
        m_applyProfileBtn->setEnabled(!isShared);
        m_renameProfileBtn->setEnabled(!isShared);
        m_deleteProfileBtn->setEnabled(!isShared);
    });

    refreshProfiles();
}

void ControllerMappingPage::loadPort(int port) {
    m_currentPort = port;
    m_currentType = m_appController->controllerType(m_emuId, port);

    // Update type combo
    m_typeCombo->blockSignals(true);
    m_typeCombo->clear();
    const QVariantList types = m_appController->controllerTypes(m_emuId);
    int selectedIdx = 0;
    for (int i = 0; i < types.size(); ++i) {
        const auto t = types[i].toMap();
        const QString id = t["id"].toString();
        m_typeCombo->addItem(t["displayName"].toString(), id);
        if (id == m_currentType) selectedIdx = i;
    }
    m_typeCombo->setCurrentIndex(selectedIdx);
    m_typeCombo->blockSignals(false);

    // Remove old widgets from stack
    if (m_bindingsWidget) {
        m_contentStack->removeWidget(m_bindingsWidget);
        m_bindingsWidget->deleteLater();
        m_bindingsWidget = nullptr;
    }
    if (m_settingsWidget) {
        m_contentStack->removeWidget(m_settingsWidget);
        m_settingsWidget->deleteLater();
        m_settingsWidget = nullptr;
    }

    // Create new widgets for current type
    m_bindingsWidget = createBindingsWidget(m_currentType);
    m_contentStack->insertWidget(0, m_bindingsWidget);

    m_settingsWidget = new ControllerSettingsWidget(m_appController, m_emuId, m_currentPort, this);
    m_contentStack->insertWidget(1, m_settingsWidget);

    switchTab(m_currentTab);
    updateSidebar();

    // Enable/disable based on type
    bool connected = (m_currentType != "NotConnected");
    m_bindingsBtn->setEnabled(connected);
    m_settingsBtn->setEnabled(connected);
    m_autoMapBtn->setEnabled(connected);
    m_clearMapBtn->setEnabled(connected);
}

QWidget* ControllerMappingPage::createBindingsWidget(const QString& type) {
    if (type == "DualShock2") return new DS2BindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "Guitar")     return new GuitarBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "Jogcon")     return new JogconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "Negcon")     return new NegconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "Popn")       return new PopnBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, PopnBindingsWidget::Variant::Pcsx2, this);

    // DuckStation controller types
    if (type == "DigitalController") return new DigitalBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "AnalogController")  return new AnalogBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "AnalogJoystick")    return new AnalogJoystickBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "NeGcon")            return new DSNegconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "NeGconRumble")      return new DSNegconRumbleBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "JogCon")            return new DSJogconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "PopnController")    return new PopnBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, PopnBindingsWidget::Variant::DuckStation, this);

    // PPSSPP
    if (type == "Standard")          return new PSPBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);

    // NotConnected — empty widget with centered label
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("background: %1;").arg(kBg));
    auto* l = new QLabel("No controller connected.", w);
    l->setStyleSheet(QString("color: %1; font-size: 14px;").arg(kTextSecondary));
    l->setAlignment(Qt::AlignCenter);
    auto* lay = new QVBoxLayout(w);
    lay->addWidget(l);
    return w;
}

void ControllerMappingPage::switchTab(int tab) {
    m_currentTab = tab;
    m_contentStack->setCurrentIndex(tab);
    m_bindingsBtn->setChecked(tab == 0);
    m_settingsBtn->setChecked(tab == 1);
    m_autoMapBtn->setVisible(tab == 0);
    m_clearMapBtn->setVisible(tab == 0);
}

void ControllerMappingPage::onPortChanged(int row) {
    loadPort(row + 1);
}

void ControllerMappingPage::onTypeChanged(int index) {
    if (index < 0) return;
    QString newType = m_typeCombo->currentData().toString();
    if (newType == m_currentType) return;

    // Clear old bindings for this port before switching type
    m_appController->clearAllBindingsForPort(m_emuId, m_currentPort);
    m_appController->setControllerType(m_emuId, m_currentPort, newType);
    loadPort(m_currentPort);
}

void ControllerMappingPage::onBindingsClicked() {
    switchTab(0);
}

void ControllerMappingPage::onSettingsClicked() {
    switchTab(1);
}

void ControllerMappingPage::onAutoMap() {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #1e1e3a; color: #e0e0ff; border: 1px solid #2a2a50; }"
        "QMenu::item { padding: 8px 24px; }"
        "QMenu::item:selected { background: #6c5ce7; }");

    menu.addAction("Keyboard", [this]() {
        m_appController->clearAllBindingsForPort(m_emuId, m_currentPort);
        loadPort(m_currentPort);
    });

    QVariantList controllers = m_inputManager->connectedControllers();
    for (const auto& c : controllers) {
        auto map = c.toMap();
        int devIdx = map["deviceIndex"].toInt();
        QString name = map["name"].toString();
        menu.addAction(QString("SDL-%1: %2").arg(devIdx).arg(name), [this, devIdx]() {
            m_appController->autoMapControllerForPort(m_emuId, m_currentPort, devIdx);
            loadPort(m_currentPort);
        });
    }
    menu.exec(QCursor::pos());
}

void ControllerMappingPage::onClearMapping() {
    m_appController->clearAllBindingsForPort(m_emuId, m_currentPort);
    loadPort(m_currentPort);
}

void ControllerMappingPage::onRestoreDefaults() {
    m_appController->restoreDefaultsForPort(m_emuId, m_currentPort);
    loadPort(m_currentPort);
}

void ControllerMappingPage::updateSidebar() {
    const QVariantList types = m_appController->controllerTypes(m_emuId);
    for (int port = 1; port <= 2; ++port) {
        const QString type = m_appController->controllerType(m_emuId, port);
        QString displayName = "Not Connected";
        for (const auto& t : types) {
            const auto map = t.toMap();
            if (map["id"].toString() == type) {
                displayName = map["displayName"].toString();
                break;
            }
        }
        m_portList->item(port - 1)->setText(QString("Controller Port %1\n%2").arg(port).arg(displayName));
    }
}

void ControllerMappingPage::refreshProfiles() {
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem("Shared");
    m_profileCombo->addItems(m_appController->controllerProfiles(m_emuId));
    m_profileCombo->setCurrentIndex(0);
    m_profileCombo->blockSignals(false);

    // Shared is selected — disable profile action buttons
    m_applyProfileBtn->setEnabled(false);
    m_renameProfileBtn->setEnabled(false);
    m_deleteProfileBtn->setEnabled(false);
}

void ControllerMappingPage::onNewProfile() {
    QString name = darkInputDialog(this, "New Profile", "Profile name:");
    if (!name.isEmpty()) {
        m_appController->createControllerProfile(m_emuId, name);
        refreshProfiles();
        int idx = m_profileCombo->findText(name);
        if (idx >= 0) m_profileCombo->setCurrentIndex(idx);
    }
}

void ControllerMappingPage::onApplyProfile() {
    QString name = m_profileCombo->currentText();
    if (name == "Shared" || name.isEmpty()) return;
    m_appController->applyControllerProfile(m_emuId, name);
    loadPort(m_currentPort);
}

void ControllerMappingPage::onRenameProfile() {
    QString oldName = m_profileCombo->currentText();
    if (oldName == "Shared" || oldName.isEmpty()) return;

    QString newName = darkInputDialog(this, "Rename Profile", "New name:", oldName);
    if (!newName.isEmpty() && newName != oldName) {
        m_appController->renameControllerProfile(m_emuId, oldName, newName);
        refreshProfiles();
        int idx = m_profileCombo->findText(newName);
        if (idx >= 0) m_profileCombo->setCurrentIndex(idx);
    }
}

void ControllerMappingPage::onDeleteProfile() {
    QString name = m_profileCombo->currentText();
    if (name == "Shared" || name.isEmpty()) return;

    if (darkConfirmDialog(this, "Delete Profile",
                          QString("Delete profile \"%1\"?").arg(name))) {
        m_appController->deleteControllerProfile(m_emuId, name);
        refreshProfiles();
    }
}
