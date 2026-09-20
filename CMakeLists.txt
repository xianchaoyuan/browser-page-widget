cmake_minimum_required(VERSION 3.21)

project(BrowserPageWidget VERSION 2.0.0 LANGUAGES CXX)

option(BROWSER_PAGE_WIDGET_BUILD_EXAMPLE "Build the BrowserPageWidget example" ON)
option(BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER "Build the AgentPageViewer program" ON)
set(BROWSER_PAGE_WIDGET_QT_ROOT "" CACHE PATH "Optional Qt 5/6 installation root")

# 版本选择：显式指定优先（build.bat 会传入 5 或 6），否则自动探测 Qt6 -> Qt5。
set(BPW_QT_MAJOR "" CACHE STRING "Major Qt version (5 or 6), empty = auto-detect")
if(NOT BPW_QT_MAJOR)
    find_package(Qt6 QUIET COMPONENTS Widgets WebEngineCore WebEngineWidgets)
    if(Qt6_FOUND)
        set(BPW_QT_MAJOR 6)
    else()
        set(BPW_QT_MAJOR 5)
    endif()
endif()
if(NOT BPW_QT_MAJOR STREQUAL "5" AND NOT BPW_QT_MAJOR STREQUAL "6")
    message(FATAL_ERROR "BPW_QT_MAJOR must be 5 or 6, got: ${BPW_QT_MAJOR}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 输出目录按 Qt 大版本隔离（bin/qt6/…、bin/qt5/…），避免两套构建互相覆盖、
# 以及增量构建误判“产物已最新”导致 Qt 版本错配。
foreach(CONFIG_TYPE debug release relwithdebinfo minsizerel)
    string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR}/${CONFIG_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR}/${CONFIG_TYPE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR}/${CONFIG_TYPE})
endforeach()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/qt${BPW_QT_MAJOR})

if(BROWSER_PAGE_WIDGET_QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${BROWSER_PAGE_WIDGET_QT_ROOT}")
endif()

if(BPW_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS
        Widgets
        WebEngineCore
        WebEngineWidgets
    )
else()
    find_package(Qt6 REQUIRED COMPONENTS
        Widgets
        WebEngineCore
        WebEngineWidgets
    )
endif()

if(BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER)
    find_package(Qt${BPW_QT_MAJOR} REQUIRED COMPONENTS Network)
endif()

# 统一 Qt 依赖别名，屏蔽 Qt5::/Qt6:: 命名差异，库与宿主无需感知版本。
add_library(BrowserPageWidget::QtDeps INTERFACE IMPORTED)
target_link_libraries(BrowserPageWidget::QtDeps INTERFACE
    Qt${BPW_QT_MAJOR}::Widgets
    Qt${BPW_QT_MAJOR}::WebEngineCore
    Qt${BPW_QT_MAJOR}::WebEngineWidgets
)

add_library(browserpagewidget SHARED
    browserpagewidget.h
    browserpagewidget.cpp
    browserpagewidgetglobal.h
    browserqtcompat.h
    browserpagewidget_p.h
    browserpagewidget_p.cpp
    browserpagepolicy.h
    browserpagepolicy.cpp
    browserwebpage_p.h
    browserwebpage_p.cpp
    browserdownloadmanager_p.h
    browserdownloadmanager_p.cpp
)
add_library(bm::browserpagewidget ALIAS browserpagewidget)

target_compile_features(browserpagewidget PUBLIC cxx_std_17)

target_include_directories(browserpagewidget
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/BrowserPageWidget>
)

target_link_libraries(browserpagewidget
    PUBLIC
    BrowserPageWidget::QtDeps
)

target_compile_definitions(browserpagewidget
    PRIVATE
        BROWSERPAGEWIDGET_LIBRARY
)

if(MSVC)
    target_compile_options(browserpagewidget PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(browserpagewidget PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(browserpagewidget PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    DEBUG_POSTFIX d
    OUTPUT_NAME browserpagewidget
)

if(BROWSER_PAGE_WIDGET_BUILD_EXAMPLE)
    add_executable(BrowserPageWidgetExample WIN32 example/main.cpp)
    target_link_libraries(BrowserPageWidgetExample PRIVATE bm::browserpagewidget)

    if(MSVC)
        target_compile_options(BrowserPageWidgetExample PRIVATE /utf-8 /W4 /permissive-)
    endif()
endif()

if(BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER)
    set(AGENT_PAGE_VIEWER_WINDOWS_RESOURCES)
    if(WIN32)
        enable_language(RC)
        list(APPEND AGENT_PAGE_VIEWER_WINDOWS_RESOURCES agentpageviewer/agentpageviewer.rc)
    endif()

    add_executable(AgentPageViewer WIN32
        ${AGENT_PAGE_VIEWER_WINDOWS_RESOURCES}
        agentpageviewer/resources.qrc
        agentpageviewer/main.cpp
        agentpageviewer/agentstartupcontroller.h
        agentpageviewer/agentstartupcontroller.cpp
        agentpageviewer/endpointwaiter.h
        agentpageviewer/endpointwaiter.cpp
        agentpageviewer/processjob.h
        agentpageviewer/processjob.cpp
        agentpageviewer/agentstartupsplash.h
        agentpageviewer/agentstartupsplash.cpp
    )
    target_link_libraries(AgentPageViewer PRIVATE bm::browserpagewidget Qt${BPW_QT_MAJOR}::Network)

    if(MSVC)
        target_compile_options(AgentPageViewer PRIVATE /utf-8 /W4 /permissive-)
    endif()
endif()

configure_package_config_file(
    cmake/BrowserPageWidgetConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/BrowserPageWidgetConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrowserPageWidget
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/BrowserPageWidgetConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS browserpagewidget
    EXPORT browserpagewidgetTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    browserpagewidget.h
    browserpagewidgetglobal.h
    browserqtcompat.h
    browserpagepolicy.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/BrowserPageWidget
)

install(EXPORT browserpagewidgetTargets
    FILE browserpagewidgetTargets.cmake
    NAMESPACE bm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrowserPageWidget
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/BrowserPageWidgetConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/BrowserPageWidgetConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BrowserPageWidget
)

message(STATUS "Qt version: ${Qt${BPW_QT_MAJOR}Core_VERSION}")
if(BROWSER_PAGE_WIDGET_QT_ROOT)
    message(STATUS "Qt root hint: ${BROWSER_PAGE_WIDGET_QT_ROOT}")
endif()
