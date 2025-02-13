###############################################################################
#	PieMenuTesting - written by Stefan <St0fF> Kaps 2024 - 2025
#	EnableIntrinsics.CMake
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
# Include the required headers
include(CheckCXXSourceCompiles)

macro( check_cpu Name IntrinMacro WFlag LFlag )
	if( MSVC )
		set( _co /arch:${WFlag} )
	else()
		set( _co -m${LFlag} )
	endif()
	set( CMAKE_REQUIRED_FLAGS ${_co} )
	check_cxx_source_compiles( "#include <immintrin.h>
	int main() {return 
		#if defined(${IntrinMacro})
			0
		#else
			1
		#endif
	;}" ${Name}_SUPPORTED )
	if( ${Name}_SUPPORTED )
		add_compile_options( ${_co} )
	endif()
	unset( _co )
endmacro( check_cpu )

#check_cpu( AVX __AVX__ AVX avx )
#check_cpu( AVX2 __AVX2__ AVX2 avx2 )
#check_cpu( AVX-512 __AVX512F__ AVX512 avx512f )
