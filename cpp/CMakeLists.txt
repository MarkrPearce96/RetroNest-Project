cmake_minimum_required(VERSION 3.16)
project(RetroNest LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 Concurrent Multimedia)
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

# ScreenScraper developer credentials (build-time, not in repo)
set(SCREENSCRAPER_DEV_ID "" CACHE STRING "ScreenScraper dev ID")
set(SCREENSCRAPER_DEV_PASSWORD "" CACHE STRING "ScreenScraper dev password")
set(SCREENSCRAPER_SOFTNAME "RetroNest" CACHE STRING "ScreenScraper software name")
include(dev_credentials.cmake OPTIONAL)

set(SOURCES
    src/main.cpp
    src/core/manifest_loader.cpp
    src/core/paths.cpp
    src/services/emulator_installer.cpp
    src/services/config_service.cpp
    src/core/game_session.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/pcsx2_adapter.cpp
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
    src/ui/theme_manager.cpp
    src/ui/theme_context.cpp
    src/ui/settings/controller_mapping_page.cpp
    src/ui/settings/bindings_widget_base.cpp
    src/ui/settings/ds2_bindings_widget.cpp
    src/ui/settings/guitar_bindings_widget.cpp
    src/ui/settings/jogcon_bindings_widget.cpp
    src/ui/settings/negcon_bindings_widget.cpp
    src/ui/settings/popn_bindings_widget.cpp
    src/ui/settings/ds_jogcon_bindings_widget.cpp
    src/ui/settings/psp_bindings_widget.cpp
    src/ui/settings/digital_bindings_widget.cpp
    src/ui/settings/analog_bindings_widget.cpp
    src/ui/settings/analog_joystick_bindings_widget.cpp
    src/ui/settings/ds_negcon_bindings_widget.cpp
    src/ui/settings/ds_negcon_rumble_bindings_widget.cpp
    src/ui/settings/emulator_settings_dialog_base.cpp
    src/ui/settings/emulator_category_hub_base.cpp
    src/ui/settings/generic_settings_page.cpp
    src/ui/settings/settings_page_builder.cpp
    src/ui/settings/hotkey_settings_page.cpp
    src/ui/settings/controller_settings_widget.cpp
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
    src/ui/settings/widgets/preview/aspect_ratio_preview.cpp
    src/ui/settings/widgets/preview/osd_preview.cpp
    src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
    src/ui/settings/pcsx2/pcsx2_category_hub.cpp
    src/ui/settings/dolphin/dolphin_settings_dialog.cpp
    src/ui/settings/dolphin/dolphin_category_hub.cpp
    src/ui/settings/duckstation/duckstation_settings_dialog.cpp
    src/ui/settings/duckstation/duckstation_category_hub.cpp
    src/ui/settings/duckstation/pages/duckstation_audio_page.cpp
    src/ui/settings/duckstation/pages/duckstation_console_page.cpp
    src/ui/settings/duckstation/pages/duckstation_emulation_page.cpp
    src/ui/settings/duckstation/pages/duckstation_graphics_page.cpp
    src/ui/settings/duckstation/pages/duckstation_graphics_rendering_page.cpp
    src/ui/settings/duckstation/pages/duckstation_graphics_advanced_page.cpp
    src/ui/settings/duckstation/pages/duckstation_graphics_osd_page.cpp
    src/ui/settings/duckstation/pages/duckstation_memory_cards_page.cpp
    src/ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.cpp
    src/ui/settings/duckstation/widgets/duckstation_osd_preview.cpp
    src/ui/settings/pcsx2/pages/pcsx2_emulation_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_audio_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_memory_cards_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp
    # PPSSPP settings
    src/ui/settings/ppsspp/ppsspp_category_hub.cpp
    src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp
    src/ui/settings/ppsspp/pages/ppsspp_emulation_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_audio_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_overlay_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.cpp
    src/ui/settings/ppsspp/pages/ppsspp_graphics_page.cpp
)

set(HEADERS
    src/core/manifest.h
    src/core/manifest_loader.h
    src/core/paths.h
    src/services/emulator_installer.h
    src/core/game_session.h
    src/adapters/emulator_adapter.h
    src/adapters/adapter_registry.h
    src/adapters/pcsx2_adapter.h
    src/adapters/duckstation_adapter.h
    src/adapters/ppsspp_adapter.h
    src/adapters/dolphin_adapter.h
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
    src/ui/theme_manager.h
    src/ui/theme_context.h
    src/ui/settings/controller_mapping_page.h
    src/ui/settings/bindings_widget_base.h
    src/ui/settings/ds2_bindings_widget.h
    src/ui/settings/guitar_bindings_widget.h
    src/ui/settings/jogcon_bindings_widget.h
    src/ui/settings/negcon_bindings_widget.h
    src/ui/settings/popn_bindings_widget.h
    src/ui/settings/ds_jogcon_bindings_widget.h
    src/ui/settings/psp_bindings_widget.h
    src/ui/settings/digital_bindings_widget.h
    src/ui/settings/analog_bindings_widget.h
    src/ui/settings/analog_joystick_bindings_widget.h
    src/ui/settings/ds_negcon_bindings_widget.h
    src/ui/settings/ds_negcon_rumble_bindings_widget.h
    src/ui/settings/emulator_settings_dialog_base.h
    src/ui/settings/emulator_category_hub_base.h
    src/ui/settings/generic_settings_page.h
    src/ui/settings/settings_page_builder.h
    src/ui/settings/hotkey_settings_page.h
    src/ui/settings/controller_settings_widget.h
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
    # DuckStation settings
    src/ui/settings/duckstation/duckstation_settings_dialog.h
    src/ui/settings/duckstation/duckstation_category_hub.h
    src/ui/settings/duckstation/pages/duckstation_audio_page.h
    src/ui/settings/duckstation/pages/duckstation_console_page.h
    src/ui/settings/duckstation/pages/duckstation_emulation_page.h
    src/ui/settings/duckstation/pages/duckstation_graphics_page.h
    src/ui/settings/duckstation/pages/duckstation_graphics_rendering_page.h
    src/ui/settings/duckstation/pages/duckstation_graphics_advanced_page.h
    src/ui/settings/duckstation/pages/duckstation_graphics_osd_page.h
    src/ui/settings/duckstation/pages/duckstation_memory_cards_page.h
    src/ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.h
    src/ui/settings/duckstation/widgets/duckstation_osd_preview.h
    src/ui/settings/widgets/settings_card.h
    src/ui/settings/widgets/settings_section_header.h
    src/ui/settings/widgets/settings_toggle.h
    src/ui/settings/widgets/settings_toggle_row.h
    src/ui/settings/widgets/settings_combo_row.h
    src/ui/settings/widgets/settings_slider_row.h
    src/ui/settings/widgets/settings_description_bar.h
    src/ui/settings/widgets/settings_graphics_sub_tab_bar.h
    src/ui/settings/widgets/preview/aspect_ratio_preview.h
    src/ui/settings/widgets/preview/osd_preview.h
    src/ui/settings/pcsx2/pcsx2_settings_dialog.h
    src/ui/settings/pcsx2/pcsx2_category_hub.h
    src/ui/settings/pcsx2/pages/pcsx2_emulation_page.h
    src/ui/settings/pcsx2/pages/pcsx2_audio_page.h
    src/ui/settings/pcsx2/pages/pcsx2_memory_cards_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_page.h
    # PPSSPP settings
    src/ui/settings/ppsspp/ppsspp_category_hub.h
    src/ui/settings/ppsspp/ppsspp_settings_dialog.h
    src/ui/settings/ppsspp/pages/ppsspp_emulation_page.h
    src/ui/settings/ppsspp/pages/ppsspp_audio_page.h
    src/ui/settings/ppsspp/pages/ppsspp_overlay_page.h
    src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.h
    src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.h
    src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.h
    src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.h
    src/ui/settings/ppsspp/pages/ppsspp_graphics_page.h
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
    ${SDL2_LIBRARIES}
    chdr-static
    "-framework AppKit"
    "-framework Carbon"
)

# Re-register the bundle with Launch Services after every rebuild so
# `open build/RetroNest.app` always launches the freshly-built binary.
# Without this, LS can cache a stale path for com.markpearce.retronest and
# launch an older copy (or fail silently) after moves/renames.
if(APPLE)
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
target_link_libraries(appui_backing PRIVATE Qt6::Qml Qt6::Quick)

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
        qml/AppUI/ResumeStateDialog.qml
        qml/AppUI/RetroAchievementsSettings.qml
        qml/AppUI/AchievementsPage.qml
        qml/AppUI/AllGamesPage.qml
        qml/AppUI/RecentlyPlayedPage.qml
        qml/AppUI/EmulatorLogos.js
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
        qml/AppUI/images/controllers/Guitar.svg
        qml/AppUI/images/controllers/Jogcon.svg
        qml/AppUI/images/controllers/Negcon.svg
        qml/AppUI/images/controllers/Popn.svg
        qml/AppUI/images/controllers/digital_controller.svg
        qml/AppUI/images/controllers/analog_controller.svg
        qml/AppUI/images/controllers/ds_negcon.svg
        qml/AppUI/images/controllers/analog_joystick.svg
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
    src/adapters/pcsx2_adapter.cpp
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
target_link_libraries(test_format_binding PRIVATE Qt6::Core Qt6::Network Qt6::Test chdr-static)
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
    src/adapters/pcsx2_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/core/sfo_parser.cpp
    src/core/ini_file.cpp
)
target_include_directories(test_rom_scanner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_rom_scanner PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test chdr-static)
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
target_link_libraries(test_dolphin_schema PRIVATE Qt6::Core Qt6::Network Qt6::Test chdr-static)
add_test(NAME DolphinSchema COMMAND test_dolphin_schema)

add_executable(test_pcsx2_preview_spec
    tests/test_pcsx2_preview_spec.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/paths.cpp
)
set_target_properties(test_pcsx2_preview_spec PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_preview_spec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_preview_spec PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME Pcsx2PreviewSpec COMMAND test_pcsx2_preview_spec)

add_executable(test_quick_settings_file_field
    tests/test_quick_settings_file_field.cpp
    src/services/config_service.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/database.cpp
    src/core/manifest_loader.cpp
)
set_target_properties(test_quick_settings_file_field PROPERTIES AUTOMOC ON)
target_include_directories(test_quick_settings_file_field PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_quick_settings_file_field PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test chdr-static)
add_test(NAME QuickSettingsFileField COMMAND test_quick_settings_file_field)

add_executable(test_pcsx2_toggle
    tests/test_pcsx2_toggle.cpp
    src/ui/settings/widgets/settings_toggle.cpp
)
set_target_properties(test_pcsx2_toggle PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_toggle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_toggle PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2Toggle COMMAND test_pcsx2_toggle)

add_executable(test_pcsx2_combo_row
    tests/test_pcsx2_combo_row.cpp
    src/ui/settings/widgets/settings_combo_row.cpp
)
set_target_properties(test_pcsx2_combo_row PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_combo_row PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_combo_row PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2ComboRow COMMAND test_pcsx2_combo_row)

add_executable(test_pcsx2_slider_row
    tests/test_pcsx2_slider_row.cpp
    src/ui/settings/widgets/settings_slider_row.cpp
)
set_target_properties(test_pcsx2_slider_row PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_slider_row PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_slider_row PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2SliderRow COMMAND test_pcsx2_slider_row)

add_executable(test_pcsx2_description_bar
    tests/test_pcsx2_description_bar.cpp
    src/ui/settings/widgets/settings_description_bar.cpp
)
set_target_properties(test_pcsx2_description_bar PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_description_bar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_pcsx2_description_bar PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2DescriptionBar COMMAND test_pcsx2_description_bar)

add_executable(test_aspect_ratio_preview
    tests/test_aspect_ratio_preview.cpp
    src/ui/settings/widgets/preview/aspect_ratio_preview.cpp
)
set_target_properties(test_aspect_ratio_preview PROPERTIES AUTOMOC ON)
target_include_directories(test_aspect_ratio_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_aspect_ratio_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME AspectRatioPreview COMMAND test_aspect_ratio_preview)

add_executable(test_duckstation_aspect_ratio_preview
    tests/test_duckstation_aspect_ratio_preview.cpp
    src/ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.cpp
)
set_target_properties(test_duckstation_aspect_ratio_preview PROPERTIES AUTOMOC ON)
target_include_directories(test_duckstation_aspect_ratio_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_duckstation_aspect_ratio_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME DuckStationAspectRatioPreview COMMAND test_duckstation_aspect_ratio_preview)

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
