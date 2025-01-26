#include "Superpolator.h"

#include "Helpers.h"

const __m256d _num0		= _mm256_setzero_pd();
const __m256d _num1		= _mm256_set1_pd( 1.0 );
const __m256d _num2		= _mm256_set1_pd( 2.0 );
const __m256d _num3		= _mm256_set1_pd( 3.0 );
const __m256d _num_2	= _mm256_set1_pd( 0.5 );
const __m256d _numPI	= _mm256_set1_pd( M_PI );
const __m256d _numPI_2	= _mm256_set1_pd( M_PI_2 );
const __m256d _num2_PI	= _mm256_set1_pd( M_2_PI );
const __m256d _num1_PI	= _mm256_set1_pd( M_1_PI );

const __m128d _nums_0_1 = _mm_setr_pd( 0., 1. );

// Ein (paar?) Helfer für AVX(2)
template < int pos >
__m256d _mm256_insert_sd( __m256d dst, qreal v )
{
	return _mm256_castsi256_pd( _mm256_insert_epi64( _mm256_castpd_si256( dst ),
													 *reinterpret_cast< long long* >( &v ), pos ) );
}

__m256d test( SPElem& e, qreal t )
{
	//================================================================
	// super-smooth-step, broadcasted ...
	// auto x	  = ( t - t01[ 0 ] ) / ( t01[ 1 ] - t01[ 0 ] );
	// auto st	  = _mm256_set1_pd( x * x * ( 3 - 2 * x ) );
	//
	// Hier kommen meine speziellen Zugriffshelfer
	// voll zum Zuge                                AVX:0, 1, 2, 3
	auto t01   = _mm256_castpd128_pd256( e.t01() ); // t0, t1, ., .
	// t01 extrahieren -> t0 muss an beide Stellen,
	// t an 0, t1 an 1 (wo es von vorn herein ist)
	auto t0	   = _mm256_permute_pd( t01, 0 );	  // t0, t0, t0, t0
	auto t1	   = _mm256_insert_sd< 0 >( t01, t ); // t , t1, . , .
	// Differenzen bilden und den Divisor zurecht rücken
	auto tt01n = _mm256_sub_pd( t1, t0 );			 // t-t0, t1-t0, ., .
	auto tt01d = _mm256_permute_pd( tt01n, 0b0001 ); // t1-t0, t-t0, t-t0, t-t0
	// Division -> k erhalten (ich bekomme sogar gleichzeitig 1/k ;)
	auto sstk  = _mm256_div_pd( tt01n, tt01d ); // k, 1/k, ., .
	// das Resultat muss in den Wertebereich geclamped werden
	sstk	   = _mm256_max_pd( _num0, _mm256_min_pd( _num1, sstk ) );
	// Schlussendlich die Kubische Funktion ausrechnen...
	auto ssts =
		_mm256_mul_pd( _mm256_mul_pd( _mm256_fnmadd_pd( sstk, _num2, _num3 ), sstk ), sstk );
	// und das Ergebnis im Register verteilen ...
	return _mm256_permute4x64_pd( ssts, 0 );
	// ... das war jetzt etwas sehr viel Code für so etwas simples wie eine SSt ...
	//=============================================================================
}

void SuperPolator::clear( int reserveSize )
{
	QList::clear();
	QList::reserve( reserveSize );
	durMs = 0;
}

int SuperPolator::append( QSize elementSize )
{
	auto index = count();
	resize( index + 1 );
	operator[]( index ) = elementSize;
	return index;
}

void SuperPolator::setAngle( int index, qreal radians )
{
	// Safeguard!
	if ( index >= 0 && index < count() ) operator[]( index ) = radians;
}

void SuperPolator::initShowUp( int duration_ms, qreal startO )
{
	//  Initialisierung der "show-up" Animation
	//  =======================================
	//  Startwerte: r = a = o = 0.0, s = 0.5
	//  Zielwerte:  r = r_g, a = a_i0, o = 1., s = 1.
	//  t0/t1 nach SST mit 0.3/0.3 -> muss auch für jedes i ausgerechnet werden
	//  =>   t_0i = index * start_max / ( count - 1 ),
	//  =>   t_1i = end_min + ( index + 1 ) * ( 1. - end_min ) / count
	int	 i = 0, cnt = count();
	auto startVals		= _mm256_setr_pd( 0., ( startO == 0.f ? first().a : startO ), 0., 0.5 );
	auto endVals		= _mm256_setr_pd( r0, 0, 1., 1. );
	auto ssst_start_max = 0.3, ssst_end_min = 0.3;
	auto sst01 = _mm_setr_pd( 0., ( ( cnt - 1. ) * ssst_end_min + 1. ) / cnt );
	auto sstO  = _mm_setr_pd( ssst_start_max / ( cnt - 1. ), ( 1. - ssst_end_min ) / cnt );

	for ( ; i < cnt; ++i, sst01 = _mm_add_pd( sst01, sstO ) )
	{
		auto& ii	= operator[]( i );
		ii.quelle() = startVals;
		ii.ziel()	= _mm256_insert_sd< 1 >( endVals, ii.a );
		ii.t01()	= sst01;
	}
	// das sollte es schon gewesen sein.
	startAnimation( duration_ms );
	debugInitialValues( "show_up" );
}

void SuperPolator::initHideAway( int duration_ms, int ai )
{
	// Bei der "Versteck-Animation" möchte ich gern das letzte aktive Element berücksichtigen.
	// Dieses Element wird das Zielelement sein, welches nur zum Mittelpunkt hin gezogen wird.
	// Alle Anderen sollen jeweils 90° davon versetzt "verschluckt werden".
	// ... kleine Erweiterung: ist ai negativ, wird zum Elemen abs(ai)-1 hin zugedreht
	int	  i = 0, cnt = count();
	qreal ssst_start_max = 0.3, ssst_end_min = 0.3, schlucki = ( ai >= 0 ? M_PI_2 : 0. );
	// Das bedeutet mehrere Dinge:
	//  -   die Supersmoothstep muss angepasst werden:
	//      Anzahl Unterteilungen: nu = qMax( ai, n - ai )
	//      aktueller Index: is = qAbs( i - ai )
	//      =>  off_t0 = ( ai ? -1 : 1 ) * s_m / ( nu - 1 )
	//          off_t1 = ( ai ? -1 : 1 ) * ( 1 - e_m ) / nu
	//          => t0( 0 ) = ai * s_m / ( nu - 1 )
	//          => t1( 0 ) = ( ai + 1 ) * ( 1 - e_m ) / nu + e_m
	//      ... das klingt nach einer einfachen Addition zur Berechnung...
	//  -   die Zielwinkel werden angepasst: a[i] < a[ai] ? a[ai]-pi/2 : a[ai]+pi/2
	// ... hmm, klingt gar nicht so schwierig ...

	// korrigiere das aktuelle item, nachdem wir das "Abbiegen oder nicht" festgelegt haben
	ai			 = ( ai >= 0 ? ai < cnt ? ai : cnt - 1 : qAbs( ai ) - 1 );
	auto nu		 = qMax( ai, cnt - ai );
	auto sst01	 = _mm_setr_pd( ai * ssst_start_max / ( nu - 1. ),
								( ai + 1 ) * ( 1. - ssst_end_min ) / nu + ssst_end_min );
	auto sstO	 = _mm_setr_pd( ssst_start_max / ( 1. - nu ), ( 1. - ssst_end_min ) / ( -nu ) );
	auto aai	 = at( ai ).a;
	auto endVals = _mm256_setr_pd( 0, aai, 0, 0.5 );

	for ( ; i < cnt; ++i, sst01 = _mm_add_pd( sst01, sstO ) )
	{
		auto& ii	= operator[]( i );
		auto  z		= ( i == ai || ai < 0 )
						  ? endVals
						  : _mm256_insert_sd< 1 >( endVals,
												   ( ii.a < aai ? aai - schlucki : aai + schlucki ) );
		ii.quelle() = ii.aktuell();
		ii.ziel()	= z;
		ii.t01()	= sst01;
		if ( i == ai ) sstO = _mm_xor_pd( sstO, _mm_set1_pd( -0.0 ) );
	}
	// das sollte es schon gewesen sein.
	startAnimation( duration_ms );
	debugInitialValues( "hide_away" );
}

void SuperPolator::initStill( int duration_ms )
{
	int	 i = 0, cnt = count();
	auto endVals		= _mm256_setr_pd( r0, 0, 1., 1. );
	auto ssst_start_max = 0.3, ssst_end_min = 0.3;
	auto sst01 = _mm_setr_pd( 0., ( ( cnt - 1. ) * ssst_end_min + 1. ) / cnt );
	auto sstO  = _mm_setr_pd( ssst_start_max / ( cnt - 1. ), ( 1. - ssst_end_min ) / cnt );

	for ( ; i < cnt; ++i )
	{
		auto& ii	= operator[]( i );
		ii.quelle() = ii.aktuell();
		ii.ziel()	= _mm256_insert_sd< 1 >( endVals, ii.a );
		ii.t01()	= _nums_0_1;
	}
	// das sollte es schon gewesen sein.
	startAnimation( duration_ms );
	debugInitialValues( "make_still" );
}

#ifndef __AVX2__
#	pragma message( "AVX2 nicht definiert - geht es trotzdem?" )
#endif // __AVX2__

bool SuperPolator::update( QList< QRect >& actions, QList< QPointF >& opaScale )
{
	auto	ms = started.msecsTo( QTime::currentTime() );
	auto	t  = qMin( ( qreal ) ms / durMs, 1. );
	__m256d sst;
	// auto	dbg = qDebug() << "update @ t=" << t << ":";
	for ( int cnt = count(), i = 0; i < cnt; ++i )
	{
		auto& ii = operator[]( i );
		// x(t) Super-Smoothstep mit den gespeicherten Parametern ...
		auto  x	 = qMax( 0., qMin( 1., ( t - ii.t0 ) / ( ii.t1 - ii.t0 ) ) );
		sst		 = _mm256_set1_pd( x * x * ( -2. * x + 3. ) );

		// Interpolation
		auto& cv = ii.aktuell();
		cv = _mm256_fmadd_pd( ii.ziel(), sst, _mm256_fnmadd_pd( ii.quelle(), sst, ii.quelle() ) );

		// 128Bit - Zuweisung für Opacity und Scale ...
		auto& os = reinterpret_cast< __m128d& >( opaScale[ i ] );
		os		 = _mm256_extractf128_pd( cv, 1 );
		// Box berechnen
		auto& a	 = actions[ i ];
		a.setSize( ( os.m128d_f64[ 1 ] * QSizeF( ii ) ).toSize() );
		a.moveCenter( ( cv.m256d_f64[ 0 ] * qSinCos( cv.m256d_f64[ 1 ] ) ).toPoint() );
		// dbg.nospace() << "\n\t" << i << ": sst =" << sst.m256d_f64[ 0 ] << ", cur =" <<
		// ii.aktuell()
		//			  << ", produces box: " << a << os;
	}
	// Return true if animation is over.
	return ( ms >= durMs );
}

QDebug SuperPolator::debug()
{
	auto d = qDebug() << "PieData: r0 =" << r() << "Duration" << durMs << "ms, started" << started;
	for ( int i( 0 ), ic( count() ); i < ic; ++i )
		d << "\n\t" << qSetFieldWidth( 2 ) << i << "anim:" << operator[]( i ).quelle() << "->"
		  << operator[]( i ).ziel() << "now:" << operator[]( i ).aktuell();
	return d;
}

void SuperPolator::debugInitialValues( const char* dsc ) const
{
#ifdef DBG_ANIM_NUMERIC
	auto dbg = qDebug() << dsc << " animation initialized: r0 =" << r0 << "Duration =" << durMs
						<< "ms, started @" << started;
	for ( int i( 0 ), c( count() ); i < c; ++i )
		dbg.nospace() << "\n\t" << qSetFieldWidth( 2 ) << i << ": " << at( i ).quelle() << " => "
					  << at( i ).ziel() << ", current: " << at( i ).aktuell();
	dbg << '\n'; // extra space-line
#endif
}

inline QDebug operator<<( QDebug& d, SuperPolator& s )
{
	QDebugStateSaver ss( d );
	d << "PieData: r0 =" << s.r() << "Duration" << s.durMs << "ms, started" << s.started;
	for ( int i( 0 ), ic( s.count() ); i < ic; ++i )
		d << "\n\t" << qSetFieldWidth( 2 ) << i << "anim:" << s[ i ].quelle() << "->"
		  << s[ i ].ziel() << "now:" << s[ i ].aktuell();
	return d;
}
