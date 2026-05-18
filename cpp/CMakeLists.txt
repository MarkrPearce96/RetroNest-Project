cmake_minimum_required(VERSION 3.16)
project(RetroNest LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 Concurrent Multimedia Svg)
find_package(SDL2 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    libchdr
    GIT_REPOSITORY https://github.com/rtissera/libchdr.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
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
# PUBLIC so consumers (only ${PROJECT_NAME}) get rcheevos headers automatically (e.g. rcheevos_runtime.cpp).
target_include_directories(rcheevos_static PUBLIC ${rcheevos_SOURCE_DIR}/include)
# rc_libretro.h lives in src/ (not include/) but consumers need it for the
# memory-map helpers (rc_libretro_memory_init / rc_libretro_memory_read) used
# by RetroNest's RcheevosRuntime to translate RA memory addresses through
# libretro's memory descriptors.
target_include_directories(rcheevos_static PUBLIC ${rcheevos_SOURCE_DIR}/src)
target_include_directories(rcheevos_static PRIVATE ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
# Enable hash-based game identification (rc_client_begin_identify_and_load_game).
target_compile_definitions(rcheevos_static PUBLIC RC_CLIENT_SUPPORTS_HASH)

# ScreenScraper developer credentials (build-time, not in repo)
set(SCREENSCRAPER_DEV_ID "" CACHE STRING "ScreenScraper dev ID")
set(SCREENSCRAPER_DEV_PASSWORD "" CACHE STRING "ScreenScraper dev password")
set(SCREENSCRAPER_SOFTNAME "RetroNest" CACHE STRING "ScreenScraper software name")
include(dev_credentials.cmake OPTIONAL)

set(SOURCES
    src/main.cpp
    src/core/manifest_loader.cpp
    src/core/migration_pcsx2.cpp
    src/core/paths.cpp
    src/services/emulator_installer.cpp
    src/services/installer_utils.cpp
    src/services/patches_sidecar.cpp
    src/services/config_service.cpp
    src/core/game_session.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/core/sfo_parser.cpp
    src/core/ini_file.cpp
    src/core/scraper.cpp
    src/core/database.cpp
    src/services/rom_scanner.cpp
    src/services/ra_credentials.cpp
    src/core/ra_client.cpp
    src/core/iso9660_reader.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/retro_log.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/services/game_service.cpp
    src/services/scraper_service.cpp
    src/services/emulator_service.cpp
    src/services/scraper_credentials.cpp
    src/services/ra_service.cpp
    src/ui/wizard_state.cpp
    src/ui/emulator_list_model.cpp
    src/ui/install_controller.cpp
    src/ui/game_list_model.cpp
    src/ui/app_controller.cpp
    src/ui/in_game_menu_panel.cpp
    src/ui/libretro_overlay_panel.cpp
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
    src/core/sdl_input_manager.cpp
    src/core/macos_fullscreen.mm
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
    src/ui/settings/dolphin/dolphin_settings_dialog.cpp
    src/ui/settings/dolphin/dolphin_category_hub.cpp
    src/ui/settings/mgba/mgba_settings_dialog.cpp
    src/ui/settings/mgba/mgba_category_hub.cpp
    src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
    src/ui/settings/pcsx2/pcsx2_category_hub.cpp
    src/ui/settings/duckstation/duckstation_settings_dialog.cpp
    src/ui/settings/duckstation/duckstation_category_hub.cpp
    # PPSSPP settings — schema-driven via GenericSettingsPage; no bespoke pages.
    src/ui/settings/ppsspp/ppsspp_category_hub.cpp
    src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp
)

set(HEADERS
    src/core/manifest.h
    src/core/manifest_loader.h
    src/core/paths.h
    src/services/emulator_installer.h
    src/services/installer_utils.h
    src/services/patches_sidecar.h
    src/core/game_session.h
    src/adapters/emulator_adapter.h
    src/adapters/adapter_registry.h
    src/adapters/duckstation_adapter.h
    src/adapters/ppsspp_adapter.h
    src/adapters/dolphin_adapter.h
    src/adapters/libretro/libretro_adapter.h
    src/adapters/libretro/mgba_libretro_adapter.h
    src/adapters/libretro/pcsx2_libretro_adapter.h
    src/core/libretro/core_loader.h
    src/core/libretro/core_runtime.h
    src/core/libretro/rcheevos_runtime.h
    src/core/libretro/environment_callbacks.h
    src/core/libretro/video_software.h
    src/core/libretro/audio_sink.h
    src/core/libretro/input_router.h
    src/core/sfo_parser.h
    src/core/ini_file.h
    src/core/scraper.h
    src/core/setting_def.h
    src/core/database.h
    src/core/cover_resolver.h
    src/core/github_client.h
    src/services/rom_scanner.h
    src/services/ra_credentials.h
    src/core/ra_client.h
    src/core/iso9660_reader.h
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
    src/ui/theme_manager.h
    src/ui/theme_context.h
    src/ui/settings/controller_mapping_page.h
    src/ui/settings/emulator_settings_dialog_base.h
    src/ui/settings/emulator_category_hub_base.h
    src/ui/settings/generic_hotkey_page.h
    src/ui/settings/hotkey_settings_dialog.h
    src/ui/settings/generic_settings_page.h
    src/ui/settings/settings_page_builder.h
    src/core/sdl_input_manager.h
    src/core/binding_def.h
    src/ui/settings/binding_display.h
    src/ui/settings/binding_widget_common.h
    src/core/controller_type_def.h
    src/core/macos_fullscreen.h
    src/ui/settings/settings_dialog_theme.h
    # Dolphin settings
    src/ui/settings/dolphin/dolphin_settings_dialog.h
    src/ui/settings/dolphin/dolphin_category_hub.h
    # mGBA settings
    src/ui/settings/mgba/mgba_settings_dialog.h
    src/ui/settings/mgba/mgba_category_hub.h
    # PCSX2 settings
    src/ui/settings/pcsx2/pcsx2_settings_dialog.h
    src/ui/settings/pcsx2/pcsx2_category_hub.h
    # DuckStation settings
    src/ui/settings/duckstation/duckstation_settings_dialog.h
    src/ui/settings/duckstation/duckstation_category_hub.h
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
    src/ui/settings/pcsx2/pcsx2_settings_dialog.h
    src/ui/settings/pcsx2/pcsx2_category_hub.h
    # PPSSPP settings — schema-driven via GenericSettingsPage; no bespoke pages.
    src/ui/settings/ppsspp/ppsspp_category_hub.h
    src/ui/settings/ppsspp/ppsspp_settings_dialog.h
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
# Note: RetroNest's bundle ID does NOT affect PPSSPP's NSUserDefaults domain
# — `defaults write org.ppsspp.ppsspp …` writes to PPSSPP's plist regardless
# of which process invokes it.
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
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SCREENSCRAPER_DEV_ID="${SCREENSCRAPER_DEV_ID}"
    SCREENSCRAPER_DEV_PASSWORD="${SCREENSCRAPER_DEV_PASSWORD}"
    SCREENSCRAPER_SOFTNAME="${SCREENSCRAPER_SOFTNAME}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Svg
    ${SDL2_LIBRARIES}
    ${CMAKE_DL_LIBS}
    chdr-static
    rcheevos_static
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
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework QuartzCore"
        "-framework Metal"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
                -f "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
        COMMENT "Refreshing Launch Services registration for RetroNest.app"
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
        qml/SetupWizard/FolderPage.qml
        qml/SetupWizard/EmulatorsPage.qml
        qml/SetupWizard/ResolutionPage.qml
        qml/SetupWizard/AspectRatioPage.qml
        qml/SetupWizard/FilesPage.qml
        qml/SetupWizard/InstallPage.qml
        qml/SetupWizard/NavBar.qml
        qml/SetupWizard/StepIndicator.qml
        qml/SetupWizard/PillButton.qml
        qml/SetupWizard/EmulatorCard.qml
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/dolphin_logo.png
    RESOURCE_PREFIX /
)

# ── AppUI QML module ──
qt_add_library(appui_backing STATIC)
target_include_directories(appui_backing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/libretro
)
target_link_libraries(appui_backing PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickControls2)

qt_add_qml_module(appui_backing
    URI AppUI
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/AppUI"
    QML_FILES
        qml/AppUI/ButtonHints.qml
        qml/AppUI/FocusableItem.qml
        qml/AppUI/AppWindow.qml
        qml/AppUI/EmptyStatePage.qml
        qml/AppUI/TopBar.qml
        qml/AppUI/StatusBar.qml
        qml/AppUI/GamesPage.qml
        qml/AppUI/SystemSidebar.qml
        qml/AppUI/GameGridView.qml
        qml/AppUI/GameCard.qml
        qml/AppUI/ToolBtn.qml
        qml/AppUI/SettingsPage.qml

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
        qml/AppUI/InGameMenuPanel.qml
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
        qml/AppUI/EmulatorLogos.js
    SOURCES
        src/ui/libretro/libretro_video_item.h
        src/ui/libretro/libretro_metal_item.h
        src/ui/libretro/libretro_video_item.cpp
        src/ui/libretro/libretro_metal_item.mm
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/dolphin_logo.png
        qml/AppUI/images/retroachievements_logo.png
        qml/AppUI/images/screenscraper_logo.png
        qml/AppUI/images/empty-state-bg.webp
        qml/AppUI/images/ar/pcsx2-4x3.webp
        qml/AppUI/images/ar/pcsx2-16x9.webp
        qml/AppUI/images/ar/duckstation-4x3.webp
        qml/AppUI/images/ar/duckstation-16x9.webp
        qml/AppUI/images/res/duckstation-720p.webp
        qml/AppUI/images/res/duckstation-1080p.webp
        qml/AppUI/images/res/duckstation-1440p.webp
        qml/AppUI/images/res/duckstation-4k.webp
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
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_ini_file
    tests/test_ini_file.cpp
    src/core/ini_file.cpp
)
target_include_directories(test_ini_file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ini_file PRIVATE Qt6::Core Qt6::Test)
add_test(NAME IniFile COMMAND test_ini_file)

add_executable(test_patch_ini_keys
    tests/test_patch_ini_keys.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_patch_ini_keys PROPERTIES AUTOMOC ON)
target_include_directories(test_patch_ini_keys PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_patch_ini_keys PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME PatchIniKeys COMMAND test_patch_ini_keys)

add_executable(test_format_binding
    tests/test_format_binding.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_format_binding PROPERTIES AUTOMOC ON)
target_include_directories(test_format_binding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_format_binding PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test chdr-static)
add_test(NAME FormatBinding COMMAND test_format_binding)

add_executable(test_rom_scanner
    tests/test_rom_scanner.cpp
    src/services/rom_scanner.cpp
    src/core/manifest_loader.cpp
    src/core/paths.cpp
    src/core/database.cpp
    src/core/iso9660_reader.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/sdl_input_manager.cpp
    src/core/libretro/retro_log.cpp
    src/core/sfo_parser.cpp
    src/core/ini_file.cpp
)
set_target_properties(test_rom_scanner PROPERTIES AUTOMOC ON)
target_include_directories(test_rom_scanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_rom_scanner PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test chdr-static rcheevos_static ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(NAME RomScanner COMMAND test_rom_scanner)

add_executable(test_iso9660_reader
    tests/test_iso9660_reader.cpp
    src/core/iso9660_reader.cpp
)
target_include_directories(test_iso9660_reader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_iso9660_reader PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME Iso9660Reader COMMAND test_iso9660_reader)

add_executable(test_sfo_parser
    tests/test_sfo_parser.cpp
    src/core/sfo_parser.cpp
)
target_include_directories(test_sfo_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sfo_parser PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SfoParser COMMAND test_sfo_parser)

add_executable(test_bitmask_helpers
    tests/test_bitmask_helpers.cpp
)
set_target_properties(test_bitmask_helpers PROPERTIES AUTOMOC ON)
target_include_directories(test_bitmask_helpers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_bitmask_helpers PRIVATE Qt6::Core Qt6::Test)
add_test(NAME BitmaskHelpers COMMAND test_bitmask_helpers)

add_executable(test_setting_dependency
    tests/test_setting_dependency.cpp
)
set_target_properties(test_setting_dependency PROPERTIES AUTOMOC ON)
target_include_directories(test_setting_dependency PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_setting_dependency PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingDependency COMMAND test_setting_dependency)

add_executable(test_setting_def_storage tests/test_setting_def_storage.cpp)
target_include_directories(test_setting_def_storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_setting_def_storage PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingDefStorage COMMAND test_setting_def_storage)

add_executable(test_manifest_libretro_fields tests/test_manifest_libretro_fields.cpp
    src/core/manifest_loader.cpp)
target_include_directories(test_manifest_libretro_fields PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_manifest_libretro_fields PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ManifestLibretroFields COMMAND test_manifest_libretro_fields)

add_executable(test_core_loader
    tests/test_core_loader.cpp
    src/core/libretro/core_loader.cpp)
set_target_properties(test_core_loader PROPERTIES AUTOMOC ON)
target_include_directories(test_core_loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
target_link_libraries(test_core_loader PRIVATE Qt6::Core Qt6::Test ${CMAKE_DL_LIBS})
add_dependencies(test_core_loader fake_libretro_core)
add_test(NAME CoreLoader COMMAND test_core_loader)

add_executable(test_ppsspp_schema
    tests/test_ppsspp_schema.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_ppsspp_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_ppsspp_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ppsspp_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME PPSSPPSchema COMMAND test_ppsspp_schema)

add_executable(test_ppsspp_controller_schema
    tests/test_ppsspp_controller_schema.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_ppsspp_controller_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_ppsspp_controller_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ppsspp_controller_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME PPSSPPControllerSchema COMMAND test_ppsspp_controller_schema)

add_executable(test_dolphin_schema
    tests/test_dolphin_schema.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_dolphin_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_dolphin_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_dolphin_schema PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test chdr-static)
add_test(NAME DolphinSchema COMMAND test_dolphin_schema)

add_executable(test_dolphin_controller_schema
    tests/test_dolphin_controller_schema.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_dolphin_controller_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_dolphin_controller_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_dolphin_controller_schema PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test chdr-static)
add_test(NAME DolphinControllerSchema COMMAND test_dolphin_controller_schema)

add_executable(test_duckstation_schema
    tests/test_duckstation_schema.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_duckstation_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_duckstation_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_duckstation_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME DuckStationSchema COMMAND test_duckstation_schema)

add_executable(test_duckstation_controller_schema
    tests/test_duckstation_controller_schema.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_duckstation_controller_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_duckstation_controller_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_duckstation_controller_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME DuckStationControllerSchema COMMAND test_duckstation_controller_schema)

# SP8: the three test_pcsx2_* QtTest targets were deleted with the
# standalone PCSX2Adapter. They asserted against standalone-specific INI
# key names (e.g. "AspectRatio", "OsdShowFPS") that the libretro core
# uses in prefixed form (pcsx2_*). The libretro adapter's option schema
# is covered end-to-end by tools/test_core_options/ (42/42 passing).
add_executable(test_controller_bindings_view
    tests/test_controller_bindings_view.cpp
    tests/stubs/app_controller_stub.cpp
    src/ui/settings/controller_bindings_view.cpp
    src/ui/settings/widgets/settings_card.cpp
    src/ui/settings/widgets/settings_combo_row.cpp
    src/ui/settings/widgets/settings_toggle.cpp
    src/ui/settings/widgets/settings_slider_row.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/retro_log.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/manifest_loader.cpp
    src/core/database.cpp
    src/core/sdl_input_manager.cpp
)
set_target_properties(test_controller_bindings_view PROPERTIES AUTOMOC ON)
target_include_directories(test_controller_bindings_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_controller_bindings_view PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network Qt6::Sql Qt6::Test
    chdr-static rcheevos_static ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS}
)
add_test(NAME ControllerBindingsView COMMAND test_controller_bindings_view)

add_executable(test_hotkey_binding_row
    tests/test_hotkey_binding_row.cpp
    src/ui/settings/widgets/hotkey_binding_row.cpp
)
set_target_properties(test_hotkey_binding_row PROPERTIES AUTOMOC ON)
target_include_directories(test_hotkey_binding_row PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_hotkey_binding_row PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
add_test(NAME HotkeyBindingRow COMMAND test_hotkey_binding_row)

add_executable(test_hotkey_defs
    tests/test_hotkey_defs.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_hotkey_defs PROPERTIES AUTOMOC ON)
target_include_directories(test_hotkey_defs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_hotkey_defs PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME HotkeyDefs COMMAND test_hotkey_defs)

add_executable(test_generic_hotkey_page
    tests/test_generic_hotkey_page.cpp
    tests/stubs/generic_hotkey_page_stubs.cpp
    src/ui/settings/generic_hotkey_page.cpp
    src/ui/settings/widgets/hotkey_binding_row.cpp
    src/ui/settings/widgets/settings_section_header.cpp
    src/core/sdl_input_manager.cpp
    src/core/libretro/input_router.cpp
)
set_target_properties(test_generic_hotkey_page PROPERTIES AUTOMOC ON)
target_include_directories(test_generic_hotkey_page PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
)
target_link_libraries(test_generic_hotkey_page PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Test ${SDL2_LIBRARIES}
)
add_test(NAME GenericHotkeyPage COMMAND test_generic_hotkey_page)

add_executable(test_quick_settings_file_field
    tests/test_quick_settings_file_field.cpp
    src/services/config_service.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/retro_log.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/database.cpp
    src/core/manifest_loader.cpp
    src/core/sdl_input_manager.cpp
)
set_target_properties(test_quick_settings_file_field PROPERTIES AUTOMOC ON)
target_include_directories(test_quick_settings_file_field PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_quick_settings_file_field PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test chdr-static rcheevos_static ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(NAME QuickSettingsFileField COMMAND test_quick_settings_file_field)

add_executable(test_settings_toggle
    tests/test_settings_toggle.cpp
    src/ui/settings/widgets/settings_toggle.cpp
)
set_target_properties(test_settings_toggle PROPERTIES AUTOMOC ON)
target_include_directories(test_settings_toggle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_settings_toggle PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME SettingsToggle COMMAND test_settings_toggle)

add_executable(test_settings_combo_row
    tests/test_settings_combo_row.cpp
    src/ui/settings/widgets/settings_combo_row.cpp
)
set_target_properties(test_settings_combo_row PROPERTIES AUTOMOC ON)
target_include_directories(test_settings_combo_row PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_settings_combo_row PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME SettingsComboRow COMMAND test_settings_combo_row)

add_executable(test_settings_slider_row
    tests/test_settings_slider_row.cpp
    src/ui/settings/widgets/settings_slider_row.cpp
)
set_target_properties(test_settings_slider_row PROPERTIES AUTOMOC ON)
target_include_directories(test_settings_slider_row PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_settings_slider_row PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME SettingsSliderRow COMMAND test_settings_slider_row)

add_executable(test_settings_description_bar
    tests/test_settings_description_bar.cpp
    src/ui/settings/widgets/settings_description_bar.cpp
)
set_target_properties(test_settings_description_bar PROPERTIES AUTOMOC ON)
target_include_directories(test_settings_description_bar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_settings_description_bar PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME SettingsDescriptionBar COMMAND test_settings_description_bar)

add_executable(test_aspect_ratio_preview
    tests/test_aspect_ratio_preview.cpp
    src/ui/settings/widgets/preview/aspect_ratio_preview.cpp
)
set_target_properties(test_aspect_ratio_preview PROPERTIES AUTOMOC ON)
target_include_directories(test_aspect_ratio_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_aspect_ratio_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME AspectRatioPreview COMMAND test_aspect_ratio_preview)

add_executable(test_osd_preview
    tests/test_osd_preview.cpp
    src/ui/settings/widgets/preview/osd_preview.cpp
)
set_target_properties(test_osd_preview PROPERTIES AUTOMOC ON)
target_include_directories(test_osd_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_osd_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME OsdPreview COMMAND test_osd_preview)

add_executable(test_setting_def_ini_routing
    tests/test_setting_def_ini_routing.cpp
    src/core/ini_file.cpp
)
set_target_properties(test_setting_def_ini_routing PROPERTIES AUTOMOC ON)
target_include_directories(test_setting_def_ini_routing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_setting_def_ini_routing PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingDefIniRouting COMMAND test_setting_def_ini_routing)

add_executable(test_video_software
    tests/test_video_software.cpp
    src/core/libretro/video_software.cpp)
set_target_properties(test_video_software PROPERTIES AUTOMOC ON)
target_include_directories(test_video_software PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
target_link_libraries(test_video_software PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME VideoSoftware COMMAND test_video_software)

add_executable(test_audio_sink
    tests/test_audio_sink.cpp
    src/core/libretro/audio_sink.cpp)
set_target_properties(test_audio_sink PROPERTIES AUTOMOC ON)
target_include_directories(test_audio_sink PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_audio_sink PRIVATE Qt6::Core Qt6::Test ${SDL2_LIBRARIES})
add_test(NAME AudioSink COMMAND test_audio_sink)

add_executable(test_input_router
    tests/test_input_router.cpp
    src/core/libretro/input_router.cpp)
set_target_properties(test_input_router PROPERTIES AUTOMOC ON)
target_include_directories(test_input_router PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
target_link_libraries(test_input_router PRIVATE Qt6::Core Qt6::Test)
add_test(NAME InputRouter COMMAND test_input_router)

add_executable(test_options_store
    tests/test_options_store.cpp
    src/core/libretro/options_store.cpp)
set_target_properties(test_options_store PROPERTIES AUTOMOC ON)
target_include_directories(test_options_store PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_options_store PRIVATE Qt6::Core Qt6::Test)
add_test(NAME OptionsStore COMMAND test_options_store)

add_executable(test_environment_callbacks
    tests/test_environment_callbacks.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/retro_log.cpp)
set_target_properties(test_environment_callbacks PROPERTIES AUTOMOC ON)
target_include_directories(test_environment_callbacks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
target_link_libraries(test_environment_callbacks PRIVATE Qt6::Core Qt6::Test)
add_test(NAME EnvironmentCallbacks COMMAND test_environment_callbacks)

add_executable(test_core_runtime
    tests/test_core_runtime.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/retro_log.cpp
    src/core/sdl_input_manager.cpp)
set_target_properties(test_core_runtime PROPERTIES AUTOMOC ON)
target_include_directories(test_core_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_core_runtime PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Test ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES} rcheevos_static)
add_dependencies(test_core_runtime fake_libretro_core)
add_test(NAME CoreRuntime COMMAND test_core_runtime)

add_executable(test_migration_pcsx2
    tests/test_migration_pcsx2.cpp
    src/core/migration_pcsx2.cpp
    src/core/paths.cpp
)
set_target_properties(test_migration_pcsx2 PROPERTIES AUTOMOC ON)
target_include_directories(test_migration_pcsx2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_migration_pcsx2 PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MigrationPcsx2 COMMAND test_migration_pcsx2)

add_executable(test_patches_sidecar
    tests/test_patches_sidecar.cpp
    src/services/patches_sidecar.cpp
)
target_include_directories(test_patches_sidecar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_patches_sidecar PRIVATE Qt6::Core)
add_test(NAME PatchesSidecar COMMAND test_patches_sidecar)

add_library(fake_libretro_core MODULE tests/fixtures/fake_libretro_core.c)
set_target_properties(fake_libretro_core PROPERTIES
    PREFIX "" OUTPUT_NAME "fake_libretro_core"
    SUFFIX ".dylib")
target_include_directories(fake_libretro_core PRIVATE
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api)
