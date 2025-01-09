/**************************************************************************************************
 *  a small collection of helper functions
 * by St0fF / 2025
 *
 * ... and maybe some helpful definitions
 *************************************************************************************************/
#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>

QT_BEGIN_NAMESPACE
template < typename T >
constexpr bool epsilonEQ( T a, T b )
{
	return std::abs( a - b ) <= std::numeric_limits< T >::epsilon() * 10;
}
template < typename T >
constexpr bool epsilonNE( T a, T b )
{
	return std::abs( a - b ) > std::numeric_limits< T >::epsilon() * 10;
}
constexpr QPointF operator*( QPointF a, QPointF b )
{
	return { a.x() * b.x(), a.y() * b.y() };
}

constexpr QPointF fromSize( const QSizeF &a )
{
	return { a.width(), a.height() };
}
constexpr QPointF fromSize( const QSize &a )
{
	return fromSize( a.toSizeF() );
}
constexpr QPointF fromSize( const QRectF &a )
{
	return fromSize( a.size() );
}

constexpr QSizeF fromPoint( const QPointF &a )
{
	return { a.x(), a.y() };
}
constexpr QSize fromPoint( const QPoint &a )
{
	return { a.x(), a.y() };
}

constexpr QPointF richtung( QPointF d )
{
	return { d.x() > 0. ? 1. : d.x() < 0. ? -1. : 0., d.y() > 0. ? 1. : d.y() < 0. ? -1. : 0. };
}

template < qreal halfCircle >
constexpr qreal normalize( qreal a )
{
	auto s = a < 0;
	auto b = ( !s && a < 2 * halfCircle ) ? a : a + 2 * halfCircle * trunc( 0.5 * a / halfCircle );
	return b + s * 2 * halfCircle;
}
constexpr auto normDeg = normalize< 180. >;
constexpr auto normRad = normalize< M_PI >;

template < qreal halfCircle >
qreal distance( qreal from, qreal to )
{
	constexpr auto w0 = 2 * halfCircle, w1 = 0.5 / halfCircle, w2 = halfCircle;
	auto		   d = to - from + w0 * ( trunc( from * w1 ) - trunc( to * w1 ) );
	return d + ( d > w2 ? -w0 : d < -w2 ? w0 : 0 );
}
inline auto			   degreesDistance = distance< 180. >;
inline auto			   radiansDistance = distance< M_PI >;

constexpr inline qreal qSgn( qreal val )
{
	return qreal( ( 0. < val ) - ( val < 0. ) );
}
constexpr inline QPointF qSgn( QPointF val )
{
	return { static_cast< qreal >( ( 0. < val.x() ) - ( val.x() < 0. ) ),
			 static_cast< qreal >( ( 0. < val.y() ) - ( val.y() < 0. ) ) };
}
constexpr inline QPointF qSgn( QPointF val, qreal scale )
{
	return { static_cast< qreal >( ( 0. < val.x() ) - ( val.x() < 0. ) ) * scale,
			 static_cast< qreal >( ( 0. < val.y() ) - ( val.y() < 0. ) ) * scale };
}

__forceinline QPointF qSinCos( qreal radians )
{
	return QPointF{ qSin( radians ), qCos( radians ) };
}

__forceinline QColor qLerpRGBA( const QColor &c0, const QColor &c1, qreal t )
{ // Only works as long as the internal QColor Design does not change
	auto mask_to   = _mm_set_epi64x( 0x0f0f0f030f0f0f02, 0x0f0f0f010f0f0f00 );
	auto mask_back = _mm_set_epi64x( 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0c080400 );
	auto _t		   = _mm_set_ps1( qMin( 1.f, qMax( 0.f, t ) ) ); // safeguard
	auto _c0	   = _mm_cvtepi32_ps( _mm_shuffle_epi8( _mm_set1_epi32( c0.rgba() ), mask_to ) );
	auto _c1	   = _mm_cvtepi32_ps( _mm_shuffle_epi8( _mm_set1_epi32( c1.rgba() ), mask_to ) );
	// We've reached the future some time ago?
	// https://fgiesen.wordpress.com/2012/08/15/linear-interpolation-past-present-and-future/
	auto resf	   = _mm_fmadd_ps( _c1, _t, _mm_fnmadd_ps( _c0, _t, _c0 ) );
	auto res_i8	   = _mm_shuffle_epi8( _mm_cvtps_epi32( resf ), mask_back );
	return QColor::fromRgba( _mm_extract_epi32( res_i8, 0 ) );
}
__forceinline QColor qLerpRGBA( Qt::GlobalColor c0, Qt::GlobalColor c1, qreal t )
{
	return qLerpRGBA( QColor( c0 ), QColor( c1 ), t );
}
__forceinline QPointF qLerp2D( const QPointF &p0, const QPointF &p1, qreal t )
{
	auto _t( _mm_set1_pd( t ) );
	auto _p0( _mm_load_pd( reinterpret_cast< const qreal * >( &p0 ) ) );
	auto _p1( _mm_load_pd( reinterpret_cast< const qreal * >( &p1 ) ) );
	auto res( _mm_fmadd_pd( _p1, _t, _mm_fnmadd_pd( _p0, _t, _p0 ) ) );
	return { res.m128d_f64[ 0 ], res.m128d_f64[ 1 ] };
}
__forceinline QRectF qLerpRect( const QRectF &src, const QRectF &tgt, qreal t )
{ // This will only work as long as QRectF stays to be x1,y1,w,h of qreal internally.
	__m256d a( *reinterpret_cast< const __m256d * >( &src ) );
	__m256d b( *reinterpret_cast< const __m256d * >( &tgt ) );
	__m256d _t( _mm256_set1_pd( t ) );
	auto	r = _mm256_fmadd_pd( b, _t, _mm256_fnmadd_pd( a, _t, a ) );
	return { r.m256d_f64[ 0 ], r.m256d_f64[ 1 ], r.m256d_f64[ 2 ], r.m256d_f64[ 3 ] };
}
__forceinline QSizeF qLerpSize( QSizeF s0, QSizeF s1, qreal t )
{
	return fromPoint( qLerp2D( fromSize( s0 ), fromSize( s1 ), t ) );
}

__forceinline qreal smoothStep( qreal t, qreal t0 = 0., qreal t1 = 1. )
{
	auto a = qMax( 0., qMin( 1., ( t - t0 ) / ( t1 - t0 ) ) );
	return a * a * ( 3 - 2 * a );
}
__forceinline qreal superSmoothStep( qreal t, qreal start_max, qreal end_min, int index, int count )
{
	return smoothStep( t, index * start_max / ( count - 1 ),
					   end_min + ( index + 1 ) * ( 1. - end_min ) / count );
}
__forceinline qreal superSmoothStep( qreal t, qreal a, int index, int count )
{
	return superSmoothStep( t, a, a, index, count );
}

__forceinline qreal length( const QPointF &p )
{
	return qSqrt( QPointF::dotProduct( p, p ) );
}

// Der Intersektor ist eine Rect(F)-Liste, die beim Hinzufügen mit den "neuen Funktionen"
// (add(),addAnyhow()...) auch einen Überlappungsstatus der hinzugefügten Rects speichert.
template < typename R, typename P >
	requires( std::is_same_v< R, QRect > && std::is_same_v< P, QPoint > )
			|| ( std::is_same_v< R, QRectF > && std::is_same_v< P, QPointF > )
struct Intersector : public QList< R >
{
	using R_type = R;
	using P_type = P;

	R	 _lastIntersection;
	bool _hasIntersection{ false };
	void resetIntersection() { _lastIntersection = {}, _hasIntersection = false; }
	bool checkIntersections()
	{
		int i( QList< R >::count() - 1 );
		while ( i > 0 )
		{
			auto j( i - 1 );
			while ( j >= 0 )
			{
				if ( QList< R >::at( i ).intersects( QList< R >::at( j ) ) )
				{
					_lastIntersection = QList< R >::at( i ).intersected( QList< R >::at( j ) );
					return ( _hasIntersection = true );
				}
				--j;
			}
			--i;
		}
		_lastIntersection = {};
		return ( _hasIntersection = false );
	}
	// add() returns true, if the item can be added without intersections.
	// If there is an intersection, lastIntersection will be set to the intersection rectangle and
	// the item is not added.
	virtual bool add( R_type r )
	{
		for ( auto i : *this )
			if ( r.intersects( i ) )
			{
				_lastIntersection = r.intersected( i );
				return false;
			};
		_lastIntersection = {};
		QList< R >::append( r );
		return true;
	}
	virtual void addAnyhow( R_type r )
	{
		for ( auto i : *this )
			if ( r.intersects( i ) )
			{
				_lastIntersection = r.intersected( i );
				_hasIntersection  = true;
				break; // finish the loop prematurely
			};
		QList< R >::append( r );
	}
	// changeItem() returns true, if the item at index can be changed without intersections.
	virtual bool changeItem( int index, R_type newValue )
	{
		if ( QList< R >::at( index ) != newValue )
		{
			// assign and recheck bounds
			QList< R >::operator[]( index ) = newValue;
			if ( !_hasIntersection ) // only check for a intersections if there aren't any, already.
				for ( int i( 0 ); i < QList< R >::count(); ++i )
					if ( i != index && newValue.intersects( QList< R >::at( i ) ) )
						_lastIntersection = newValue.intersected( QList< R >::at( i ) ),
						_hasIntersection  = true;
		}
		return !_hasIntersection;
	}
	virtual bool changeItem( int index, P_type newCenter )
	{
		return changeItem( index, QList< R >::at( index ).translated(
									  newCenter - QList< R >::at( index ).center() ) );
	}
	virtual void resetToFirst()
	{
		QList< R >::resize( 1 );
		resetIntersection();
	}
};

// Mal ein Versuch, die Winkeldifferenzen-Geschichte aus Strategie 3 zu vereinfachen:
class BestDelta
{
  public:
	__forceinline void init( qreal direction, qreal reference_angle )
	{
		bad = -( good = std::numeric_limits< qreal >::max() );
		dir = direction;
		w0	= reference_angle;
	}
	constexpr auto	reference() const { return w0; }
	constexpr bool	hasGood() const { return good < std::numeric_limits< qreal >::max(); }
	constexpr bool	hasBad() const { return bad > -std::numeric_limits< qreal >::max(); }
	constexpr qreal best() const { return dir * ( hasGood() ? good : hasBad() ? bad : 0.0 ); }
	template < double halfCircle >
	__forceinline void addAngle( qreal a )
	{
		auto d = distance< halfCircle >( w0, a ) * dir;
		if ( d > 0 ) good = qMin( good, d );
		else if ( d < 0 ) bad = qMax( bad, d );
	}
	// Instanziierungen für die beiden Winkelmaße:
	__forceinline void addDeg( qreal a ) { addAngle< 180.0 >( a ); }
	__forceinline void addRad( qreal a ) { addAngle< M_PI >( a ); }

  private:
	qreal good, bad, dir, w0;
};
QT_END_NAMESPACE
