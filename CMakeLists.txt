cmake_minimum_required(VERSION 3.10)

project(aurextranslator VERSION 0.0.6 LANGUAGES C CXX)

find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_HASH_RESULT
    )
endif()

if(NOT GIT_HASH_RESULT EQUAL 0 OR NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

set(APP_NAME "AurexTranslator")
set(APP_VERSION "${PROJECT_VERSION} (alpha, ${GIT_COMMIT_HASH})")
set(CMAKE_AUTOUIC_SEARCH_PATHS "src/UI/forms")
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

string(TIMESTAMP CURRENT_YEAR "%Y")

configure_file(
    src/data.h.in
    src/data.h
)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets LinguistTools Svg)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(PkgConfig REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 COMPONENTS DBus)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    find_package(X11 REQUIRED)
    pkg_check_modules(LIBPIPIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
    find_program(WAYLAND_SCANNER wayland-scanner)
endif()

pkg_check_modules(TESSERACT REQUIRED tesseract)

set(PROJECT_SOURCES
    src/main.cpp
    src/UI/mainwindow.cpp src/UI/mainwindow.h src/UI/forms/mainwindow.ui
    src/UI/textprocessingdelegates.h
    src/UI/screencastwindow.h src/UI/screencastwindow.cpp src/UI/forms/screencastwindow.ui
    src/UI/textoutputwindow.h src/UI/textoutputwindow.cpp src/UI/forms/textoutputwindow.ui
    src/UI/hooktextmodel.h src/UI/hooktextmodel.cpp
    src/UI/hookselectordialog.h src/UI/hookselectordialog.cpp
    src/UI/overlaywindow.h src/UI/overlaywindow.cpp
    src/UI/previewwindow.h src/UI/previewwindow.cpp
    src/UI/hoverlabel.h src/UI/hoverlabel.cpp
    src/UI/ollamasettingsdialog.h src/UI/ollamasettingsdialog.cpp
    src/UI/tesseractsettingsdialog.h src/UI/tesseractsettingsdialog.cpp
    src/UI/googlesettingsdialog.h src/UI/googlesettingsdialog.cpp
    src/UI/hooksettingsdialog.h src/UI/hooksettingsdialog.cpp

    src/controllers/translationcontroller.h src/controllers/translationcontroller.cpp
    src/controllers/hotkeycontroller.h src/controllers/hotkeycontroller.cpp
    src/controllers/capturecontroller.h src/controllers/capturecontroller.cpp
    src/controllers/ocrcontroller.h src/controllers/ocrcontroller.cpp
    src/controllers/hookcontroller.h src/controllers/hookcontroller.cpp
    src/controllers/clipboardcontroller.h src/controllers/clipboardcontroller.cpp

    src/engines/opencv.cpp src/engines/opencv.h
    src/engines/tesseractocr.cpp src/engines/tesseractocr.h
    src/engines/ollama.cpp src/engines/ollama.h
    src/engines/clipboardqt.h src/engines/clipboardqt.cpp

    src/utils/config.cpp src/utils/config.h
    src/utils/logger.cpp src/utils/logger.h
    src/utils/hotkeys.cpp src/utils/hotkeys.h
    src/utils/plugininterface.h
    src/utils/pluginloader.cpp src/utils/pluginloader.h
    src/utils/metadatareader.cpp src/utils/metadatareader.h
    src/utils/dialogutils.h

    src/translations/google.cpp src/translations/google.h
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PROJECT_SOURCES
        src/screencasts/linux-capture-portal/screencast-pipewire.cpp src/screencasts/linux-capture-portal/screencast-pipewire.h
        src/screencasts/linux-capture-portal/screencast-portal.cpp src/screencasts/linux-capture-portal/screencast-portal.h
        src/screencasts/linux-capture-x11/screencast-x11.cpp src/screencasts/linux-capture-x11/screencast-x11.h
        src/utils/portal_hotkeys.h src/utils/portal_hotkeys.cpp
        src/engines/clipboardportal.h src/engines/clipboardportal.cpp
    )

    qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.ScreenCast.xml screencast_portal_interface)
    qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.GlobalShortcuts.xml globalshortcuts_portal_interface)
    qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.RemoteDesktop.xml remotedesktop_portal_interface)
    qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.Clipboard.xml clipboard_portal_interface)

    # Wayland clipboard: generate C bindings from bundled protocol XML
    if(WAYLAND_SCANNER AND WAYLAND_CLIENT_FOUND)
        # ext-data-control-v1
        set(WL_EXT_XML    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/wayland-protocols/ext-data-control-v1.xml)
        set(WL_EXT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ext-data-control-v1-client-protocol.h)
        set(WL_EXT_CODE   ${CMAKE_CURRENT_BINARY_DIR}/ext-data-control-v1-client-protocol.c)

        add_custom_command(
            OUTPUT ${WL_EXT_HEADER}
            COMMAND ${WAYLAND_SCANNER} client-header ${WL_EXT_XML} ${WL_EXT_HEADER}
            DEPENDS ${WL_EXT_XML}
            COMMENT "Generating ext-data-control client header"
        )
        add_custom_command(
            OUTPUT ${WL_EXT_CODE}
            COMMAND ${WAYLAND_SCANNER} private-code ${WL_EXT_XML} ${WL_EXT_CODE}
            DEPENDS ${WL_EXT_XML}
            COMMENT "Generating ext-data-control client code"
        )

        # wlr-data-control-unstable-v1
        set(WL_ZWL_XML    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/wayland-protocols/wlr-data-control-unstable-v1.xml)
        set(WL_ZWL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-data-control-unstable-v1-client-protocol.h)
        set(WL_ZWL_CODE   ${CMAKE_CURRENT_BINARY_DIR}/wlr-data-control-unstable-v1-client-protocol.c)

        add_custom_command(
            OUTPUT ${WL_ZWL_HEADER}
            COMMAND ${WAYLAND_SCANNER} client-header ${WL_ZWL_XML} ${WL_ZWL_HEADER}
            DEPENDS ${WL_ZWL_XML}
            COMMENT "Generating wlr-data-control client header"
        )
        add_custom_command(
            OUTPUT ${WL_ZWL_CODE}
            COMMAND ${WAYLAND_SCANNER} private-code ${WL_ZWL_XML} ${WL_ZWL_CODE}
            DEPENDS ${WL_ZWL_XML}
            COMMENT "Generating wlr-data-control client code"
        )

        add_library(wlr_data_control_protocol STATIC
            ${WL_EXT_CODE}   ${WL_EXT_HEADER}
            ${WL_ZWL_CODE}   ${WL_ZWL_HEADER}
        )
        set_target_properties(wlr_data_control_protocol PROPERTIES LINKER_LANGUAGE C)
        target_include_directories(wlr_data_control_protocol PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}
            ${WAYLAND_CLIENT_INCLUDE_DIRS}
        )
        target_link_libraries(wlr_data_control_protocol PUBLIC ${WAYLAND_CLIENT_LIBRARIES})

        list(APPEND PROJECT_SOURCES
            src/engines/clipboardwayland.h src/engines/clipboardwayland.cpp
        )

        add_compile_definitions(AT_WAYLAND_DATA_CONTROL)
    else()
        message(WARNING "wayland-scanner or wayland-client not found – WaylandClipboardController disabled")
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND PROJECT_SOURCES
        src/screencasts/win-capture/screencast-win.h src/screencasts/win-capture/screencast-win.cpp
    )
endif()

add_subdirectory(3rdparty/QHotkey)

qt_add_resources(RESOURCES resources.qrc)

qt_add_executable(${PROJECT_NAME}
  WIN32
    ${PROJECT_SOURCES}
    ${RESOURCES}
)

qt_add_translations(${CMAKE_PROJECT_NAME}
  TS_FILES
    translations/${PROJECT_NAME}_en_US.ts
    translations/${PROJECT_NAME}_ru_RU.ts
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_lupdate)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} src/UI)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX_LIBS
        Qt::DBus
        X11::X11
        X11::Xcomposite
        X11::Xfixes
        X11::Xrandr
        ${LIBPIPIPEWIRE_LIBRARIES}
)
    if(WAYLAND_CLIENT_FOUND AND WAYLAND_SCANNER)
        list(APPEND LINUX_LIBS wlr_data_control_protocol)
    endif()
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
        Qt::Svg

        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        QHotkey

        ${TESSERACT_LIBRARIES}
        ${LINUX_LIBS}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE version)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${LIBPIPIPEWIRE_INCLUDE_DIRS}
        ${OPENCV_INCLUDE_DIRS}
        ${TESSERACT_INCLUDE_DIRS}
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)
