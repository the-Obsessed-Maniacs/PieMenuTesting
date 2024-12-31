#include "qpiemenu.h"

#include <QEvent>
#include <QPaintEvent>
#include <QStyleOptionMenuItem>
#include <QStylePainter>

QPieMenu::QPieMenu( QWidget *parent )
	: QMenu( parent )
{}

QPieMenu::QPieMenu( const QString &title, QWidget *parent )
	: QMenu( title, parent )
{}

void QPieMenu::setSpread( qreal newSpread )
{
	// step #1: check if it's really a change
	if ( qAbs( newSpread - getSpread() ) < 0.0001 ) return;

	auto dbg = qDebug() << "setSpread(" << newSpread << "):";
	// step #2: update all offsets, that are still moving and
	int	 still_moving( 0 );
	for ( auto &i : _offsets )
		if ( std::get< 2 >( i ) )
		{
			auto w = std::get< 3 >( i );
			auto r = w->geometry();
			r.moveTopLeft( pos() - 0.5 * QPoint( r.width(), r.height() )
						   + ( ( std::get< 1 >( i ) = newSpread )
							   * QPointF( -qCos( qDegreesToRadians( std::get< 0 >( i ) ) ),
										  qSin( qDegreesToRadians( std::get< 0 >( i ) ) ) ) )
								 .toPoint() );
			w->setGeometry( r );
			++still_moving;
		}
	dbg << "still moving before check:" << still_moving;
	// step #3: check for stopping conditions ...
	for ( int i( 0 ); i < _offsets.count(); ++i )
	{
		auto  mrg = QMargins( 10, 10, 10, 10 );
		auto &a	  = std::get< 0 >( _offsets[ i ] );
		auto &s	  = std::get< 1 >( _offsets[ i ] );
		auto &m	  = std::get< 2 >( _offsets[ i ] );
		auto  r	  = std::get< 3 >( _offsets[ i ] )->geometry().marginsAdded( mrg );
		if ( m )
		{ // check the moving objects
			int	 i0 = ( i == 0 ? i + 2 : i - 1 );
			int	 i1 = ( i == _offsets.count() - 1 ? i - 2 : i + 1 );
			bool is0 =
				std::get< 3 >( _offsets[ i0 ] )->geometry().marginsAdded( mrg ).intersects( r );
			bool is1 =
				std::get< 3 >( _offsets[ i1 ] )->geometry().marginsAdded( mrg ).intersects( r );
			switch ( is0 + is1 )
			{
				case 0: // no intersections left -> stop asap
					m = false;
					--still_moving;
					break;
				case 1: // only one intersection left: stop if the other one is still moving
						// if ( ( is0 && std::get< 2 >( _offsets[ i0 ] ) )
						//	 || ( is1 && std::get< 2 >( _offsets[ i1 ] ) ) )
						//	m = false, --still_moving;
				default: break;
			}
		}
	}
	dbg << "still moving after check:" << still_moving;
	if ( !( still_moving ) && _anim ) _anim->stop();
}

qreal QPieMenu::getSpread() const
{
	qreal res = 0.;
	if ( !_offsets.isEmpty() )
		for ( const auto &i : _offsets ) res = qMax( std::get< 1 >( i ), res );
	else if ( _anim ) res = _anim->currentValue().toReal();
	return res;
}

void QPieMenu::showEvent( QShowEvent *e )
{
	auto dbg = qDebug() << "menu show start: build position list and corresponding buttons";
	// this is the point where we should assume the menu won't change and we can compute
	calculateItems();
	if ( _anim == nullptr )
	{
		_anim = new QPropertyAnimation( this, "spread", this );
		_anim->setStartValue( 0. );
		_anim->setEndValue( 1. );
		_anim->setDuration( 500 );
	}
	QMenu::showEvent( e );
	_anim->setCurrentTime( 0 );
	_anim->setDirection( QAbstractAnimation::Forward );
	_anim->start();
	dbg << "menu show end." << e;
}

void QPieMenu::hideEvent( QHideEvent *e )
{
	_anim->setCurrentTime( _anim->duration() );
	_anim->setDirection( QAbstractAnimation::Backward );
	_anim->start();
}

void QPieMenu::calculateItems()
{
	auto dbg = qDebug() << "begin: QPieMenu::calculateItems(), # of Actions:" << actions().count();
	_itemRects.clear();
	_itemRects.resize( actions().count() + 1, {} );
	// wir berechnen hier actions_count + 1 Item-Rects, die unser Menu ausmachen sollen.
	// Das letzte Rect steht für den Menutitel + Close - Button.
	// Die Rects sind alle initialisiert auf "invalid".
	auto				 qfm   = fontMetrics();
	auto				 style = this->style();
	QStyleOptionMenuItem mopt;
	mopt.initFrom( this );
	auto	   previousWasSeparator = true; // this is true to allow removing the leading separators
	const bool contextMenu			= true;
	const auto fw					= style->pixelMetric( QStyle::PM_MenuPanelWidth, &mopt, this );
	const auto deskFw = style->pixelMetric( QStyle::PM_MenuDesktopFrameWidth, &mopt, this );
	const auto hmarg  = style->pixelMetric( QStyle::PM_MenuHMargin, &mopt, this );
	const auto vmarg  = style->pixelMetric( QStyle::PM_MenuVMargin, &mopt, this );
	QMargins   menuMargins{ vmarg, vmarg, hmarg, vmarg };
	const auto panelWidth = style->pixelMetric( QStyle::PM_MenuPanelWidth, &mopt, this );
	const auto icone	  = style->pixelMetric( QStyle::PM_SmallIconSize, &mopt, this );
	int		   i		  = 0;
	// Zuerst berechnen wir die Größen der Items
	for ( ; i < actions().count(); ++i )
	{
		// Each Action needs its size or is an invisible separator with a default size.
		auto	   action = actions().at( i );
		const bool isSection =
			action->isSeparator() && ( !action->text().isEmpty() || !action->icon().isNull() );
		const bool isPlainSeparator =
			( isSection && !style->styleHint( QStyle::SH_Menu_SupportsSections ) )
			|| ( action->isSeparator() && !isSection );

		if ( !action->isVisible() || ( previousWasSeparator && isPlainSeparator ) )
			continue; // we continue, this action will get an empty QRect

		previousWasSeparator = isPlainSeparator;

		// let the style modify the above size..
		QStyleOptionMenuItem opt;
		initStyleOption( &opt, action );
		const QFontMetrics &fm = opt.fontMetrics;
		QSize				sz;
		// calc what I think the size is..
		if ( action->isSeparator() )
		{
			sz = QSize( 2, 2 );
		} else {
			auto s		  = action->text();
			auto t		  = s.indexOf( u'\t' );
			auto tabWidth = 0;
			if ( t != -1 )
			{
				tabWidth = qMax( int( tabWidth ), qfm.horizontalAdvance( s.mid( t + 1 ) ) );
				s		 = s.left( t );
			} else if ( action->isShortcutVisibleInContextMenu() || !contextMenu ) {
				QKeySequence seq = action->shortcut();
				if ( !seq.isEmpty() )
					tabWidth =
						qMax( int( tabWidth ),
							  qfm.horizontalAdvance( seq.toString( QKeySequence::NativeText ) ) );
			}
			// TODO: TABWIDTH benutzen!
			sz.setWidth(
				fm.boundingRect( QRect(), Qt::TextSingleLine | Qt::TextShowMnemonic, s ).width()
				+ tabWidth );
			sz.setHeight( qMax( fm.height(), qfm.height() ) );

			QIcon is = action->icon();
			if ( !is.isNull() )
			{
				QSize is_sz = QSize( icone, icone );
				if ( is_sz.height() > sz.height() ) sz.setHeight( is_sz.height() );
			}
		}
		sz = style->sizeFromContents( QStyle::CT_MenuItem, &opt, sz, this );

		if ( !sz.isEmpty() )
		{
			_itemRects[ i ].setSize( sz );
			dbg << "\n\titem" << opt.text << "=" << sz;
		} else qWarning() << "no size for menuitem" << action;
	}
	// Am Ende nocht das "MenuItem" selbst: Titel, X-Button zum Abbrechen
	mopt.icon	   = QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit );
	mopt.state	   = QStyle::State_None;
	mopt.checkType = QStyleOptionMenuItem::NotCheckable;
	if ( mopt.text.isEmpty() )
		mopt.text =
			title().isEmpty() ? windowTitle().isEmpty() ? "context menu" : windowTitle() : title();
	_itemRects[ i ].setSize(
		style->sizeFromContents( QStyle::CT_MenuItem,
								 &mopt,
								 qfm.size( Qt::TextSingleLine | Qt::TextShowMnemonic, mopt.text ),
								 this ) );
	_itemRects[ i ].moveCenter( {} );
	dbg << "\n\tcenter" << mopt.text << "=" << _itemRects[ i ];
	// AT THIS POINT WE SHOULD HAVE ALL ITEM SIZES CALCULATED!
	// Now it's the time to invoke some algorithm for the placement of all items.
	_itemRA = _calculator( this, _itemRects );
}

QList< QPointF > QPieMenu::baseCalcPositions( QList< QRectF > &ir )
{
	/**************************************************************************************************
	 * Grundlegende Strategie zur Anordnung der Menuelemente:
	 * - Start bei ca. 1 Uhr / 166° (alles davor erhöht nur den Radius)
	 * - minimale halbe Manhattan-Länge der Item-Größen als Mindestradius
	 * - Item platzieren mit aktuellem Winkel und Radius.
	 *  -> Suche nach der richtigen Richtung: wir wollen in negative Winkelrichtung gehen, ergo
	 *können wir alle Anstiege umdrehen (schonmal eine gute Erkenntnis) # Kreisbögen! # wie wäre
	 *eine Weiterschreit-Funktion, die die länge der
	 *************************************************************************************************/
	QRectF			 masterBR( ir.last() );
	QList< QPointF > res( ir.count() );
	QList< qreal >	 hml( ir.count() );
	qreal			 w0i( 166. ), r0i( std::numeric_limits< qreal >::max() );
	for ( int i = 0; i < ir.count(); ++i )
	{ // get all half manhattan lengths, as we need the minimum and maybe later on don't need to
	  // recalculate them ...
		hml[ i ] = ( ir.at( i ).width() + ir.at( i ).height() ) * .5;
		r0i		 = qMin( r0i, hml[ i ] );
	}
	for ( int i = 0; i < ir.count() - 1; ++i )
	{
		auto sc	 = qSinCos( qDegreesToRadians( w0i ) );
		auto pos = r0i * sc;
		ir[ i ].moveCenter( pos );
		qDebug() << "unite" << masterBR << "|" << ir[ i ] << "="
				 << ( masterBR = masterBR.united( ir[ i ] ) );
		res[ i ] = { r0i, w0i };
		if ( i < ir.count() - 2 )
		{
			auto bogen1 = style()->pixelMetric( QStyle::PM_MenuBarItemSpacing )
						  + 0.5
								* QPointF::dotProduct( -sc.transposed(),
													   { ir[ i ].width(), ir[ i ].height() } );
			auto delta_w1 = 360. * bogen1 / ( 2 * M_PI * r0i );
			auto sc1	  = qSinCos( w0i - delta_w1 );
			auto bogen2	  = 0.5
						  * QPointF::dotProduct( -sc1.transposed(),
												 { ir[ i + 1 ].width(), ir[ i + 1 ].height() } );
			auto delta_w = 360. * bogen2 / ( 2 * M_PI * r0i ) + delta_w1;
			// todo: überlappungscheck ...
			w0i -= delta_w;
		}
	}
	// This "master bounding rect" needs to be translated, so that "pos()" falls into that point,
	// which is now the center of the last item.
	QRectF br{ masterBR.topLeft() + pos(), masterBR.size() };
	ir.last().moveCenter( -masterBR.topLeft() );
	setGeometry( br.toRect() );
	return res;
}

void QPieMenu::drawMenuBackground( QStylePainter &p ) {}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	if ( _itemRA.isEmpty() )	/**/
		QMenu::paintEvent( e ); // so we see what happens in a normal menu
	else						/**/
	{
		auto		  dbg	= qDebug() << "QPieMenu::paintEvent(" << e->region() << "):\n";
		auto		  style = this->style();
		QStylePainter p( this );
		p.drawRoundedRect( e->rect(), 5, 5 );
		QStyleOptionMenuItem opt;
		opt.initFrom( this );
		opt.state				  = QStyle::State_None;
		opt.checkType			  = QStyleOptionMenuItem::NotCheckable;
		opt.icon				  = QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit );
		opt.maxIconWidth		  = 0;
		opt.reservedShortcutWidth = 0;
		QMargins menuMargins{ style->pixelMetric( QStyle::PM_MenuHMargin, &opt, this ),
							  style->pixelMetric( QStyle::PM_MenuVMargin, &opt, this ),
							  style->pixelMetric( QStyle::PM_MenuHMargin, &opt, this ),
							  style->pixelMetric( QStyle::PM_MenuVMargin, &opt, this ) };
		opt.rect	= _itemRects.last().toRect().marginsAdded( menuMargins );
		auto center = opt.rect.center();
		p.drawPrimitive( QStyle::PE_PanelMenu, opt );
		opt.rect = opt.rect.marginsRemoved( menuMargins );
		p.drawControl( QStyle::CE_MenuItem, opt );
		dbg << "=> center drawn:" << opt << "with margins:" << menuMargins;

		for ( int i( 0 ); i < actions().count(); ++i )
		{
			QStyleOptionMenuItem opt;
			initStyleOption( &opt, actions().at( i ) );
			opt.rect = _itemRects[ i ].toRect().marginsAdded( menuMargins );
			opt.rect.translate( center );
			p.drawPrimitive( QStyle::PE_PanelMenu, opt );
			opt.rect = opt.rect.marginsRemoved( menuMargins );
			p.drawControl( QStyle::CE_MenuItem, opt );
			dbg << "\n=> item drawn:" << opt;
		}


		// const auto leftmargin	= panelWidth + hmargin + _leftmargin;
		// const auto topmargin	= panelWidth + vmargin + _topmargin;
		// const auto bottommargin = panelWidth + vmargin + _bottommargin;
		// const auto contentWidth = width() - ( fw + hmargin ) * 2 - _leftmargin - _rightmargin;


		// const int hmargin		   = style->pixelMetric( QStyle::PM_MenuHMargin, &opt, this ),
		//		  vmargin		   = style->pixelMetric( QStyle::PM_MenuVMargin, &opt, this ),
		//		  icone			   = style->pixelMetric( QStyle::PM_SmallIconSize, &opt, this );
		// const int fw			   = style->pixelMetric( QStyle::PM_MenuPanelWidth, &opt, this );
		// const int deskFw		   = style->pixelMetric( QStyle::PM_MenuDesktopFrameWidth, &opt,
		// this ); const int base_y		   = vmargin + fw + topmargin; const int column_max_y	   =
		// screen.height() - 2 * deskFw - ( vmargin + bottommargin + fw ); int
		// max_column_width = 0;
	}
}
