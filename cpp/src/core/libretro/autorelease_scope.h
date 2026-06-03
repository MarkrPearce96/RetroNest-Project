// SPDX-License-Identifier: GPL-3.0+
#pragma once

// RAII autorelease-pool scope, usable from plain C++ (.cpp) translation units.
//
// Why this exists: the libretro run loop (core_runtime.cpp) calls the core's
// retro_run()/retro_deinit() on a worker QThread, then CoreLoader::close()
// dlclose()s the core dylib. With a Metal-rendering core (e.g. DuckStation's
// hardware Metal renderer), those calls can leave Objective-C/Metal objects on
// the *thread's* lifetime autorelease pool. That pool only drains when the
// QThread exits — which is AFTER dlclose() has unmapped the core dylib — so
// releasing an object whose class/dealloc lived in the unloaded dylib jumps to
// unmapped memory and crashes (SIGILL). Diagnosed from a resume crash:
// duckstation_libretro.dylib absent from the crashed image list, faulting PC in
// unmapped memory, inside objc_autoreleasePoolPop during _pthread_exit.
//
// Wrapping the per-frame work and the teardown sequence in this scope drains
// those objects *before* dlclose(), while the dylib (and the Metal device) are
// still alive. RAII means break/return/continue all drain correctly.
//
// core_runtime.cpp is plain C++ (and uses `id` as an identifier), so we can't
// use the ObjC `@autoreleasepool` keyword here without compiling the whole file
// as ObjC++. Instead we call the Objective-C runtime's pool push/pop directly —
// exactly what `@autoreleasepool` lowers to. libobjc is always loaded on macOS
// and linked transitively via Qt.

// Declared in <objc/objc-internal.h> (runtime SPI); forward-declared here so
// this header stays a plain-C++ include with no Objective-C dependency.
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void* pool);

namespace mac {

class AutoreleaseScope {
public:
    AutoreleaseScope() : m_pool(objc_autoreleasePoolPush()) {}
    ~AutoreleaseScope() { objc_autoreleasePoolPop(m_pool); }

    AutoreleaseScope(const AutoreleaseScope&) = delete;
    AutoreleaseScope& operator=(const AutoreleaseScope&) = delete;

private:
    void* m_pool;
};

} // namespace mac
