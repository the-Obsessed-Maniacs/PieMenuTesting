/**************************************************************************************************
 * Datei: BerechnungsModell.cpp
 * Autor: Stefan <St0fF / Neoplasia ^ the Obsessed Maniacs> Kaps, 2024-2025
 *
 * Dieses BerechnungsModell stellt über die StrategieFactory eine aktuelle Berechnung und Animation
 * zur Verfügung.
 **************************************************************************************************
 *  Diese Datei ist Teil von PieMenuTesting.
 *
 *  PieMenuTesting ist Freie Software: Sie können es unter den Bedingungen
 *  der GNU General Public License, wie von der Free Software Foundation,
 *  Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
 *  veröffentlichten Version, weiter verteilen und/oder modifizieren.
 *
 *  PieMenuTesting wird in der Hoffnung, dass es nützlich sein wird, aber
 *  OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
 *  Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 *  Siehe die GNU General Public License für weitere Details.
 *
 *  Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
 *  Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
 *************************************************************************************************/
#include "Placements.h"

#include <QtWidgets>

ersterVersuch::ersterVersuch()
	: StrategieBasis::Registrar< ersterVersuch >()
{
	_description = QObject::tr( "erste Idee: \"Aufdrehen\"" );
}

void	  ersterVersuch::calculateItems( Items& items_with_sizes ) {}

Opacities ersterVersuch::animateItems( Items& items, qreal progress )
{
	Opacities result;
	if ( !items.isEmpty() )
	{
		// -> should position the items according to time and stop the animation if no
		// overlaps are left we need some parameters:
		qreal sa = _startAngle; // - starting angle -> which Angle for the first Item?
		qreal sd = _minDelta;	// - starting_delta -> minimum angle difference between items
		qreal md = -240 / items.count(); // - maximum_delta -> maximum angle difference
										 // between items (depends on number of items)
		for ( int i( 0 ); i < items.count(); ++i )
		{
			auto r =
				qDegreesToRadians( sa + i * ( sd + smoothStep( progress / 5 ) * ( md - sd ) ) );
			items[ i ].moveCenter( progress * _openParam * QPointF{ qSin( r ), qCos( r ) } );
			result.append( progress );
		}
	}
	return result;
}

StrategieNo2::StrategieNo2()
	: StrategieBasis::Registrar< StrategieNo2 >()
{
	_description = QObject::tr( "zweite Idee: positionieren und Abstände berechnen..." );
	_options << QObject::tr( "mittlere Breite" ) << QObject::tr( "minimale halbe Manhattan-Länge" )
			 << QObject::tr( "min( n*h/4, mhML )" );
}

void StrategieNo2::calculateItems( Items& items )
{
	if ( items.isEmpty() ) return;
	// Wir beschaffen uns die mittlere Breite
	qreal startRadius{ std::numeric_limits< qreal >::max() };
	switch ( selectedOption() )
	{
		case 0:
			startRadius = 0.;
			for ( auto i : items ) startRadius += i.width();
			startRadius /= items.count();
			break;
		case 1:
			for ( auto i : items )
				startRadius = qMin( startRadius, ( i.height() + i.width() ) * 0.5 );
			break;
		case 2:
			for ( auto i : items )
				startRadius = qMin( startRadius, ( i.height() + i.width() ) * 0.5 );
			startRadius = qMin( items.count() * items[ 0 ].height() / 4., startRadius );
			break;
	}
	bool badRadius;
	// Und nun durchlaufen wir all unsere Items
	auto radius{ startRadius };
	// qDebug() << "animate_1: radius=" << radius;
	do {
		badRadius = false;
		data.clear();
		int	 idx{ 0 };
		auto winkel{ static_cast< qreal >( startAngle() ) };
		auto min_plus{ static_cast< qreal >( -minDelta() ) };
		auto max_plus{ static_cast< qreal >( -maxDelta() ) };
		auto br = items.first();
		while ( idx < items.count() )
		{ // setze das Item auf die berechneten bzw. initialen Werte
			br.moveCenter( radius * qSinCos( qDegreesToRadians( winkel ) ) );
			data.append( qMakePair( radius, winkel ) );
			items[ idx++ ] = br;
			// nun haben wir eine bounding Box.  Schauen wir erstmal, ob die des nächsten Elements
			// jene überschneiden würde, wenn das nächste Item nur um den Minimalwinkel verschoben
			// angeordnet werden würde ...
			if ( idx < items.count() )
			{
				auto nbr		= items[ idx ];
				auto winkelplus = min_plus;
				for ( ; abs( winkelplus ) <= abs( max_plus );
					  winkelplus += ( max_plus - min_plus ) / 128. )
				{
					nbr.moveCenter( radius * qSinCos( qDegreesToRadians( winkel + winkelplus ) ) );
					if ( !br.intersects( nbr ) ) break; // found next item's position
				}
				if ( br.intersects( nbr ) ) // no final position found? Maybe we need to re-run with
											// a larger radius.
				{
					badRadius = true;
					if ( radius < 50 ) radius += 5;
					else if ( radius < 100 ) radius += 8;
					else radius *= 1.05;
					break;
				} else {
					br = nbr;
					winkel += winkelplus;
				}
			}
			if ( badRadius ) break;
		}
	} while ( badRadius );
}

Opacities StrategieNo2::animateItems( Items& items, qreal progress )
{
	Opacities result;
	if ( items.isEmpty() ) return result;
	if ( data.count() != items.count() ) calculateItems( items );
	auto t = progress;
	for ( int i( 0 ); i < items.count(); ++i )
	{
		// smoothstep-Fenster für t: i*1/items.count()
		auto a	 = 0.3;
		auto t_i = superSmoothStep( t, a, i, items.count() );
		auto r	 = data[ i ].first;
		auto w	 = data[ i ].second;
		auto w0	 = startAngle();
		items[ i ].moveCenter( qSinCos( qDegreesToRadians( w0 + t_i * ( w - w0 ) ) ).transposed()
							   * qSin( t_i * M_PI_2 ) * r );
		result.append( t_i );
	}
	return result;
}

StrategieNo3::StrategieNo3()
	: StrategieBasis::Registrar< StrategieNo3 >()
{
	_description = QObject::tr( "die eleganteste Iterationslösung" );
}

void StrategieNo3::calculateItems( Items& items )
{
	if ( items.isEmpty() ) return;
	data.clear();
	// Init-daten brauchen wir ...
	int		direction = static_cast< int >( _direction );
	qreal	w0 = _startAngle, ww = 0;
	qreal	ds	  = qApp->style()->pixelMetric( QStyle::PM_LayoutVerticalSpacing );
	QPointF scDDt = { 1., -1. }, sz = fromSize( items.first().size() );
	// Variablen
	int		btc( 1 ), ic( items.count() );
	auto	baseHeight = ( ( items.first().height() + ds ) * items.count() /* - ds*/ );
	qreal	r( baseHeight / 4 ), w( w0 ), np, off;
	// auto		   dbg = qDebug() << "StrategieNo3 - Startwerte: w0=" << w0 << "r0=" << r;
	Intersector< QRectF, QPointF > usedSpace;
	QList< qreal >				   winkelz, delta_w, bad_deltas;
	auto						   st = __rdtsc();
	for ( int i = 0; i < ic; ++i )
	{
		data.append( { r, w } );
		if ( i < ic - 1 )
		{
			// Basisdaten berechnen/beschaffen: sin / cos, ableitung, Mittelpunkt, nächste Box-Größe
			auto sc = qSinCos( qDegreesToRadians( w ) ), c = r * sc, dt = sc.transposed() * scDDt,
				 nxt_sz = fromSize( items[ i + 1 ].size() );
			if ( i == 0 ) usedSpace.add( { c - 0.5 * sz, c + 0.5 * sz } );
			// suchen wir nach dem nächsten möglichen Winkel:
			// => alle Möglichkeiten ihn in eine Liste packen...
			winkelz.clear();
			auto offset = 0.5 * ( sz + nxt_sz ) + QPointF{ ds, ds };
			for ( auto v : { c + offset, c - offset } )
			{
				if ( abs( v.x() ) <= r )
				{
					winkelz.append( qRadiansToDegrees( qAsin( v.x() / r ) ) );
					winkelz.append( 180. - winkelz.last() );
				}
				if ( abs( v.y() ) <= r )
				{
					winkelz.append( qRadiansToDegrees( qAcos( v.y() / r ) ) );
					winkelz.append( -winkelz.last() );
				}
			}
			// Diese Liste dürfte nun maximal 8 Winkel enthalten, bauen wir daraus mal eben deltas
			delta_w.clear();
			for ( auto i : winkelz ) delta_w << degreesDistance( w, i );
			// dbg << "\n\tItem #" << i << QSizeF( sz.x(), sz.y() ) << "@ r=" << r << "w=" << w
			//	<< "c=" << c << "next possible angles:" << winkelz << "deltas:" << delta_w;
			//  sortierte Liste, wir wollen ja den kleinstmöglichen Schritt gehen
			std::sort( delta_w.begin(), delta_w.end() );
			// falls es unterschiedliche delta-Vorzeichen gibt
			// -> parken wir die zwischen, die falsch herum sind
			int ii( 0 );
			bad_deltas.clear();
			while ( ii < delta_w.count() )
				if ( qSgn( delta_w[ ii ] ) - direction ) bad_deltas << delta_w.takeAt( ii );
				else ++ii;
			// Jetzt wählen wir einen Winkel aus ...
			qreal delta = 0.;
			// Wir präferieren Winkel in die richtige Richtung
			if ( delta_w.size() ) delta = direction > 0 ? delta_w.first() : delta_w.last();
			// Wenn keine da sind, nehmen wir auch "falsch herum"
			else if ( bad_deltas.size() )
				delta = direction > 0 ? bad_deltas.last() : bad_deltas.first();
			// wir brauchen gleich: die aktualisierte Winkelsumme und den nächsten Mittelpunkt
			ww += qAbs( delta );
			c = r * qSinCos( qDegreesToRadians( w + delta ) );
			// Haben wir ein Delta? Sind wir unter 360° insgesamt?
			// Haben wir keine Überlappung?
			if ( qAbs( delta ) > .001 && ww < 350.
				 && usedSpace.add( { c - 0.5 * nxt_sz, c + 0.5 * nxt_sz } ) )
			{
				// dbg << "-> selected delta:" << delta;
				w += delta;
				sz = nxt_sz;
			} else {
				// if ( ww >= 360. ) dbg << "=> Circle is overfilled.";
				// else if ( qAbs( delta ) > .001 )
				//	dbg << "=> overlapping items detected @" << usedSpace.lastIntersection;
				// else dbg << "=> no follow-up-angles found.";
				//  keine Winkel übrig?  Ergo keine Platzierung möglich.  Mehr Platz, bitte!
				if ( btc++ > 5 ) // bzw. Abbruch nach 5 Iterationsversuchen.
					break;
				if ( r < baseHeight / 3 ) r += baseHeight / 30;
				else if ( r < baseHeight / 2 ) r += baseHeight / 20;
				else r *= 1.05;
				i  = -1;
				w  = w0;
				ww = 0;
				data.clear();
				usedSpace.clear();
				// dbg << "Retry with new r=" << r;
			}
		} else
			/*dbg << "\n\tItem #" << i << QSizeF( sz.x(), sz.y() ) << "@ r=" << r << "w=" << w
				<< "c=" << r * qSinCos( qDegreesToRadians( w ) ) << "\nDONE with calculate_3()."*/
			;
	}
	auto et = __rdtsc();
	qDebug()
		/*dbg */
		<< "Strategie No°3 took" << ( et - st ) << "cycles.";
	items = usedSpace;
}

Opacities animate3( StrategieBasis::Items& items, qreal t, qreal w0, RadiusAngles& data )
{
	Opacities result;
	if ( items.isEmpty() ) return result;
	for ( int i( 0 ); i < items.count(); ++i )
	{
		// smoothstep-Fenster für t: i*1/items.count()
		auto t_i = superSmoothStep( t, 0.3, 0.7, i, items.count() );
		auto r	 = data[ i ].first;
		auto w	 = data[ i ].second;

		items[ i ].moveCenter( qSinCos( qDegreesToRadians( w0 + t_i * ( w - w0 ) ) )
							   * qSin( t_i * M_PI_2 ) * r );
		result.append( t_i );
	}
	return result;
}

Opacities StrategieNo3::animateItems( Items& items, qreal progress )
{
	if ( data.count() != items.count() ) calculateItems( items );
	return animate3( items, progress, startAngle(), data );
}

StrategieNo3plus::StrategieNo3plus()
{
	_description = QObject::tr( "die eleganteste Iterationslösung, noch besser" );
	_options << QObject::tr( "0.28" ) << QObject::tr( "h * PI / Abdeckung" );
}

void StrategieNo3plus::calculateItems( Items& items )
{
	if ( items.isEmpty() ) return;
	data.resize( items.count() );
	// Init-daten brauchen wir ...
	int	  direction = static_cast< int >( _direction ), ic( items.count() );
	qreal w0 = _startAngle, ww = 0, r, w( w0 ), n,
		  ds	  = qApp->style()->pixelMetric( QStyle::PM_LayoutVerticalSpacing );
	QPointF csDDt = { -1., 1. }, sz = fromSize( items.first().size() );
	// Variablen
	int		btc( 1 );
	auto	baseHeight = ( ( sz.y() + ds ) * ic /* - ds*/ );
	switch ( _selectedOption )
	{
		case 1: r = baseHeight * ( sz.y() * M_PI / _openParam ); break;
		default: r = baseHeight * 0.28; break;
	}
	// auto		   dbg = qDebug() << "StrategieNo3 - Startwerte: w0=" << w0 << "r0=" << r;
	Intersector< QRectF, QPointF > usedSpace;
	BestDelta					   bd;
	usedSpace.reserve( ic );
	auto st = __rdtsc();
	for ( int i = 0; i < ic; ++i )
	{
		data[ i ] = { r, w };
		if ( i < ic - 1 )
		{
			// Basisdaten berechnen/beschaffen: sin / cos, ableitung, Mittelpunkt, nächste Box-Größe
			auto sc = qSinCos( qDegreesToRadians( w ) ), c = r * sc, dt = sc.transposed() * csDDt,
				 nxt_sz = fromSize( items[ i + 1 ].size() );
			if ( i == 0 ) usedSpace.add( { c - 0.5 * sz, c + 0.5 * sz } );
			// suchen wir nach dem nächsten möglichen Winkel:
			bd.init( direction, w );
			auto offset = 0.5 * ( sz + nxt_sz ) + QPointF{ ds, ds };
			for ( auto v : { c + offset, c - offset } )
			{
				if ( abs( v.x() ) <= r )
					n = 180 * qAsin( v.x() / r ) / M_PI, bd.addDeg( n ), bd.addDeg( 180 - n );
				if ( abs( v.y() ) <= r )
					n = 180 * qAcos( v.y() / r ) / M_PI, bd.addDeg( n ), bd.addDeg( -n );
			}
			// Jetzt wählen wir einen Winkel aus ...
			qreal delta = bd.best();
			// wir brauchen gleich: die aktualisierte Winkelsumme und den nächsten Mittelpunkt
			ww += qAbs( delta );
			c = r * qSinCos( qDegreesToRadians( w + delta ) );
			// Haben wir ein Delta? Sind wir unter 360° insgesamt?
			// Haben wir keine Überlappung?
			if ( qAbs( delta ) > .001 && ww < _openParam
				 && usedSpace.add( { c - 0.5 * nxt_sz, c + 0.5 * nxt_sz } ) )
			{
				// dbg << "-> selected delta:" << delta;
				w += delta;
				sz = nxt_sz;
			} else {
				// if ( ww >= 360. ) dbg << "=> Circle is overfilled.";
				// else if ( qAbs( delta ) > .001 )
				//	dbg << "=> overlapping items detected @" << usedSpace.lastIntersection;
				// else dbg << "=> no follow-up-angles found.";
				//  keine Winkel übrig?  Ergo keine Platzierung möglich.  Mehr Platz, bitte!
				if ( btc++ > 15 ) // bzw. Abbruch nach 15 Iterationsversuchen.
					break;
				if ( r < baseHeight / 3 ) r += baseHeight / 30;
				else if ( r < baseHeight / 2 ) r += baseHeight / 20;
				else r *= 1.05;
				i  = -1;
				w  = w0;
				ww = 0;
				usedSpace.clear();
				// dbg << "Retry with new r=" << r;
			}
		} else
			/*dbg << "\n\tItem #" << i << QSizeF( sz.x(), sz.y() ) << "@ r=" << r << "w=" << w
				<< "c=" << r * qSinCos( qDegreesToRadians( w ) ) << "\nDONE with calculate_3()."*/
			;
	}
	auto et = __rdtsc();
	qDebug()
		/*dbg */
		<< "Strategie No°3 PLUS took" << ( et - st ) << "cycles, running" << btc
		<< "iterations, final radius =" << r;
	items = usedSpace;
}

Opacities StrategieNo3plus::animateItems( Items& items, qreal progress )
{
	if ( data.count() != items.count() ) calculateItems( items );
	return animate3( items, progress, startAngle(), data );
}
