#include "qpiemenu.h"

#include <QApplication>
#include <QEvent>
#include <QMetaProperty>
#include <QPaintEvent>
#include <QStyleOptionMenuItem>
#include <QStylePainter>
#include <QWidgetAction>
#if _WIN32
#	pragma comment( lib, "dwmapi.lib" )
#	include "dwmapi.h"
#endif

QPieMenuSettings::QPieMenuSettings( QPieMenu *menu )
{
	auto				 style = menu->style();
	QStyleOptionMenuItem mopt;
	mopt.initFrom( menu );
	panelWidth = fw = style->pixelMetric( QStyle::PM_MenuPanelWidth, &mopt, menu );
	deskFw			= style->pixelMetric( QStyle::PM_MenuDesktopFrameWidth, &mopt, menu );
	hmarg			= style->pixelMetric( QStyle::PM_MenuHMargin, &mopt, menu );
	vmarg			= style->pixelMetric( QStyle::PM_MenuVMargin, &mopt, menu );
	menuMargins		= QMargins{ vmarg, vmarg, hmarg, vmarg };
	icone			= style->pixelMetric( QStyle::PM_SmallIconSize, &mopt, menu );
	sp				= qMin( style->pixelMetric( QStyle::PM_LayoutHorizontalSpacing, &mopt, menu ),
							qMax( 3, style->pixelMetric( QStyle::PM_ToolBarItemSpacing, &mopt, menu ) ) );
	ih				= qMax( icone, menu->fontMetrics().height() ) + 2 * hmarg;
	startO			= qDegreesToRadians( 170 ); // vorerst legen wir hier Defaults fest!
	maxO			= M_PI + M_PI_2 + M_PI_4;	// vorerst legen wir hier Defaults fest!
	dir				= -1.;
	startR			= getStartR( menu );
}

qreal QPieMenuSettings::getStartR( QPieMenu *menu ) const
{
	return qMax( qreal( ih ), 0.2833 * qreal( ih + sp ) * menu->actions().count() );
}

qreal QPieMenuSettings::nextR( QPieMenu *menu ) const
{
	auto r	= startR;
	auto sr = getStartR( menu );
	// Möglicherweise eine Änderung der Item-Anzahl?
	if ( r < sr ) r = sr;
	else r *= 1.15;
	return r;
}

bool PieRects::add( R_type r )
{
	_boundingRect |= r;
	return Intersector::add( r );
}

void PieRects::addAnyhow( R_type r )
{
	_boundingRect |= r;
	return Intersector::addAnyhow( r );
}

bool PieRects::changeItem( int index, R_type newValue )
{
	_boundingRect |= newValue; // Der einfache Weg: Tatsächlich könnte dieses Rect Teil einer Grenze
							   // des BR gewesen sein, das nach dem Verkleinern dieses Rects
							   // tatsächlich schrumpfen könnte.  Macht aber erstmal nichts.
	return Intersector::changeItem( index, newValue );
}

void PieRects::reset( int reserved_size )
{
	clear();
	reserve( reserved_size );
	resetIntersection();
	_boundingRect = QRect();
}

QPieMenu::QPieMenu( const QString &title, QWidget *parent )
	: QMenu( title, parent )
	, _settings( this )
	, _anim( new QPropertyAnimation( this, "showState", this ) )
{
	setTearOffEnabled( false );
	qApp->setEffectEnabled( Qt::UI_AnimateMenu, false );
	setWindowFlag( Qt::FramelessWindowHint );
	setAttribute( Qt::WA_TranslucentBackground );
	// setAttribute( Qt::WA_NoSystemBackground );
	// setAttribute( Qt::WA_PaintOnScreen );
	// setAutoFillBackground( false );
	_anim->setStartValue( 0. );
	_anim->setEndValue( 1. );
	_anim->setDuration( 500 );
	connect( _anim,
			 &QAbstractAnimation::finished,
			 []() { qApp->setEffectEnabled( Qt::UI_FadeMenu, true ); } );
	qDebug() << "QPieMenu created" << this;
}

QPieMenu::~QPieMenu()
{
	qDebug() << "QPieMenu destroying" << this;
}

void QPieMenu::setShowState( qreal value )
{
	if ( epsilonNE( _showState, value ) )
	{
		emit showStateChanged( _showState = value );
		update();
	}
}

QSize QPieMenu::sizeHint() const
{
	return _actionRects._boundingRect.size();
}

void QPieMenu::actionEvent( QActionEvent *event )
{
	// Whatever QMenu does, we do it, too.
	QMenu::actionEvent( event );
	// Wenn eine Action hinzugefügt wird, ändert sich die Anordnung des Menüs.
	// Die Frage ist jetzt, machen wir nur eine Restberechnung oder immer und immer wieder eine
	// Ganzberechnung?
	//
	// Wir können ja mal der Logik folgen: Menus werden i.d.R. sequenziell zusammengebaut.  Es ist
	// eher selten, dass Actions mittendrin eingefügt werden.  Eine solch einfache Unterscheidung
	// macht glaub ich viel Sinn.
	const auto	t  = event->type();
	const auto &al = actions();
	qDebug() << "QPieMenu::actionEvent(" << event << "):" << event->action()
			 << ( t == QEvent::ActionAdded	   ? tr( "hinzugefügt" ).toUtf8()
				  : t == QEvent::ActionRemoved ? tr( "entfernt" ).toUtf8()
											   : tr( "geändert" ).toUtf8() );

	if ( al.last() == event->action() && t == QEvent::ActionAdded )
	{
		// Die einfache Variante: die letzte Box berechnen ...
		auto lb = getNextRect( al.count() - 1 );
		// gabs es ein Ergebnis?  Gibt es eine Überlappung?
		if ( lb.isNull() || !_actionRects.add( lb ) )
			++_settings.round,
				updateActionRects(); // kein Ergebnis oder überlappung?  Dann alles neu berechnen.
		else
			updateGeometry(),
				qDebug() << "new box:" << lb << "changed bb:" << _actionRects._boundingRect;
	} else
		// Die komplizierte Variante: Wir müssen alle Boxen neu berechnen.
		updateActionRects();
}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	auto		  sv	= _anim->startValue().toReal();
	auto		  ev	= _anim->endValue().toReal();
	// auto dbg = qDebug() << "QPieMenu::paintEvent(" << e << "): t=" << t;
	auto		  style = this->style();
	QStylePainter p( this );
	p.translate( -_actionRects._boundingRect.topLeft() );
	QStyleOptionMenuItem opt;
	auto				 ac = actions().count();
	for ( int i( 0 ); i < ac; ++i )
	{
		auto t_i = superSmoothStep( _showState, sv + 0.3, ev - 0.3, i, ac );
		initStyleOption( &opt, actions().at( i ) );
		opt.rect = _actionRects[ i ].marginsAdded( _settings.menuMargins );
		opt.rect.moveCenter(
			( qSinCos( _settings.startO + t_i * ( _actionRAs[ i ].y() - _settings.startO ) )
			  * qSin( t_i * M_PI_2 ) * _actionRAs[ i ].x() )
				.toPoint() );
		p.setOpacity( t_i );
		p.drawPrimitive( QStyle::PE_PanelMenu, opt );
		opt.rect = opt.rect.marginsRemoved( _settings.menuMargins );
		p.drawControl( QStyle::CE_MenuItem, opt );
		// dbg << "\n=> item drawn:" << opt << _actionRects[ i ];
	}
}

void QPieMenu::showEvent( QShowEvent *e )
{
	qDebug() << "QPieMenu::showEvent(" << e << ")";
#if _WIN32
	// grab the native window handle and remove the CS_DROPSHADOW
	HWND  hwnd		  = reinterpret_cast< HWND >( winId() );
	DWORD class_style = ::GetClassLong( hwnd, GCL_STYLE );
	class_style &= ~CS_DROPSHADOW;
	::SetClassLong( hwnd, GCL_STYLE, class_style );
#endif
	// An dieser Stelle sollten wir die Position zentrieren ...
	setGeometry( geometry().translated( _actionRects._boundingRect.topLeft() ) );
	qApp->setEffectEnabled( Qt::UI_FadeMenu, false );
	_anim->setCurrentTime( 0 );
	_anim->setDirection( QAbstractAnimation::Forward );
	_anim->start();
	QMenu::showEvent( e );
}

void QPieMenu::updateActionRects()
{
	auto na	 = actions().count();
	auto dbg = qDebug() << "begin: QPieMenu::updateActionRects(), # of Actions:" << na
						<< ", round:" << _settings.round << ", radius:"
						<< ( _settings.round ? _settings.nextR( this ) : _settings.startR );
	const auto &hi = _actionRects._hasIntersection;
	int			i;
	auto		inc = [ & ]( bool isStart = false )
	{
		if ( isStart || _actionRects[ i ].isNull() || hi )
		{
			if ( !isStart )
			{
				_settings.round++;
				dbg << "... need more space" << ( hi ? "(overlap:" : "(no solution" );
				if ( hi ) dbg << _actionRects._lastIntersection;
				dbg << ") -> next radius:" << _settings.nextR( this ) << "";
			}
			i = 0;
			_actionRects.reset( na );
			_actionRAs.clear();
		} else ++i, dbg << "-> OK.";
	};
	for ( inc( true ); i < na; inc() )
	{
		_actionRects.addAnyhow( getNextRect( i ) );
		dbg << "\n\titem" << actions()[ i ]->text() << "=" << _actionRects[ i ];
	}
	updateGeometry();
}

QRect QPieMenu::getNextRect( int actionIndex )
{
	if ( actionIndex == 0 )
	{ // Die erste Aktion bekommt ihren Platz ohne große Berechnung und diehnt dem beschaffen der
	  // Style-Daten.  Hier - wir wollen ja die Berechnung auseinander reissen - besorgen wir die
	  // aktuellen style-daten.
		if ( _settings.round > 0 )
		{ // Der Radius muss vergrößert werden.
			_settings.startR = _settings.nextR( this );
			_actionRects.resetIntersection();
			_actionRAs.clear();
		} else _settings = QPieMenuSettings( this );
		QRect ar = { {}, getActionSize( actions().at( actionIndex ) ) };
		ar.moveCenter( ( _settings.startR * qSinCos( _settings.startO ) ).toPoint() );
		_actionRAs.append( { _settings.startR, _settings.startO } );
		return ar;
	} else { // Die weiteren Aktionen bekommen ihren Platz durch die Berechnung der Positionen.
		BestDelta bd;
		auto	  last = _actionRAs.last();
		auto	  sz   = fromSize( getActionSize( actions().at( actionIndex ) ) );
		auto	  c	   = _actionRects[ actionIndex - 1 ].center();
		auto offs = 2 * _settings.SP() + 0.5 * ( sz + fromSize( _actionRects[ actionIndex - 1 ] ) );
		bd.init( _settings.dir, last.y() );
		qreal n;
		for ( auto v : { c + offs, c - offs } )
		{
			if ( abs( v.x() ) <= last.x() )
				n = qAsin( v.x() / last.x() ), bd.addRad( n ), bd.addRad( M_PI - n );
			if ( abs( v.y() ) <= last.x() )
				n = qAcos( v.y() / last.x() ), bd.addRad( n ), bd.addRad( -n );
		}
		// Jetzt wählen wir einen Winkel aus ...
		n		= bd.best();
		auto np = qAbs( n );
		// Kriterien: Ungleich 0, Summe kleiner maximalsumme, keine Überlappung
		// hier wird die Überlappung nicht geprüft, da wir den Algorithmus zerlegen!
		if ( np > std::numeric_limits< qreal >::epsilon()
			 && ( qAbs( radiansDistance( _settings.startO, last.y() ) ) + np < _settings.maxO ) )
		{
			_actionRAs.append( { last.x(), last.y() + n } );
			return QRectF{ ( last.x() * qSinCos( last.y() + n ) - 0.5 * sz ), fromPoint( sz ) }
				.toRect();
		} else {
			_settings.round++;
			return QRect();
		}
	}
}

QSize QPieMenu::getActionSize( QAction *action )
{
	const bool isSection =
		action->isSeparator() && ( !action->text().isEmpty() || !action->icon().isNull() );
	const bool isLücke = ( isSection && !style()->styleHint( QStyle::SH_Menu_SupportsSections ) )
						 || ( action->isSeparator() && !isSection ) || !action->isVisible();
	auto				 sz = QSize( 2, 2 );

	// let the style modify the above size..
	QStyleOptionMenuItem opt;
	initStyleOption( &opt, action );
	const QFontMetrics &fm = opt.fontMetrics;
	auto				w  = qobject_cast< QWidgetAction * >( action );
	auto				ww = w ? w->defaultWidget() : nullptr;
	if ( ww ) // Widget-Action -> hat ihre eigene Größe!
		sz = ww->sizeHint()
				 .expandedTo( ww->minimumSize() )
				 .expandedTo( ww->minimumSizeHint() )
				 .boundedTo( ww->maximumSize() );
	else if ( !isLücke )
	{
		// Keine Widget-Action, keine Lücke: also Text und Icon
		auto s = action->text();
		auto t = s.indexOf( u'\t' );
		if ( t != -1 )
		{
			_settings.tabWid =
				qMax( _settings.tabWid, fontMetrics().horizontalAdvance( s.mid( t + 1 ) ) );
			s = s.left( t );
		} else if ( action->isShortcutVisibleInContextMenu() || !_settings.contextMenu ) {
			QKeySequence seq = action->shortcut();
			if ( !seq.isEmpty() )
				_settings.tabWid =
					qMax( _settings.tabWid,
						  fm.horizontalAdvance( seq.toString( QKeySequence::NativeText ) ) );
		}
		sz		 = fm.boundingRect( QRect(), Qt::TextSingleLine | Qt::TextShowMnemonic, s ).size();
		QIcon is = action->icon();
		if ( !is.isNull() )
		{
			QSize is_sz = is.actualSize( QSize( _settings.icone, _settings.icone ) );
			if ( is_sz.height() > sz.height() ) sz.setHeight( is_sz.height() );
		}
		sz = style()->sizeFromContents( QStyle::CT_MenuItem, &opt, sz, this );
	}
	sz.rwidth() += _settings.tabWid;
	_settings.previousWasSeparator = isLücke;
	if ( sz.height() > _settings.ih )
	{
		_settings.ih	 = qMax( _settings.ih, sz.height() );
		_settings.startR = qMax( _settings.startR, _settings.getStartR( this ) );
	}
	return sz;
}
