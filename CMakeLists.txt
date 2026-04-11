cmake_minimum_required(VERSION 3.10)

project(aurextranslator VERSION 0.0.4 LANGUAGES CXX)

set(APP_NAME "AurexTranslator")
set(APP_VERSION "${PROJECT_VERSION} (alpha)")
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
endif()

pkg_check_modules(TESSERACT REQUIRED tesseract)

set(PROJECT_SOURCES
    src/main.cpp
    src/UI/mainwindow.cpp src/UI/mainwindow.h src/UI/forms/mainwindow.ui
    src/UI/screencastwindow.h src/UI/screencastwindow.cpp src/UI/forms/screencastwindow.ui
    src/UI/textoutputwindow.h src/UI/textoutputwindow.cpp src/UI/forms/textoutputwindow.ui
    src/UI/overlaywindow.h src/UI/overlaywindow.cpp
    src/UI/previewwindow.h src/UI/previewwindow.cpp
    src/UI/hoverlabel.h src/UI/hoverlabel.cpp
    src/UI/ollamasettingsdialog.h src/UI/ollamasettingsdialog.cpp
    src/UI/tesseractsettingsdialog.h src/UI/tesseractsettingsdialog.cpp
    src/UI/googlesettingsdialog.h src/UI/googlesettingsdialog.cpp
    src/UI/hooksettingsdialog.h src/UI/hooksettingsdialog.cpp

    src/utils/opencv.cpp src/utils/opencv.h
    src/utils/tesseractocr.cpp src/utils/tesseractocr.h
    src/utils/config.cpp src/utils/config.h
    src/utils/logger.cpp src/utils/logger.h
    src/utils/hotkeys.cpp src/utils/hotkeys.h
    src/utils/ollama.cpp src/utils/ollama.h
    src/utils/plugininterface.h
    src/utils/pluginloader.cpp src/utils/pluginloader.h
    src/utils/metadatareader.cpp src/utils/metadatareader.h
    src/translations/google.cpp src/translations/google.h
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PROJECT_SOURCES
        src/screencasts/linux-capture-portal/screencast-pipewire.cpp src/screencasts/linux-capture-portal/screencast-pipewire.h
        src/screencasts/linux-capture-portal/screencast-portal.cpp src/screencasts/linux-capture-portal/screencast-portal.h
        src/screencasts/linux-capture-x11/screencast-x11.cpp src/screencasts/linux-capture-x11/screencast-x11.h
        src/utils/portal_hotkeys.h src/utils/portal_hotkeys.cpp
    )

qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.ScreenCast.xml screencast_portal_interface)
qt_add_dbus_interface(PROJECT_SOURCES 3rdparty/org.freedesktop.portal/org.freedesktop.portal.GlobalShortcuts.xml globalshortcuts_portal_interface)

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
