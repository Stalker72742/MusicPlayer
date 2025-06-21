cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "MusicPlayer")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(ANDROID)
    set(PLATFORM_ANDROID TRUE)
    set(PLATFORM_WINDOWS FALSE)
elseif(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    set(PLATFORM_ANDROID FALSE)
else()
    set(PLATFORM_WINDOWS FALSE)
    set(PLATFORM_ANDROID FALSE)
endif()

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Widgets
        Multimedia
)

if(PLATFORM_WINDOWS)
    add_definitions(-DUNICODE -D_UNICODE)
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
        add_compile_options(/utf-8)
    endif()
endif()

if(PLATFORM_ANDROID)
    set(ANDROID_PLATFORM android-24)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")

    set_property(TARGET Qt6::Core PROPERTY INTERFACE_QT_ANDROID_DEPLOYMENT_DEPENDENCIES "")
endif()

add_subdirectory(Core)

if(PLATFORM_ANDROID)

    add_subdirectory(Platforms/Android)
elseif(PLATFORM_WINDOWS)

    add_subdirectory(Platforms/Desktop)
else()
    message("Update cmake code for new platform")
endif ()

qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION main.cpp)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        MusicPlayerCore
        DesktopMusicPlayerUI
        Qt6::Multimedia
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(PLATFORM_ANDROID)

elseif(PLATFORM_WINDOWS)

    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/bin")
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
            COMMENT "Deploying Qt dependencies to Release/bin..."
    )

    set(DLL_FILES
            "D:/Qt/Tools/mingw1310_64/bin/libgcc_s_seh-1.dll"
            "D:/Qt/Tools/mingw1310_64/bin/libstdc++-6.dll"
            "D:/Qt/Tools/mingw1310_64/bin/libwinpthread-1.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/Core/libMusicPlayerCore.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/Platforms/Desktop/libDesktopMusicPlayerUI.dll"
    )

    add_custom_command(TARGET MusicPlayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_FILES} "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Copying all required DLL files"
    )
else ()
    message("Update cmake code for new platform")
endif ()

qt_finalize_executable(${PROJECT_NAME})