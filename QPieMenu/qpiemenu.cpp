/******************************************************************************
 * QPieMenu.cpp - eine Implementation radialer Kontextmenüs in Qt
 * ==============================================================
 * Author: Stefan <St0fF / the0bsessedManiacs> Kaps
 * Entwicklungsbeginn: 31.12.2024
 ******************************************************************************
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
 *****************************************************************************/
#include "qpiemenu.h"

#include <QApplication>
#include <QPaintEvent>
#include <QStyleOptionMenuItem>
#include <QStylePainter>
#include <QWidgetAction>
#if _WIN32
#	pragma comment( lib, "dwmapi.lib" )
#	include "dwmapi.h"
#endif
// Helpers
constexpr QPointF asPointF( int v )
{
	return { qreal( v ), qreal( v ) };
}
constexpr QPointF asPointF( qreal v )
{
	return { v, v };
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

__forceinline QPointF qSinCos( qreal radians )
{
	return QPointF{ qSin( radians ), qCos( radians ) };
}
__forceinline qreal smoothStep( qreal t, qreal t0 = 0., qreal t1 = 1. )
{
	auto a = qMax( 0., qMin( 1., ( t - t0 ) / ( t1 - t0 ) ) );
	return a * a * ( 3 - 2 * a );
}

// PieMenu
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

QPieMenu::QPieMenu( const QString &title, const QList< QAction * > &actions, QWidget *parent )
	: QPieMenu( title, parent )
{
	addActions( actions );
}

QPieMenu::~QPieMenu()
{
	qDebug() << "QPieMenu destroying" << this;
}

QAction *QPieMenu::execPieStartRange( QPoint pos, float phi0, float dphimax )
{
	qDebug() << "QPieMenu" << title() << "::execPieStartRange(" << pos << "," << phi0 << ","
			 << dphimax << ")";
	_initData._execPoint		 = pos;
	_initData._start0			 = phi0;
	_initData._max0				 = qAbs( dphimax );
	_initData._negativeDirection = ( dphimax < 0.f );
	createStillData();
	_actionRectsDirty = true;
	return exec( pos );
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
	createStillData();
	_actionRectsDirty = true;
	event->accept();
	// QMenuPrivate at least needs to know about whatever they think is needed
	QMenu::actionEvent( event );
}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	updateCurrentVisuals();
	QStylePainter		 p( this );
	QStyleOptionMenuItem opt;
	auto				 ac = actions().count();
	p.translate( -_boundingRect.topLeft() );

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

	// Elemente
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
	auto acc = !_kbdOvr.isActive();
	if ( acc )
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
			qreal r	  = _data.r(), d2( std::numeric_limits< qreal >::max() ), tmpD;
			bool  out = _lastDm
					   >= qMax( 0.5 * qMin( _avgSz.width(), _avgSz.height() ), 2. * _styleData.sp );
			bool hit	 = hitTest( p, _lastDi, id );
			bool closeBy = !hit && out && ( _lastDi <= 2 * r ) && ( id != -1 );
			// Testaufgabe #26 - atan2-test => stelle die Winkel-nächste ID fest
			auto _lastW	 = qAtan2( p.x(), p.y() );
			_lastWi		 = -1;
			for ( auto i( 0ll ), c( _data.count() ); i < c; ++i )
				if ( !actions().at( i )->isSeparator()
					 && d2 > ( tmpD = qAbs( distance< M_PI >( _lastW, _data[ i ].a ) ) ) )
					d2 = tmpD, _lastWi = i;
			if ( closeBy
				 && _lastDi > fromSize( _actionRects[ id ].size() ).manhattanLength() * 0.5 )
				id = _lastWi;
			// Alle Informationen sind beschafft und Schlussfolgerungen stehen bereit
			switch ( _state )
			{
				case PieMenuStatus::still:
					// Der Priorität nach prüfen: hover, closeby
					if ( hit ) initHover( id );
					else if ( closeBy ) initCloseBy( id );
					else acc = false;
					break;
				case PieMenuStatus::closeby:
					if ( hit ) initHover( id );
					else if ( !closeBy ) initStill();
					else if ( _folgeId != id ) initCloseBy( id );
					else acc = false;
					break;
				case PieMenuStatus::hover:
					if ( !hit ) // Kein Hover mehr? Kann dennoch zu 2 anderen Stati wechseln...
					{
						initHover();
						if ( closeBy ) initCloseBy( id );
						else initStill();
					} else if ( id != _hoverId ) initHover( id );
					break;
				// In allen anderen Stati ist hier (vorerst) nichts zu tun.
				case PieMenuStatus::item_active: [[fallthru]];
				default: acc = false; break;
			}
			if ( acc ) update();
		} else acc = false;
	}
	if ( acc ) e->accept();
	else QWidget::mouseMoveEvent( e );
}

void QPieMenu::mousePressEvent( QMouseEvent *e )
{
	if ( _kbdOvr.isActive() ) return e->ignore();
	qDebug() << "QPieMenu::mousePressEvent(" << e << ")";
	_mouseDown	= true;
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
				return e->ignore();
				// qDebug() << "hit test failed!";
			}
		default: // Jeder Mouse-Press ausserhalb von Elementen ist dazu da, das Menu wieder zu
				 // schließen.  Bin mir gerade nicht sicher, ob ich den Release besser abwarten
				 // sollte...
			initVisible( false );
			if ( _causedMenu ) _causedMenu->childHidden( this, false );
			return e->accept();
	}
}

void QPieMenu::mouseReleaseEvent( QMouseEvent *e )
{
	if ( _kbdOvr.isActive() || !_mouseDown ) return e->ignore();
	qDebug() << "QPieMenu::mouseReleaseEvent(" << e << ")";
	_mouseDown	= false;
	auto p		= e->pos() + _boundingRect.topLeft();
	auto launch = e->buttons().testAnyFlags( Qt::RightButton | Qt::LeftButton );
	if ( ( _state == PieMenuStatus::hover ) && ( _hoverId > -1 ) &&
		 // HitTest, wenn sich was bewegt hat ...
		 ( ( p == _lastPos ) || ( boxDistance( p, _actionRects[ _hoverId ] ) <= 0. ) ) )
	{
		auto a = actions().at( _hoverId );
		if ( auto am = qobject_cast< QPieMenu * >( a->menu() ) )
		{
			initActive();
		} else {
			setActiveAction( a );
			QKeyEvent ev( QEvent::Type::KeyPress, Qt::Key_Return, Qt::NoModifier );
			QMenu::keyPressEvent( &ev );
			initVisible( false );
			if ( _causedMenu ) _causedMenu->childHidden( this, true );
		}
	} else if ( _state != PieMenuStatus::hidden ) {
		initVisible( false );
		if ( _causedMenu ) _causedMenu->childHidden( this, false );
	}
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
			initActive();
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
		if ( _state == PieMenuStatus::hover && _hoverId == _alertId ) initActive();
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

void QPieMenu::hideEvent( QHideEvent *e )
{
	qDebug() << "QPieMenu" << title() << "::hideEvent( " << e << " ) ";
	QMenu::hideEvent( e );
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

qreal QPieMenu::startR( int runde ) const
{
	auto asz = _avgSz;
	auto avp = fromSize( asz );
	// Radius, der spätestens in der 3. Runde verwendet werden sollte (ggf. noch ein "+
	// Spacing"?)
	auto r3	 = qSqrt( QPointF::dotProduct( avp, avp ) );
	// Bisher eine gute Schätzung:
	//  ( AspectRatio * (item_height+spacing) * itemCount ) / floor( winkelÜberdeckung / 90° )
	// MUSS ÜBERARBEITET WERDEN!!! Zu viele Rechenvorgänge ...
	auto r0	 = qMax( avp.y(), qMax( _initData._minR,
									( avp.y() / avp.x() ) * ( asz.height() + _styleData.sp )
										* actions().count() / qFloor( _initData._max0 / M_PI_2 ) ) );
	// Also: 0. - 3. Runde bewegen sich der Radius linear zwischen r0 und r3, falls _minR nicht zu
	// gross ist
	if ( r0 < r3 && runde < 4 ) return r0 + runde * ( r3 - r0 ) / 3;
	// Danach addieren wir für jede Runde eine halbe Item-Höhe drauf.
	return qMax( r0, r3 ) + ( runde - 3 ) * ( asz.height() >> 1 );
}

void QPieMenu::createStillData()
{
	// Die "neue" Rechenfunktion für die Basisdaten.
	// =============================================
	// Wichtige Eckpunkte:
	//  _initData._isSubMenu:   -   ich beginne beim zentralen Element (je nachdem, ob geradzahlig
	//                              oder nicht) und gehe erst in eine, dann in die andere Richtung
	// Ansonsten: wenn ich bei 0 beginne und nur in eine Richtung laufe muss ich sicherstellen, dass
	// der Anfansgwinkel nicht überdeckt ist.  Dort soll das Menu schliesslich anfangen und nicht
	// schon längst angefangen haben ... ergo beim Item #0: lastSz = 0, aber berechnen

	int ac = actions().count(), runde = -1, ip, im;
	if ( _data.count() != ac ) return;

	QRectF rwsd0{ _initData._minR, _initData._start0, 1.f, _initData.dir() }, rwsd{ rwsd0 }, nr;
	QSizeF lstSz0{ 0., 0. }, lstSz{ lstSz0 };
	qreal  deltaSum, delta;
	Intersector< QRectF, QPointF > overlap;
	bool						   needMoreSpace( false );
	do {
		deltaSum = 0., needMoreSpace = false, runde++, overlap.clear();
		rwsd0.moveLeft( startR( runde ) );
		// Den Start feststellen: ist der ExecPoint nicht gesetzt, wurde dieses Objekt nicht mit den
		// Hilfsfunktionen, sondern mit QMenu gestartet -> standard Werte nehmen!
		if ( _initData._isSubMenu )
		{
			// Submenüs erhalten Radius- und Winkel-Angaben vor dem Popup.  Sie sollen von der Mitte
			// aus berechnet werden, um eine möglichst gleichmäßige Überdeckung zu erhalten.
			rwsd0.moveTop( _initData._start0 + _initData.dir( 0.5 ) * _initData._max0 );
			ip = im = ac >> 1;
			if ( ac % 1 )
			{ // ungerade Anzahl -> mittlere Option kommt auf den Mittenwinkel
				lstSz0				 = _data[ ip++ ]; // implizit als QSizeF
				_data[ im-- ].ziel() = *reinterpret_cast< __m256d * >( &rwsd0 );
			} else { // gerade Anzahl -> jeweils vom Startwinkel aus losschreiten.
				--im;
			}
		} else {
			// Standard: nur vorwärts laufen ...
			im = -2, ip = 0;
		}
		rwsd = rwsd0;
		rwsd.setHeight( _initData.dir( -1. ) ); // Rückwärts für die erste Runde
		// ACHTUNG: stepBox wird immer 1x öfter aufgerufen, um die Winkelabdeckung des jew. letzten
		// Elementes korrekt in die Berechnung mit einzubeziehen.
		while ( !needMoreSpace && im >= -1 )
		{
			// Schreite rückwärts
			delta = stepBox( im, rwsd, lstSz );
			needMoreSpace =
				( qFuzzyIsNull( delta ) ) || ( deltaSum += qAbs( delta ) > _initData._max0 );
			if ( im >= 0 )
			{
				nr = { {}, rwsd.width() * QSizeF( _data[ im ] ) };
				nr.moveCenter( rwsd.x() * qSinCos( rwsd.y() ) );
				needMoreSpace |= !overlap.add( nr );
			}
			--im;
		}
		rwsd = rwsd0, lstSz = lstSz0;
		while ( !needMoreSpace && ip <= ac )
		{
			// Schreite vorwärts
			delta = stepBox( ip, rwsd, lstSz );
			needMoreSpace =
				( qFuzzyIsNull( delta ) ) || ( deltaSum += qAbs( delta ) > _initData._max0 );
			if ( ip < ac )
			{
				nr = { {}, rwsd.width() * QSizeF( _data[ ip ] ) };
				nr.moveCenter( rwsd.x() * qSinCos( rwsd.y() ) );
				needMoreSpace |= !overlap.add( nr );
			}
			++ip;
		}
	} while ( needMoreSpace );
	// Berechnungen sind abgeschlossen.  Jetzt müssen die Animationsdaten noch in Still-Daten
	// umgewandelt werden
	makeZielStill( rwsd.x() );
}

void QPieMenu::createZoom()
{
	int ac = actions().count(), ip = _folgeId + 1, im = _folgeId - 1;
	if ( _data.count() != ac ) return;
	_rectsAnimiert.stop();
	_data.copyCurrent2Source();
	// ich möchte das Element _folgeId auf Skalierungsfaktor 1.5 fahren und alle anderen Boxen
	// ausweichen lassen - bisher scheint das leider nicht richtig zu funktionieren, vermutlich wird
	// zum Ausweichen doch mehr Radius gebraucht.
	_data[ _folgeId ].ziel() = _mm256_setr_pd( _data.r(), _data[ _folgeId ].a, 1., SCALE_MAX );
	auto t01				 = _mm_setr_pd( 0., 1. );
	_data[ _folgeId ].t01()	 = t01;
	QRectF rwsd0{ _data.r(), _data[ _folgeId ].a, 1., _initData.dir() }, rwsd{ rwsd0 }, nr;
	QSizeF lstSz0{ QSizeF( _data[ _folgeId ] ) * SCALE_MAX }, lstSz{ lstSz0 };
	qreal  delta;

	// In dieser Situation ist die Ruhedatenberechnung längst passiert und die Boxen sind alle
	// sichtbar auf dem Bildschirm.  Daher kann ich auf mehrere Prüfungen verzichten:
	//  - needMorSpace wird (hoffentlich) nicht nötig sein - Ausweichstrategie, falls doch: Radius
	//  nur für diese Elemente etwas vergrößern - ist aber leider nicht mit der Datenstruktur
	//  abbildbar, da ich die Radien für die Elemente nur animiere, aber nicht extra Standardwerte
	//  speichere.
	while ( ip < ac )
	{
		delta				= stepBox( ip, rwsd, lstSz );
		_data[ ip++ ].t01() = t01;
	}
	rwsd = rwsd0, lstSz = lstSz0;
	rwsd.setHeight( _initData.dir( -1. ) );
	while ( im >= 0 )
	{
		delta				= stepBox( im, rwsd, lstSz );
		_data[ im-- ].t01() = t01;
	}
	_data.startAnimation( _initData._animBaseDur );
	_rectsAnimiert.start( 10, this );
}

qreal QPieMenu::stepBox( int index, QRectF &rwsd, QSizeF &lastSz )
{
	// Ich berechne hier die nächste Box in die entsprechende Richtung.
	BestDelta bd;
	qreal	  d;
	bool	  needO	 = ( index < 0 || index >= _data.count() );
	auto	  c		 = rwsd.x() * qSinCos( rwsd.y() );
	auto	  ns	 = rwsd.width() * ( needO ? QSizeF{ 0., 0. } : QSizeF( _data[ index ] ) );
	auto	  offset = ( fromSize( lastSz ) + fromSize( ns ) ) * 0.5 + asPointF( _styleData.sp );
	bd.init( rwsd.height(), rwsd.y() );
	for ( auto i : { c + offset, c - offset } )
	{
		if ( abs( i.x() ) <= rwsd.x() ) bd.addRad2( qAsin( i.x() / rwsd.x() ), true );
		if ( abs( i.y() ) <= rwsd.x() ) bd.addRad2( qAcos( i.y() / rwsd.x() ) );
	}
	d = bd.best();
	if ( !needO )
	{
		auto &i = _data[ index ];
		if ( d != 0. || rwsd.height() == 0. )
		{
			// nächste Box berechnet - aktualisiere die "Laufvariablen"
			i.er = rwsd.x();
			rwsd.moveTop( i.ea = ( rwsd.y() + d ) );
			i.es   = rwsd.width();
			lastSz = ns;
		}
	}
	return d;
}

void QPieMenu::makeZielStill( qreal r0 )
{
	// Übertragen berechneter Animationszieldaten in die Still-Data
	for ( auto &i : _data ) i.a = i.ea;
	_data.setR( r0 );
	_boundingRect = QRectF{ { -r0, -r0 }, QPointF{ r0, r0 } }.toRect();
	auto xa		  = _avgSz.width();	 //>> 1;
	auto ya		  = _avgSz.height(); // >> 1;
	_boundingRect.adjust( -xa, -ya, xa, ya );
	updateGeometry();
}

void QPieMenu::showChild( int index )
{
	auto r	= _actionRects[ index ];
	auto a	= actions().at( index );
	auto pm = ( r.center() - _boundingRect.topLeft() + pos() /**/ );
	qDebug() << "show child: p=" << r.center() << ", p_mapped=" << pm << ", myPos=" << pos()
			 << ", myGeometry=" << geometry() << ", myBR=" << _boundingRect;
	if ( auto cpm = qobject_cast< QPieMenu * >( a->menu() ) )
	{
		qDebug() << "-> child QPieMenu showing ...";
		cpm->showAsChild( this, pm, fromSize( r.size() ).manhattanLength() * 1.2,
						  _data[ index ].a + M_2_SQRTPI, _data[ index ].a - M_2_SQRTPI );
	} else if ( auto m = a->menu() ) {
		qDebug() << "-> popup QMenu ...";
		m->popup( pm );
	}
	qDebug() << "show child: showing done, setting action active" << a;
	setActiveAction( a );
	qDebug() << "show child: FINISHED!";
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
		_data.initShowUp( _initData._animBaseDur,
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
			auto a = activeAction();
			qDebug() << "QPieMenu::internalHide, activeAction =" << a;
			if ( a ) a->activate( QAction::Trigger );
			QMenu::setVisible( false );
		} else { // 1. Aufruf -> Anim starten, Zustand merken
			_data.initHideAway( _initData._animBaseDur * 2, actionIndex( activeAction() ) );
			_rectsAnimiert.start( 10, this );
			auto c = _styleData.HLtransparent;
			if ( _state == PieMenuStatus::hover )
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

void QPieMenu::initStill()
{ // STILL-Zustand herstellen - sollte immer akzeptabel
  // sein.  Alle Elemente fahren auf ihren Ursprungszustand zurück.
	_folgeId = _hoverId = -1;
	_rectsAnimiert.stop();
	_data.initStill( _initData._animBaseDur >> 1 );
	_rectsAnimiert.start( 10, this );
	startSelRect( { 0, 0, -1, -1 } );
	setState( PieMenuStatus::still );
}

void QPieMenu::initCloseBy( int newFID )
{
	_folgeId = newFID;
	// Ziel des Folgemodus: das nächstgelegene Item etwas zu vergrößern
	if ( newFID == -1 ) _data.initStill( _initData._animBaseDur >> 1 );
	else
	{
		createZoom();
		setState( PieMenuStatus::closeby );
	}
}

void QPieMenu::initHover( int newHID )
{
	auto sub = ( _state == PieMenuStatus::item_active );
	if ( newHID == _hoverId && !sub ) return;
	if ( newHID == -1 )
	{
		_hoverId = newHID;
		// Das Selection-Rect "weganimieren"
		if ( !_kbdOvr.isActive() ) startSelRect( { 0, 0, -1, -1 } );
		// Wenn Hover verlassen wird, muss der hover-Timer gestoppt werden.
		_alertTimer.stop();
	} else
	{ // Was braucht die Selection Rect-Animation? -> Position und Farbe
	  // Dummerweise muss die Position vom Ziel aus berechnet werden!
		_folgeId = _hoverId = newHID;
		createZoom();
		auto r = QRect{ {}, SCALE_MAX * QSize( _data[ _hoverId ] ) };
		r.moveCenter( ( _data.r() * qSinCos( _data[ _hoverId ].a ) ).toPoint() );
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

void QPieMenu::initActive()
{
	// Die _hoverId sollte vorab gesetzt werden.  Hier wird nur der Status durch das
	// Selection Rect übernommen. Wenn der Timer für den Keyboard Override aktiv ist,
	// war es eine Keyboard-Aktivierung.  Wenn nicht, ist der Submenu-Timer abgelaufen.
	auto r = QRect{ {}, SCALE_MAX * QSize( _data[ _hoverId ] ) };
	r.moveCenter( ( _data.r() * qSinCos( _data[ _hoverId ].a ) ).toPoint() );
	startSelRect( r );
	if ( !_kbdOvr.isActive() ) showChild( _hoverId );
	setState( PieMenuStatus::item_active );
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
				// qDebug() << "SRST0PP" << _selRect;
			}
		}
		_selRectDirty = false;
	}
}

bool QPieMenu::hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID )
{
	int	 md = std::numeric_limits< int >::max(), d;
	auto i( _actionRects.count() - 1 );
	for ( ; i >= 0; --i )
		if ( !actions().at( i )->isSeparator() )
		{
			d = boxDistance( p, _actionRects.at( i ) );
			if ( d < md ) md = d, minDistID = i;
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
		else _srE = { {}, _styleData.HLtransparent } /*, qDebug() << "SRSTART" << _srS << _srE*/;
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
	_causedMenu					 = source;
	createStillData();
	auto p = pos - QPointF{ _data.r(), _data.r() }.toPoint();
	popup( p );
}

void QPieMenu::childHidden( QPieMenu *child, bool hasTriggered )
{
	qDebug() << "QPieMenu::childHidden" << hasTriggered;
	if ( hasTriggered ) initVisible( false );
	else initHover( actionIndex( child->menuAction() ) );
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

/***************************************************************************************************
 * Der Super-polator. .. ... .. . Eigentlich nur eine einfache Listen-Interpolator-Klasse.
 * ---------------------------------------------------------------------------------------
 * Author: Stefan <St0fF / Neoplasia ^ the 0bsessed Maniacs> Kaps, 10.Jan.2025
 ***************************************************************************************************
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
	return _mm256_castsi256_pd( _mm256_insert_epi64(
		_mm256_castpd_si256( dst ), *reinterpret_cast< long long * >( &v ), pos ) );
}

__m256d test( SPElem &e, qreal t )
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
		auto &ii	= operator[]( i );
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
		auto &ii	= operator[]( i );
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
		auto &ii	= operator[]( i );
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

bool SuperPolator::update( QList< QRect > &actions, QList< QPointF > &opaScale )
{
	auto	ms = started.msecsTo( QTime::currentTime() );
	auto	t  = qMin( ( qreal ) ms / durMs, 1. );
	__m256d sst;
	// auto	dbg = qDebug() << "update @ t=" << t << ":";
	for ( int cnt = count(), i = 0; i < cnt; ++i )
	{
		auto &ii = operator[]( i );
		// x(t) Super-Smoothstep mit den gespeicherten Parametern ...
		auto  x	 = qMax( 0., qMin( 1., ( t - ii.t0 ) / ( ii.t1 - ii.t0 ) ) );
		sst		 = _mm256_set1_pd( x * x * ( -2. * x + 3. ) );

		// Interpolation
		auto &cv = ii.aktuell();
		cv = _mm256_fmadd_pd( ii.ziel(), sst, _mm256_fnmadd_pd( ii.quelle(), sst, ii.quelle() ) );

		// 128Bit - Zuweisung für Opacity und Scale ...
		auto &os = reinterpret_cast< __m128d & >( opaScale[ i ] );
		os		 = _mm256_extractf128_pd( cv, 1 );
		// Box berechnen
		auto &a	 = actions[ i ];
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

void SuperPolator::debugInitialValues( const char *dsc ) const
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

inline QDebug operator<<( QDebug &d, SuperPolator &s )
{
	QDebugStateSaver ss( d );
	d << "PieData: r0 =" << s.r() << "Duration" << s.durMs << "ms, started" << s.started;
	for ( int i( 0 ), ic( s.count() ); i < ic; ++i )
		d << "\n\t" << qSetFieldWidth( 2 ) << i << "anim:" << s[ i ].quelle() << "->"
		  << s[ i ].ziel() << "now:" << s[ i ].aktuell();
	return d;
}
