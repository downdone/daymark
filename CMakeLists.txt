cmake_minimum_required(VERSION 3.24)
project(Daymark VERSION 1.0.0 LANGUAGES CXX)
set(DAYMARK_VERSION "1.0.0-rc.2")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)

find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Sql Network)
qt_standard_project_setup(REQUIRES 6.10)

set(DAYMARK_STYLE_FILES
    src/ui/style/DesignTokens.qml src/ui/style/Button.qml src/ui/style/ToolButton.qml
    src/ui/style/TextField.qml src/ui/style/TextArea.qml src/ui/style/CheckBox.qml
    src/ui/style/Dialog.qml src/ui/style/DialogButtonBox.qml
    src/ui/style/ComboBox.qml src/ui/style/Menu.qml src/ui/style/MenuItem.qml src/ui/style/MenuSeparator.qml
    src/ui/style/Slider.qml src/ui/style/SpinBox.qml src/ui/style/ScrollBar.qml
    src/ui/style/ItemDelegate.qml
)
foreach(style_file IN LISTS DAYMARK_STYLE_FILES)
    get_filename_component(style_name "${style_file}" NAME)
    set_source_files_properties("${style_file}" PROPERTIES QT_RESOURCE_ALIAS "${style_name}")
endforeach()
set_source_files_properties(src/ui/style/DesignTokens.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(daymark_style STATIC URI DaymarkStyle VERSION 6.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/DaymarkStyle"
    RESOURCE_PREFIX /qt/qml IMPORTS QtQuick.Controls.Basic/auto QML_FILES ${DAYMARK_STYLE_FILES})
target_link_libraries(daymark_style PUBLIC Qt6::Quick Qt6::QuickControls2)

add_library(daymark_core STATIC
    src/domain/CalendarEvent.h src/persistence/EventRepository.h src/persistence/EventRepository.cpp src/services/EventService.h src/services/EventService.cpp
    src/domain/Task.h
    src/persistence/SqliteDatabase.h src/persistence/SqliteDatabase.cpp
    src/persistence/TaskRepository.h src/persistence/TaskRepository.cpp
    src/services/TaskService.h src/services/TaskService.cpp
    src/persistence/CatalogRepository.h src/persistence/CatalogRepository.cpp
    src/services/CatalogService.h src/services/CatalogService.cpp
    src/services/CalendarService.h src/services/CalendarService.cpp
    src/persistence/RecurrenceRepository.h src/persistence/RecurrenceRepository.cpp
    src/services/RecurrenceService.h src/services/RecurrenceService.cpp
    src/persistence/DataTransfer.h src/persistence/DataTransfer.cpp
)
target_include_directories(daymark_core PUBLIC src)
target_link_libraries(daymark_core PUBLIC Qt6::Core Qt6::Sql)
qt_add_resources(daymark_core daymark_schema PREFIX "/daymark" FILES src/persistence/migrations/001_init.sql src/persistence/migrations/002_area_archive.sql src/persistence/migrations/003_calendar_events.sql)
if(MSVC)
    target_compile_options(daymark_core PUBLIC /utf-8)
endif()

set(DAYMARK_WINDOW_SOURCES
    src/platform/IWindowPlatform.h
    src/platform/common/QtWindowPlatform.h src/platform/common/QtWindowPlatform.cpp
    src/models/WidgetViewModel.h src/models/WidgetViewModel.cpp
)

qt_add_executable(daymark src/main.cpp src/app/AppRuntime.h src/app/AppRuntime.cpp)
set_source_files_properties(src/ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
target_sources(daymark PRIVATE src/platform/common/DesktopTray.h src/platform/common/DesktopTray.cpp)
target_sources(daymark PRIVATE src/platform/common/WindowStateStore.h src/platform/common/WindowStateStore.cpp)
target_sources(daymark PRIVATE src/platform/common/SystemEventFilter.h src/platform/common/SystemEventFilter.cpp)
qt_add_qml_module(daymark
    URI Daymark
    VERSION 1.0
    QML_FILES src/ui/AppIcon.qml src/ui/ActionBar.qml src/ui/SectionLabel.qml src/ui/StyleGallery.qml src/ui/WidgetAction.qml src/ui/EventDialog.qml src/ui/DateField.qml
    QML_FILES src/ui/Main.qml src/ui/WidgetWindow.qml src/ui/OrganizationDialog.qml src/ui/TaskDetailsDialog.qml src/ui/CalendarPanel.qml src/ui/RecurrenceDialog.qml src/ui/DataToolsDialog.qml src/ui/Theme.qml src/ui/NavRow.qml src/ui/TaskRow.qml src/ui/DatePickerDialog.qml src/ui/BatchTaskDialog.qml
    SOURCES ${DAYMARK_WINDOW_SOURCES} src/models/TaskWorkspace.h src/models/TaskWorkspace.cpp src/platform/common/StartupController.h src/platform/common/StartupController.cpp
        src/platform/common/SingleInstance.h src/platform/common/SingleInstance.cpp
)
target_include_directories(daymark PRIVATE src src/models src/platform/common)
target_link_libraries(daymark PRIVATE daymark_core daymark_styleplugin Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network)
target_compile_definitions(daymark PRIVATE DAYMARK_VERSION="${DAYMARK_VERSION}")
set_target_properties(daymark PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)
if(MSVC)
    target_compile_options(daymark PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(daymark PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Test)
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
    qt_add_executable(test_task_workspace tests/integration/TaskWorkspaceTest.cpp src/models/TaskWorkspace.h src/models/TaskWorkspace.cpp)
    target_include_directories(test_task_workspace PRIVATE src)
    target_link_libraries(test_task_workspace PRIVATE daymark_core Qt6::Qml Qt6::Test)
    add_test(NAME task_workspace COMMAND test_task_workspace)
    set_tests_properties(task_workspace PROPERTIES TIMEOUT 40)
    qt_add_executable(test_single_instance tests/integration/SingleInstanceTest.cpp src/platform/common/SingleInstance.h src/platform/common/SingleInstance.cpp)
    target_include_directories(test_single_instance PRIVATE src)
    target_link_libraries(test_single_instance PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME single_instance COMMAND test_single_instance)
    set_tests_properties(single_instance PROPERTIES TIMEOUT 20)
    qt_add_executable(test_widget_window tests/integration/WidgetWindowTest.cpp ${DAYMARK_WINDOW_SOURCES})
    target_include_directories(test_widget_window PRIVATE src)
    target_link_libraries(test_widget_window PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    add_test(NAME widget_window COMMAND test_widget_window)
    set_tests_properties(widget_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
    add_test(NAME qml_startup COMMAND daymark --smoke-test)
    set_tests_properties(qml_startup PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    if(WIN32)
        set_property(TEST qml_startup APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
    endif()
    set_tests_properties(sqlite_runtime PROPERTIES TIMEOUT 20)
endif()
