cmake_minimum_required(VERSION 3.21)

project(BrowserPageWidget VERSION 2.0.0 LANGUAGES CXX)

option(BROWSER_PAGE_WIDGET_BUILD_EXAMPLE "Build the BrowserPageWidget example" ON)
option(BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER "Build the AgentPageViewer program" ON)
set(BROWSER_PAGE_WIDGET_QT_ROOT "" CACHE PATH "Optional Qt 6 installation root")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

foreach(CONFIG_TYPE debug release relwithdebinfo minsizerel)
    string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/${CONFIG_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/${CONFIG_TYPE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${PROJECT_SOURCE_DIR}/bin/${CONFIG_TYPE})
endforeach()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if(BROWSER_PAGE_WIDGET_QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${BROWSER_PAGE_WIDGET_QT_ROOT}")
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    WebEngineCore
    WebEngineWidgets
)

if(BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER)
    find_package(Qt6 REQUIRED COMPONENTS Network)
endif()

add_library(browserpagewidget SHARED
    browserpagewidget.h
    browserpagewidget.cpp
    browserpagewidgetglobal.h
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
    Qt6::Widgets
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
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
    add_executable(AgentPageViewer WIN32
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
    target_link_libraries(AgentPageViewer PRIVATE bm::browserpagewidget Qt6::Network)

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

message(STATUS "Qt version: ${Qt6Core_VERSION}")
if(BROWSER_PAGE_WIDGET_QT_ROOT)
    message(STATUS "Qt root hint: ${BROWSER_PAGE_WIDGET_QT_ROOT}")
endif()
