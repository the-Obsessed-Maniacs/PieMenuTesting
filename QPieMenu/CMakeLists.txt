###############################################################################
#	QPieMenu - written by Stefan <St0fF> Kaps 2024 - 2025
#	CMake Steuerung des Library-Projektes.
###############################################################################
#   Diese Datei ist Teil von PieMenuTesting.
#
#   PieMenuTesting ist Freie Software: Sie können es unter den Bedingungen
#   der GNU General Public License, wie von der Free Software Foundation,
#   Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
#   veröffentlichten Version, weiter verteilen und/oder modifizieren.
#
#   PieMenuTesting wird in der Hoffnung, dass es nützlich sein wird, aber
#   OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
#   Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
#   Siehe die GNU General Public License für weitere Details.
#
#   Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
#   Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
###############################################################################

cmake_minimum_required( VERSION 3.16 )

project( QPieMenu VERSION 0.5 LANGUAGES CXX )

option( DBG_EVENTS "Enable event-logging in QPieMenu" off )
option( DBG_ANIM_NUMERIC "Enable numeric animation debugging in QPieMenu" off )
include( EnableIntrinsics.cmake )
check_cpu( AVX2 __AVX2__ AVX2 avx2 )
set( CMAKE_AUTOUIC ON )
set( CMAKE_AUTOMOC ON )
set( CMAKE_AUTORCC ON )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( QT_COMP Widgets )
find_package( QT NAMES Qt6 REQUIRED COMPONENTS ${QT_COMP} )
find_package( Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMP} )
add_definitions( -DNOMINMAX )
if ( ${DBG_EVENTS} )
	add_definitions( -DDEBUG_EVENTS )
endif()
if ( ${DBG_ANIM_NUMERIC} )
	add_definitions( -DDEBUG_ANIM_NUMERIC )
endif()

list( TRANSFORM QT_COMP PREPEND Qt${QT_VERSION_MAJOR}:: OUTPUT_VARIABLE QT_LIBS )
qt_add_library( QPieMenu STATIC QPieMenu.h QPieMenu.cpp )
target_link_libraries( QPieMenu PRIVATE ${QT_LIBS} )
