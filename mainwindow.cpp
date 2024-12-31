// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "mainwindow.h"

#include "qpiemenu.h"

#include <QtWidgets>
#include <ranges>

QColor lerpRGB( Qt::GlobalColor c0, Qt::GlobalColor c1, float t )
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

QPointF qLerp2D( QPointF p0, QPointF p1, qreal t )
{
	auto _p0( _mm_setr_pd( p0.x(), p0.y() ) );
	auto _p1( _mm_setr_pd( p1.x(), p1.y() ) );
	auto _t( _mm_set1_pd( t ) );
	auto res( _mm_fmadd_pd( _t, _p1, _mm_fnmsub_pd( _t, _p0, _p0 ) ) );
	return { res.m128d_f64[ 0 ], res.m128d_f64[ 1 ] };
}

//! [0]
MainWindow::MainWindow()
	: QMainWindow()
	, anim( this, "showTime", nullptr )
{
	setupUi( this );
	sb_dstart->setRange( d_start->minimum(), d_start->maximum() );
	sb_dfaktor->setRange( d_faktor->minimum(), d_faktor->maximum() );
	sb_rmin->setRange( r_min->minimum(), r_min->maximum() );
	sb_rmax->setRange( r_max->minimum(), r_max->maximum() );

	gv->setScene( new QGraphicsScene );
	auto xb = new QToolButton;
	xb->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit ) );
	connect( xb, &QToolButton::clicked, this, &MainWindow::clearItems );
	x_button = gv->scene()->addWidget( xb );
	x_button->setPos( -0.5 * fromSize( xb->sizeHint() ) );
	gv->scene()->addItem( &c3_group );
	//! [1]

	//! [2]
	createActions();
	createMenus();

	QString message = tr( "A context menu is available by right-clicking" );
	statusBar()->showMessage( message );

	auto wire = [ & ]( auto *v1, auto *v2 )
	{
		v2->setRange( v1->minimum(), v1->maximum() );
		v2->setValue( v1->value() );
		connect( v1, SIGNAL( valueChanged( int ) ), v2, SLOT( setValue( int ) ) );
		connect( v2, SIGNAL( valueChanged( int ) ), v1, SLOT( setValue( int ) ) );
	};
	wire( d_start, sb_dstart );
	wire( d_faktor, sb_dfaktor );
	wire( r_min, sb_rmin );
	wire( r_max, sb_rmax );
	anim.setStartValue( dsb_anistartwert->value() );
	anim.setEndValue( dsb_aniendwert->value() );
	anim.setDuration( sl_anidur->value() * 100 );
	connect( dsb_anistartwert, &QDoubleSpinBox::valueChanged, this, &MainWindow::computePositions );
	connect( dsb_aniendwert, &QDoubleSpinBox::valueChanged, this, &MainWindow::computePositions );
	connect( sl_anidur, &QSlider::valueChanged, this, &MainWindow::computePositions );
	connect( this, &MainWindow::showTimeChanged, this, &MainWindow::animatePositions );
	connect( d_start, &QDial::valueChanged, this, &MainWindow::computePositions );
	connect( r_min, &QSlider::valueChanged, this, &MainWindow::computePositions );
	connect( r_max, &QSlider::valueChanged, this, &MainWindow::computePositions );
	connect( d_faktor, &QDial::valueChanged, this, &MainWindow::computePositions );
	connect( rb_minus, &QRadioButton::toggled, this, &MainWindow::computePositions );
	connect( &anim, &QAbstractAnimation::stateChanged, this, &MainWindow::animStateChange );
	connect( cb_PlacementPolicies, &QComboBox::currentIndexChanged, this,
			 &MainWindow::computePositions );
	connect( cb_rstart, &QComboBox::currentIndexChanged, this, &MainWindow::computePositions );
	// reflect manual animation changes live
	connect( tb_fwd, &QPushButton::toggled, this, &MainWindow::switchAnimFwd );
	connect( tb_rev, &QPushButton::toggled, this, &MainWindow::switchAnimRev );
	connect( tb_playPause, &QPushButton::toggled, this, &MainWindow::toggleAnim );


	setWindowTitle( tr( "Menus" ) );
	setMinimumSize( 160, 160 );
	// setWindowState( Qt::WindowFullScreen );
}

qreal smoothStep( qreal t, qreal t0 = 0., qreal t1 = 1. )
{
	auto a = qMax( 0., qMin( 1., ( t - t0 ) / ( t1 - t0 ) ) );
	return a * a * ( 3 - 2 * a );
}

qreal superSmoothStep( qreal t, qreal start_max, qreal end_min, int index, int count )
{
	return smoothStep( t, index * start_max / ( count - 1 ),
					   end_min + ( index + 1 ) * ( 1. - end_min ) / count );
}

qreal superSmoothStep( qreal t, qreal a, int index, int count )
{
	return superSmoothStep( t, a, a, index, count );
}

void MainWindow::setShowTime( qreal newShowTime )
{
	if ( qAbs( newShowTime - lastTime ) > 0.0001 )
	{
		emit showTimeChanged( lastTime = newShowTime );
	}
}

inline void MainWindow::clearItems( bool onlyRecalculate )
{
	if ( onlyRecalculate )
	{
		data.clear();
		data.resize( items.count(), { -1, 0 } );
	} else
		while ( items.count() ) delete items.takeFirst();
	// erst den Zugriff auf die erstellten GraphicsItems entfernen
	while ( c3_items.count() ) delete c3_items.takeLast();
	// dann die Items an sich entfernen.
	while ( c3_group.childItems().count() ) delete c3_group.childItems().takeLast();
}

void MainWindow::animStateChange( QAbstractAnimation::State state )
{
	// reflect animation state in the UI
	QSignalBlocker bf( tb_fwd ), br( tb_rev ), bp( tb_playPause );
	tb_fwd->setChecked( anim.direction() == QAbstractAnimation::Forward );
	tb_rev->setChecked( anim.direction() == QAbstractAnimation::Backward );
	switch ( state )
	{
		case QAbstractAnimation::State::Running: // kind of "started"
			tb_playPause->setChecked( true );
			tb_playPause->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::MediaPlaybackPause ) );
			break;
		default: //  Paused or Stopped
			tb_playPause->setChecked( false );
			tb_playPause->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::MediaPlaybackStart ) );
			break;
	}
}

void MainWindow::animDirectionChange( QAbstractAnimation::Direction newDirection )
{
	QSignalBlocker bf( tb_fwd ), br( tb_rev );
	tb_fwd->setChecked( newDirection == QAbstractAnimation::Forward );
	tb_rev->setChecked( newDirection == QAbstractAnimation::Backward );
}

void MainWindow::toggleAnim( bool on )
{
	// If animation is at end, turn around ...
	if ( on )
	{
		if ( anim.endValue() == anim.currentValue() )
			anim.setDirection( QAbstractAnimation::Backward );
		if ( anim.startValue() == anim.currentValue() )
			anim.setDirection( QAbstractAnimation::Forward );
	}
	// at last, we toggle the animation state.
	switch ( anim.state() )
	{
		case QAbstractAnimation::Paused: anim.resume(); break;
		case QAbstractAnimation::Stopped: anim.start(); break;
		case QAbstractAnimation::Running: anim.pause(); break;
		default: break;
	}
}

void MainWindow::switchAnimFwd( bool on )
{
	auto isFwd{ anim.direction() == QAbstractAnimation::Forward };
	if ( isFwd != on ) // switch directions
	{
		anim.setDirection( on ? QAbstractAnimation::Forward : QAbstractAnimation::Backward );
		tb_rev->setChecked( !on );
		tb_fwd->setChecked( on );
	}
}

void MainWindow::switchAnimRev( bool on )
{
	switchAnimFwd( !on );
}

void MainWindow::menuAbout2show()
{
	if ( auto m = qobject_cast< QMenu * >( QObject::sender() ) )
	{
		auto dbg = qDebug() << "menu about to show:" << m->title();
		// clear items
		clearItems();
		// really move x-button to the center
		auto x	= gv->scene()->items().first();
		auto rx = x->boundingRect();
		rx.moveCenter( { 0, 0 } );
		x->setPos( rx.topLeft() );
		// create items (in reverse order, so the top item is on top)
		for ( auto a : std::ranges::reverse_view( m->actions() ) )
		{
			auto pb = new QPushButton( a->icon(), a->text() );
			items.prepend( gv->scene()->addWidget( pb ) );
			// pb->setVisible( !a->isSeparator() );
		}
		for ( auto i : items )
			dbg << "\n  " << i->widget()->property( "text" ).toString() << i->boundingRect().size();
		anim.setCurrentTime( 0 );
		anim.start();
	}
}

void MainWindow::computePositions()
{
	switch ( cb_PlacementPolicies->currentIndex() )
	{
		case 1: calculate_1(); break;
		case 2: calculate_2(); break;
		case 3: calculate_3(); break;
		default: break;
	}
	animatePositions();
}

void MainWindow::animatePositions()
{
	anim.setStartValue( dsb_anistartwert->value() );
	anim.setEndValue( dsb_aniendwert->value() );
	anim.setDuration( sl_anidur->value() * 100 );
	if ( items.isEmpty() ) return;
	switch ( cb_PlacementPolicies->currentIndex() )
	{
		case 0: animate_0(); break;
		case 2: animate_2(); break;
		case 3: animate_3(); break;
		default: animate_1(); break;
	}
}

auto setItemCenter( QGraphicsWidget *gw, qreal angle, qreal radius )
{
	auto br = gw->boundingRect();
	br.moveCenter( radius * qSinCos( qDegreesToRadians( angle ) ).transposed() );
	gw->setPos( br.topLeft() );
	return br;
}

auto setItemCenter( QGraphicsWidget *gw, QPointF ctr )
{
	auto br = gw->boundingRect();
	br.moveCenter( ctr );
	gw->setPos( br.topLeft() );
	return br;
}

void MainWindow::animate_0()
{
	if ( items.isEmpty() ) return;
	// -> should position the items according to time and stop the animation if no
	// overlaps are left we need some parameters:
	qreal sa = d_start->value();	 // - starting angle -> which Angle for the first Item?
	qreal sd = -20;					 // - starting_delta -> minimum angle difference between items
	qreal md = -240 / items.count(); // - maximum_delta -> maximum angle difference
									 // between items (depends on number of items)
	for ( int i( 0 ); i < items.count(); ++i )
	{
		auto r = qDegreesToRadians( sa + i * ( sd + smoothStep( lastTime / 5 ) * ( md - sd ) ) );
		auto center = QPointF( qSin( r ), qCos( r ) ) * lastTime * d_faktor->value();
		setItemCenter( items[ i ], center );
	}
}

void MainWindow::calculate_1()
{
	if ( items.isEmpty() ) return;
	// Wir beschaffen uns die mittlere Breite
	qreal startRadius{ std::numeric_limits< qreal >::max() };
	switch ( cb_rstart->currentIndex() )
	{
		case 0:
			startRadius = 0.;
			for ( auto i : items ) startRadius += i->size().width();
			startRadius /= items.count();
			break;
		case 1:
			for ( auto i : items )
				startRadius = qMin( startRadius, ( i->size().height() + i->size().width() ) * 0.5 );
			break;
		case 2:
			for ( auto i : items )
				startRadius = qMin( startRadius, ( i->size().height() + i->size().width() ) * 0.5 );
			startRadius = qMin( items.count() * items[ 0 ]->size().height() / 4., startRadius );
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
		auto winkel{ static_cast< qreal >( d_start->value() ) };
		auto min_plus{ static_cast< qreal >( -r_min->value() ) };
		auto max_plus{ static_cast< qreal >( -r_max->value() ) };
		auto br = items.first()->boundingRect();
		while ( idx < items.count() )
		{ // setze das Item auf die berechneten bzw. initialen Werte
			br.moveCenter( radius
						   * QPointF{ qSin( qDegreesToRadians( winkel ) ),
									  qCos( qDegreesToRadians( winkel ) ) } );
			// qDebug() << "Item #" << idx << "@" << winkel << ":" << br;
			data.append( qMakePair( radius, winkel ) );
			items[ idx++ ]->setPos( br.topLeft() );
			// nun haben wir eine bounding Box.  Schauen wir erstmal, ob die des nächsten Elements
			// jene überschneiden würde, wenn das nächste Item nur um den Minimalwinkel verschoben
			// angeordnet werden würde ...
			if ( idx < items.count() )
			{
				auto nbr		= items[ idx ]->boundingRect();
				auto winkelplus = min_plus;
				for ( ; abs( winkelplus ) <= abs( max_plus );
					  winkelplus += ( max_plus - min_plus ) / 128. )
				{
					nbr.moveCenter( radius
									* QPointF{ qSin( qDegreesToRadians( winkel + winkelplus ) ),
											   qCos( qDegreesToRadians( winkel + winkelplus ) ) } );
					if ( !br.intersects( nbr ) ) break; // found next item's position
				}
				if ( br.intersects( nbr ) ) // no final position found? Maybe we need to re-run with
											// a larger radius.
				{
					badRadius = true;
					radius *= 1.05;
					break;
				} else {
					br = nbr;
					winkel += winkelplus;
				}
			}
			if ( badRadius ) break;
		}
	} while ( badRadius );
	anim.setLoopCount( 1 );
}

void MainWindow::animate_1()
{
	if ( items.isEmpty() ) return;
	if ( data.count() != items.count() ) computePositions();
	auto t = getShowTime();
	for ( int i( 0 ); i < items.count(); ++i )
	{
		// smoothstep-Fenster für t: i*1/items.count()
		auto a	 = 0.3;
		auto t_i = superSmoothStep( t, a, i, items.count() );
		auto r	 = data[ i ].first;
		auto w	 = data[ i ].second;
		auto w0	 = d_start->value();
		items[ i ]->setOpacity( t_i );
		setItemCenter( items[ i ], w0 + t_i * ( w - w0 ), qSin( t_i * M_PI_2 ) * r );
	}
	x_button->setOpacity( smoothStep( t, 0., .2 ) );
}

void MainWindow::calculate_2()
{
	if ( items.isEmpty() ) return;
	data.clear();
	// Init-daten brauchen wir ...
	int		direction = rb_minus->isChecked() ? -1 : 1;
	qreal	w0 = d_start->value(), ww = 0;
	qreal	ds	  = style()->pixelMetric( QStyle::PM_LayoutVerticalSpacing );
	QPointF scDDt = { 1., -1. }, sz = fromSize( items.first()->geometry().size() );
	// Variablen
	int		btc( 1 ), ic( items.count() );
	auto	baseHeight =
		( ( items.first()->contentsRect().size().height() + ds ) * items.count() - ds );
	qreal		   r( baseHeight / 4 ), w( w0 ), np, off;
	auto		   dbg = qDebug() << "calculate_2() - Startwerte: w0=" << w0 << "r0=" << r;
	Intersector	   usedSpace;
	QList< qreal > winkelz, delta_w, bad_deltas;
	for ( int i = 0; i < ic; ++i )
	{
		data.append( { r, w } );
		if ( i < ic - 1 )
		{
			// Basisdaten berechnen/beschaffen: sin / cos, ableitung, Mittelpunkt, nächste Box-Größe
			auto sc = qSinCos( qDegreesToRadians( w ) ), c = r * sc, dt = sc.transposed() * scDDt,
				 nxt_sz = fromSize( items[ i + 1 ]->geometry().size() );
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
			dbg << "\n\tItem #" << i << QSizeF( sz.x(), sz.y() ) << "@ r=" << r << "w=" << w
				<< "c=" << c << "next possible angles:" << winkelz << "deltas:" << delta_w;
			// sortierte Liste, wir wollen ja den kleinstmöglichen Schritt gehen
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
				dbg << "-> selected delta:" << delta;
				w += delta;
				sz = nxt_sz;
			} else {
				if ( ww >= 360. ) dbg << "=> Circle is overfilled.";
				else if ( qAbs( delta ) > .001 )
					dbg << "=> overlapping items detected @" << usedSpace.lastIntersection;
				else dbg << "=> no follow-up-angles found.";
				// keine Winkel übrig?  Ergo keine Platzierung möglich.  Mehr Platz, bitte!
				if ( btc++ > 5 ) // bzw. Abbruch nach 5 Iterationsversuchen.
					break;
				if ( r < baseHeight / 3 ) r = baseHeight / 3;
				else if ( r < baseHeight / 2 ) r = baseHeight / 2;
				else r *= 1.05;
				i  = -1;
				w  = w0;
				ww = 0;
				data.clear();
				usedSpace.clear();
				dbg << "Retry with new r=" << r;
			}
		} else
			dbg << "\n\tItem #" << i << QSizeF( sz.x(), sz.y() ) << "@ r=" << r << "w=" << w
				<< "c=" << r * qSinCos( qDegreesToRadians( w ) ) << "\nDONE with calculate_2().";
	}
	anim.setLoopCount( 1 );
}

void MainWindow::animate_2()
{
	if ( items.isEmpty() ) return;
	if ( data.count() != items.count() ) computePositions();
	auto t = getShowTime();
	for ( int i( 0 ); i < items.count(); ++i )
	{
		// smoothstep-Fenster für t: i*1/items.count()
		auto a	 = 0.3;
		auto t_i = superSmoothStep( t, a, i, items.count() );
		auto r	 = data[ i ].first;
		auto w	 = data[ i ].second;
		auto w0	 = d_start->value();
		items[ i ]->setOpacity( t_i );
		auto br = items[ i ]->boundingRect();
		br.moveCenter( qSin( t_i * M_PI_2 ) * r
					   * qSinCos( qDegreesToRadians( w0 + t_i * ( w - w0 ) ) ) );
		items[ i ]->setGeometry( br );
	}
	x_button->setOpacity( smoothStep( t, 0., .2 ) );
}

void MainWindow::calculate_3()
{
	if ( items.isEmpty() ) return;
	// falls wir init-daten brauchen ...
	qreal		wx, wy, w0 = d_start->value(), w, dw_ges;
	qreal		di = r_min->value();
	qreal		da = r_max->value();
	qreal		f0 = d_faktor->value();
	qreal		is = style()->pixelMetric( QStyle::PM_LayoutHorizontalSpacing );
	qreal		d  = rb_minus->isChecked() ? -1. : 1.;
	Intersector usedSpace;
	//---------------------------------------------------------------
	clearItems( true );
	// Start-Radius: ein guter Ratewert könnte das hier sein...
	qreal	 r0 = ( items.first()->size().height() + is ) * items.count() / 3, r = r0;
	// Und los geht's ...
	auto	 dbg = qDebug() << "calculate_3:";
	QPointF	 anstieg, ch, cv, sc;
	QRectF	 bhv, bvh, br1, br;
	int		 rc( 0 );
	c3_item *currentRoot{ nullptr };
	for ( int i = 0; i < items.count(); ++i )
	{
		if ( i == 0 ) // Initialisierung, Zählung der Iterationen, Platzierung erstes Item
		{
			++rc, dw_ges = 0;
			currentRoot = c3_items.emplaceBack( new c3_item );
			br			= items.first()->geometry();
			br.moveCenter( r
						   * ( sc = qSinCos( qDegreesToRadians( ( w = 0 ) + w0 ) ) ).transposed() );
			auto ti = gv->scene()->addRect( br );
			c3_group.addToGroup( ti );
			auto ci = new QGraphicsSimpleTextItem(
				items[ i ]->widget()->property( "text" ).toString(), ti );
			ci->setBrush( palette().brush( QPalette::ButtonText ) );
			currentRoot->addItem( ti )->good = true;
			usedSpace.clear(), usedSpace.add( br );
		}
		// aktuelle Werte speichern
		data[ i ] = { r, normDegrees( w + w0 ) };
		dbg << "\n\titem #" << i << items[ i ]->size() << "to" << data[ i ] << "@" << br.center();
		if ( i < items.count() - 1 ) // Berechnung der nächsten Box:
		{
			bhv = bvh = br1 = items[ i + 1 ]->geometry(); // Daten des nächsten Items beschaffen
			auto txt		= items[ i + 1 ]->widget()->property( "text" ).toString();
			// Anstieg in Öffnungsrichtung
			anstieg			= sc * QPointF{ -d, d }; // entsprechend cos|sin d/dt => -sin | cos
			bool	 hasH, hasV;
			c3_item *bhv_it, *bvh_it;
			dbg << "nextBox:" << br1.size() << "anstieg:" << anstieg;
			// 2 Situationen erstellen: daneben bzw. darüber/darunter -> dazu ein lambda ...
			auto box = [ & ]( QRectF &box, c3_item *&box_item, QPointF anstieg, bool &box_solves,
							  double direction, const char *box_text, Qt::Orientation box_ori )
			{
				auto box_baseAngle = [ & ]()
				{
					return qRadiansToDegrees( box_ori == Qt::Horizontal
												  ? qAcos( box.center().x() / r )
												  : qAsin( box.center().y() / r ) );
				};
				auto box_flipAngle = [ & ]()
				{ return box_ori == Qt::Horizontal ? -box_baseAngle() : 180 - box_baseAngle(); };
				box = br1;
				box.moveCenter( br.center() );
				box.translate(
					0.5 * richtung( anstieg )
					* ( fromSize( br ) + /*2 * */ QPointF{ is, is } + fromSize( br1 ) ) );
				double nw = w;
				auto   ri = gv->scene()->addRect( box );
				ri->setBrush( palette().brush( QPalette::Button ) );
				auto ni = new QGraphicsSimpleTextItem( txt, ri );
				ni->setBrush( palette().brush( QPalette::ButtonText ) );
				c3_group.addToGroup( ri ); // correct parenting for those items
				//				c3_group.addToGroup( ni ); // correct parenting for those items
				box_item = currentRoot->addItem( ri );
				box_item->centers.append( box.center() );
				dbg << "\n\t\tadded" << box_text << "box @" << box.center();
				if ( box_solves =
						 abs( box_ori == Qt::Horizontal ? box.center().x() : box.center().y() )
						 <= r )
				{
					box_item->good = true;
					nw			   = box_baseAngle();
					auto nw1	   = box_flipAngle();
					auto d0 = degreesDistance( w + w0, nw ), d1 = degreesDistance( w + w0, nw1 );
					auto dd0{ d0 * direction > 0. }, dd1{ d1 * direction > 0. };
					if ( !dd1 && !dd0 ) box_solves = box_item->good = false;
					else if ( dd1 && dd0 )
					{
						dbg << "gives 2 results:" << nw << normDegrees( nw1 );
						if ( abs( d1 ) < abs( d0 ) ) nw = normDegrees( nw1 );
						dbg << "using" << nw;
					} else dbg << "gives result:" << ( nw = normDegrees( dd1 ? nw1 : nw ) );
					// Neuen Zentrumspunkt nach Verschiebungsauswahl hinzugefügt.
					box_item->centers.append( r * qSinCos( nw ).transposed() );
				}
				dbg << ( box_item->good ? "-> good box." : "-> bad box." );
				return nw;
			};

			wx = box( bhv, bhv_it, { anstieg.x(), 0 }, hasH, d, "horizontal-vertical",
					  Qt::Horizontal );
			wy = box( bvh, bvh_it, { 0., anstieg.y() }, hasV, d, "vertical-horizontal",
					  Qt::Vertical );

			// Haben wir keine Lösungen gefunden? FALLBACK: gehen wir doch mal in die andere
			// Richtung
			if ( !hasH && !hasV )
			{
				// Jetzt suchen wir mit umgedrehtem Anstieg:
				wx = box( bhv, bhv_it, { -anstieg.x(), 0 }, hasH, d, "-(horizontal-vertical)",
						  Qt::Horizontal );
				wy = box( bvh, bvh_it, { 0., -anstieg.y() }, hasV, d, "-(vertical-horizontal)",
						  Qt::Vertical );
			}
			// boxes are built.
			dbg << "\n\t=>";
			if ( hasV || hasH )
			{
				int which( !hasH   ? 1
						   : !hasV ? 0
								   : d * degreesDistance( w + w0, wx )
										 > d * degreesDistance( w + w0, wy ) );
				dbg << "results:";
				( ( hasV && hasH ) ? ( dbg << wx << wy ) : ( hasH ? dbg << wx : dbg << wy ) );
				qreal nw;
				switch ( which ) // Super - welche haben wir jetzt gewählt?
				{
					case 0: nw = normDegrees( wx - w0 ); break;
					case 1: nw = normDegrees( wy - w0 ); break;
				}
				if ( ( dw_ges += qAbs( nw - w ) ) < 360 )
				{
					sc = qSinCos( ( w = nw ) + w0 );
					br1.moveCenter( r * sc.transposed() );
					if ( usedSpace.add( br1 ) )
					{
						br = br1;
						dbg << "best next angle:" << w;
						continue;
					} else dbg << "intersection detected!";
				} else dbg << "circle overflow detected!";
			} else dbg << "no solution found!";
			// increase radius, restart loop
			if ( qAbs( r - r0 ) < 0.0001 ) r = r0 * 1.5; // make a bigger first step!
			else if ( qAbs( r - r0 * 1.5 ) < 0.0001 ) r = r0 * 2;
			else r *= 1.05;
			i = -1;
			dbg << "-> change radius to" << r;
		}
		if ( rc > 5 )
		{
			dbg << "\nABORT calculation - we get no solution, you fucked up!";
			break;
		}
	}
	anim.setLoopCount( rc + 1 );
}

void setOpa( QGraphicsItem *r, double opa )
{
	if ( r )
	{
		auto l = r->childItems();
		l.prepend( r );
		for ( auto i : l ) i->setOpacity( opa );
	}
}

void moveCenter( QGraphicsItem *i, QPointF center ) {}

void MainWindow::animate_3()
{
	// Die Animation von Variante 3 zum Verstehen:
	//  -> wir können mehrere Loops haben, weil wir ggf. mehrfach den Radius des Menükreises
	//  anpassen mussten.
	//  -> jede Loop wird unterteilt in die Items, die sie beinhaltet, außer der letzten (welche
	//  ja hoffentlich eine finale Lösung des Menüs darstellen sollte), die läuft wie die 1er
	//  anim ab.

	if ( items.isEmpty() ) return;
	if ( data.count() != items.count() ) computePositions();
	auto t	= getShowTime();
	auto cl = anim.currentLoop();
	if ( cl < c3_items.count() )
	{
		auto root = c3_items.at( cl );
		auto ic = root->count(), si = root->countSimples(), i = 0;
		do {
			auto t_i = superSmoothStep( t, 0.7, 0.3, i, ic - si );
			if ( root->item )
			{
				if ( root->centers.isEmpty() ) // einfaches Item (Box und Text z.B.)
				{							   // -> schnell Einblenden (0.2*d_loop)
					setOpa( root->item, smoothStep( t, 0., 0.2 ) * smoothStep( t, 1., 0.8 ) );
				} else {
					// Animiere ein Item - es beginnt als eine Box oder ein Pfeil,
					// t_i: [0..1]
					auto dti = t_i * qMax( 2, root->centers.count() + 1 ); // [0..2+]
					auto ii	 = int( floor( dti ) );						   // current center index
					auto ti	 = qMin( qMax( dti - ii, 1. ), 0. );
					auto sz	 = root->item->boundingRect().size();
					switch ( ii )
					{
						case 0: // Das erste "center" positioniert das Item, diese Phase ist ein
								// Blend-in
							root->item->setPos( root->centers.at( ii ) - 0.5 * fromSize( sz ) );
							setOpa( root->item, ti );
							root->item->setBrush( palette().brush( QPalette::Button ) );
							break;
						case 1: // second part: go into green or red color
							root->item->setBrush(
								lerpRGB( Qt::gray, root->good ? Qt::green : Qt::red, ti ) );
							[[fallthrough]];
						default:
							// in case of a "good solution", also move to the next center
							if ( ii < root->centers.count() )
								root->item->setPos( qLerp2D( root->centers.at( ii - 1 ),
															 root->centers.at( ii ), ti )
													- 0.5 * fromSize( sz ) );
							else setOpa( root->item, 1. - ti ); // fade out all items
							break;
					}
					++i;
				}
			}
		} while ( ( root = root->next ) );
	} else {
		for ( int i( 0 ); i < items.count(); ++i )
		{
			// smoothstep-Fenster für t: i*1/items.count()
			auto a	 = 0.3;
			auto t_i = superSmoothStep( t, a, i, items.count() );
			auto r	 = data[ i ].first;
			auto w	 = data[ i ].second;
			auto w0	 = d_start->value();
			items[ i ]->setOpacity( t_i );
			setItemCenter( items[ i ], w0 + t_i * ( w - w0 ), qSin( t_i * M_PI_2 ) * r );
		}
	}
	x_button->setOpacity( smoothStep( t ) );
}

void MainWindow::newFile()
{
	infoLabel->setText( tr( "Invoked <b>File|New</b>" ) );
}

void MainWindow::open()
{
	infoLabel->setText( tr( "Invoked <b>File|Open</b>" ) );
}

void MainWindow::save()
{
	infoLabel->setText( tr( "Invoked <b>File|Save</b>" ) );
}

void MainWindow::print()
{
	infoLabel->setText( tr( "Invoked <b>File|Print</b>" ) );
}

void MainWindow::undo()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Undo</b>" ) );
}

void MainWindow::redo()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Redo</b>" ) );
}

void MainWindow::cut()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Cut</b>" ) );
}

void MainWindow::copy()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Copy</b>" ) );
}

void MainWindow::paste()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Paste</b>" ) );
}

void MainWindow::bold()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Bold</b>" ) );
}

void MainWindow::italic()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Italic</b>" ) );
}

void MainWindow::leftAlign()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Left Align</b>" ) );
}

void MainWindow::rightAlign()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Right Align</b>" ) );
}

void MainWindow::justify()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Justify</b>" ) );
}

void MainWindow::center()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Center</b>" ) );
}

void MainWindow::setLineSpacing()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Set Line Spacing</b>" ) );
}

void MainWindow::setParagraphSpacing()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Set Paragraph Spacing</b>" ) );
}

void MainWindow::about()
{
	infoLabel->setText( tr( "Invoked <b>Help|About</b>" ) );
	QMessageBox::about( this,
						tr( "About Menu" ),
						tr( "The <b>Menu</b> example shows how to create "
							"menu-bar menus and context menus." ) );
}

void MainWindow::aboutQt()
{
	infoLabel->setText( tr( "Invoked <b>Help|About Qt</b>" ) );
}

void MainWindow::contextMenuEvent( QContextMenuEvent *event )
{
	QPieMenu menu( this );
	menu.addAction( cutAct );
	menu.addAction( copyAct );
	menu.addAction( pasteAct );
	menu.exec( event->globalPos() );
}

//! [4]
void MainWindow::createActions()
{
	//! [5]
	newAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentNew ), tr( "&New" ), this );
	newAct->setShortcuts( QKeySequence::New );
	newAct->setStatusTip( tr( "Create a new file" ) );
	connect( newAct, &QAction::triggered, this, &MainWindow::newFile );
	//! [4]

	openAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentOpen ), tr( "&Open..." ), this );
	openAct->setShortcuts( QKeySequence::Open );
	openAct->setStatusTip( tr( "Open an existing file" ) );
	connect( openAct, &QAction::triggered, this, &MainWindow::open );
	//! [5]

	saveAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentSave ), tr( "&Save" ), this );
	saveAct->setShortcuts( QKeySequence::Save );
	saveAct->setStatusTip( tr( "Save the document to disk" ) );
	connect( saveAct, &QAction::triggered, this, &MainWindow::save );

	printAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentPrint ), tr( "&Print..." ), this );
	printAct->setShortcuts( QKeySequence::Print );
	printAct->setStatusTip( tr( "Print the document" ) );
	connect( printAct, &QAction::triggered, this, &MainWindow::print );

	exitAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit ), tr( "E&xit" ), this );
	exitAct->setShortcuts( QKeySequence::Quit );
	exitAct->setStatusTip( tr( "Exit the application" ) );
	connect( exitAct, &QAction::triggered, this, &QWidget::close );

	undoAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditUndo ), tr( "&Undo" ), this );
	undoAct->setShortcuts( QKeySequence::Undo );
	undoAct->setStatusTip( tr( "Undo the last operation" ) );
	connect( undoAct, &QAction::triggered, this, &MainWindow::undo );

	redoAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditRedo ), tr( "&Redo" ), this );
	redoAct->setShortcuts( QKeySequence::Redo );
	redoAct->setStatusTip( tr( "Redo the last operation" ) );
	connect( redoAct, &QAction::triggered, this, &MainWindow::redo );

	cutAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditCut ), tr( "Cu&t" ), this );
	cutAct->setShortcuts( QKeySequence::Cut );
	cutAct->setStatusTip(
		tr( "Cut the current selection's contents to the "
			"clipboard" ) );
	connect( cutAct, &QAction::triggered, this, &MainWindow::cut );

	copyAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditCopy ), tr( "&Copy" ), this );
	copyAct->setShortcuts( QKeySequence::Copy );
	copyAct->setStatusTip(
		tr( "Copy the current selection's contents to the "
			"clipboard" ) );
	connect( copyAct, &QAction::triggered, this, &MainWindow::copy );

	pasteAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditPaste ), tr( "&Paste" ), this );
	pasteAct->setShortcuts( QKeySequence::Paste );
	pasteAct->setStatusTip(
		tr( "Paste the clipboard's contents into the current "
			"selection" ) );
	connect( pasteAct, &QAction::triggered, this, &MainWindow::paste );

	boldAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatTextBold ), tr( "&Bold" ), this );
	boldAct->setCheckable( true );
	boldAct->setShortcut( QKeySequence::Bold );
	boldAct->setStatusTip( tr( "Make the text bold" ) );
	connect( boldAct, &QAction::triggered, this, &MainWindow::bold );

	QFont boldFont = boldAct->font();
	boldFont.setBold( true );
	boldAct->setFont( boldFont );

	italicAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatTextItalic ),
							 tr( "&Italic" ), this );
	italicAct->setCheckable( true );
	italicAct->setShortcut( QKeySequence::Italic );
	italicAct->setStatusTip( tr( "Make the text italic" ) );
	connect( italicAct, &QAction::triggered, this, &MainWindow::italic );

	QFont italicFont = italicAct->font();
	italicFont.setItalic( true );
	italicAct->setFont( italicFont );

	setLineSpacingAct = new QAction( tr( "Set &Line Spacing..." ), this );
	setLineSpacingAct->setStatusTip(
		tr( "Change the gap between the lines of a "
			"paragraph" ) );
	connect( setLineSpacingAct, &QAction::triggered, this, &MainWindow::setLineSpacing );

	setParagraphSpacingAct = new QAction( tr( "Set &Paragraph Spacing..." ), this );
	setParagraphSpacingAct->setStatusTip( tr( "Change the gap between paragraphs" ) );
	connect( setParagraphSpacingAct, &QAction::triggered, this, &MainWindow::setParagraphSpacing );

	aboutAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::HelpAbout ), tr( "&About" ), this );
	aboutAct->setStatusTip( tr( "Show the application's About box" ) );
	connect( aboutAct, &QAction::triggered, this, &MainWindow::about );

	aboutQtAct = new QAction( tr( "About &Qt" ), this );
	aboutQtAct->setStatusTip( tr( "Show the Qt library's About box" ) );
	connect( aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt );
	connect( aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt );

	leftAlignAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyLeft ),
								tr( "&Left Align" ), this );
	leftAlignAct->setCheckable( true );
	leftAlignAct->setShortcut( tr( "Ctrl+L" ) );
	leftAlignAct->setStatusTip( tr( "Left align the selected text" ) );
	connect( leftAlignAct, &QAction::triggered, this, &MainWindow::leftAlign );

	rightAlignAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyRight ),
								 tr( "&Right Align" ), this );
	rightAlignAct->setCheckable( true );
	rightAlignAct->setShortcut( tr( "Ctrl+R" ) );
	rightAlignAct->setStatusTip( tr( "Right align the selected text" ) );
	connect( rightAlignAct, &QAction::triggered, this, &MainWindow::rightAlign );

	justifyAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyFill ),
							  tr( "&Justify" ), this );
	justifyAct->setCheckable( true );
	justifyAct->setShortcut( tr( "Ctrl+J" ) );
	justifyAct->setStatusTip( tr( "Justify the selected text" ) );
	connect( justifyAct, &QAction::triggered, this, &MainWindow::justify );

	centerAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyCenter ),
							 tr( "&Center" ), this );
	centerAct->setCheckable( true );
	centerAct->setShortcut( tr( "Ctrl+E" ) );
	centerAct->setStatusTip( tr( "Center the selected text" ) );
	connect( centerAct, &QAction::triggered, this, &MainWindow::center );

	//! [6] //! [7]
	alignmentGroup = new QActionGroup( this );
	alignmentGroup->addAction( leftAlignAct );
	alignmentGroup->addAction( rightAlignAct );
	alignmentGroup->addAction( justifyAct );
	alignmentGroup->addAction( centerAct );
	leftAlignAct->setChecked( true );
	//! [6]
}
//! [7]

//! [8]
void MainWindow::createMenus()
{
	//! [9] //! [10]
	fileMenu = menuBar()->addMenu( tr( "&File" ) );
	fileMenu->addAction( newAct );
	//! [9]
	fileMenu->addAction( openAct );
	//! [10]
	fileMenu->addAction( saveAct );
	fileMenu->addAction( printAct );
	//! [11]
	fileMenu->addSeparator();
	//! [11]
	fileMenu->addAction( exitAct );
	connect( fileMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	editMenu = menuBar()->addMenu( tr( "&Edit" ) );
	editMenu->addAction( undoAct );
	editMenu->addAction( redoAct );
	editMenu->addSeparator();
	editMenu->addAction( cutAct );
	editMenu->addAction( copyAct );
	editMenu->addAction( pasteAct );
	editMenu->addSeparator();
	connect( editMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	helpMenu = menuBar()->addMenu( tr( "&Help" ) );
	helpMenu->addAction( aboutAct );
	helpMenu->addAction( aboutQtAct );
	//! [8]
	connect( helpMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	//! [12]
	formatMenu = editMenu->addMenu( tr( "&Format" ) );
	formatMenu->addAction( boldAct );
	formatMenu->addAction( italicAct );
	formatMenu->addSeparator()->setText( tr( "Alignment" ) );
	formatMenu->addAction( leftAlignAct );
	formatMenu->addAction( rightAlignAct );
	formatMenu->addAction( justifyAct );
	formatMenu->addAction( centerAct );
	formatMenu->addSeparator();
	formatMenu->addAction( setLineSpacingAct );
	formatMenu->addAction( setParagraphSpacingAct );
	connect( formatMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );
}
//! [12]
