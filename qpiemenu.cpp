#include "qpiemenu.h"

#include <QApplication>
#include <QEvent>
#include <QMetaProperty>
#include <QPaintEvent>
#include <QPainterPath>
#include <QStyleOptionMenuItem>
#include <QStylePainter>
#include <QWidgetAction>
#if _WIN32
#	pragma comment( lib, "dwmapi.lib" )
#	include "dwmapi.h"
#endif

QPieMenu::QPieMenu( const QString &title, QWidget *parent )
	: QMenu( title, parent )
{
	// Tearing this off would not be a good idea!
	setTearOffEnabled( false );
	qApp->setEffectEnabled( Qt::UI_AnimateMenu, false );
	setWindowFlag( Qt::FramelessWindowHint );
	setAttribute( Qt::WA_TranslucentBackground );
	// initialize style-dependent data
	readStyleData();

	qDebug() << "QPieMenu" << title << "created" << this;
}

QPieMenu::~QPieMenu()
{
	qDebug() << "QPieMenu destroying" << this;
}

QSize QPieMenu::sizeHint() const
{
	return _actionPieData._boundingRect.size();
}

int QPieMenu::actionIndexAt( const QPoint &pt ) const
{
	// Elegant way of checking this: backwards.
	// If no elements are left, the loop counter already contains the result.
	int i( _actionRects.count() - 1 );
	do
		if ( _actionRects[ i ].contains( pt ) ) return i;
	while ( 0 < i-- );
	return i;
}

bool QPieMenu::event( QEvent *e )
{
	switch ( e->type() )
	{
		case QEvent::ApplicationPaletteChange: [[fallthru]];
		case QEvent::PaletteChange: [[fallthru]];
		case QEvent::StyleChange: readStyleData(); break;
#if _WIN32
			// Ein Drop-Shadow um das Menu herum sieht nicht brauchbar aus, deshalb
			// entferne ich zumindest unter Windows den DropShadow tief in der WinAPI
		case QEvent::WinIdChange:
			{
				HWND hwnd = reinterpret_cast< HWND >( winId() );
				if ( _dropShadowRemoved != hwnd )
				{
					DWORD class_style = ::GetClassLong( hwnd, GCL_STYLE );
					class_style &= ~CS_DROPSHADOW;
					::SetClassLong( hwnd, GCL_STYLE, class_style );
					_dropShadowRemoved = hwnd;
				}
				return true;
			}
#endif
#ifdef DEBUG_EVENTS
		// Events, die ich nicht in der Ausgabe sehen möchte:
		case QEvent::MouseMove:
		case QEvent::Paint:
		case QEvent::Show:
		case QEvent::ShowToParent:
		case QEvent::ActionAdded:
		case QEvent::ActionRemoved:
		case QEvent::ActionChanged:
		case QEvent::Timer:
		case QEvent::UpdateRequest:
			/* einfach mal nix tun */ break;
		// übrige Events zum Debugger rausschicken
		default: qDebug() << "QPieMenu::event(" << e << ") passed down..."; break;
#else
		default: break;
#endif
	}
	return QMenu::event( e );
}

void QPieMenu::actionEvent( QActionEvent *event )
{
	// Egal was das Event besagt, unsere Actions haben sich invalidisiert.
	calculatePieDataSizes();
	calculateStillData();
	_actionRects	  = _actionPieData; // operator QList<QRect>
	_actionRectsDirty = false;
	updateGeometry();
	event->accept();
	// QMenuPrivate at least needs to know about whatever they think is needed
	QMenu::actionEvent( event );
}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	updateCurrentVisuals();
	QStylePainter p( this );
	p.translate( -_actionPieData._boundingRect.topLeft() );
	QStyleOptionMenuItem opt;
	auto				 ac = actions().count();
	// Folgen: Sektorhighlighting
	if ( _folgeId != -1 || _folgenAnimiert )
	{
		if ( _folgenAnimiert )
		{
			auto ct = _folgeBeginn.msecsTo( QTime::currentTime() );
			auto t	= qreal( ct ) / _folgeDauer;
			// Wenn 1 erreicht ist, können wir die Animation abschalten
			if ( t >= 1. )
				_folge = _folgeStop, ereignis( PieMenuEreignis::zeitanimation_abgeschlossen,
											   reinterpret_cast< qint64 >( &_folgenAnimiert ) );
			else _folge( t, _folgeStart, _folgeStop );
			// qDebug() << "animated:" << ct << "of" << _folgeDauer << "ms elapsed -> t=" << t
			//		 << _folgeId << _folge;
		}
		// Das war's schon zum Ein- und Ausblenden.  Nun zeichnen wir den aktuellen Zustand.
		DistArc da( _folge.first );
		// 4 Eckpunkte bekommen wir recht einfach hin ...
		auto	sc0 = qSinCos( da.a0() ), sc1 = qSinCos( da.a1() ), sc = qSinCos( da.a() );
		QPen	pen( p.pen() );
		p.setPen( _folge.second );
		p.drawLine( {}, da.r1() * sc );
		p.drawChord( -da.r1(), -da.r1(), 2 * da.r1(), 2 * da.r1(), da.a0(), da.a0() + da.d() );

		// QPainterPath path;
		// path.lineTo( da.r1() * sc ); // sollte zum Mittelpunkt des Elementes gehen
		// path.lineTo( da.r1() * sc0 );
		// path.lineTo( da.r0() * sc0 );
		// path.lineTo( da.r0() * sc1 );
		// path.lineTo( da.r1() * sc1 );
		// path.lineTo( da.r1() * sc );
		//  QConicalGradient gradient( {}, qRadiansToDegrees( om ) );
		//  gradient.setColorAt( 0., _styleData.HL );
		//  gradient.setColorAt( qAbs( dw1 * M_2_PI ), _styleData.HLtransparent );
		//  gradient.setColorAt( qAbs( dw2 * M_2_PI ), _styleData.HLtransparent );
		//  gradient.setColorAt( 1., _styleData.HL );
		// p.strokePath( path, _folge.second );
		// p.fillPath( path, gradient );
	}
	// SelectionRect
	if ( _selRect.first.isValid() )
	{
		// p.setOpacity( 1. ); // In der Hoffnung, dass der Alpha-Wert der Brush berücksichtigt
		// wird...
		auto r =
			qMin( qreal( _styleData.sp ), qMin( _selRect.first.width(), _selRect.first.height() ) );
		p.setPen( Qt::NoPen );
		p.setBrush( _selRect.second );
		p.drawRoundedRect( _selRect.first, r, r );
	}
	// Zum Schluss die Elemente drüberbügeln
	for ( int i( 0 ); i < ac; ++i )
	{
		initStyleOption( &opt, actions().at( i ) );
		opt.state.setFlag( QStyle::State_Selected, ( i == _hoverId ) && !_selRectAnimiert );
		opt.rect = _actionRects[ i ].marginsAdded( _styleData.menuMargins );
		p.setOpacity( _actionRenderData[ i ].x() );
		// ToDo: Benutze den Skalierfaktor "_actionRenderData[ i ].y()" korrekt.
		//  z.B.:
		opt.font.setPointSizeF( opt.font.pointSizeF() * _actionRenderData[ i ].y() );
		p.drawPrimitive( QStyle::PE_PanelMenu, opt );
		opt.rect = opt.rect.marginsRemoved( _styleData.menuMargins );
		p.drawControl( QStyle::CE_MenuItem, opt );
	}
}

void QPieMenu::showEvent( QShowEvent *e )
{
	// An dieser Stelle sollten wir die Position zentrieren ...
	updateCurrentVisuals();
	setGeometry( geometry().translated( _actionPieData._boundingRect.topLeft() ) );
	qApp->setEffectEnabled( Qt::UI_FadeMenu, false );
	ereignis( PieMenuEreignis::getsShown );
	QMenu::showEvent( e );
}

void QPieMenu::mouseMoveEvent( QMouseEvent *e )
{
	if ( !isVisible() ) return;

	// Finde die nächstgelegene Aktion und den Abstand zum Mittelpunkt durch den Hit-Test
	auto  p = e->position().toPoint() + _actionPieData._boundingRect.topLeft();
	qreal d, dm = qSqrt( QPoint::dotProduct( p, p ) ), r = _actionPieData._radius,
			 minmax = qMin( qMax( 10., r / 8 ), ( r < 0.1 ? 1 : 0.8 * r ) );
	int	 id( -1 );
	bool hit		= hitTest( p, d, id );
	// Weiter weg vom Zentrum als das Minimum, aber näher am Element als Radius? -> closeBy
	bool closeBy	= ( dm > minmax ) && ( d <= r ) && ( id != -1 );
	// Falls Ereignisse ausgelöst werden -> schonmal die Position kodieren
	auto posEncoded = ( qint64( p.x() ) << 32 ) | p.y();

	// Ok, jetzt wissen wir Bescheid - erstellen wir die entsprechenden Ereignisse
	// -> der Hit-Test sagt uns, ob wir über einem Item hovern
	if ( hit ) ereignis( PieMenuEreignis::hover_Item_start, id );
	else ereignis( PieMenuEreignis::hover_Item_end, posEncoded );
	// -> der Close-Test hat ergeben, ob der Cursor in der Nähe eines Items ist
	if ( closeBy )
	{
		// Ereignis auslösen: sollte die Animation eingeschaltet werden, werden die letzten
		// Daten des Sektors genutzt -> die stehen seit der letzten Animation in _folge.
		if ( _folgeId != id )
		{
			qDebug() << "ME_FOLGEN_INIT: values before\n\t" << _folgeStart << "\n\t" << _folge
					 << "\n\t" << _folgeStop;
			DistArc a( _folgeStop.first );
			// Und nun die Daten bereitstellen
			auto	ac = _actionPieData.count();
			a.a()	   = _actionPieData[ id ]._angle;
			// haben wir eine oder 2 Winkeldifferenzen? -> baue daraus einen Faktor
			auto dwf   = 0.5 / ( 1. + ( id > 0 && id + 1 < ac ) );
			// Das sollte dann die halbe mittlere Winkeldifferenz werden:
			a.d()	   = ( ( id > 0 ? qAbs( _actionPieData[ id - 1 ]._angle - a.a() ) : 0 )
					   + ( id + 1 < ac ? qAbs( _actionPieData[ id + 1 ]._angle - a.a() ) : 0 ) )
					* dwf;
			a.r0()			  = minmax;
			a.r1()			  = r;
			_folgeStop.second = _styleData.HL;
			qDebug() << "ME_FOLGEN_INIT: stop calculated=" << _folgeStop;
			// Falls vor der Animation komplett abgeschaltet war, sollte der Mittenwinkel konstant
			// bleiben ... dann geht der Bogen auf ;)
			if ( _folgeId == -1 && !_folgenAnimiert )
			{
				DistArc s( _folge.first );
				s.a() = a.a(), s.d() = 0.;
				s.r0() = a.r0(), s.r1() = a.r1();
			}
			qDebug() << "ME_FOLGEN_INIT: before ereignis - currentValue =" << _folge;
			ereignis( PieMenuEreignis::distance_closing_in, id );
			qDebug() << "ME_FOLGEN_INIT: finished:\n\t" << _folgeStart << "\n\t" << _folge << "\n\t"
					 << _folgeStop;
		}
		// Ansonsten ist nur die Farbe zu aktualisieren - die soll ja von der Nähe abhängig sein.
		// der Alpha-Wert der Farbe sollte von der Entfernung zum Item abhängig sein ...
		_folgeStop.second.setAlphaF( smoothStep( d, r, r * 0.5 ) );
		if ( !_folgenAnimiert ) update();
	} else ereignis( PieMenuEreignis::distance_leaving );
	e->accept();
}

void QPieMenu::timerEvent( QTimerEvent *e )
{
	if ( e->timerId() == _timerId ) ereignis( PieMenuEreignis::animate ), e->accept();
}

void QPieMenu::readStyleData()
{
	auto				 style = QWidget::style();
	QStyleOptionMenuItem mopt;
	mopt.initFrom( this );
	_styleData.panelWidth = _styleData.fw =
		style->pixelMetric( QStyle::PM_MenuPanelWidth, &mopt, this );
	_styleData.deskFw = style->pixelMetric( QStyle::PM_MenuDesktopFrameWidth, &mopt, this );
	_styleData.hmarg  = style->pixelMetric( QStyle::PM_MenuHMargin, &mopt, this );
	_styleData.vmarg  = style->pixelMetric( QStyle::PM_MenuVMargin, &mopt, this );
	_styleData.menuMargins =
		QMargins{ _styleData.vmarg, _styleData.vmarg, _styleData.hmarg, _styleData.vmarg };
	_styleData.icone = style->pixelMetric( QStyle::PM_SmallIconSize, &mopt, this );
	_styleData.sp =
		qMin( style->pixelMetric( QStyle::PM_LayoutHorizontalSpacing, &mopt, this ),
			  qMax( 3, style->pixelMetric( QStyle::PM_ToolBarItemSpacing, &mopt, this ) ) );
	_styleData.HL = _styleData.HLtransparent = palette().color( QPalette::Highlight );
	_styleData.HL.setAlphaF( _initData._selRectAlpha );
	_styleData.HLtransparent.setAlphaF( 0.0 );
	_selRect.second = _hoverId == -1 ? _styleData.HLtransparent : _styleData.HL;
}

void QPieMenu::calculatePieDataSizes()
{
	// Alle Größen sind voneinander abhängig, wenn wir einen konsistenten Stil (wie ihn Menüs nunmal
	// haben sollten) mit zumindest gleichem Tabstopp verwenden wollen.  Da wir alle anfassen
	// müssen, können wir auch gleich immer alle berechnen.
	QStyleOptionMenuItem opt;
	QAction				*action;
	QSize				 sz, allSz;
	int					 tab = 0, noSzItems = 0;
	// Schritt #1: Tabstopp suchen
	for ( int i( 0 ); i < actions().count(); ++i )
	{
		action = actions().at( i );
		initStyleOption( &opt, action );
		const bool isSection =
			action->isSeparator() && ( !action->text().isEmpty() || !action->icon().isNull() );
		const bool isLücke =
			( isSection && !style()->styleHint( QStyle::SH_Menu_SupportsSections ) )
			|| ( action->isSeparator() && !isSection ) || !action->isVisible();
		auto w	= qobject_cast< QWidgetAction * >( action );
		auto ww = w ? w->defaultWidget() : nullptr;
		if ( ww == nullptr && !isLücke )
		{
			// Keine Widget-Action, keine Lücke: also Text und Icon
			const QFontMetrics &fm = opt.fontMetrics;
			auto				s  = action->text();
			auto				t  = s.indexOf( u'\t' );
			if ( t != -1 )
			{
				tab = qMax( tab, fontMetrics().horizontalAdvance( s.mid( t + 1 ) ) );
				s	= s.left( t );
			} else if ( action->isShortcutVisibleInContextMenu() || !_initData._isContext ) {
				QKeySequence seq = action->shortcut();
				if ( !seq.isEmpty() )
					tab = qMax( tab,
								fm.horizontalAdvance( seq.toString( QKeySequence::NativeText ) ) );
			}
		}
	}
	// Schritt #2: Größen (nach)berechnen
	bool previousWasSeparator = true;
	_actionPieData.resizeForOverwrite( actions().count() );
	_actionRenderData.resizeForOverwrite( actions().count() );
	for ( int i( 0 ); i < actions().count(); ++i )
	{
		action = actions().at( i );
		initStyleOption( &opt, action );
		const bool isSection =
			action->isSeparator() && ( !action->text().isEmpty() || !action->icon().isNull() );
		const bool isPlainSep =
			( isSection && !style()->styleHint( QStyle::SH_Menu_SupportsSections ) )
			|| ( action->isSeparator() && !isSection );
		sz = QSize(); // invalidisieren!
		if ( !action->isVisible()
			 || ( separatorsCollapsible() && previousWasSeparator && isPlainSep ) )
			; // Action bekommt keine Größe - ist aus der Berechnung zu entfernen
		else
		{
			if ( isPlainSep && !previousWasSeparator ) sz = { _styleData.sp, _styleData.sp };
			auto w	= qobject_cast< QWidgetAction * >( action );
			auto ww = w ? w->defaultWidget() : nullptr;
			if ( ww ) // Widget-Action -> hat ihre eigene Größe!
				sz = ww->sizeHint()
						 .expandedTo( ww->minimumSize() )
						 .expandedTo( ww->minimumSizeHint() )
						 .boundedTo( ww->maximumSize() );
			else if ( !isPlainSep )
			{
				// Keine Widget-Action, keine Lücke: also Text und Icon
				const QFontMetrics &fm = opt.fontMetrics;
				auto				s  = action->text();
				sz =
					fm.boundingRect( QRect(), Qt::TextSingleLine | Qt::TextShowMnemonic, s ).size();
				QIcon is = action->icon();
				if ( !is.isNull() )
				{
					QSize is_sz = is.actualSize( QSize( _styleData.icone, _styleData.icone ) );
					if ( is_sz.height() > sz.height() ) sz.setHeight( is_sz.height() );
				}
				sz = style()->sizeFromContents( QStyle::CT_MenuItem, &opt, sz, this );
			}
			if ( sz.isValid() ) sz.rwidth() += tab;
			previousWasSeparator = isPlainSep;
		}
		_actionPieData[ i ]	   = sz;
		_actionRenderData[ i ] = { 1., 1. };
		if ( sz.isValid() ) allSz += sz;
		else ++noSzItems;
	}
	// Speichere die Durchschnittsgröße der Items mit! (Division durch 0 ist zu verhindern)
	if ( auto cnt = actions().count() - noSzItems ) _actionPieData._avgSz = allSz / cnt;
}

void QPieMenu::calculateStillData()
{
	if ( _actionPieData.isEmpty() ) return;
	// Hier kommt erst einmal der optimierte Algorithmus hinein: die "stillData" berechnen wir
	// einmalig nach ActionEvent, sie sind die Ruhepositionen für unser Menü.  Diese sind auch
	// wichtig für das Bounding Rect: nach Abschluss der Berechnungen sollten wir ein paar kluge
	// Margins auf das boundingRect draufrechnen, damit wir ohne unsere Geometrie ändern zu müssen,
	// auch Skalierungseffekte erstellen können.
	//
	// Zuerst brauchen wir eine gute Schätzung für den startRadius.  Wir haben gelernt, dass hier
	// sowohl Breite, als auch Höhe eingehen.
	// Weiterhin erscheint mir gerade relativ klar, dass bis zu einem
	//      Radius < length( fromSize( average_item_size ) )
	// der Durchschnittsgröße weniger Items im Kreis platziert werden können, weil im Endeffekt nur
	// eine Seite des Kreises zum Anordnen von Elementen zur Verfügung steht.  Wären auf der anderen
	// Seite, also ab 180° Gesamtüberdeckung auch Elemente, würde es bei diesen kleinen Radien
	// zwangsläufig zu Überdeckungen kommen.
	int		  runde = 0, ac = actions().count();
	QPointF	  sc, off;
	QPoint	  c;
	QRect	  box;
	auto	  scddt = [ &sc ]() { return QPointF{ sc.y(), -sc.x() }; };
	qreal	  w, dir, r, n, ww;
	BestDelta bd;
	for ( int i = 0; i < ac; ++i )
	{
		// Winkel initialisieren oder aus der letzten Runde übernehmen
		if ( i == 0 )
		{
			r	= startR( runde++ );
			w	= _initData._start0;
			dir = ( _initData._negativeDirection ? -1. : 1. );
			sc	= qSinCos( w );
			c	= {};
			ww	= 0;
			_actionPieData.resetCalculations();
		}
		// nur sichtbare Elemente berücksichtigen
		if ( _actionPieData[ i ].operator QSize().isValid() )
		{ // wenn c nocht nicht gesetzt wurde, sind wir beim allerersten Element, was sichtbar ist.
			if ( c.isNull() ) _actionPieData.setCenter( i, c = ( r * sc ).toPoint() );
			_actionPieData.setAngle( i, w );
			// Prüfung: gibt es noch ein Folgeelement?
			auto ii = i + 1;
			while ( ii < ac && !_actionPieData[ ii ].operator QSize().isValid() ) ++ii;
			if ( ii < ac )
			{
				// Es gibt noch eine Folgebox zu berechnen...
				off = dir * qSgn( scddt() )
					  * ( QPoint{ _styleData.sp, _styleData.sp }.toPointF()
						  + 0.5
								* ( fromSize( _actionPieData[ i ] )
									+ fromSize( _actionPieData[ ii ] ) ) );
				bd.init( dir, w );
				for ( auto v : { c + off, c - off } )
				{
					if ( abs( v.x() ) <= r )
						n = qAsin( v.x() / r ), bd.addRad( n ), bd.addRad( M_PI - n );
					if ( abs( v.y() ) <= r )
						n = qAcos( v.y() / r ), bd.addRad( n ), bd.addRad( -n );
				}
				// Ergebnis: der nächste Winkel - vielleicht ...
				n = bd.best();
				ww += qAbs( n );
				if ( !qFuzzyIsNull( n ) && ww < _initData._max0 )
				{
					c = ( r * ( sc = qSinCos( w + n ) ) ).toPoint();
					if ( _actionPieData.setCenter( ii, c ) )
					{
						w += n;
						// Überspringen von leeren Elementen - wir haben ja schon danach geschaut
						// ...
						i = ii - 1;
						// Ich habe ein Ergebnis berechnet - auf zum nächsten!
						continue;
					}
				}
				// Landet der Program Counter hier, dann brauchen wir mehr Platz.
				// -> mit "i = -1;" starten wir die Schleife von vorn.
				i = -1;
			}
		} else {
			_actionPieData[ i ]._angle = 0.;
		}
	}
	// Justiere zur Sicherheit mal jeweils um die halbe Durchschnittsgröße.
	auto xa = _actionPieData._avgSz.width() >> 1;
	auto ya = _actionPieData._avgSz.height() >> 1;
	_actionPieData._boundingRect.adjust( -xa, -ya, xa, ya );
	// Nicht den Radius vergessen, der wird später gebraucht!!!
	_actionPieData._radius = r;
}

qreal QPieMenu::startR( int runde ) const
{
	auto asz = _actionPieData._avgSz;
	auto avp = fromSize( asz );
	// Radius, der spätestens in der 3. Runde verwendet werden sollte (ggf. noch ein "+ Spacing"?)
	auto r3	 = qSqrt( QPointF::dotProduct( avp, avp ) );
	// Bisher eine gute Schätzung:
	//      ( AspectRatio * (item_height+spacing) * itemCount ) / floor( winkelÜberdeckung / 90° )
	auto r0	 = ( avp.y() / avp.x() ) * ( asz.height() + _styleData.sp ) * actions().count()
			  / qFloor( _initData._max0 / M_PI_2 );
	// Also: 0. - 3. Runde bewegen wir uns linear zwischen r0 und r3
	if ( runde < 4 ) return r0 + runde * ( r3 - r0 ) / 3;
	// Danach addieren wir für jede Runde eine halbe Item-Höhe drauf.
	return r3 + ( runde - 3 ) * ( asz.height() >> 1 );
}

void QPieMenu::ereignis( PieMenuEreignis e, qint64 p, qint64 p2 )
{
	// output helper macro
#define a_sz( S, Z ) "start:" << S << "ziel:" << Z << "startzeit:" << t

	auto t			= QTime::currentTime();
	auto posDecoded = QPoint{ p >> 32, int( p & 0xffffffffll ) };
	switch ( e )
	{
#pragma region( Animationen_allgemein )
		// Schreibe aktuelle Animationen weiter, ggf. timer stoppen
		case PieMenuEreignis::animate:
			// Läuft die SelRect-Animation?
			if ( _selRectAnimiert ) _selRectDirty = true;
			if ( _raAnimiert ) _actionRectsDirty = true;
			if ( _folgenAnimiert || _actionRectsDirty || _selRectDirty ) update();
			else qWarning() << "PieMenuEreignis::animate, but nothing left to animate!";
			break;
#pragma endregion
#pragma region( Zeitanimationen )
		// Schalte die Animation zum Anzeigen ein
		case PieMenuEreignis::getsShown:
			qDebug() << "QPieMenu::ereignis( gets_shown ) -> current setup:" << _actionPieData
					 << _actionRects;
			break;
		// Beginne, das Selection Rect zu animieren (p: ID der Aktion)
		case PieMenuEreignis::hover_Item_start:
			if ( p != _hoverId )
			{
				_hoverId = int( p );
				// - nimm die letzte Position des SelRects as Startwert
				_srS	 = _selRect;
				// Falls wir mal das Zentrum highlighten wollen ...
				if ( _hoverId == -1 ) _srE = { {}, _styleData.HLtransparent };
				else // Was braucht die Selection Rect-Animation? -> Position und Farbe
				{
					auto r = _actionPieData[ _hoverId ]._geo;
					auto p = r.center();
					r.setSize( 1.2 * r.size() );
					r.moveCenter( p );
					_srE = { r, _styleData.HL };
				}
				// - initiiere SelRectAnimation bzw. setze ihre Werte zurück
				_selRectStart = t;
				ereignis( PieMenuEreignis::zeitanimation_beginnt,
						  reinterpret_cast< quint64 >( &_selRectAnimiert ) );
				// qDebug() << "=> Zeitanimation Selection Rect #" << _hoverId << a_sz( _srS, _srE
				// );
			}
			break;
		// Beginne, das SelRect wieder zu verstecken (p: Zielpunkt 2x32 Bit in 64 Bit gepackt)
		case PieMenuEreignis::hover_Item_end:
			// - setze das Zentrum und eine Größe von (0,0) als Zielwert
			if ( _hoverId != -1 )
			{
				_hoverId	  = -1;
				_srS		  = _selRect;
				_srE		  = { { posDecoded, posDecoded }, _styleData.HLtransparent };
				_selRectStart = t;
				ereignis( PieMenuEreignis::zeitanimation_beginnt,
						  reinterpret_cast< quint64 >( &_selRectAnimiert ) );
				// qDebug() << "=> Zeitanimation Selection Rect Hide" << a_sz( _srS, _srE );
			}
			break;
#pragma endregion
#pragma region( EchtzeitAnimation )
		// Starte die Echtzeitanimation
		case PieMenuEreignis::distance_closing_in:
			if ( !_folgenAnimiert || p != _folgeId )
			{
				_folgeDauer = ( _folgeId == -1 ? 500 : 250 );
				ereignis( PieMenuEreignis::folgeanimation_beginnt,
						  reinterpret_cast< quint64 >( &_folgenAnimiert ), p );
				qWarning() << "FOLGE" << _folgeId << a_sz( _folgeStart, _folgeStop )
						   << "Dauer[ms]:" << _folgeDauer;
			}
			break;
		// Beende die Echtzeitanimation
		case PieMenuEreignis::distance_leaving:
			if ( _folgeId != -1 )
			{
				_folgeDauer = 1000;
				ereignis( PieMenuEreignis::folgeanimation_beginnt,
						  reinterpret_cast< quint64 >( &_folgenAnimiert ), -1 );
				DistArc da( _folgeStop.first );
				da.d() = da.r0() = da.r1() = 0.;
				_folgeStop.second		   = _styleData.HLtransparent;
				qWarning() << "STOPP:" << a_sz( _folgeStart, _folgeStop );
			}
			break;
#pragma endregion
#pragma region( Auswahl )
		// Ein Menuitem wird gedrückt
		case PieMenuEreignis::selecting_item: break;
		// Ein Menuitem wird losgelassen -> ich könnte auch hier erst prüfen, ob es sich um ein
		// submenu oder eine action an sich handelt...
		case PieMenuEreignis::open_sub_menu: break;
		case PieMenuEreignis::execute_action: break;
#pragma endregion
#pragma region( Zeitanimation_Helfer )
		// Bei Zeitanimationen ist der Parameter ein Zeiger auf den bool, der durch das
		// Ereignis geändert wird.
		case PieMenuEreignis::folgeanimation_beginnt:
			_folgeId	 = p2;
			_folgeBeginn = t;
			_folgeStart	 = _folge; // letzte Darstellung als Start!!!
			[[fallthru]];
		case PieMenuEreignis::zeitanimation_beginnt:
			if ( p ) // Wir machen hier nix ohne Bescheid zu geben ...
			{
				auto &b = *reinterpret_cast< bool * >( p );
				if ( !b )
				{
					b = true;
					if ( _timerId == 0 ) _timerId = startTimer( 10 );
				}
			}
			break;
		case PieMenuEreignis::zeitanimation_abgeschlossen:
			if ( p ) // Wir machen hier nix ohne Bescheid zu geben ...
			{
				auto &b = *reinterpret_cast< bool * >( p );
				if ( b )
				{
					b		  = false;
					bool kill = !( _selRectAnimiert || _raAnimiert || _folgenAnimiert ) && _timerId;
					if ( kill ) _timerId = ( killTimer( _timerId ), 0 );
					// qDebug() << "=> Zeitanimation abgeschlossen,"
					//		 << ( kill ? "Timer gestoppt." : "weitere Animationen laufen noch..." );
				}
			}
			break;
#pragma endregion
	}
#undef a_sz
}

void QPieMenu::updateCurrentVisuals()
{
	if ( !_actionRectsDirty && !_selRectDirty ) return;
	// Die Action Rects werden auf unterschiedliche Art und Weise dirty.
	// Wenn wir auch während der Animationen die Action Rects aktuell halten wollen, so muss das
	// hier geschehen.  Diese Funktion aktualisiert die dreckigen Action Rects je nach aktueller
	// Darstellungssituation.
	auto t = QTime::currentTime();
	if ( _actionRectsDirty )
	{
		if ( _raAnimiert )
		{
			auto tx = static_cast< qreal >( _raStart.msecsTo( t ) ) / _raDauerMS;
			// Radius-Winkel-Animation bitte hier durchführen ...
			if ( tx >= 1. )
				ereignis( PieMenuEreignis::zeitanimation_abgeschlossen,
						  reinterpret_cast< quint64 >( &_raAnimiert ) );
		}
		if ( _folgenAnimiert )
		{
			// nutze: _folgePunkt, _folgeId
			// Zuerst mal wollen wir nur den Sektor highlighten.
			// => das geht am Einfachsten, wenn wir das Sector-HighLighting direkt im paintEvent
			// ausrechnen.  Dort können wir einen Gradienten passend bauen und einen Kreissektor
			// damit zeichnen ...

			// Der eigentliche Spass geht dort los, wo wir wirklich ein Rect Vergrößern und den
			// Rest rechts und links davon ausweichen lassen.
		}
		_actionRectsDirty = false;
	}

	if ( _selRectDirty )
	{
		if ( _selRectAnimiert )
		{
			auto x = smoothStep( static_cast< qreal >( _selRectStart.msecsTo( t ) ) / 1000., 0.,
								 _initData._selRectDuration );
			_selRect( x, _srS, _srE );
			if ( x >= 1. )
				ereignis( PieMenuEreignis::zeitanimation_abgeschlossen,
						  reinterpret_cast< quint64 >( &_selRectAnimiert ) );
			// qDebug() << "=> Selection Rect animiert:" << x << _selRect;
		}
		_selRectDirty = false;
	}
}

bool QPieMenu::hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID )
{
	int d, md = std::numeric_limits< int >::max();
	for ( auto i( 0 ); i < _actionRects.count(); ++i )
	{
		d = boxDistance( p, _actionRects.at( i ) );
		if ( d < md ) md = d, minDistID = i;
	}
	if ( Q_LIKELY( md < std::numeric_limits< int >::max() ) )
		mindDistance = static_cast< qreal >( md );
	return md <= 0; // Hit bei Distance <= 0 ...
}

#pragma optimize( "t", on )
inline PieSelectionRect &PieSelectionRect::operator()( const qreal f, const PieSelectionRect &a,
													   const PieSelectionRect &b )
{
	// We've reached the future some time ago?
	// https://fgiesen.wordpress.com/2012/08/15/linear-interpolation-past-present-and-future/
	// =>   actually, that blog post contains a small mistake, maybe because AVX wasn't born
	//      at the time of writing.
	//          fma( a, b, c ) = ab + c  <=>  fnma( a, b, c ) = -ab + c     (Intel DOCs)
	//          lerp: f(t,f0,f1) = f0 * (1-t) + f1 * t = f1*t -f0*t + f0    (simple Algebra)
	//      Ryg wrote: "lerp_3(t, a, b) = fma(t, b, fnms(t, a, a))" - this is not correct in
	//      current AVX/FMA speech.
	//      What we need regarding algebra is mentioned above:
	//          fma( t, b, fnma( t, a, a ) )
	//          fma( b, t, fnma( a, t, a ) ) -> this may be nicer to read ;)
	//      And - "BTATA" - there you have the next gen lerp, as long as you make sure to
	//      clamp t into [0.0 .. 1.0] !!!111!!11!!!1!!!
	// --------------------------------------------------------------------------------------
	// SSE Shuffle-Maskem der RGBA-Werte aus QColor
	const __m128i QCload  = _mm_set_epi64x( 0xffff0b0affff0908ull, 0xffff0706ffff0504ull );
	const __m128i QCback  = _mm_set_epi64x( 0xffffffff0d0c0908ull, 0x05040100ffffffffull );
	const __m128i QCstore = _mm_set_epi64x( 0x00000000ffffffffull, 0xffffffff00000000ull );

	// Wir wissen, dass QRectF intern einfach 4 doubles abspeichert.  Die können wir ja direkt
	// laden. Ebenso wissen wir, dass QColor eine 4-Bytes-Spec beinhaltet und darauf folgt ein
	// ushort[5] Array mit den Farbdaten.  Können wir also alles stream-Laden.
	auto		  t		  = _mm256_set1_pd( f );
	auto		  r0	  = _mm256_load_pd( reinterpret_cast< const qreal * >( &a ) );
	auto		  cc0	  = _mm_load_si128( reinterpret_cast< const __m128i * >( &a ) + 2 );
	auto		  r1	  = _mm256_load_pd( reinterpret_cast< const qreal * >( &b ) );
	auto		  cc1	  = _mm_load_si128( reinterpret_cast< const __m128i * >( &b ) + 2 );
	// Maybe a manual split-up of operations increases parallelity
	auto		  c0	  = _mm256_cvtepi32_pd( _mm_shuffle_epi8( cc0, QCload ) );
	auto		  c1	  = _mm256_cvtepi32_pd( _mm_shuffle_epi8( cc1, QCload ) );
	// -> directly store it in here
	_mm256_store_pd( reinterpret_cast< qreal * >( &first ),
					 /* Lerp the QRectF*/ _mm256_fmadd_pd( r1, t, _mm256_fnmadd_pd( r0, t, r0 ) ) );
	_mm_maskstore_epi32( reinterpret_cast< int * >( &second ), QCstore,
						 /*Lerp the colors*/
						 _mm_shuffle_epi8( _mm256_cvtpd_epi32( _mm256_fmadd_pd(
											   c1, t, _mm256_fnmadd_pd( c0, t, c0 ) ) ),
										   QCback ) );
	return *this;
}
#pragma optimize( "", on )


QDebug operator<<( QDebug dbg, const PieDataItem &pd )
{
	QDebugStateSaver s( dbg );
	auto			 t = QString( pd._ok ? " => dataset complete." : "=> incomplete: " );
	if ( !pd._ok )
	{
		t += "missing ";
		t += ( !pd._s ? "size" : ( !pd._a ? "angle" : "position" ) );
		if ( pd._s && !pd._a && !pd._p ) t += ", position";
		else
		{
			if ( !pd._a ) t += ", angle";
			if ( !pd._p ) t += ", position";
		}
	}
	dbg << "\u03c6 = " << pd._angle << ", R = " << pd._geo << t.toUtf8().data();
	return dbg;
}

QDebug operator<<( QDebug dbg, const PieData &pd )
{
	QDebugStateSaver s( dbg );
	dbg << "PieData( #items =" << pd.count() << ", r =" << pd._radius << ", avg.Size =" << pd._avgSz
		<< ", bounds =" << pd._boundingRect << ", Intersections:" << pd._intersections.count()
		<< ")";
	return dbg;
}
