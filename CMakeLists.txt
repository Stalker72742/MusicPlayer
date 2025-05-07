cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "MusicPlayer")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets QMultimedia)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.ui
)

file(GLOB_RECURSE HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/**/Public/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/**/Widgets/*.h"
)

foreach(header ${HEADERS})
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS} Source/)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets Qt6::Multimedia
        ${AVCODEC_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWSCALE_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/libs")
    file(MAKE_DIRECTORY ${DEPLOY_DIR})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E env PATH="${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
            "$<TARGET_FILE:${PROJECT_NAME}>"
            --dir "${DEPLOY_DIR}"
            --no-translations
            --no-compiler-runtime
            --release
            --verbose 1
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/Release"
            COMMENT "Deploying Qt dependencies to Release/libs..."
    )
elseif(UNIX AND NOT APPLE)
    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt)
    if(LINUXDEPLOYQT_EXECUTABLE)
        set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/libs")
        file(MAKE_DIRECTORY ${DEPLOY_DIR})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${DEPLOY_DIR}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
                COMMAND "${LINUXDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE:${PROJECT_NAME}>"
                -bundle-non-qt-libs
                -extra-libs="${DEPLOY_DIR}"
                -verbose=1
                COMMENT "Deploying Qt dependencies to Release/libs..."
        )
    endif()
endif()

qt_finalize_executable(${PROJECT_NAME})