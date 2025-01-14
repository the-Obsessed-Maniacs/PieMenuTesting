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
	return _boundingRect.size();
}

QAction *QPieMenu::actionAt( const QPoint &pt ) const
{
	auto id = actionIndexAt( pt );
	qDebug() << "QPieMenu::actionAt(" << pt << ") =" << id;
	if ( id == -1 ) return nullptr;
	else return actions().at( id );
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
	//
	// ToDo für später: Animation beim Einfügen/Entfernen
	calculatePieDataSizes();
	calculateStillData();
	_actionRectsDirty = true;
	updateGeometry();
	event->accept();
	// QMenuPrivate at least needs to know about whatever they think is needed
	QMenu::actionEvent( event );
}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	updateCurrentVisuals();
	QStylePainter p( this );
	p.translate( -_boundingRect.topLeft() );
	QStyleOptionMenuItem opt;
	auto				 ac = actions().count();
	// Folgen: Sektorhighlighting
	if ( _folge.first.isValid() && _folge.second.alpha() > 0 )
	{
		// Das war's schon zum Ein- und Ausblenden.  Nun zeichne ich den aktuellen Zustand.
		p.setPen( _folge.second );
		auto			&f = _folge.first;
		// Zielpunkte aus dem Rect bestimmen:
		// ->   erstmal abhängig von der Lage.  Bin ich komplett innerhalb eines Sektors, wird es
		//      vergleichsweise einfach.
		QList< QPointF > l( 1 ); // Das Zentrum darf schon mal drin sein ...
		if ( f.bottom() < 0 )	 // ich bin oben links und rechts
		{
			if ( f.right() < 0 ) l << f.topRight() << f.topLeft() << f.bottomLeft();
			else if ( f.left() > 0 ) l << f.bottomRight() << f.topRight() << f.topLeft();
		} else if ( f.top() > 0 ) { // ich bin unten links und rechts
			if ( f.right() < 0 ) l << f.topLeft() << f.bottomLeft() << f.bottomRight();
			else if ( f.left() > 0 ) l << f.bottomLeft() << f.bottomRight() << f.topRight();
		}
		if ( l.count() < 2 ) // ich bin irgendwo auf den Koordinatenachsen
		{
			// jetzt bin ich sicher unten:
			if ( f.top() >= 0 )
				l << f.topLeft() << f.bottomLeft() << f.bottomRight() << f.topRight();
			// sicher oben
			if ( f.bottom() <= 0 )
				l << f.bottomRight() << f.topRight() << f.topLeft() << f.bottomLeft();
			// sicher links
			if ( f.right() <= 0 )
				l << f.topRight() << f.topLeft() << f.bottomLeft() << f.bottomRight();
			// sicher rechts
			if ( f.left() >= 0 )
				l << f.topLeft() << f.topRight() << f.bottomRight() << f.bottomLeft();
			// die letzte Möglichkeit bedeutet, dass die Box irgendwie das Zentrum überdeckt. Diesen
			// Fall haben wir am Anfang der Animation, bis das animierte Rect das Zentrum verlassen
			// hat.  Wir sollten hier einfach nichts zeichnen ...
		}
		l << QPointF();
		if ( l.count() > 2 ) p.drawPolyline( l.constData(), l.count() );

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
		opt.state.setFlag( QStyle::State_Selected, ( i == _hoverId ) && !_selRectStart.isValid() );
		opt.rect = _actionRects[ i ].marginsAdded( _styleData.menuMargins );
		p.setOpacity( _actionRenderData[ i ].x() );
		// ToDo: Benutze den Skalierfaktor "_actionRenderData[ i ].y()" korrekt.
		//  z.B.: das Folgende (was schon mal sehr gut geht!)
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
	setGeometry( geometry().translated( _boundingRect.topLeft() ) );
	qApp->setEffectEnabled( Qt::UI_FadeMenu, false );
	ereignis( PieMenuEreignis::show_up );
	QMenu::showEvent( e );
}

void QPieMenu::mouseMoveEvent( QMouseEvent *e )
{
	// Nur, wenn es sich wirklich um eine Bewegung handelt, stelle ich die aktuellen Daten zur
	// Verfügung und werte sie aus.
	auto p = e->position().toPoint() + _boundingRect.topLeft();
	if ( _lastPos != p )
	{
		_lastPos = p;
		_lastDm	 = qSqrt( QPoint::dotProduct( p, p ) );
		// Action Rects aktualisieren
		updateCurrentVisuals();
		// Finde die nächstgelegene Aktion und den Abstand zum Mittelpunkt durch den Hit-Test
		int	  id( -1 );
		// -> ich möchte einen kleinen, nicht-reaktiven Kreis rund um den Mittelpunkt bewahren
		// (minmax) -> (out)
		qreal r		  = _data.r();
		bool  out	  = _lastDm > qMax( 0.5 * qMin( _avgSz.width(), _avgSz.height() ), 10. );
		bool  hit	  = hitTest( p, _lastDi, id );
		bool  closeBy = !hit && out && ( _lastDi <= 2 * r ) && ( id != -1 );
		// Alle Informationen sind beschafft und Schlussfolgerungen stehen bereit -> State-Engine
		// bearbeiten und Ereignisse produzieren:
		switch ( _state )
		{
			case PieMenuStatus::still:
				// Der Priorität nach prüfen: hover, closeby
				if ( hit ) ereignis( PieMenuEreignis::hover_start, id );
				else if ( closeBy ) ereignis( PieMenuEreignis::closeBy_start, id );
				// Wenn das Menü "still" ist, gibt es keine weiteren Möglichkeiten.
				break;
			case PieMenuStatus::closeby:
				if ( hit ) ereignis( PieMenuEreignis::hover_start, id );
				else if ( !closeBy ) ereignis( PieMenuEreignis::closeBy_leave );
				else if ( _folgeId != id ) ereignis( PieMenuEreignis::closeBy_switch, id );
				break;
			case PieMenuStatus::hover:
				if ( !hit ) // Kein Hover mehr? Kann dennoch zu 2 anderen Stati wechseln...
				{
					if ( closeBy ) ereignis( PieMenuEreignis::closeBy_start, id );
					else ereignis( PieMenuEreignis::hover_leave );
				} else if ( id != _hoverId )
					ereignis( PieMenuEreignis::hover_switch, id ),
						qDebug() << "hover_switch_current to id = " << id;
				break;
			// In allen anderen Stati ist hier (vorerst) nichts zu tun.
			case PieMenuStatus::item_active: break;
			default: break;
		}
		e->accept();
		// Soweit - mehr sollte das mouseMoveEvent nicht tun müssen.

		// ALTERCODE: (ein paar nette Berechnungen drin)
		// -> der Close-Test hat ergeben, ob der Cursor in der Nähe eines Items ist
		// if ( closeBy )
		//{
		//	// Ereignis auslösen: sollte die Animation eingeschaltet werden, werden die letzten
		//	// Daten des Sektors genutzt -> die stehen seit der letzten Animation in _folge.
		//	if ( _folgeId != id )
		//	{
		//		// Da die "Distance Arc"-Idee Moppelkotze war ... animieren wir einfach die Rects an
		//		// sich.
		//		auto g = _actionPieData[ id ]._geo;
		//		_folgeStop.first.setSize( QSizeF( _actionPieData[ id ] ) * 1.5 );
		//		_folgeStop.first.moveCenter( g.center() );
		//		_folgeStop.second = _styleData.HL;
		//		if ( _folgeId == -1 ) _folgeDW = 0; // Starte Folgen an sich
		//		else
		//		{
		//			// Folgen wechselt -> das heisst, das neue Folge-Element ist im Moment etwas von
		//			// seinem ursprünglichen Winkel verschoben.  Die Beste Idee ist vermutlich,
		//			// wieder eine kleinste Differenz zu bestimmen (asin/acos oder so), die dann zu
		//			// _folgeDW wird.
		//			BestDelta bd;
		//			bd.init( 1., _actionPieData[ id ]._angle );
		//			auto w = qAcos( _actionRects[ id ].center().y() / _actionPieData._radius );
		//			bd.addRad( w ), bd.addRad( -w );
		//			_folgeDW = bd.best();
		//		}
		//		ereignis( PieMenuEreignis::distance_closing_in, id );
		//	}
		//	// Ansonsten ist nur die Farbe zu aktualisieren - die soll ja von der Nähe abhängig
		//	// sein. der Alpha-Wert der Farbe sollte von der Entfernung zum Item abhängig sein ...
		//	auto alpha = sin( ( 1. - _lastDi / ( 2 * r ) ) * M_PI_2 );
		//	if ( _folgenAnimiert ) _folgeStop.second.setAlphaF( alpha );
		//	else _folge.second.setAlphaF( alpha ), update();
		//} else ereignis( PieMenuEreignis::distance_leaving );
	}
}

void QPieMenu::timerEvent( QTimerEvent *e )
{
	if ( e->timerId() == _timerId ) ereignis( PieMenuEreignis::animate ), e->accept();
	else if ( e->timerId() == _alertId ) ereignis( PieMenuEreignis::time_out ), e->accept();
}

void QPieMenu::mousePressEvent( QMouseEvent *e )
{
	/*QMenu::mousePressEvent( e ); */
	qDebug() << "QPieMenu::mousePressEvent(" << e << ")";
	auto p		= e->pos() + _boundingRect.topLeft();
	auto launch = e->buttons().testAnyFlags( Qt::RightButton | Qt::LeftButton );
	switch ( _state )
	{
		case PieMenuStatus::hover: // HitTest, wenn sich was bewegt hat ...
			if ( p == _lastPos || boxDistance( p, _actionRects[ _hoverId ] ) <= 0. )
			{ // Ich probiere mal einen Sel Rect Color Fade
				_srS   = _selRect;
				auto c = _srE.second.convertTo( QColor::Hsv );
				_srE.second.setHsv( ( c.hsvHue() + 180 ) % 360, c.hsvSaturation(), c.value(), 208 );
				ereignis( PieMenuEreignis::zeitanimation_beginnt,
						  reinterpret_cast< quint64 >( &_selRectAnimiert ),
						  reinterpret_cast< quint64 >( &_selRectStart ) );
			} else qDebug() << "hit test failed!";
			break;
		case PieMenuStatus::hidden: break;
		case PieMenuStatus::still: break;
		case PieMenuStatus::closeby: break;
		case PieMenuStatus::item_active: break;
		default: e->ignore(); break;
	}
}

void QPieMenu::mouseReleaseEvent( QMouseEvent *e )
{
	/*QMenu::mouseReleaseEvent( e ); */
	auto p		= e->localPos().toPoint();
	auto launch = e->buttons().testAnyFlags( Qt::RightButton | Qt::LeftButton );
	switch ( _state )
	{
		case PieMenuStatus::hover: // HitTest, wenn sich was bewegt hat ...
			if ( p == _lastPos || boxDistance( p, _actionRects[ _hoverId ] ) <= 0. )
			{
				actions().at( _hoverId )->activate( QAction::ActionEvent::Trigger );
			}
			break;
		default: e->ignore(); break;
	}
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
	_selRect.second	  = _hoverId == -1 ? _styleData.HLtransparent : _styleData.HL;
	// Es wurden auch Größen und Margins neu gelesen -> die rects sind vmtl. dirty.
	_actionRectsDirty = true;
}

void QPieMenu::calculatePieDataSizes()
{
	// Alle Größen sind voneinander abhängig, wenn wir einen konsistenten Stil (wie ihn Menüs nunmal
	// haben sollten) mit zumindest gleichem Tabstopp verwenden wollen.  Da wir alle anfassen
	// müssen, können wir auch gleich immer alle berechnen.
	QStyleOptionMenuItem opt;
	QAction				*action;
	QSize				 sz, allSz;
	int					 tab = 0, noSzItems = 0, ac = actions().count();
	// Schritt #1: Tabstopp suchen
	for ( int i( 0 ); i < ac; ++i )
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
	_actionRects.resizeForOverwrite( ac );
	_actionRenderData.resizeForOverwrite( ac );
	_hitDist.resizeForOverwrite( ac );
	_data.clear( ac );

	for ( int i( 0 ); i < ac; ++i )
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
		_data.append( sz );
		_actionRenderData[ i ] = { 1., 1. };
		if ( sz.isValid() ) allSz += sz;
		else ++noSzItems;
	}
	// Speichere die Durchschnittsgröße der Items mit! (Division durch 0 ist zu verhindern)
	if ( auto cnt = ac - noSzItems ) _avgSz = allSz / cnt;
}

void QPieMenu::calculateStillData()
{
	int runde = 0, ac = actions().count();
	if ( _data.count() != ac ) return;
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
	QPointF						 sc, off;
	QPoint						 c;
	QRect						 box;
	QSize						 si, sii;
	auto						 scddt = [ &sc ]() { return QPointF{ sc.y(), -sc.x() }; };
	qreal						 w, dir, r, n, ww;
	BestDelta					 bd;
	Intersector< QRect, QPoint > occ;
	for ( int i = 0; i < ac; ++i )
	{
		si = _data[ i ];
		// Winkel initialisieren oder aus der letzten Runde übernehmen
		if ( i == 0 )
		{
			r = startR( runde++ );
			w = _initData._start0, ww = 0;
			dir = ( _initData._negativeDirection ? -1. : 1. );
			sc	= qSinCos( w );
			c	= ( r * sc ).toPoint();
			box.setSize( si ), box.moveCenter( c );
			occ.clear(), occ.add( box );
		}
		// nur sichtbare Elemente berücksichtigen
		if ( si.isValid() )
		{
			_data.setAngle( i, w );
			// Prüfung: gibt es noch ein Folgeelement?
			auto ii = i + 1;
			while ( ii < ac && !( sii = _data[ ii ] ).isValid() ) ++ii;
			if ( ii < ac )
			{
				// Es gibt noch eine Folgebox zu berechnen...
				off = /*dir * qSgn( scddt() ) * ... scheinbar doch völlig unwichtig */
					( QPoint{ _styleData.sp, _styleData.sp }.toPointF()
					  + 0.5 * ( fromSize( si ) + fromSize( sii ) ) );
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
					box.setSize( sii ), box.moveCenter( c );
					if ( occ.add( box ) )
					{
						w += n;
						// Überspringen von leeren Elementen
						// ...
						i = ii - 1;
						// Ich habe ein Ergebnis berechnet - auf zum nächsten!
						continue;
					}
				}
				// Landet der Program Counter hier, dann brauchen die Elemente mehr Platz.
				// -> mit "i = -1;" startet die Schleife von vorn.
				i = -1;
			}
		} else {
			_data.setAngle( i, 0. );
		}
	}
	// Nicht den Radius vergessen, der wird später gebraucht!!!
	_data.setR( r );
	// Debug-Hilfe: ich möchte den ganzen Kreis sehen - ggf. muss es auch so bleiben, damit die
	// Animationen nicht out of bounds gerendert werden.
	_boundingRect /*= occ._br;*/ = QRectF{ { -r, -r }, QPointF{ r, r } }.toRect();
	// Justiere zur Sicherheit mal jeweils um die (halbe) Durchschnittsgröße.
	auto xa						 = _avgSz.width();	//>> 1;
	auto ya						 = _avgSz.height(); // >> 1;
	_boundingRect.adjust( -xa, -ya, xa, ya );
	updateGeometry();
}

qreal QPieMenu::startR( int runde ) const
{
	auto asz = _avgSz;
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
	auto t	= QTime::currentTime();
	auto os = _state;
	switch ( e )
	{
#pragma region( Animieren_und_Timer )
		case PieMenuEreignis::animate:
			// Animationen werden ohne Beschränkungen des Statuses durchgeführt:
			if ( _selRectAnimiert ) _selRectDirty = true;
			if ( _rectsAnimiert ) _actionRectsDirty = true;
			if ( _actionRectsDirty || _selRectDirty ) update();
			break;
		case PieMenuEreignis::time_out:
			// Zeitauslöser:
			killTimer( _alertId ), _alertId = 0;
			switch ( _state )
			{
				case PieMenuStatus::hover: ereignis( PieMenuEreignis::hover_activate ); break;
				default: break;
			}
			break;
		// Parameter: p:= bool* auf "*Animiert", p2:= ptr auf QTime (Startzeit), darf null sein
		// (dieser State-Change steckt am Ende in _timerId != 0)
		case PieMenuEreignis::zeitanimation_beginnt:
			if ( p ) // Wir machen hier nix ohne Bescheid zu geben ...
			{
				auto &b = *reinterpret_cast< bool * >( p );
				if ( !b )
				{
					b = true;
					if ( _timerId == 0 ) _timerId = startTimer( 10 );
					if ( p2 ) *reinterpret_cast< QTime * >( p2 ) = t;
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
					bool kill = !( _selRectAnimiert || _rectsAnimiert ) && _timerId;
					if ( kill )
						_timerId =
							( killTimer( _timerId ), ( qDebug() << "timer stopped:" << t ), 0 );
				}
			}
			break;
		case PieMenuEreignis::timer_startet:
			_alertId = startTimer( p, Qt::TimerType::VeryCoarseTimer );
			if ( p2 ) *reinterpret_cast< QTime * >( p2 ) = t;
			break;
		case PieMenuEreignis::timer_abbruch: killTimer( _alertId ), _alertId = 0; break;
#pragma endregion
#pragma region( Show_Hide )
		// Keine Prüfungen nötig bei show_up und hide_away.
		case PieMenuEreignis::show_up:
			// Schalte die Animation zum Anzeigen ein
			_data.initShowUp( _initData._animBaseDur * 4 );
			ereignis( PieMenuEreignis::zeitanimation_beginnt,
					  reinterpret_cast< qint64 >( &_rectsAnimiert ) );
			_state = PieMenuStatus::still; // Not calling makeState on Purpose!
			break;
		case PieMenuEreignis::hide_away: makeState( PieMenuStatus::hidden ); break;
#pragma endregion
#pragma region( Hover )
		// Beginne, das Selection Rect zu animieren (p: ID der Aktion)
		case PieMenuEreignis::hover_start:
		case PieMenuEreignis::hover_switch:
			_hoverId = p;
			makeState( PieMenuStatus::hover );
			break;
		// Beginne, das SelRect wieder zu verstecken
		case PieMenuEreignis::hover_leave:
			_hoverId = -1;
			makeState( PieMenuStatus::still );
			break;
		case PieMenuEreignis::hover_activate:
			if ( _hoverId >= 0 ) _activeId = _hoverId, makeState( PieMenuStatus::item_active );
			break;
#pragma endregion
#pragma region( EchtzeitAnimation )
		// Starte die Echtzeitanimation
		case PieMenuEreignis::closeBy_start:
			_folgeId	 = p;
			_folgeBeginn = t;
			makeState( PieMenuStatus::closeby );
			break;
			// if ( !_rectsAnimiert || p != _folgeId )
			//{
			//	_folgeId	  = p;
			//	_folgeDauerMS = _initData._selRectDuration * ( _folgeId == -1 ? 2000 : 1000 );
			//	ereignis( PieMenuEreignis::folgeanimation_beginnt,
			//			  reinterpret_cast< quint64 >( &_rectsAnimiert ),
			//			  reinterpret_cast< quint64 >( &_folgeBeginn ) );
			//	// qWarning() << "FOLGE" << _folgeId << "Dauer[ms]:" << _folgeDauer
			//	//		   << a_sz( _folgeStart, _folgeStop );
			// }
			// break;
		// Beende die Echtzeitanimation
		case PieMenuEreignis::closeBy_leave:
			makeState( PieMenuStatus::still );
			break;
			// if ( _folgeId != -1 )
			//{
			//	_folgeId		  = -1;
			//	_folgeStop.first  = {};
			//	_folgeStop.second = _styleData.HLtransparent;
			//	_folgeDauerMS	  = _initData._selRectDuration * 4000;
			//	ereignis( PieMenuEreignis::folgeanimation_beginnt,
			//			  reinterpret_cast< quint64 >( &_rectsAnimiert ),
			//			  reinterpret_cast< quint64 >( &_folgeBeginn ) );
			//	// qWarning() << "STOPP:" << a_sz( _folgeStart, _folgeStop );
			// }
			// break;
#pragma endregion
#pragma region( Auswahl )
#pragma endregion
#pragma region( neue_Region )
#pragma endregion
		default: break;
	}
	if ( os != _state ) qDebug() << "QPieMenu: STATE changed to" << _state;
}

void QPieMenu::makeState( PieMenuStatus s )
{
	if ( _state != s )
	{
		switch ( s )
		{
			case PieMenuStatus::hidden:
				_data.initHideAway( _initData._animBaseDur * 4, qMax( _hoverId, _folgeId ) );
				ereignis( PieMenuEreignis::zeitanimation_beginnt,
						  reinterpret_cast< qint64 >( &_rectsAnimiert ) );
				break;
			case PieMenuStatus::still: // STILL-Zustand herstellen - sollte immer akzeptabel sein!
				// Alle Elemente fahren auf ihren Ursprungszustand zurück.
				_data.initStill( _initData._animBaseDur >> 1 ), _rectsAnimiert = true;
				// Das Selection Rect verschwindet.
				initHover();
				break;
			case PieMenuStatus::closeby:
				// Wenn ich im closeby-Status ankomme, muss das Selection Rect erstmal versteckt
				// werden
				initHover();
				break;
			case PieMenuStatus::hover:
				initHover();
				// Und QMenu / die QActions brauchen noch
				actions().at( _hoverId )->hover();
				break;
			case PieMenuStatus::item_active: // still to find out..
				// setActiveAction( actions().at( _activeId ) );
				break;
			default:
				qDebug() << "State-Change-Handler NYI, state changed from" << _state << "to" << s;
				break;
		}
		_state = s;
	}
}

void QPieMenu::updateCurrentVisuals()
{
	if ( !_actionRectsDirty && !_selRectDirty ) return;

	if ( _actionRectsDirty )
	{
		if ( _data.update( _actionRects, _actionRenderData ) )
			ereignis( PieMenuEreignis::zeitanimation_abgeschlossen,
					  reinterpret_cast< qint64 >( &_rectsAnimiert ) );
		_actionRectsDirty = false;
	}

	if ( _selRectDirty )
	{
		if ( _selRectAnimiert )
		{
			auto x =
				smoothStep( static_cast< qreal >( _selRectStart.msecsTo( QTime::currentTime() ) )
							/ _initData._animBaseDur );
			_selRect( x, _srS, _srE );
			if ( x >= 1. )
				ereignis( PieMenuEreignis::zeitanimation_abgeschlossen,
						  reinterpret_cast< quint64 >( &_selRectAnimiert ) );
		}
		_selRectDirty = false;
	}
}

bool QPieMenu::hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID )
{
	int	 md = std::numeric_limits< int >::max();
	auto i( _actionRects.count() - 1 );
	for ( ; i >= 0; --i )
	{
		_hitDist[ i ] = boxDistance( p, _actionRects.at( i ) );
		if ( _hitDist[ i ] < md ) md = _hitDist[ i ], minDistID = i;
	}
	if ( Q_LIKELY( md < std::numeric_limits< int >::max() ) )
		mindDistance = static_cast< qreal >( md );
	else minDistID = i;
	return md <= 0; // Hit bei Distance <= 0 ...
}

void QPieMenu::initHover()
{
	// - nimm die letzte Position des SelRects as Startwert
	_srS = _selRect;
	if ( _hoverId == -1 )
	{
		// Das Selection-Rect "weganimieren"
		_srE = { { _lastPos.toPointF(), _lastPos.toPointF() }, _styleData.HLtransparent };
		// Wenn Hover verlassen wird, muss der hover-Timer gestoppt werden.
		if ( _alertId != 0 ) ereignis( PieMenuEreignis::timer_abbruch );
	} else // Was braucht die Selection Rect-Animation? -> Position und Farbe
	{
		auto r = _actionRects[ _hoverId ];
		auto p = r.center();
		r.setSize( 1.25 * r.size() );
		r.moveCenter( p );
		_srE = { r, _styleData.HL };
		if ( actions().at( _hoverId )->menu() )
			ereignis( PieMenuEreignis::timer_startet, _initData._subMenuDelayMS,
					  reinterpret_cast< qint64 >( &_hoverStart ) );
	}
	ereignis( PieMenuEreignis::zeitanimation_beginnt,
			  reinterpret_cast< quint64 >( &_selRectAnimiert ),
			  reinterpret_cast< quint64 >( &_selRectStart ) );
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
