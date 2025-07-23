cmake_minimum_required(VERSION 3.19)

set(QHOTKEY_SOURCES
    QHotkey
    qhotkey.cpp
    qhotkey.h
    qhotkey_p.h
    LICENSE
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND QHOTKEY_SOURCES qhotkey_x11.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND QHOTKEY_SOURCES qhotkey_win.cpp)
endif()

add_library(QHotkey STATIC
    ${QHOTKEY_SOURCES}
)

target_link_libraries(QHotkey PRIVATE Qt::Core Qt::Gui)

target_include_directories(QHotkey PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(QHotkey::QHotkey ALIAS QHotkey)
