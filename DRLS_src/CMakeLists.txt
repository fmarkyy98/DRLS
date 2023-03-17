cmake_minimum_required(VERSION 3.5)

project(DRLS_src VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

set(PROJECT_SOURCES

    client/src/MainWindow.h
    client/src/MainWindow.cpp
    client/src/MainWindow.ui
    client/src/main.cpp

    common/src/service/EntityServiceSpecializations/AdministratorEntityService.cpp
    common/src/service/EntityServiceSpecializations/FruitEntityService.cpp
    common/src/service/EntityServiceSpecializations/UserEntityService.cpp

    common/src/service/interface/IDelayedResourceLockService.h
    common/src/service/interface/IResourceLockService.h
    common/src/service/interface/IService.h

    common/src/service/AsyncTaskService.h
    common/src/service/AsyncTaskService.cpp
    common/src/service/DelayedResourceLockService.h
    common/src/service/DelayedResourceLockService.cpp
    common/src/service/EntityService.h
    common/src/service/ResourceLockService.h
    common/src/service/ResourceLockService.cpp

    common/src/AsyncTask.h
    common/src/AsyncTask.cpp
    common/src/CallerContext.h
    common/src/Delegate.h
    common/src/Delegate.cpp
    common/src/LockableResource.h
    common/src/TaskManager.h
    common/src/TaskManager.cpp
    common/src/TasksUpdatedSignalProxy.h
    common/src/TasksUpdatedSignalProxy.cpp

    persistence/EntityType.h

    persistence/Entity.h
    persistence/Entity.cpp
    persistence/Administrator.h
    persistence/Fruit.h
    persistence/User.h

    utils/Finally.h
    utils/ThreadHelper.h
    utils/ThreadHelper.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DRLS_src
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(DRLS_src SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(DRLS_src
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(DRLS_src PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

set_target_properties(DRLS_src PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DRLS_src)
endif()
