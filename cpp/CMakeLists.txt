cmake_minimum_required(VERSION 3.16)
project(RetroNest LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 Concurrent Multimedia Svg)
find_package(SDL2 REQUIRED)

include(FetchContent)
# Pinned to the exact commit the build trees had checked out while GIT_TAG
# was still "master" (verified identical on 2026-07-02), so pinning changes
# nothing behaviorally — it only stops upstream pushes from silently
# changing our dependency. No GIT_SHALLOW: shallow clones can't check out
# an arbitrary commit SHA.
FetchContent_Declare(
    libchdr
    GIT_REPOSITORY https://github.com/rtissera/libchdr.git
    GIT_TAG        04a177ee3cea055d93da2d5839d3413168837c6f
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libchdr)

FetchContent_Declare(
    rcheevos
    GIT_REPOSITORY https://github.com/RetroAchievements/rcheevos.git
    GIT_TAG        v12.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(rcheevos)
# rcheevos has no CMakeLists.txt — build a static lib manually from its sources.
file(GLOB RCHEEVOS_SOURCES
    ${rcheevos_SOURCE_DIR}/src/rcheevos/*.c
    ${rcheevos_SOURCE_DIR}/src/rapi/*.c
    ${rcheevos_SOURCE_DIR}/src/rhash/*.c
    ${rcheevos_SOURCE_DIR}/src/*.c)
add_library(rcheevos_static STATIC ${RCHEEVOS_SOURCES})
# PUBLIC so consumers (retronest_core and its dependents) get rcheevos
# headers automatically (e.g. rcheevos_runtime.cpp).
target_include_directories(rcheevos_static PUBLIC ${rcheevos_SOURCE_DIR}/include)
# rc_libretro.h lives in src/ (not include/) but consumers need it for the
# memory-map helpers (rc_libretro_memory_init / rc_libretro_memory_read) used
# by RetroNest's RcheevosRuntime to translate RA memory addresses through
# libretro's memory descriptors.
target_include_directories(rcheevos_static PUBLIC ${rcheevos_SOURCE_DIR}/src)
target_include_directories(rcheevos_static PRIVATE ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro)
# Enable hash-based game identification (rc_client_begin_identify_and_load_game).
target_compile_definitions(rcheevos_static PUBLIC RC_CLIENT_SUPPORTS_HASH)

# ScreenScraper developer credentials (build-time, not in repo)
set(SCREENSCRAPER_DEV_ID "" CACHE STRING "ScreenScraper dev ID")
set(SCREENSCRAPER_DEV_PASSWORD "" CACHE STRING "ScreenScraper dev password")
set(SCREENSCRAPER_SOFTNAME "RetroNest" CACHE STRING "ScreenScraper software name")
set(RETRONEST_GITHUB_TOKEN "" CACHE STRING "GitHub PAT for private core downloads")
include(dev_credentials.cmake OPTIONAL)

# ── retronest_core: shared non-UI closure (core/, adapters/, services/) ──
# Every translation unit that both the app and the QtTest targets compile.
# Previously each of ~10 test targets hand-copied the ~30-file libretro
# dependency closure, causing ~10x redundant compilation and historical
# vtable link failures whenever a newly-added file was forgotten in one of
# the lists. Tests now link this archive and the linker pulls exactly the
# objects each test actually references.
add_library(retronest_core STATIC
    # core/
    # game_session.cpp lives here as of the P10 render-seam refactor: it
    # no longer #includes ui/libretro/libretro_gl_item.h — the GL
    # shutdown fence is driven through the abstract LibretroRenderSurface
    # interface (core/), so GameSession needs no Qt Quick and moc-ing it
    # into this archive is safe. VideoHardwareGL (its only other HW dep)
    # already lives in this lib.
    src/core/game_session.cpp
    src/core/manifest_loader.cpp
    src/core/paths.cpp
    src/core/system_registry.cpp
    src/core/detail_actions.cpp
    src/core/sfo_parser.cpp
    src/core/ini_file.cpp
    src/core/path_overrides_store.cpp
    src/core/database.cpp
    src/core/iso9660_reader.cpp
    src/core/sdl_input_manager.cpp
    # adapters/
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/duckstation_libretro_adapter.cpp
    src/adapters/libretro/dolphin_libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/adapters/libretro/ppsspp_libretro_adapter.cpp
    src/adapters/libretro/snes9x_libretro_adapter.cpp
    # core/libretro/
    src/core/libretro/options_store.cpp
    src/core/libretro/declared_options.cpp
    src/core/libretro/core_prober.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/retro_log.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/video_hardware_gl.mm
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/libretro_hotkey_defs.cpp
    src/core/libretro/hotkey_matcher.cpp
    src/core/libretro/hotkey_dispatcher.cpp
    # NOTE: libretro_hotkey_controller.cpp is deliberately NOT here even
    # though it lives under core/ — its action callbacks call GameSession
    # methods, and game_session.cpp stays out of this lib (see the note
    # above). If the controller's moc entered this lib's monolithic
    # mocs_compilation.o, every test linking the archive would need
    # GameSession (and thus LibretroGLItem/Qt Quick) to resolve. The app
    # and test_libretro_hotkey_controller compile it directly.
    # services/
    src/services/config_service.cpp
    src/services/hotkey_service.cpp
    src/services/rom_scanner.cpp
    src/services/patches_sidecar.cpp
    src/services/patches_installer.cpp
    src/services/installer_utils.cpp
    # headers (each header lives in exactly ONE target's source list so
    # AUTOMOC generates its moc output exactly once — listing a moved
    # header in the app too would duplicate staticMetaObject symbols)
    src/core/game_session.h
    src/core/libretro/libretro_render_surface.h
    src/core/manifest.h
    src/core/manifest_loader.h
    src/core/paths.h
    src/core/sfo_parser.h
    src/core/ini_file.h
    src/core/database.h
    src/core/iso9660_reader.h
    src/core/sdl_input_manager.h
    src/adapters/emulator_adapter.h
    src/adapters/adapter_registry.h
    src/adapters/libretro/libretro_adapter.h
    src/adapters/libretro/duckstation_libretro_adapter.h
    src/adapters/libretro/dolphin_libretro_adapter.h
    src/adapters/libretro/mgba_libretro_adapter.h
    src/adapters/libretro/pcsx2_libretro_adapter.h
    src/adapters/libretro/ppsspp_libretro_adapter.h
    src/adapters/libretro/snes9x_libretro_adapter.h
    src/core/libretro/core_loader.h
    src/core/libretro/core_runtime.h
    src/core/libretro/rcheevos_runtime.h
    src/core/libretro/environment_callbacks.h
    src/core/libretro/video_software.h
    src/core/libretro/video_hardware_gl.h
    src/core/libretro/audio_sink.h
    src/core/libretro/input_router.h
    src/services/hotkey_service.h
    src/services/rom_scanner.h
    src/services/patches_sidecar.h
    src/services/patches_installer.h
    src/services/installer_utils.h
)
# Q_OBJECT classes inside the static lib must moc here (their .cpp lives here).
set_target_properties(retronest_core PROPERTIES AUTOMOC ON)
target_include_directories(retronest_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro
    ${SDL2_INCLUDE_DIRS}
)
# PUBLIC: consumers (app + every test) inherit Qt components, third-party
# libs and the OpenGL/IOSurface frameworks video_hardware_gl.mm needs.
target_link_libraries(retronest_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    chdr-static
    rcheevos_static
    ${SDL2_LIBRARIES}
    ${CMAKE_DL_LIBS}
    "-framework OpenGL"
    "-framework IOSurface"
)
# ── retronest_ui: shared Qt Widgets settings building blocks ──
# The widget files under src/ui/settings/widgets/ are compiled by the app
# and by the widget-level QtTest targets. Page-level UI sources shared with
# only a single test (controller_bindings_view.cpp, generic_hotkey_page.cpp)
# stay per-target because those tests pair them with linker stubs.
add_library(retronest_ui STATIC
    src/ui/settings/widgets/settings_card.cpp
    src/ui/settings/widgets/settings_section_header.cpp
    src/ui/settings/widgets/settings_toggle.cpp
    src/ui/settings/widgets/settings_toggle_row.cpp
    src/ui/settings/widgets/settings_combo_row.cpp
    src/ui/settings/widgets/settings_slider_row.cpp
    src/ui/settings/widgets/settings_description_bar.cpp
    src/ui/settings/widgets/settings_graphics_sub_tab_bar.cpp
    src/ui/settings/widgets/hotkey_binding_row.cpp
    src/ui/settings/widgets/preview/aspect_ratio_preview.cpp
    src/ui/settings/widgets/preview/osd_preview.cpp
    src/ui/settings/widgets/settings_card.h
    src/ui/settings/widgets/settings_section_header.h
    src/ui/settings/widgets/settings_toggle.h
    src/ui/settings/widgets/settings_toggle_row.h
    src/ui/settings/widgets/settings_combo_row.h
    src/ui/settings/widgets/settings_slider_row.h
    src/ui/settings/widgets/settings_description_bar.h
    src/ui/settings/widgets/settings_graphics_sub_tab_bar.h
    src/ui/settings/widgets/hotkey_binding_row.h
    src/ui/settings/widgets/preview/aspect_ratio_preview.h
    src/ui/settings/widgets/preview/osd_preview.h
)
set_target_properties(retronest_ui PROPERTIES AUTOMOC ON)
target_include_directories(retronest_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(retronest_ui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ── RetroNest app: app-only sources; the shared closure comes from the libs ──
set(SOURCES
    src/main.cpp
    src/core/libretro/libretro_hotkey_controller.cpp
    src/core/scraper.cpp
    src/core/ra_client.cpp
    src/core/macos_fullscreen.mm
    src/services/emulator_installer.cpp
    src/services/ra_credentials.cpp
    src/services/game_service.cpp
    src/services/scraper_service.cpp
    src/services/emulator_service.cpp
    src/services/scraper_credentials.cpp
    src/services/github_credentials.cpp
    src/services/ra_service.cpp
    src/ui/wizard_state.cpp
    src/ui/emulator_list_model.cpp
    src/ui/install_controller.cpp
    src/ui/game_list_model.cpp
    src/ui/app_controller.cpp
    src/ui/libretro_overlay_panel.cpp
    src/ui/in_game_menu_controller.cpp
    src/ui/theme_manager.cpp
    src/ui/theme_context.cpp
    src/ui/settings/controller_mapping_page.cpp
    src/ui/settings/controller_bindings_view.cpp
    src/ui/settings/emulator_settings_dialog_base.cpp
    src/ui/settings/emulator_category_hub_base.cpp
    src/ui/settings/generic_hotkey_page.cpp
    src/ui/settings/hotkey_settings_dialog.cpp
    src/ui/settings/generic_settings_page.cpp
    src/ui/settings/settings_page_builder.cpp
    # Per-emulator settings dialogs collapsed into one schema-driven dialog
    # whose hub layout lives as data on each adapter (settingsHubCards()).
    src/ui/settings/generic_emulator_settings_dialog.cpp
)

set(HEADERS
    src/services/emulator_installer.h
    # game_session.h moved to retronest_core's header list (P10) — it must
    # be moc'd in exactly one target, and game_session.cpp now lives in
    # that lib.
    src/core/scraper.h
    src/core/setting_def.h
    src/core/cover_resolver.h
    src/core/github_client.h
    src/services/ra_credentials.h
    src/core/ra_client.h
    src/services/game_service.h
    src/services/scraper_service.h
    src/services/emulator_service.h
    src/services/scraper_credentials.h
    src/services/ra_service.h
    src/ui/wizard_state.h
    src/ui/wizard_theme.h
    src/ui/settings_theme.h
    src/ui/emulator_list_model.h
    src/ui/install_controller.h
    src/ui/game_list_model.h
    src/ui/app_controller.h
    src/ui/libretro_overlay_panel.h
    src/ui/in_game_menu_controller.h
    src/ui/theme_manager.h
    src/ui/theme_context.h
    src/ui/settings/controller_mapping_page.h
    src/ui/settings/emulator_settings_dialog_base.h
    src/ui/settings/emulator_category_hub_base.h
    src/ui/settings/generic_hotkey_page.h
    src/ui/settings/hotkey_settings_dialog.h
    src/ui/settings/generic_settings_page.h
    src/ui/settings/settings_page_builder.h
    src/core/binding_def.h
    src/ui/settings/binding_display.h
    src/ui/settings/binding_widget_common.h
    src/core/controller_type_def.h
    src/core/macos_fullscreen.h
    src/ui/settings/settings_dialog_theme.h
    src/ui/settings/generic_emulator_settings_dialog.h
)

# App icon — bundled into RetroNest.app/Contents/Resources/AppIcon.icns.
# Listing the .icns as a source and setting MACOSX_PACKAGE_LOCATION is the
# standard CMake pattern for macOS bundle resources.
set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${APP_ICON_MACOS})

# macOS bundle metadata — gives the app a stable CFBundleIdentifier and
# exposes bundle-only behaviors (NSApplicationActivationPolicyRegular,
# Launch Services, Gatekeeper, Carbon global hotkey reliability) during
# development so we catch issues as they appear rather than at release.
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "RetroNest"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.markpearce.retronest"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_ICON_FILE "AppIcon"
)

# Executable needs a minimal QML module (required by Qt6)
qt_add_qml_module(${PROJECT_NAME}
    URI EmulatorApp
    VERSION 1.0
    RESOURCE_PREFIX /
    NO_RESOURCE_TARGET_PATH
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro
)

# Only src/services/scraper_credentials.cpp consumes these, and that TU is
# app-only, so the defines stay scoped to the app target.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SCREENSCRAPER_DEV_ID="${SCREENSCRAPER_DEV_ID}"
    SCREENSCRAPER_DEV_PASSWORD="${SCREENSCRAPER_DEV_PASSWORD}"
    SCREENSCRAPER_SOFTNAME="${SCREENSCRAPER_SOFTNAME}"
    RETRONEST_GITHUB_TOKEN="${RETRONEST_GITHUB_TOKEN}"
)

# Qt6 Core/Gui/Network/Sql/Concurrent, SDL2, chdr-static, rcheevos_static
# and dl are inherited PUBLIC through retronest_core.
target_link_libraries(${PROJECT_NAME} PRIVATE
    retronest_core
    retronest_ui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Svg
    "-framework AppKit"
    "-framework Carbon"
)

# Re-register the bundle with Launch Services after every rebuild so
# `open build/RetroNest.app` always launches the freshly-built binary.
# Without this, LS can cache a stale path for com.markpearce.retronest and
# launch an older copy (or fail silently) after moves/renames.
if(APPLE)
    set_source_files_properties(
        src/ui/libretro/libretro_metal_item.mm
        src/ui/libretro/libretro_gl_item.mm
        src/core/libretro/video_hardware_gl.mm
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework QuartzCore"
        "-framework Metal"
        "-framework OpenGL"
        "-framework IOSurface"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
                -f "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
        COMMENT "Refreshing Launch Services registration for RetroNest.app"
        VERBATIM
    )
    # Self-maintaining Qt deployment: after the bundle has been macdeployqt'ed
    # once by hand (Contents/Frameworks/QtCore.framework exists), every app
    # relink re-runs the matching-arch macdeployqt automatically, so an
    # incremental rebuild never leaves a half-deployed bundle that aborts at
    # launch with a Qt double-load. First-ever builds (no Frameworks/ yet)
    # skip silently. POST_BUILD only fires on an actual relink, so the
    # macdeployqt cost (~1-5 min under Rosetta) is only paid when needed.
    if(TARGET Qt6::macdeployqt)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND /bin/sh -c "if [ -d \"$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/QtCore.framework\" ]; then echo 'RetroNest: previously deployed bundle detected - re-running macdeployqt (this can take a few minutes)...'; \"$<TARGET_FILE:Qt6::macdeployqt>\" \"$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>\" -qmldir=${CMAKE_CURRENT_SOURCE_DIR}/qml -no-codesign -always-overwrite; fi"
            COMMENT "Checking whether RetroNest.app needs a macdeployqt refresh"
            VERBATIM
        )
    else()
        message(WARNING "Qt6::macdeployqt tool target not found - automatic "
            "bundle redeployment after relink is disabled; run macdeployqt "
            "manually after app rebuilds.")
    endif()

    # DuckStation dlopens libspirv-cross-c-shared.0.dylib at RUNTIME (from the
    # app bundle's Frameworks) to cross-compile its shaders (SPIR-V -> Metal).
    # Nothing LINKS it, so macdeployqt never bundles it and otool-based
    # dependency checks miss it entirely — it must be copied in explicitly or
    # DuckStation's renderer fails to initialize ("Failed to load spirv-cross",
    # both HW and SW fallback). The vendored dylib is universal (x86_64+arm64).
    # Runs after the macdeployqt step above so it is never clobbered.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-cross/libspirv-cross-c-shared.0.68.0.dylib"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libspirv-cross-c-shared.0.68.0.dylib"
        COMMAND ${CMAKE_COMMAND} -E rm -f
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libspirv-cross-c-shared.0.dylib"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libspirv-cross-c-shared.dylib"
        COMMAND ${CMAKE_COMMAND} -E create_symlink libspirv-cross-c-shared.0.68.0.dylib
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libspirv-cross-c-shared.0.dylib"
        COMMAND ${CMAKE_COMMAND} -E create_symlink libspirv-cross-c-shared.0.dylib
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libspirv-cross-c-shared.dylib"
        COMMENT "Bundling libspirv-cross-c-shared (DuckStation runtime shader dependency)"
        VERBATIM
    )
endif()

# ── SetupWizard QML module (separate library so types aren't evaluated by app engine) ──
qt_add_library(setupwizard_backing STATIC)
target_link_libraries(setupwizard_backing PRIVATE Qt6::Qml Qt6::Quick)
qt_add_qml_module(setupwizard_backing
    URI SetupWizard
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SetupWizard"
    QML_FILES
        qml/SetupWizard/Main.qml
        qml/SetupWizard/WelcomePage.qml
        qml/SetupWizard/StorageLocationsPage.qml
        qml/SetupWizard/EmulatorsPage.qml
        qml/SetupWizard/RetroAchievementsPage.qml
        qml/SetupWizard/ScreenScraperPage.qml
        qml/SetupWizard/InstallPage.qml
        qml/SetupWizard/NavBar.qml
        qml/SetupWizard/PillButton.qml
        qml/SetupWizard/EmulatorCard.qml
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/dolphin_logo.png
        qml/AppUI/images/mgba_logo.png
    RESOURCE_PREFIX /
)

# ── AppUI QML module ──
qt_add_library(appui_backing STATIC)
target_include_directories(appui_backing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/libretro
    ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro
)
# libretro_gl_item.mm calls into VideoHardwareGL (which uses Metal /
# IOSurface APIs) — the QML module needs those frameworks too.
target_link_libraries(appui_backing PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickControls2
    "-framework Metal" "-framework IOSurface")

qt_add_qml_module(appui_backing
    URI AppUI
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/AppUI"
    QML_FILES
        qml/AppUI/BaseModalCard.qml
        qml/AppUI/BaseToast.qml
        qml/AppUI/ButtonHints.qml
        qml/AppUI/GenericListPage.qml
        qml/AppUI/GenericMultiCardPicker.qml
        qml/AppUI/GenericRowPicker.qml
        qml/AppUI/PillRow.qml
        qml/AppUI/FocusableItem.qml
        qml/AppUI/ConfirmDialog.qml
        qml/AppUI/FocusRing.qml
        qml/AppUI/AppWindow.qml
        qml/AppUI/EmptyStatePage.qml
        qml/AppUI/TopBar.qml
        qml/AppUI/StatusBar.qml
        qml/AppUI/SystemSidebar.qml
        qml/AppUI/GameCard.qml
        qml/AppUI/ToolBtn.qml

        qml/AppUI/EmulatorManagePage.qml
        qml/AppUI/EmulatorManageGrid.qml
        qml/AppUI/EmulatorDetailPage.qml
        qml/AppUI/BiosSection.qml
        qml/AppUI/PathsSettings.qml
        qml/AppUI/ScraperSettings.qml
        qml/AppUI/VirtualKeyboard.qml
        qml/AppUI/ThemesPage.qml
        qml/AppUI/SettingsOverlay.qml
        qml/AppUI/ResolutionSettings.qml
        qml/AppUI/AspectRatioSettings.qml
        qml/AppUI/UpdateNotification.qml
        qml/AppUI/ProgressPopup.qml
        qml/AppUI/GameActionPopup.qml
        qml/AppUI/InGameMenu.qml
        qml/AppUI/LibretroOverlayPanel.qml
        qml/AppUI/InGameAchievementsPopup.qml
        qml/AppUI/ActionToast.qml
        qml/AppUI/AchievementToast.qml
        qml/AppUI/RAIndicatorBar.qml
        qml/AppUI/ResumeStateDialog.qml
        qml/AppUI/RetroAchievementsSettings.qml
        qml/AppUI/AchievementsPage.qml
        qml/AppUI/AllGamesPage.qml
        qml/AppUI/RecentlyPlayedPage.qml
        qml/AppUI/EmulationView.qml
    SOURCES
        src/ui/libretro/libretro_video_item.h
        src/ui/libretro/libretro_metal_item.h
        src/ui/libretro/libretro_gl_item.h
        src/ui/libretro/libretro_video_item.cpp
        src/ui/libretro/libretro_metal_item.mm
        src/ui/libretro/libretro_gl_item.mm
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/dolphin_logo.png
        qml/AppUI/images/mgba_logo.png
        qml/AppUI/images/retroachievements_logo.png
        qml/AppUI/images/screenscraper_logo.png
        qml/AppUI/images/empty-state-bg.webp
        qml/AppUI/images/ar/pcsx2-4x3.webp
        qml/AppUI/images/ar/pcsx2-16x9.webp
        qml/AppUI/images/ar/duckstation-4x3.webp
        qml/AppUI/images/ar/duckstation-16x9.webp
        qml/AppUI/images/controllers/DualShock_2.svg
        qml/AppUI/images/controllers/analog_controller.svg
        qml/AppUI/images/controllers/PSP.svg
        qml/AppUI/images/controllers/GameCube.svg
        qml/AppUI/images/controllers/Wii_classiccontroller.svg
        qml/AppUI/images/controllers/Gameboy.svg
        qml/AppUI/images/hud/resume.svg
        qml/AppUI/images/hud/achievements.svg
        qml/AppUI/images/hud/save_state.svg
        qml/AppUI/sounds/Libretro_Achievement_Unlock.wav
        qml/AppUI/images/hud/load_state.svg
        qml/AppUI/images/hud/fast_forward.svg
        qml/AppUI/images/hud/save_quit.svg
        qml/AppUI/images/hud/quit.svg
    RESOURCE_PREFIX /
)

target_link_libraries(${PROJECT_NAME} PRIVATE setupwizard_backingplugin appui_backingplugin)

# ── Tests ──────────────────────────────────────────────────────────────
# Each test target compiles only its own test .cpp (plus stubs/fixtures and
# any page-level UI source it uniquely shares with the app) and links
# retronest_core / retronest_ui for the shared closure. Include dirs, Qt
# components, SDL2, chdr, rcheevos and frameworks are inherited PUBLIC.
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_ini_file tests/test_ini_file.cpp)
target_link_libraries(test_ini_file PRIVATE retronest_core Qt6::Test)
add_test(NAME IniFile COMMAND test_ini_file)

add_executable(test_path_overrides_store tests/test_path_overrides_store.cpp)
target_link_libraries(test_path_overrides_store PRIVATE retronest_core Qt6::Test)
add_test(NAME PathOverridesStore COMMAND test_path_overrides_store)

add_executable(test_paths_roots tests/test_paths_roots.cpp)
target_link_libraries(test_paths_roots PRIVATE retronest_core Qt6::Test)
add_test(NAME PathsRoots COMMAND test_paths_roots)

add_executable(test_wizard_discard
    tests/test_wizard_discard.cpp
    # wizard_state.cpp is app-side (not in retronest_core/retronest_ui), so
    # it's compiled directly into the test binary, same pattern as
    # test_libretro_hotkey_controller above.
    src/ui/wizard_state.cpp
)
target_link_libraries(test_wizard_discard PRIVATE retronest_core Qt6::Test Qt6::Widgets)
add_test(NAME WizardDiscard COMMAND test_wizard_discard)

add_executable(test_database_migrations tests/test_database_migrations.cpp)
target_link_libraries(test_database_migrations PRIVATE retronest_core Qt6::Test)
add_test(NAME DatabaseMigrations COMMAND test_database_migrations)

add_executable(test_libretro_hotkey_defs tests/test_libretro_hotkey_defs.cpp)
target_link_libraries(test_libretro_hotkey_defs PRIVATE retronest_core Qt6::Test)
add_test(NAME LibretroHotkeyDefs COMMAND test_libretro_hotkey_defs)

add_executable(test_hotkey_service_libretro tests/test_hotkey_service_libretro.cpp)
target_link_libraries(test_hotkey_service_libretro PRIVATE retronest_core Qt6::Test)
add_test(NAME HotkeyServiceLibretro COMMAND test_hotkey_service_libretro)

add_executable(test_hotkey_matcher tests/test_hotkey_matcher.cpp)
target_link_libraries(test_hotkey_matcher PRIVATE retronest_core Qt6::Test)
add_test(NAME HotkeyMatcher COMMAND test_hotkey_matcher)

add_executable(test_libretro_hotkey_controller
    tests/test_libretro_hotkey_controller.cpp
    # The controller stays out of retronest_core (it forwards to
    # GameSession, but keeping it app-side avoids re-mocing here);
    # GameSession itself now comes from retronest_core (P10).
    src/core/libretro/libretro_hotkey_controller.cpp
)
target_link_libraries(test_libretro_hotkey_controller PRIVATE
    retronest_core Qt6::Test
)
add_test(NAME LibretroHotkeyController COMMAND test_libretro_hotkey_controller)

add_executable(test_hotkey_dispatcher tests/test_hotkey_dispatcher.cpp)
target_link_libraries(test_hotkey_dispatcher PRIVATE retronest_core Qt6::Test)
add_test(NAME HotkeyDispatcher COMMAND test_hotkey_dispatcher)

add_executable(test_rom_scanner tests/test_rom_scanner.cpp)
target_link_libraries(test_rom_scanner PRIVATE retronest_core Qt6::Test)
add_test(NAME RomScanner COMMAND test_rom_scanner)

add_executable(test_iso9660_reader tests/test_iso9660_reader.cpp)
target_link_libraries(test_iso9660_reader PRIVATE retronest_core Qt6::Test)
add_test(NAME Iso9660Reader COMMAND test_iso9660_reader)

add_executable(test_sfo_parser tests/test_sfo_parser.cpp)
target_link_libraries(test_sfo_parser PRIVATE retronest_core Qt6::Test)
add_test(NAME SfoParser COMMAND test_sfo_parser)

add_executable(test_bitmask_helpers tests/test_bitmask_helpers.cpp)
target_link_libraries(test_bitmask_helpers PRIVATE retronest_core Qt6::Test)
add_test(NAME BitmaskHelpers COMMAND test_bitmask_helpers)

add_executable(test_asset_match tests/test_asset_match.cpp)
target_link_libraries(test_asset_match PRIVATE retronest_core Qt6::Test)
add_test(NAME AssetMatch COMMAND test_asset_match)

add_executable(test_setting_dependency tests/test_setting_dependency.cpp)
target_link_libraries(test_setting_dependency PRIVATE retronest_core Qt6::Test)
add_test(NAME SettingDependency COMMAND test_setting_dependency)

add_executable(test_setting_def_storage tests/test_setting_def_storage.cpp)
target_link_libraries(test_setting_def_storage PRIVATE retronest_core Qt6::Test)
add_test(NAME SettingDefStorage COMMAND test_setting_def_storage)

add_executable(test_manifest_libretro_fields tests/test_manifest_libretro_fields.cpp)
target_link_libraries(test_manifest_libretro_fields PRIVATE retronest_core Qt6::Test)
add_test(NAME ManifestLibretroFields COMMAND test_manifest_libretro_fields)

# EmulatorService/-Installer are app-only sources (not in retronest_core),
# so this test compiles them directly, like the page-level UI tests do.
add_executable(test_emulator_version_records
    tests/test_emulator_version_records.cpp
    src/services/emulator_service.cpp
    src/services/emulator_installer.cpp
    src/services/github_credentials.cpp
)
target_link_libraries(test_emulator_version_records PRIVATE retronest_core Qt6::Test)
add_test(NAME EmulatorVersionRecords COMMAND test_emulator_version_records)

add_executable(test_github_download_auth
    tests/test_github_download_auth.cpp
    src/services/github_credentials.cpp
)
target_link_libraries(test_github_download_auth PRIVATE retronest_core Qt6::Test)
add_test(NAME GitHubDownloadAuth COMMAND test_github_download_auth)

add_executable(test_core_loader tests/test_core_loader.cpp)
target_link_libraries(test_core_loader PRIVATE retronest_core Qt6::Test)
add_dependencies(test_core_loader fake_libretro_core)
add_test(NAME CoreLoader COMMAND test_core_loader)

# SP8: the three test_pcsx2_* QtTest targets were deleted with the
# standalone PCSX2Adapter (they asserted standalone-specific INI key names
# that the libretro core uses in prefixed pcsx2_* form). PCSX2 option-schema
# coverage is being restored as a dedicated QtTest target,
# test_pcsx2_libretro_schema, alongside the other *_libretro_schema guards.
add_executable(test_controller_bindings_view
    tests/test_controller_bindings_view.cpp
    tests/stubs/app_controller_stub.cpp
    src/ui/settings/controller_bindings_view.cpp
)
target_link_libraries(test_controller_bindings_view PRIVATE
    retronest_core retronest_ui Qt6::Svg Qt6::Test
)
add_test(NAME ControllerBindingsView COMMAND test_controller_bindings_view)

add_executable(test_hotkey_binding_row tests/test_hotkey_binding_row.cpp)
target_link_libraries(test_hotkey_binding_row PRIVATE retronest_ui Qt6::Test)
add_test(NAME HotkeyBindingRow COMMAND test_hotkey_binding_row)

add_executable(test_ppsspp_libretro_bindings tests/test_ppsspp_libretro_bindings.cpp)
target_link_libraries(test_ppsspp_libretro_bindings PRIVATE retronest_core Qt6::Test)
add_test(NAME PpssppLibretroBindings COMMAND test_ppsspp_libretro_bindings)

add_executable(test_ppsspp_libretro_schema tests/test_ppsspp_libretro_schema.cpp)
target_link_libraries(test_ppsspp_libretro_schema PRIVATE retronest_core Qt6::Test)
add_test(NAME PpssppLibretroSchema COMMAND test_ppsspp_libretro_schema)

add_executable(test_quick_settings_shortlist tests/test_quick_settings_shortlist.cpp)
target_link_libraries(test_quick_settings_shortlist PRIVATE retronest_core Qt6::Test)
add_test(NAME QuickSettingsShortlist COMMAND test_quick_settings_shortlist)

# SP6 Task 10: DolphinLibretroAdapter settings schema shape guard.
add_executable(test_dolphin_libretro_schema tests/test_dolphin_libretro_schema.cpp)
target_link_libraries(test_dolphin_libretro_schema PRIVATE retronest_core Qt6::Test)
add_test(NAME DolphinLibretroSchema COMMAND test_dolphin_libretro_schema)

# Phase 5: DuckStationLibretroAdapter settings-schema structure + first-run defaults.
add_executable(test_duckstation_libretro_schema tests/test_duckstation_libretro_schema.cpp)
target_link_libraries(test_duckstation_libretro_schema PRIVATE retronest_core Qt6::Test)
add_test(NAME DuckStationLibretroSchema COMMAND test_duckstation_libretro_schema)

# Pcsx2LibretroAdapter settings-schema + hub/preview/controller shape guard
# (restores the coverage lost when the SP8 tools/test_core_options harness
# was retired).
add_executable(test_pcsx2_libretro_schema tests/test_pcsx2_libretro_schema.cpp)
target_link_libraries(test_pcsx2_libretro_schema PRIVATE retronest_core Qt6::Test)
add_test(NAME Pcsx2LibretroSchema COMMAND test_pcsx2_libretro_schema)

# Packet 7 Stage 1: ABI pin for the retronest-libretro contract package.
add_executable(test_retronest_contract tests/test_retronest_contract.cpp)
target_link_libraries(test_retronest_contract PRIVATE retronest_core Qt6::Test)
add_test(NAME RetronestContract COMMAND test_retronest_contract)

# Packet 7 Stage 2: declared-options capture + sidecar round-trip.
add_executable(test_declared_options tests/test_declared_options.cpp)
target_link_libraries(test_declared_options PRIVATE retronest_core Qt6::Test)
add_test(NAME DeclaredOptions COMMAND test_declared_options)

# Packet 7 Stage 3: system-facts registry (manifests/systems.json).
add_executable(test_system_registry tests/test_system_registry.cpp)
target_link_libraries(test_system_registry PRIVATE retronest_core Qt6::Test)
add_test(NAME SystemRegistry COMMAND test_system_registry)

# Packet 7 Stage 3: manifest-driven detail-page ACTIONS row model.
add_executable(test_detail_actions tests/test_detail_actions.cpp)
target_link_libraries(test_detail_actions PRIVATE retronest_core Qt6::Test)
add_test(NAME DetailActions COMMAND test_detail_actions)

# Packet 7 Stage 2: offline sidecar seeding (probes the fake core dylib).
add_executable(test_core_prober tests/test_core_prober.cpp)
target_link_libraries(test_core_prober PRIVATE retronest_core Qt6::Test)
add_dependencies(test_core_prober fake_libretro_core)
add_test(NAME CoreProber COMMAND test_core_prober)

# Packet 7 Stage 2: declared-options × overlay → SettingDef merge.
add_executable(test_libretro_schema_merge tests/test_libretro_schema_merge.cpp)
target_link_libraries(test_libretro_schema_merge PRIVATE retronest_core Qt6::Test)
add_test(NAME LibretroSchemaMerge COMMAND test_libretro_schema_merge)

# Packet 7 Stage 2 TRANSITIONAL: per-core conversion parity net (deleted in
# Task 11 when the last adapter converts).
# SP5: DolphinLibretroAdapter controller schema (GCPad + Wii Classic).
add_executable(test_dolphin_libretro_controller_schema tests/test_dolphin_libretro_controller_schema.cpp)
target_link_libraries(test_dolphin_libretro_controller_schema PRIVATE retronest_core Qt6::Test)
add_test(NAME DolphinLibretroControllerSchema COMMAND test_dolphin_libretro_controller_schema)

add_executable(test_generic_hotkey_page
    tests/test_generic_hotkey_page.cpp
    tests/stubs/generic_hotkey_page_stubs.cpp
    src/ui/settings/generic_hotkey_page.cpp
)
target_link_libraries(test_generic_hotkey_page PRIVATE
    retronest_core retronest_ui Qt6::Test
)
add_test(NAME GenericHotkeyPage COMMAND test_generic_hotkey_page)

add_executable(test_quick_settings_file_field tests/test_quick_settings_file_field.cpp)
target_link_libraries(test_quick_settings_file_field PRIVATE retronest_core Qt6::Test)
add_test(NAME QuickSettingsFileField COMMAND test_quick_settings_file_field)

add_executable(test_settings_toggle tests/test_settings_toggle.cpp)
target_link_libraries(test_settings_toggle PRIVATE retronest_ui Qt6::Test)
add_test(NAME SettingsToggle COMMAND test_settings_toggle)

add_executable(test_settings_combo_row tests/test_settings_combo_row.cpp)
target_link_libraries(test_settings_combo_row PRIVATE retronest_ui Qt6::Test)
add_test(NAME SettingsComboRow COMMAND test_settings_combo_row)

add_executable(test_dependency_master_roots tests/test_dependency_master_roots.cpp)
target_link_libraries(test_dependency_master_roots PRIVATE retronest_ui Qt6::Test)
add_test(NAME DependencyMasterRoots COMMAND test_dependency_master_roots)

add_executable(test_settings_slider_row tests/test_settings_slider_row.cpp)
target_link_libraries(test_settings_slider_row PRIVATE retronest_ui Qt6::Test)
add_test(NAME SettingsSliderRow COMMAND test_settings_slider_row)

add_executable(test_settings_description_bar tests/test_settings_description_bar.cpp)
target_link_libraries(test_settings_description_bar PRIVATE retronest_ui Qt6::Test)
add_test(NAME SettingsDescriptionBar COMMAND test_settings_description_bar)

add_executable(test_aspect_ratio_preview tests/test_aspect_ratio_preview.cpp)
target_link_libraries(test_aspect_ratio_preview PRIVATE retronest_ui Qt6::Test)
add_test(NAME AspectRatioPreview COMMAND test_aspect_ratio_preview)

add_executable(test_osd_preview tests/test_osd_preview.cpp)
target_link_libraries(test_osd_preview PRIVATE retronest_ui Qt6::Test)
add_test(NAME OsdPreview COMMAND test_osd_preview)

add_executable(test_setting_def_ini_routing tests/test_setting_def_ini_routing.cpp)
target_link_libraries(test_setting_def_ini_routing PRIVATE retronest_core Qt6::Test)
add_test(NAME SettingDefIniRouting COMMAND test_setting_def_ini_routing)

add_executable(test_video_software tests/test_video_software.cpp)
target_link_libraries(test_video_software PRIVATE retronest_core Qt6::Test)
add_test(NAME VideoSoftware COMMAND test_video_software)

add_executable(test_audio_sink tests/test_audio_sink.cpp)
target_link_libraries(test_audio_sink PRIVATE retronest_core Qt6::Test)
add_test(NAME AudioSink COMMAND test_audio_sink)

add_executable(test_audio_sink_mute_volume tests/test_audio_sink_mute_volume.cpp)
target_link_libraries(test_audio_sink_mute_volume PRIVATE retronest_core Qt6::Test)
add_test(NAME AudioSinkMuteVolume COMMAND test_audio_sink_mute_volume)

add_executable(test_game_session_save_slot tests/test_game_session_save_slot.cpp)
# GameSession now lives in retronest_core (P10 render-seam refactor) — no
# more compiling it + the GL item here, no Qt::Quick.
target_link_libraries(test_game_session_save_slot PRIVATE
    retronest_core Qt6::Test
)
add_test(NAME GameSessionSaveSlot COMMAND test_game_session_save_slot)

add_executable(test_input_router tests/test_input_router.cpp)
target_link_libraries(test_input_router PRIVATE retronest_core Qt6::Test)
add_test(NAME InputRouter COMMAND test_input_router)

add_executable(test_options_store tests/test_options_store.cpp)
target_link_libraries(test_options_store PRIVATE retronest_core Qt6::Test)
add_test(NAME OptionsStore COMMAND test_options_store)

add_executable(test_environment_callbacks tests/test_environment_callbacks.cpp)
target_link_libraries(test_environment_callbacks PRIVATE retronest_core Qt6::Test)
add_test(NAME EnvironmentCallbacks COMMAND test_environment_callbacks)

add_executable(test_core_runtime tests/test_core_runtime.cpp)
target_link_libraries(test_core_runtime PRIVATE retronest_core Qt6::Test)
add_dependencies(test_core_runtime fake_libretro_core)
add_test(NAME CoreRuntime COMMAND test_core_runtime)

add_executable(test_theme_contract tests/test_theme_contract.cpp)
target_link_libraries(test_theme_contract PRIVATE Qt6::Test)
add_test(NAME ThemeContract COMMAND test_theme_contract)

add_executable(test_theme_manager
    tests/test_theme_manager.cpp
    # theme_manager.cpp is an app-only source (not in retronest_ui), so the
    # test compiles it directly; Paths comes from retronest_core.
    src/ui/theme_manager.cpp
)
target_link_libraries(test_theme_manager PRIVATE retronest_core Qt6::Test)
add_test(NAME ThemeManager COMMAND test_theme_manager)

add_executable(test_patches_sidecar tests/test_patches_sidecar.cpp)
target_link_libraries(test_patches_sidecar PRIVATE retronest_core)
add_test(NAME PatchesSidecar COMMAND test_patches_sidecar)

add_executable(test_patches_installer tests/test_patches_installer.cpp)
target_link_libraries(test_patches_installer PRIVATE retronest_core)
add_test(NAME PatchesInstaller COMMAND test_patches_installer)

add_library(fake_libretro_core MODULE tests/fixtures/fake_libretro_core.c)
set_target_properties(fake_libretro_core PROPERTIES
    PREFIX "" OUTPUT_NAME "fake_libretro_core"
    SUFFIX ".dylib")
target_include_directories(fake_libretro_core PRIVATE
    ${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro)
