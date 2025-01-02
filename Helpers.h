/**************************************************************************************************
 *  a small collection of helper functions
 * by St0fF / 2025
 *************************************************************************************************/
#pragma once
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>

QT_BEGIN_NAMESPACE
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
constexpr QPointF richtung( QPointF d )
{
	auto sub = []( auto sub ) { return qAbs( sub ) < 0.000001 ? 0 : sub > 0 ? 1. : -1.; };
	return { sub( d.x() ), sub( d.y() ) };
}
inline qreal normDegrees( qreal a )
{
	return fmod( a, 360. ) + ( a < 0 ? 360. : 0. );
}
inline qreal degreesDistance( qreal from, qreal to )
{
	// Zwischen 2 Winkeln haben wir immer eine positive und eine negative Distanz.
	// Wir suchen aber die Distanz in eine Richtung!
	auto f_clean = fmod< double >( from, 360. ); // der Wert liegt jetzt zwischen [-360,360]
	auto t_clean = fmod< double >( to, 360. );	 // dito
	auto diff1	 = t_clean - f_clean;			 // Die Differenz auch.
	return diff1 + ( diff1 > 180 ? -360 : diff1 < -180 ? 360 : 0 );
}
template < typename T >
int qSgn( T val )
{
	return ( T( 0 ) < val ) - ( val < T( 0 ) );
}

__forceinline QPointF qSinCos( qreal radians )
{
	return QPointF{ qSin( radians ), qCos( radians ) };
}

__forceinline QColor qLerpRGB( Qt::GlobalColor c0, Qt::GlobalColor c1, float t )
{
	auto mask_to   = _mm_set_epi64x( 0x0f0f0f030f0f0f02, 0x0f0f0f010f0f0f00 );
	auto mask_back = _mm_set_epi64x( 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0c080400 );
	auto _t		   = _mm_set_ps1( qMin( 1.f, qMax( 0.f, t ) ) ); // safeguard
	auto _c0 =
		_mm_cvtepi32_ps( _mm_shuffle_epi8( _mm_set1_epi32( QColor( c0 ).rgba() ), mask_to ) );
	auto _c1 =
		_mm_cvtepi32_ps( _mm_shuffle_epi8( _mm_set1_epi32( QColor( c1 ).rgba() ), mask_to ) );
	// We've reached the future some time ago?
	// https://fgiesen.wordpress.com/2012/08/15/linear-interpolation-past-present-and-future/
	auto resf	= _mm_fmadd_ps( _t, _c1, _mm_fnmsub_ps( _t, _c0, _c0 ) );
	auto res_i8 = _mm_shuffle_epi8( _mm_cvtps_epi32( resf ), mask_back );
	return QColor::fromRgba( _mm_extract_epi32( res_i8, 0 ) );
}

__forceinline QPointF qLerp2D( QPointF p0, QPointF p1, qreal t )
{
	auto _p0( _mm_setr_pd( p0.x(), p0.y() ) );
	auto _p1( _mm_setr_pd( p1.x(), p1.y() ) );
	auto _t( _mm_set1_pd( t ) );
	auto res( _mm_fmadd_pd( _t, _p1, _mm_fnmsub_pd( _t, _p0, _p0 ) ) );
	return { res.m128d_f64[ 0 ], res.m128d_f64[ 1 ] };
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


// Der Intersektor ist eine RectF-Liste, die beim Hinzufügen mit den "neuen Funktionen"
// (add(),addAnyhow()...) auch einen Überlappungsstatus der hinzugefügten Rects speichert.
struct Intersector : public QList< QRectF >
{
	QRectF lastIntersection;
	bool   hasIntersection{ false };
	bool   checkIntersections()
	{
		int i( count() - 1 );
		while ( i > 0 )
		{
			auto j( i - 1 );
			while ( j >= 0 )
			{
				if ( at( i ).intersects( at( j ) ) )
				{
					lastIntersection = at( i ).intersected( at( j ) );
					return ( hasIntersection = true );
				}
				--j;
			}
			--i;
		}
		lastIntersection = {};
		return ( hasIntersection = false );
	}
	bool add( QRectF r )
	{
		for ( auto i : *this )
			if ( r.intersects( i ) )
			{
				lastIntersection = r.intersected( i );
				return false;
			};
		lastIntersection = {};
		append( r );
		return true;
	}
	void addAnyhow( QRectF r )
	{
		for ( auto i : *this )
			if ( r.intersects( i ) )
			{
				lastIntersection = r.intersected( i );
				hasIntersection	 = true;
				break; // finish the loop prematurely
			};
		append( r );
	}
	bool changeItem( int index, QRectF newValue )
	{
		if ( at( index ) != newValue )
		{
			// assign and recheck bounds
			operator[]( index ) = newValue;
			if ( !hasIntersection ) // only check for a intersections if there aren't any, already.
				for ( int i( 0 ); i < count(); ++i )
					if ( i != index && newValue.intersects( at( i ) ) )
						lastIntersection = newValue.intersected( at( i ) ), hasIntersection = true;
		}
	}
	bool changeItem( int index, QPointF newCenter )
	{
		return changeItem( index, at( index ).translated( newCenter - at( index ).center() ) );
	}
	void resetIntersection() { lastIntersection = {}, hasIntersection = false; }
};

QT_END_NAMESPACE
