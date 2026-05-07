// Minimal stub for test_controller_bindings_view.
// Provides only the symbols needed by ControllerBindingsView that aren't
// exercised in the test (AppController is passed as nullptr; this stub
// satisfies the linker without pulling in the full AppController + its
// wide dependency graph).
//
// NOTE: app_controller.h includes many headers but none of them require
// symbols from their corresponding .cpp files for a simple function stub.
#include "ui/app_controller.h"
#include <QVariantList>

QVariantList AppController::controllerBindingsForPort(const QString& /*emuId*/, int /*port*/,
                                                       const QString& /*controllerTypeId*/) const {
    // Never called in tests (AppController* is always nullptr in test context).
    return {};
}
