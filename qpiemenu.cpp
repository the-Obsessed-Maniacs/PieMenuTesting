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
	ih	   = qMax( icone, menu->fontMetrics().height() ) + 2 * hmarg;
	startO = qDegreesToRadians( 170 ); // vorerst legen wir hier Defaults fest!
	maxO   = M_PI + M_PI_2 + M_PI_4;   // vorerst legen wir hier Defaults fest!
	dir	   = -1.;
	startR = getStartR( menu );
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

QPieMenu::QPieMenu( const QString &title, QWidget *parent )
	: QMenu( title, parent )
	, _anim( new QPropertyAnimation( this, "showState", this ) )
{
	// Tearing this off would not be a good idea!
	setTearOffEnabled( false );
	qApp->setEffectEnabled( Qt::UI_AnimateMenu, false );
	setWindowFlag( Qt::FramelessWindowHint );
	setAttribute( Qt::WA_TranslucentBackground );
	// initialize style-dependent data
	readStyleData();
	// to be removed later on:
	_anim->setStartValue( 0. );
	_anim->setEndValue( 1. );
	_anim->setDuration( 500 );
	connect( _anim,
			 &QAbstractAnimation::finished,
			 []() { qApp->setEffectEnabled( Qt::UI_FadeMenu, true ); } );

	qDebug() << "QPieMenu" << title << "created" << this;
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
	// Ein Drop-Shadow um das Menu herum sieht nicht brauchbar aus, deshalb
	// entfernen wir zumindest unter Windows den DropShadow tief in der WinAPI
	switch ( e->type() )
	{
		case QEvent::StyleChange: readStyleData(); return true;
#if _WIN32
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
	}
	qDebug() << "QPieMenu::event(" << e << ") passed down...";
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
}

void QPieMenu::paintEvent( QPaintEvent *e )
{
	auto		  sv	= _anim->startValue().toReal();
	auto		  ev	= _anim->endValue().toReal();
	auto		  style = this->style();
	QStylePainter p( this );
	p.translate( -_actionPieData._boundingRect.topLeft() );
	QStyleOptionMenuItem opt;
	auto				 ac = actions().count();
	qreal				 t	= 1.;
	for ( int i( 0 ); i < ac; ++i )
	{
		initStyleOption( &opt, actions().at( i ) );
		opt.rect = _actionRects[ i ].marginsAdded( _styleData.menuMargins );
		p.setOpacity( currentAlpha( i ) );
		p.drawPrimitive( QStyle::PE_PanelMenu, opt );
		opt.rect = opt.rect.marginsRemoved( _styleData.menuMargins );
		p.drawControl( QStyle::CE_MenuItem, opt );
	}
}

void QPieMenu::showEvent( QShowEvent *e )
{
	qDebug() << "QPieMenu::showEvent(" << e << ")";
	// An dieser Stelle sollten wir die Position zentrieren ...
	updateActionRects();
	setGeometry( geometry().translated( _actionPieData._boundingRect.topLeft() ) );
	qApp->setEffectEnabled( Qt::UI_FadeMenu, false );
	_anim->setCurrentTime( 0 );
	_anim->setDirection( QAbstractAnimation::Forward );
	_anim->start();
	QMenu::showEvent( e );
}

void QPieMenu::mouseMoveEvent( QMouseEvent *e )
{
	if ( !isVisible() ) return;

	// Find action at position:
	auto p	= e->position().toPoint() + _actionPieData._boundingRect.topLeft();
	auto ai = actionIndexAt( p );
	qDebug() << "QPieMenu::mouseMoveEvent(" << p << "):"
			 << ( ai >= 0 ? tr( "Treffer = '%1'" ).arg( actions().at( ai )->text() )
						  : tr( "Daneben" ) )
					.toUtf8();
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
		_actionPieData[ i ] = sz;
		if ( sz.isValid() ) allSz += sz;
		else ++noSzItems;
	}
	// Speichere die Durchschnittsgröße der Items mit!
	_actionPieData._avgSz = allSz / ( actions().count() - noSzItems );
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
	// Wir justieren zur Sicherheit mal jeweils um die halbe Durchschnittsgröße.
	auto xa = _actionPieData._avgSz.width() >> 1;
	auto ya = _actionPieData._avgSz.height() >> 1;
	_actionPieData._boundingRect.adjust( -xa, -ya, xa, ya );
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

void QPieMenu::updateActionRects()
{
	if ( !_actionRectsDirty ) return;
	// Die Action Rects werden auf unterschiedliche Art und Weise dirty.
	// Wenn wir auch während der Animationen die Action Rects aktuell halten wollen, so muss das
	// hier geschehen.  Diese Funktion aktualisiert die dreckigen Action Rects je nach aktueller
	// Darstellungssituation.

	// Die direkte Schlussfolgerung ist: die Action Rects können direkt zum Rendern verwendet
	// werden.
	// Hier ist ein Arbeitsplatz des State Managers!
	// TODO!
}
