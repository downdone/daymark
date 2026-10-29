cmake_minimum_required(VERSION 3.24)
project(Daymark VERSION 1.1.0 LANGUAGES CXX)
set(DAYMARK_VERSION "1.1.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# All C++ test/platform sources contain UTF-8, including targets outside daymark_core.
add_compile_options($<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)

find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Sql Network)
qt_standard_project_setup(REQUIRES 6.10)

set(DAYMARK_STYLE_FILES
    src/ui/style/Label.qml src/ui/style/Glyph.qml src/ui/style/IconPaths.qml
    src/ui/style/ScrollView.qml src/ui/style/ScrollPhysics.qml src/ui/style/FieldSurface.qml
    src/ui/style/DesignTokens.qml src/ui/style/Button.qml src/ui/style/ToolButton.qml
    src/ui/style/TextField.qml src/ui/style/TextArea.qml src/ui/style/CheckBox.qml
    src/ui/style/Dialog.qml src/ui/style/DialogButtonBox.qml
    src/ui/style/ComboBox.qml src/ui/style/Menu.qml src/ui/style/MenuItem.qml src/ui/style/MenuSeparator.qml
    src/ui/style/Slider.qml src/ui/style/SpinBox.qml src/ui/style/ScrollBar.qml
    src/ui/style/ItemDelegate.qml src/ui/style/ToolTip.qml
)
foreach(style_file IN LISTS DAYMARK_STYLE_FILES)
    get_filename_component(style_name "${style_file}" NAME)
    set_source_files_properties("${style_file}" PROPERTIES QT_RESOURCE_ALIAS "${style_name}")
endforeach()
set_source_files_properties(src/ui/style/DesignTokens.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(src/ui/style/IconPaths.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(daymark_style STATIC URI DaymarkStyle VERSION 6.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/DaymarkStyle"
    RESOURCE_PREFIX /qt/qml IMPORTS QtQuick.Controls.Basic/auto QML_FILES ${DAYMARK_STYLE_FILES})
target_link_libraries(daymark_style PUBLIC Qt6::Quick Qt6::QuickControls2)

add_library(daymark_core STATIC
    src/domain/CalendarEvent.h src/persistence/EventRepository.h src/persistence/EventRepository.cpp src/services/EventService.h src/services/EventService.cpp
    src/platform/common/AccessibilityPreferences.h src/platform/common/AccessibilityPreferences.cpp
    src/domain/Task.h
    src/persistence/SqliteDatabase.h src/persistence/SqliteDatabase.cpp
    src/persistence/TaskRepository.h src/persistence/TaskRepository.cpp src/persistence/TaskComponents.cpp src/persistence/UnifiedTaskMigration.h src/persistence/UnifiedTaskMigration.cpp
    src/services/TaskTime.cpp src/services/TaskService.h src/services/TaskService.cpp src/services/TaskTreeCommands.cpp
    src/services/ReminderSchedule.h src/services/ReminderSchedule.cpp
    src/persistence/CatalogRepository.h src/persistence/CatalogRepository.cpp
    src/services/CatalogService.h src/services/CatalogService.cpp
    src/services/CalendarService.h src/services/CalendarService.cpp
    src/persistence/RecurrenceRepository.h src/persistence/RecurrenceRepository.cpp
    src/services/RecurrenceService.h src/services/RecurrenceService.cpp
    src/persistence/DataTransfer.h src/persistence/DataTransfer.cpp
)
target_include_directories(daymark_core PUBLIC src)
target_link_libraries(daymark_core PUBLIC Qt6::Core Qt6::Sql)
qt_add_resources(daymark_core daymark_schema PREFIX "/daymark" FILES src/persistence/migrations/001_init.sql src/persistence/migrations/002_area_archive.sql src/persistence/migrations/003_calendar_events.sql src/persistence/migrations/004_event_series.sql
        src/persistence/migrations/005_categories.sql
        src/persistence/migrations/006_task_relations.sql
        src/persistence/migrations/007_recurring_reminders.sql
        src/persistence/migrations/008_unified_tasks.sql
        src/persistence/migrations/009_task_tree_actions.sql)
if(MSVC)
    target_compile_options(daymark_core PUBLIC /utf-8)
endif()

set(DAYMARK_WINDOW_SOURCES
    src/platform/IWindowPlatform.h
    src/models/WidgetViewModel.h src/models/WidgetViewModel.cpp
)

set(DAYMARK_WORKSPACE_SOURCES
    src/models/TaskWorkspace.h src/models/TaskWorkspace.cpp src/models/TaskWorkspaceCalendar.cpp src/models/TaskWorkspaceWindow.cpp
)

qt_add_executable(daymark src/app/Ux35SmokeProbe.h src/app/Ux35SmokeProbe.cpp src/app/Ux34SmokeProbe.h src/app/Ux34SmokeProbe.cpp src/platform/common/ApplicationMenus.h src/platform/common/ApplicationMenus.cpp src/main.cpp src/app/AppRuntime.h src/app/AppRuntime.cpp src/app/SoakRunner.h src/app/SoakRunner.cpp)
qt_add_resources(daymark daymark_brand PREFIX "/daymark/icons" BASE assets/icons FILES
    assets/icons/daymark-16.png assets/icons/daymark-32.png assets/icons/daymark-48.png assets/icons/daymark-256.png)
if(WIN32)
    target_link_libraries(daymark_core PRIVATE user32)
    enable_language(RC)
    configure_file(src/platform/windows/daymark.rc.in "${CMAKE_CURRENT_BINARY_DIR}/daymark.rc" @ONLY)
    target_sources(daymark PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/daymark.rc")
elseif(APPLE)
    enable_language(OBJCXX)
    set_target_properties(daymark PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    find_library(DAYMARK_APPKIT_FRAMEWORK AppKit REQUIRED)
    set_source_files_properties(src/platform/common/AccessibilityPreferences.cpp PROPERTIES LANGUAGE OBJCXX)
    set_target_properties(daymark_core PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_link_libraries(daymark_core PRIVATE ${DAYMARK_APPKIT_FRAMEWORK})
    target_sources(daymark PRIVATE src/platform/macos/SmokeActivation.h src/platform/macos/SmokeActivation.mm)
    target_link_libraries(daymark PRIVATE ${DAYMARK_APPKIT_FRAMEWORK})
    target_sources(daymark PRIVATE assets/icons/daymark.icns)
    set_source_files_properties(assets/icons/daymark.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(daymark PROPERTIES MACOSX_BUNDLE_ICON_FILE daymark.icns)
endif()
# Shared UI platform port, linked by the app and isolated integration tests.
set(DAYMARK_NATIVE_LAYER src/platform/common/NativeWindowLayerStub.cpp)
if(APPLE)
    set(DAYMARK_NATIVE_LAYER src/platform/macos/NativeWindowLayer.mm)
elseif(WIN32)
    set(DAYMARK_NATIVE_LAYER src/platform/windows/NativeWindowLayer.cpp)
endif()
qt_add_library(daymark_window_platform STATIC src/platform/common/QtWindowPlatform.h src/platform/common/QtWindowPlatform.cpp src/platform/common/NativeWindowLayer.h ${DAYMARK_NATIVE_LAYER})
target_include_directories(daymark_window_platform PUBLIC src)
target_link_libraries(daymark_window_platform PUBLIC Qt6::Core Qt6::Gui)
if(APPLE)
    find_library(DAYMARK_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    set_target_properties(daymark_window_platform PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_link_libraries(daymark_window_platform PRIVATE ${DAYMARK_APPKIT_FRAMEWORK} ${DAYMARK_COREGRAPHICS_FRAMEWORK})
elseif(WIN32)
    target_link_libraries(daymark_window_platform PRIVATE user32)
endif()
set_source_files_properties(src/ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
target_sources(daymark PRIVATE src/platform/common/DesktopTray.h src/platform/common/DesktopTray.cpp)
target_sources(daymark PRIVATE src/platform/common/WindowStateStore.h src/platform/common/WindowStateStore.cpp)
target_sources(daymark PRIVATE src/platform/common/SystemEventFilter.h src/platform/common/SystemEventFilter.cpp)
qt_add_qml_module(daymark
    URI Daymark
    VERSION 1.0
    QML_FILES src/ui/AccountWindow.qml src/ui/TaskSelection.qml src/ui/BrandWordmark.qml src/ui/SettingsAboutPage.qml src/ui/TaskRowSummary.qml src/ui/TagMenu.qml src/ui/WindowResizeHandles.qml src/ui/TimeRangePicker.qml src/ui/PopoverSurface.qml
    QML_FILES src/ui/FormRow.qml src/ui/CatalogColors.qml src/ui/InlineClock.qml src/ui/CatalogPicker.qml src/ui/CatalogEditPopup.qml src/ui/CatalogCreateDialog.qml src/ui/DiscardChangesDialog.qml src/ui/CalendarPicker.qml src/ui/TimeField.qml src/ui/ReminderField.qml src/ui/TaskQuickEditor.qml src/ui/SettingSwitch.qml src/ui/DetailSection.qml src/ui/IconToolButton.qml src/ui/ActionButton.qml src/ui/AppIcon.qml src/ui/ActionBar.qml src/ui/SectionLabel.qml src/ui/StyleGallery.qml src/ui/WidgetAction.qml src/ui/WidgetTaskRow.qml src/ui/DateField.qml src/ui/FormGroup.qml src/ui/SegmentedPicker.qml src/ui/EmptyState.qml src/ui/SettingsDialog.qml src/ui/SettingsGroup.qml src/ui/SettingsRow.qml src/ui/SettingsToggle.qml src/ui/SettingsLink.qml src/ui/InspectorTabs.qml src/ui/ReminderPage.qml src/ui/DesktopCalendarWindow.qml
    QML_FILES src/ui/CalendarMetrics.qml src/ui/CalendarMonthStream.qml src/ui/CalendarMonthGrid.qml src/ui/ListViewportState.qml src/ui/CalendarDateMoveDialog.qml src/ui/CalendarCreatePopup.qml src/ui/UnscheduledTasksPanel.qml src/ui/TaskDragGesture.qml src/ui/TaskDragPayload.qml src/ui/TaskInspectorPicker.qml src/ui/CalendarTimeline.qml src/ui/YearCalendar.qml src/ui/TaskEditorSession.qml src/ui/Main.qml src/ui/WidgetWindow.qml src/ui/OrganizationPage.qml src/ui/TaskDetailsFields.qml src/ui/TaskTimingFields.qml src/ui/CalendarPanel.qml src/ui/DataToolsDialog.qml src/ui/Theme.qml src/ui/NavSection.qml src/ui/NavRow.qml src/ui/SmartViewTile.qml src/ui/TaskRow.qml src/ui/DatePickerDialog.qml src/ui/BatchTaskDialog.qml
    SOURCES ${DAYMARK_WINDOW_SOURCES} ${DAYMARK_WORKSPACE_SOURCES} src/platform/common/StartupController.h src/platform/common/StartupController.cpp
        src/platform/common/SingleInstance.h src/platform/common/SingleInstance.cpp
)
target_include_directories(daymark PRIVATE src src/models src/platform/common)
target_sources(daymark PRIVATE src/app/Ux37SmokeProbe.h src/app/Ux37SmokeProbe.cpp src/app/Ux38SmokeProbe.h src/app/Ux38SmokeProbe.cpp src/app/Ux39SmokeProbe.h src/app/Ux39SmokeProbe.cpp)
target_link_libraries(daymark PRIVATE daymark_window_platform daymark_core daymark_styleplugin Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network)
target_compile_definitions(daymark PRIVATE DAYMARK_VERSION="${DAYMARK_VERSION}")
set_target_properties(daymark PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${DAYMARK_VERSION}")
if(MSVC)
    target_compile_options(daymark PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(daymark PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Test)
    if(WIN32)
        find_program(DAYMARK_POWERSHELL NAMES pwsh REQUIRED)
        add_test(NAME package_validation COMMAND "${DAYMARK_POWERSHELL}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/PackageValidationTest.ps1")
        set_tests_properties(package_validation PROPERTIES TIMEOUT 30)
    endif()
    qt_add_executable(test_integrity tests/integration/IntegrityTest.cpp)
    target_link_libraries(test_integrity PRIVATE daymark_core Qt6::Test)
    add_test(NAME storage_integrity COMMAND test_integrity)
    qt_add_executable(test_system_recovery tests/integration/SystemRecoveryTest.cpp src/platform/common/SystemEventFilter.cpp src/platform/common/WindowStateStore.h src/platform/common/WindowStateStore.cpp)
    target_include_directories(test_system_recovery PRIVATE src)
    target_link_libraries(test_system_recovery PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME system_recovery COMMAND test_system_recovery)
    set_tests_properties(system_recovery PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
    qt_add_executable(test_startup tests/integration/StartupTest.cpp src/platform/common/StartupController.h src/platform/common/StartupController.cpp)
    target_include_directories(test_startup PRIVATE src)
    target_link_libraries(test_startup PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
    add_test(NAME startup_settings COMMAND test_startup)
    qt_add_executable(test_sqlite_runtime tests/integration/SqliteRuntimeTest.cpp)
    target_link_libraries(test_sqlite_runtime PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
    add_test(NAME sqlite_runtime COMMAND test_sqlite_runtime)
    qt_add_executable(test_task_service tests/services/TaskServiceTest.cpp)
    target_link_libraries(test_task_service PRIVATE daymark_core Qt6::Test)
    add_test(NAME task_service COMMAND test_task_service)
    qt_add_executable(test_catalog_service tests/services/CatalogServiceTest.cpp)
    target_link_libraries(test_catalog_service PRIVATE daymark_core Qt6::Test)
    add_test(NAME catalog_service COMMAND test_catalog_service)
    qt_add_executable(test_calendar_service tests/services/CalendarServiceTest.cpp)
    target_link_libraries(test_calendar_service PRIVATE daymark_core Qt6::Test)
    add_test(NAME calendar_service COMMAND test_calendar_service)
    qt_add_executable(test_unified_tasks tests/services/UnifiedTaskTest.cpp)
    target_link_libraries(test_unified_tasks PRIVATE daymark_core Qt6::Test)
    add_test(NAME unified_tasks COMMAND test_unified_tasks)
    qt_add_executable(test_event_service tests/services/EventServiceTest.cpp)
    target_link_libraries(test_event_service PRIVATE daymark_core Qt6::Test)
    add_test(NAME event_service COMMAND test_event_service)
    qt_add_executable(test_recurrence_service tests/services/RecurrenceServiceTest.cpp)
    target_link_libraries(test_recurrence_service PRIVATE daymark_core Qt6::Test)
    add_test(NAME recurrence_service COMMAND test_recurrence_service)
    qt_add_executable(test_reminders tests/services/ReminderTest.cpp)
    target_link_libraries(test_reminders PRIVATE daymark_core Qt6::Test)
    add_test(NAME reminders COMMAND test_reminders)
    qt_add_executable(test_data_transfer tests/services/DataTransferTest.cpp)
    target_link_libraries(test_data_transfer PRIVATE daymark_core Qt6::Test)
    add_test(NAME data_transfer COMMAND test_data_transfer)
    qt_add_executable(test_performance tests/services/PerformanceTest.cpp)
    target_link_libraries(test_performance PRIVATE daymark_core Qt6::Test)
    add_test(NAME performance COMMAND test_performance)
    set_tests_properties(task_service PROPERTIES TIMEOUT 30)
    qt_add_executable(test_ux34_workspace tests/integration/Ux34WorkspaceTest.cpp ${DAYMARK_WORKSPACE_SOURCES})
    target_include_directories(test_ux34_workspace PRIVATE src)
    target_link_libraries(test_ux34_workspace PRIVATE daymark_window_platform daymark_core Qt6::Qml Qt6::Gui Qt6::Widgets Qt6::Test)
    add_test(NAME ux34_workspace COMMAND test_ux34_workspace)
    set_tests_properties(ux34_workspace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    qt_add_executable(test_task_workspace tests/integration/TaskWorkspaceTest.cpp ${DAYMARK_WORKSPACE_SOURCES})
    target_include_directories(test_task_workspace PRIVATE src)
    target_link_libraries(test_task_workspace PRIVATE daymark_window_platform daymark_core Qt6::Qml Qt6::Gui Qt6::Test)
    add_test(NAME task_workspace COMMAND test_task_workspace)
    set_tests_properties(task_workspace PROPERTIES TIMEOUT 40)
    qt_add_executable(test_single_instance tests/integration/SingleInstanceTest.cpp src/platform/common/SingleInstance.h src/platform/common/SingleInstance.cpp)
    target_include_directories(test_single_instance PRIVATE src)
    target_link_libraries(test_single_instance PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME single_instance COMMAND test_single_instance)
    set_tests_properties(single_instance PROPERTIES TIMEOUT 20)
    qt_add_executable(test_widget_window tests/integration/WidgetWindowTest.cpp src/platform/common/DesktopTray.cpp src/platform/common/DesktopTray.h ${DAYMARK_WINDOW_SOURCES})
    target_include_directories(test_widget_window PRIVATE src)
    target_link_libraries(test_widget_window PRIVATE daymark_window_platform Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Test)
    add_test(NAME widget_window COMMAND test_widget_window)
    set_tests_properties(widget_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
    add_test(NAME qml_startup COMMAND daymark --smoke-test)
    add_test(NAME contextual_workspace COMMAND daymark --contextual-workspace-smoke)
    set_tests_properties(contextual_workspace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    add_test(NAME continuous_calendar COMMAND daymark --continuous-calendar-smoke)
    add_test(NAME calendar_interaction COMMAND daymark --calendar-interaction-smoke)
    set_tests_properties(calendar_interaction PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    add_test(NAME desktop_surfaces COMMAND daymark --desktop-surfaces-smoke)
    add_test(NAME account_access COMMAND daymark --desktop-surfaces-smoke)
    set_tests_properties(account_access PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DAYMARK_SMOKE_UI_SLICE=registration" TIMEOUT 30)
    set_tests_properties(desktop_surfaces PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    add_test(NAME task_list_interaction COMMAND daymark --task-list-smoke)
    set_tests_properties(task_list_interaction PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(continuous_calendar PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    add_test(NAME qml_watchdog_late COMMAND ${CMAKE_COMMAND} -DDAYMARK_BINARY=$<TARGET_FILE:daymark> -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-smoke-watchdog.cmake)
    set_tests_properties(qml_watchdog_late PROPERTIES TIMEOUT 160)
    set_tests_properties(qml_startup PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    if(WIN32)
        set_property(TEST qml_startup APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
        set_property(TEST contextual_workspace continuous_calendar task_list_interaction desktop_surfaces calendar_interaction account_access APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
    endif()
    # One owner per risk layer; full CTest remains the release default.
    set_tests_properties(task_service catalog_service calendar_service unified_tasks event_service recurrence_service reminders data_transfer PROPERTIES LABELS "core")
    set_tests_properties(storage_integrity sqlite_runtime PROPERTIES LABELS "storage")
    set_tests_properties(system_recovery startup_settings ux34_workspace task_workspace single_instance widget_window PROPERTIES LABELS "integration")
    set_tests_properties(performance PROPERTIES LABELS "performance")
    set_tests_properties(qml_startup contextual_workspace continuous_calendar task_list_interaction desktop_surfaces calendar_interaction account_access PROPERTIES LABELS "ui" RESOURCE_LOCK daymark_ui)
    set_tests_properties(qml_watchdog_late PROPERTIES LABELS "watchdog" RESOURCE_LOCK daymark_ui)
    if(WIN32)
        set_tests_properties(package_validation PROPERTIES LABELS "package")
    endif()
    set_tests_properties(sqlite_runtime PROPERTIES TIMEOUT 20)
endif()
