// Linker stubs for test_generic_hotkey_page.
//
// GenericHotkeyPage references AppController::{hotkeyBindings,saveHotkey,
// clearHotkey,resetHotkeys}, and pulling in src/core/sdl_input_manager.cpp
// drags InputRouter::setButtonPressed into the link. The page guards every
// AppController call behind a nullptr check, and the test passes nullptr
// for both AppController and SdlInputManager — none of these stubs are
// reached at runtime; they exist only to satisfy ld.
//
// Mirrors the pattern in tests/stubs/app_controller_stub.cpp.

#include "ui/app_controller.h"
#include "core/libretro/input_router.h"
#include <QVariantList>

QVariantList AppController::hotkeyBindings(const QString& /*emuId*/) const {
    return {};
}

void AppController::saveHotkey(const QString& /*emuId*/, const QString& /*section*/,
                                const QString& /*key*/, const QString& /*value*/) {}

void AppController::clearHotkey(const QString& /*emuId*/, const QString& /*section*/,
                                 const QString& /*key*/) {}

void AppController::resetHotkeys(const QString& /*emuId*/) {}

QString AppController::formatCapturedBinding(const QString& /*emuId*/, int /*deviceIndex*/,
                                              const QString& /*element*/, bool /*isAxis*/,
                                              bool /*positive*/) const {
    return {};
}

QString AppController::formatCapturedKeyboard(const QString& /*emuId*/, int /*qtKey*/,
                                               int /*modifiers*/) const {
    return {};
}

QString AppController::formatCapturedMouse(const QString& /*emuId*/, int /*qtButton*/) const {
    return {};
}

QString AppController::formatCapturedWheel(const QString& /*emuId*/, int /*direction*/) const {
    return {};
}

void InputRouter::setButtonPressed(int /*port*/, RetroPadSlot /*slot*/, bool /*down*/) {}
