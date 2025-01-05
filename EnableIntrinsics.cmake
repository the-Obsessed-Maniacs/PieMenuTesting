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

check_cpu( AVX __AVX__ AVX avx )
check_cpu( AVX2 __AVX2__ AVX2 avx2 )
check_cpu( AVX-512 __AVX512F__ AVX512 avx512f )
