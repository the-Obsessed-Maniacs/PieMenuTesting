cmake_minimum_required(VERSION 3.16)

project(PieMenuTesting VERSION 0.1 LANGUAGES CXX)

option( DBG_EVENTS "Enable event-logging in QPieMenu" off )
option( DBG_ANIM_NUMERIC "Enable numeric animation debugging in QPieMenu" off )
#include( EnableIntrinsics.cmake )
#add_compile_definitions( SIMDE_ARCH_X86_SVML )

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set( QT_COMP Widgets Scxml )
find_package( QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMP} LinguistTools )
find_package( Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMP} LinguistTools )

set( TS_FILES PieMenuTesting_de_DE.ts )

set( PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
		SelfRegFactory.h
		Placements.h
		Placements.cpp
		Helpers.h
		BerechnungsModell.h
		BerechnungsModell.cpp
        qpiemenu.h
		qpiemenu.cpp
		FastTrigo/FastTrigo.h
		FastTrigo/FastTrigo.cpp
		Superpolator.h
		Superpolator.cpp

        ${TS_FILES}
)
set( STATE_CHARTS
	diagrams/States.scxml
)
add_definitions( -DNOMINMAX )
if ( ${DBG_EVENTS} )
	add_definitions( -DDEBUG_EVENTS )
endif()
if ( ${DBG_ANIM_NUMERIC} )
	add_definitions( -DDEBUG_ANIM_NUMERIC )
endif()
include_directories( FastTrigo )
message( NOTICE "QT Version: ${QT_VERSION}" )

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(PieMenuTesting
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
	#qt_add_statecharts( PieMenuTesting ${STATE_CHARTS} OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/StateEngine )

# Define target properties for Android with Qt 6 as:
#    set_property(TARGET PieMenuTesting APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(PieMenuTesting SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(PieMenuTesting
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

list( TRANSFORM QT_COMP PREPEND Qt${QT_VERSION_MAJOR}:: OUTPUT_VARIABLE QT_LIBS )
target_link_libraries( PieMenuTesting PRIVATE ${QT_LIBS} )

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.PieMenuTesting)
endif()
set_target_properties(PieMenuTesting PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include( GNUInstallDirs )
install( TARGETS PieMenuTesting
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable( PieMenuTesting )
endif()
