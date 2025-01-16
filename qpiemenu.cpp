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
}

QPieMenu::~QPieMenu()
{
	qDebug() << "QPieMenu destroying" << this;
}

QSize QPieMenu::sizeHint() const
{
	return _boundingRect.size();
}

void QPieMenu::setVisible( bool vis )
{
	if ( vis != isVisible() ) initVisible( vis );
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

	QPainterPath		 pp;
	QRectF				 ab = { { -_data.r(), -_data.r() }, QPointF{ _data.r(), _data.r() } };
	pp.arcTo( ab, -90 + qRadiansToDegrees( _data.first().a ),
			  qRadiansToDegrees( qAbs( _data.last().a - _data.first().a )
								 * ( _initData._negativeDirection ? -1. : 1. ) ) );
	pp.closeSubpath();
	p.fillPath( pp, _styleData.HLtransparent );

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
		opt.state.setFlag( QStyle::State_Selected,
						   ( i == _hoverId ) && !_selRectAnimiert.isActive() );
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
	qDebug() << "QPieMenu" << title() << "::showEvent - geo:" << geometry()
			 << ", radius:" << _data.r() << ", globalCenter:" << geometry().center();
	qApp->setEffectEnabled( Qt::UI_FadeMenu, false );
	QMenu::showEvent( e );
}

void QPieMenu::mouseMoveEvent( QMouseEvent *e )
{
	if ( _kbdOvr.isActive() ) return e->ignore();
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
		qreal r = _data.r();
		bool  out =
			_lastDm >= qMax( 0.5 * qMin( _avgSz.width(), _avgSz.height() ), 2. * _styleData.sp );
		bool hit	 = hitTest( p, _lastDi, id );
		bool closeBy = !hit && out && ( _lastDi <= 2 * r ) && ( id != -1 );
		// Alle Informationen sind beschafft und Schlussfolgerungen stehen bereit -> State-Engine
		// bearbeiten und Ereignisse produzieren:
		//_stateEngine->submitEvent(
		//	"mouseMove",
		//	QVariantMap{ { "dM", { _lastDm } }, { "dI", { _lastDi } }, { "id", { id } } } );
		switch ( _state )
		{
			case PieMenuStatus::still:
				// Der Priorität nach prüfen: hover, closeby
				if ( hit ) initHover( id );
				else if ( closeBy ) initCloseBy( id );
				break;
			case PieMenuStatus::closeby:
				if ( hit ) initCloseBy(), initHover( id );
				else if ( !closeBy ) initCloseBy(), makeState( PieMenuStatus::still );
				else if ( _folgeId != id ) initCloseBy( id );
				break;
			case PieMenuStatus::hover:
				if ( !hit ) // Kein Hover mehr? Kann dennoch zu 2 anderen Stati wechseln...
				{
					initHover();
					if ( closeBy ) initCloseBy( id );
					else makeState( PieMenuStatus::still );
				} else if ( id != _hoverId ) initHover( id );
				break;
			// In allen anderen Stati ist hier (vorerst) nichts zu tun.
			case PieMenuStatus::item_active: break;
			default: break;
		}

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
	if ( _state == PieMenuStatus::hover || _lastDm < _avgSz.height() ) e->accept();
	else QWidget::mouseMoveEvent( e );
}

void QPieMenu::mousePressEvent( QMouseEvent *e )
{
	if ( _kbdOvr.isActive() ) return e->ignore();
	qDebug() << "QPieMenu::mousePressEvent(" << e << ")";
	auto p		= e->pos() + _boundingRect.topLeft();
	auto launch = e->buttons().testAnyFlags( Qt::RightButton | Qt::LeftButton );
	switch ( _state )
	{
		case PieMenuStatus::hover: // HitTest, wenn sich was bewegt hat ...
			if ( p == _lastPos || boxDistance( p, _actionRects[ _hoverId ] ) <= 0. )
			{ // Ich probiere mal einen Sel Rect Color Fade
				auto c = _srE.second.convertTo( QColor::Hsv );
				c.setHsv( ( c.hsvHue() + 180 ) % 360, c.hsvSaturation(), c.value(), 208 );
				startSelRectColorFade( c );
				return e->accept();
			} else {
				e->ignore();
				qDebug() << "hit test failed!";
			}
			break;
		default: // Jeder Mouse-Press ausserhalb von Elementen ist dazu da, das Menu wieder zu
				 // schließen.  Bin mir gerade nicht sicher, ob ich den Release besser abwarten
				 // sollte...
			setActiveAction( nullptr );
			initVisible( false );
			e->accept();
			break;
	}
	QWidget::mousePressEvent( e );
}

void QPieMenu::mouseReleaseEvent( QMouseEvent *e )
{
	if ( _kbdOvr.isActive() ) return e->ignore();
	qDebug() << "QPieMenu::mouseReleaseEvent(" << e << ")";
	auto p		= e->pos() + _boundingRect.topLeft();
	auto launch = e->buttons().testAnyFlags( Qt::RightButton | Qt::LeftButton );
	if ( _state == PieMenuStatus::hover && // HitTest, wenn sich was bewegt hat ...
		 ( p == _lastPos || boxDistance( p, _actionRects[ _hoverId ] ) <= 0. ) )
	{
		auto a = actions().at( _hoverId );
		if ( auto am = qobject_cast< QPieMenu * >( a->menu() ) )
		{
			makeState( PieMenuStatus::item_active );
		} else {
			setActiveAction( a );
			a->activate( QAction::Trigger );
			emit QMenu::triggered( a );
			initVisible( false );
		}
	} else if ( _state != PieMenuStatus::hidden ) return QWidget::mouseReleaseEvent( e );
	e->accept();
}

void QPieMenu::keyPressEvent( QKeyEvent *e )
{
	enum { none, prev, next, use, out } dir;
	switch ( e->key() )
	{
		case Qt::Key_ApplicationLeft: [[fallthru]];
		case Qt::Key_Left: dir = layoutDirection() == Qt::LeftToRight ? prev : next; break;
		case Qt::Key_ApplicationRight: [[fallthru]];
		case Qt::Key_Right: dir = layoutDirection() != Qt::LeftToRight ? prev : next; break;
		case Qt::Key_Close: [[fallthru]];
		case Qt::Key_Exit: [[fallthru]];
		case Qt::Key_Cancel: [[fallthru]];
		case Qt::Key_No: [[fallthru]];
		case Qt::Key_Escape: dir = out; break;
		case Qt::Key_Execute: [[fallthru]];
		case Qt::Key_Yes: [[fallthru]];
		case Qt::Key_Space: [[fallthru]];
		case Qt::Key_Return: [[fallthru]];
		case Qt::Key_Enter: dir = use; break;
		case Qt::Key_Up: dir = prev; break;
		case Qt::Key_Down: dir = next; break;
		default: dir = none; break;
	}
	switch ( dir )
	{
		case out:
			setActiveAction( nullptr );
			initVisible( false );
			break;
		case use:
			if ( _hoverId != -1 )
			{
				auto a = actions().at( _hoverId );
				setActiveAction( a );
				if ( a->menu() )
				{
				} else {
					a->activate( QAction::Trigger );
					emit triggered( a );
					initVisible( false );
				}
			}
			break;
		case prev: [[fallthru]];
		case next:
			focusNextPrevChild( dir == next );
			_hoverId = actionIndex( activeAction() );
			_kbdOvr.start( _initData._subMenuDelayMS * 2, this );
			makeState( PieMenuStatus::item_active );
			break;
		case none: return QMenu::keyPressEvent( e );
	}
	e->accept();
}

void QPieMenu::timerEvent( QTimerEvent *e )
{
	auto tid = e->timerId();
	if ( tid == _alertTimer.timerId() )
	{
		_alertTimer.stop();
		if ( _state == PieMenuStatus::hover && _hoverId == _alertId )
			makeState( PieMenuStatus::item_active );
	} else if ( tid == _kbdOvr.timerId() ) {
		_kbdOvr.stop();
		initHover( _hoverId );
		qDebug() << "END_KEYBOARD_OVERRIDE";
	} else {
		if ( tid == _selRectAnimiert.timerId() ) _selRectDirty = true, update();
		else if ( tid == _rectsAnimiert.timerId() )
		{
			if ( _data.update( _actionRects, _actionRenderData ) )
			{
				_rectsAnimiert.stop();
				// Verschobenes Hiding ...
				if ( _state == PieMenuStatus::hidden ) initVisible( false );
			}
			_actionRectsDirty = false;
			update();
		} else return QMenu::timerEvent( e );
	}
	e->accept();
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
	_styleData.HLtransparent.setAlphaF( 0.05 );
	_selRect.second	  = _hoverId == -1 ? _styleData.HLtransparent : _styleData.HL;
	// Es wurden auch Größen und Margins neu gelesen -> die rects sind vmtl. dirty.
	_actionRectsDirty = true;
}

void QPieMenu::calculatePieDataSizes()
{
	// Alle Größen sind voneinander abhängig, wenn wir einen konsistenten Stil (wie ihn Menüs
	// nunmal haben sollten) mit zumindest gleichem Tabstopp verwenden wollen.  Da wir alle
	// anfassen müssen, können wir auch gleich immer alle berechnen.
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
	// Margins auf das boundingRect draufrechnen, damit wir ohne unsere Geometrie ändern zu
	// müssen, auch Skalierungseffekte erstellen können.
	//
	// Zuerst brauchen wir eine gute Schätzung für den startRadius.  Wir haben gelernt, dass
	// hier sowohl Breite, als auch Höhe eingehen. Weiterhin erscheint mir gerade relativ klar,
	// dass bis zu einem
	//      Radius < length( fromSize( average_item_size ) )
	// der Durchschnittsgröße weniger Items im Kreis platziert werden können, weil im Endeffekt
	// nur eine Seite des Kreises zum Anordnen von Elementen zur Verfügung steht.  Wären auf der
	// anderen Seite, also ab 180° Gesamtüberdeckung auch Elemente, würde es bei diesen kleinen
	// Radien zwangsläufig zu Überdeckungen kommen.
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
	// Informiere die State Engine über die Datenänderungen
	//_stateEngine->submitEvent(
	//	"initEvent", QVariantMap{ { "r0", r },
	//							  { "minDm", qMax( 0.5 * qMin( _avgSz.width(), _avgSz.height()
	//),
	//											   2. * _styleData.sp ) } } );
}

qreal QPieMenu::startR( int runde ) const
{
	auto asz = _avgSz;
	auto avp = fromSize( asz );
	// Radius, der spätestens in der 3. Runde verwendet werden sollte (ggf. noch ein "+
	// Spacing"?)
	auto r3	 = qSqrt( QPointF::dotProduct( avp, avp ) );
	// Bisher eine gute Schätzung:
	//      ( AspectRatio * (item_height+spacing) * itemCount ) / floor( winkelÜberdeckung / 90°
	//      )
	auto r0	 = qMax( _initData._minR, ( avp.y() / avp.x() ) * ( asz.height() + _styleData.sp )
										  * actions().count() / qFloor( _initData._max0 / M_PI_2 ) );
	// Also: 0. - 3. Runde bewegen sich der Radius linear zwischen r0 und r3, falls _minR nicht zu
	// gross ist
	if ( r0 < r3 && runde < 4 ) return r0 + runde * ( r3 - r0 ) / 3;
	// Danach addieren wir für jede Runde eine halbe Item-Höhe drauf.
	return qMax( r0, r3 ) + ( runde - 3 ) * ( asz.height() >> 1 );
}

void QPieMenu::makeState( PieMenuStatus s )
{
	if ( _state != s || s == PieMenuStatus::hidden )
	{
		switch ( s )
		{
			case PieMenuStatus::hidden: break;
			case PieMenuStatus::still: // STILL-Zustand herstellen - sollte immer akzeptabel
									   // sein!
				// Alle Elemente fahren auf ihren Ursprungszustand zurück.
				_folgeId = _hoverId = -1;
				_data.initStill( _initData._animBaseDur >> 1 );
				_rectsAnimiert.start( 10, this );
				startSelRect( { 0, 0, -1, -1 } );
				break;
			case PieMenuStatus::closeby: break;
			case PieMenuStatus::hover: break; // using initHover(id)
			case PieMenuStatus::item_active:
				// Die _hoverId sollte vorab gesetzt werden.  Hier wird nur der Status durch das
				// Selection Rect übernommen. Wenn der Timer für den Keyboard Override aktiv ist,
				// war es eine Keyboard-Aktivierung.  Wenn nicht, ist der Submenu-Timer abgelaufen.
				{
					auto r = _actionRects[ _hoverId ];
					auto p = r.center();
					r.setSize( 1.25 * r.size() );
					r.moveCenter( p );
					startSelRect( r );
					if ( !_kbdOvr.isActive() ) showChild( _hoverId );
				}
				break;
			default: break;
		}
		setState( s );
	}
}

void QPieMenu::showChild( int index )
{
	auto r	= _actionRects[ index ];
	auto a	= actions().at( index );
	auto pm = ( r.center() - _boundingRect.topLeft() + pos() /**/ );
	qDebug() << "show as child: p=" << r.center() << ", p_mapped=" << pm << ", myPos=" << pos()
			 << ", myGeometry=" << geometry() << ", myBR=" << _boundingRect;
	if ( auto cpm = qobject_cast< QPieMenu * >( a->menu() ) )
	{
		qDebug() << "-> child QPieMenu showing ...";
		cpm->showAsChild( this, pm, qMax( r.width(), r.height() ) * 1.25,
						  _data[ index ].a + M_2_SQRTPI, _data[ index ].a - M_2_SQRTPI );
	} else if ( auto m = a->menu() ) {
		qDebug() << "-> popup QMenu ...";
		m->popup( pm );
	}
	qDebug() << "show as child: showing done, setting action active" << a;
	setActiveAction( a );
	qDebug() << "show as child: FINISHED!";
}

void QPieMenu::initVisible( bool show )
{
	if ( show )
	{
		// In dem Moment, wo das Menu sichtbar gesetzt wird, sollte es eine Position erhalten haben.
		// => wenn die internen Funktionen genutzt wurden, sollte die berechnete Position in
		// _initData stehen. Anonsten nutzen wir die Verschiebung, wie vorher im ShowEvent ...
		auto fromPar = !_initData._execPoint.isNull();
		if ( !fromPar ) setGeometry( geometry().translated( _boundingRect.topLeft() ) );
		else
			move( _initData._execPoint ),
				setGeometry( geometry().translated( _boundingRect.topLeft() ) );
		QMenu::setVisible( true );
		// Schalte die Animation zum Anzeigen ein
		_data.initShowUp( _initData._animBaseDur * 4,
						  fromPar ? _initData._start0 + _initData.dir( 0.5 ) * _initData._max0
								  : 0.f );
		_rectsAnimiert.start( 10, this );
		_selRect	  = { {}, _styleData.HLtransparent };
		_selRectDirty = false;
		setState( PieMenuStatus::still ); // Not calling makeState on Purpose!
	} else {
		// 2. Aufruf, nach der Anim ...
		if ( _state == PieMenuStatus::hidden )
		{
			QMenu::setVisible( false );
			if ( _causedMenu ) _causedMenu->childHidden( this );
		} else { // 1. Aufruf -> Anim starten, Zustand merken
			_data.initHideAway( _initData._animBaseDur * 4, _laId > -1		? _laId
															: _hoverId > -1 ? _hoverId
																			: _folgeId );
			_rectsAnimiert.start( 10, this );
			auto c = _styleData.HLtransparent;
			if ( activeAction() )
				// Element wurde ausgewählt und aktiviert -> das selRect hat eine andere
				// Farbe bekommen!
				c = _srE.second;
			startSelRect( { 0, 0, -1, -1 } );
			_srE.second = c, _srE.second.setAlpha( 0 );
			// alle IDs zurücksetzen, ausser _activeId!
			_folgeId = _hoverId = -1;
			setState( PieMenuStatus::hidden );
		}
	}
}

void QPieMenu::initCloseBy( int newFID )
{
	// Ziel des Folgemodus: das nächstgelegene Item etwas vergrößern
	if ( newFID == -1 )
		;
	else
	{
		_folgeBeginn = QTime::currentTime();
		setState( PieMenuStatus::closeby );
	}
	_folgeId = newFID;
}

void QPieMenu::initHover( int newHID )
{
	auto sub = _state == PieMenuStatus::item_active;
	if ( newHID == _hoverId && !sub ) return;
	if ( newHID == -1 )
	{
		std::swap( _laId, _hoverId );
		_hoverId = newHID;
		// Das Selection-Rect "weganimieren"
		if ( !_kbdOvr.isActive() ) startSelRect( { 0, 0, -1, -1 } );
		// Wenn Hover verlassen wird, muss der hover-Timer gestoppt werden.
		_alertTimer.stop();
		setActiveAction( nullptr );
	} else { // Was braucht die Selection Rect-Animation? -> Position und Farbe
		auto r = _actionRects[ _hoverId = newHID ];
		auto p = r.center();
		r.setSize( 1.25 * r.size() );
		r.moveCenter( p );
		startSelRect( r );
		if ( !sub ) // nicht schon aktiv?
		{
			// Und QMenu / die QActions brauchen noch
			if ( auto a = actions().at( _hoverId ) )
				if ( a->menu() )
				{
					emit QMenu::hovered( a );
					a->activate( QAction::Hover );
					_alertId = _hoverId, _alertTimer.start( _initData._subMenuDelayMS, this );
				} else setActiveAction( a );
			else _alertTimer.stop();
		}
		// Wird Hover betreten, muss auch der Status gesetzt werden
		setState( PieMenuStatus::hover );
	}
}

void QPieMenu::updateCurrentVisuals()
{
	if ( !_actionRectsDirty && !_selRectDirty ) return;

	if ( _actionRectsDirty )
		_data.update( _actionRects, _actionRenderData ), _actionRectsDirty = false;
	if ( _selRectDirty )
	{
		if ( _selRectAnimiert.isActive() )
		{
			auto x =
				smoothStep( static_cast< qreal >( _selRectStart.msecsTo( QTime::currentTime() ) )
							/ _initData._animBaseDur );
			_selRect( x, _srS, _srE );
			if ( x >= 1. )
			{
				_selRectAnimiert.stop();
				qDebug() << "SRST0PP" << _selRect;
			}
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

void QPieMenu::startSelRect( QRect tr )
{
	if ( !tr.isNull() ) // Ziel wird hier vorbereitet
	{
		if ( tr.isValid() ) _srE = { tr.toRectF(), _styleData.HL };
		else _srE = { {}, _styleData.HLtransparent }, qDebug() << "SRSTART" << _srS << _srE;
	}
	// - nimm die letzte Position des SelRects as Startwert
	_srS		  = _selRect;
	_selRectStart = QTime::currentTime();
	_selRectAnimiert.start( 10, this );
}

void QPieMenu::startSelRectColorFade( const QColor &target_color )
{
	_srE.second = target_color;
	startSelRect();
}

void QPieMenu::showAsChild( QPieMenu *source, QPoint pos, qreal minRadius, qreal startAngle,
							qreal endAngle )
{
	// Ein Versuch, das automatische oder (via Tasta) manuelle öffnen eines Submenüs, was auch ein
	// QPieMenu ist, zu basteln ...
	auto dl						 = endAngle - startAngle;
	_initData._execPoint		 = pos;
	_initData._minR				 = minRadius;
	_initData._start0			 = startAngle;
	_initData._max0				 = qAbs( dl );
	_initData._negativeDirection = dl < 0.;
	calculateStillData();
	auto p = pos - QPointF{ _data.r(), _data.r() }.toPoint();
	popup( p );
}

void QPieMenu::childHidden( QPieMenu *child )
{
	initHover( actionIndex( child->menuAction() ) );
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
