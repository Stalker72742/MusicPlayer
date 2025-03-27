cmake_minimum_required(VERSION 3.16)

project(untitled2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets QMultimedia)


set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        Source/Private/Widgets/Components/musicbutton.cpp
        Source/Public/Widgets/Components/musicbutton.h
        Source/Private/Subsystems/PlayerSubsystem.cpp
        Source/Public/Subsystems/PlayerSubsystem.h
        Source/Public/Data/songPath.h
        Source/Private/Subsystems/ytSearcherSub.cpp
        Source/Public/Subsystems/ytSearcherSub.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(untitled2
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(untitled2 SHARED
                ${PROJECT_SOURCES}
        )
    else()
        add_executable(untitled2
                ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(untitled2 PRIVATE
        Source/Public/Widgets/Components/
        Source/Public/Subsystems
        Source/Public/Data
)

target_link_libraries(untitled2 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.untitled2)
endif()
set_target_properties(untitled2 PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
)

include(GNUInstallDirs)
install(TARGETS untitled2
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    # Определяем путь к windeployqt
    get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    # Создаем подпапку libs в папке сборки
    set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/libs")
    file(MAKE_DIRECTORY ${DEPLOY_DIR})

    # Добавляем кастомную команду для копирования зависимостей
    add_custom_command(TARGET untitled2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${DEPLOY_DIR}" # Очистка перед копированием
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E env PATH="${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
            "$<TARGET_FILE:untitled2>"
            --dir "${DEPLOY_DIR}" # Копируем зависимости в libs
            --no-translations # Исключаем переводы, если не нужны
            --no-compiler-runtime # Исключаем runtime компилятора
            --release # Только релизные библиотеки
            --verbose 1
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:untitled2>"
            "${CMAKE_CURRENT_BINARY_DIR}/Release" # Копируем сам исполняемый файл в корень
            COMMENT "Deploying Qt dependencies to Release/libs..."
    )
elseif(UNIX AND NOT APPLE)
    # Для Linux (аналогично, но с linuxdeployqt)
    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt)
    if(LINUXDEPLOYQT_EXECUTABLE)
        set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/libs")
        file(MAKE_DIRECTORY ${DEPLOY_DIR})
        add_custom_command(TARGET untitled2 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${DEPLOY_DIR}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
                COMMAND "${LINUXDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE:untitled2>"
                -bundle-non-qt-libs
                -extra-libs="${DEPLOY_DIR}"
                -verbose=1
                COMMENT "Deploying Qt dependencies to Release/libs..."
        )
    endif()
elseif(APPLE)
    # Для macOS (оставляем бандл как есть, так как это стандарт)
    get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")

    add_custom_command(TARGET untitled2 POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            "$<TARGET_BUNDLE_DIR:untitled2>"
            -verbose=1
            COMMENT "Running macdeployqt to copy Qt dependencies..."
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(untitled2)
endif()