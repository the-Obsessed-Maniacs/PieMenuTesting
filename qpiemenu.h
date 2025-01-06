#ifndef QPIEMENU_H
#define QPIEMENU_H

#include "Helpers.h"

#include <QMenu>
#include <QPropertyAnimation>
#include <QTime>

class QStylePainter;
class QPieMenu;

// Erste Helferstruktur: zum Berechnen der "Standardpositionen" der Items und für die
// Größenberechnung -> wird gleich mit der 2. Iteration obsolet.
struct QPieMenuSettings
{
	bool	 previousWasSeparator{ true }; // this is true to allow removing the leading separators
	bool	 contextMenu{ true };
	qint32	 fw, deskFw, hmarg, vmarg, panelWidth, icone, sp, ih, round{ 0 }, tabWid{ 0 };
	QMargins menuMargins;
	qreal	 startR, startO, dir, maxO;

	QPieMenuSettings( QPieMenu *menu );
	qreal	getStartR( QPieMenu *menu ) const;
	qreal	nextR( QPieMenu *menu ) const;
	QPointF SP() const { return QPointF{ qreal( sp ), qreal( sp ) }; }
};

struct PieStyleData
{
	qint32	 fw, deskFw, hmarg, vmarg, panelWidth, icone, sp{ 0 };
	QMargins menuMargins;
};

struct PieInitData
{
	QPoint _execPoint;
	qreal  _start0{ qDegreesToRadians( 175 ) }, _max0{ qDegreesToRadians( 285 ) };
	bool   _negativeDirection{ true }, _isContext{ true }, _isSubMenu{ false };

	void   init( QPoint menuExecPoint, bool isContextMenu = true )
	{
		_execPoint = menuExecPoint, _isContext = isContextMenu;
	}
};

struct alignas( 32 ) PieDataItem
{
	QRect		   _geo;
	qreal		   _angle;
	bool		   _s{ false }, _a{ false }, _p{ false }, _ok{ false };
	constexpr	   operator bool() { return ( _ok = _s && _a && _p ); }
	constexpr	   operator QRect() const { return _geo; }
	constexpr	   operator QSize() const { return _geo.size(); }
	constexpr	   operator qreal() const { return _angle; }
	constexpr auto operator=( const QPoint &p ) { _geo.moveCenter( p ), _p = true; }
	constexpr auto operator=( const QSize &sz ) { _geo.setSize( sz ), _s = true; }
	constexpr auto operator=( const qreal angle ) { _angle, _a = angle, true; }
};

struct PieData : QList< PieDataItem >
{
	QList< QRect > _intersections;
	QSize		   _avgSz;
	QRect		   _boundingRect;
	qreal		   _radius;

	// Extra-Funktionen:
	// -> automatisierte Wandlung zu Action Rects via Operator
	operator QList< QRect >() const
	{ // durch den "QRect"-Operator der Items müsste dies gehen ...
		QList< QRect > r{ cbegin(), cend() };
		return r;
	}
	// -> setzen eines bestimmten Wertes (liess sich schnell implementieren)
	auto setSize( int index, const QSize &sz ) { operator[]( index ) = sz; }
	auto setAngle( int index, const qreal o ) { operator[]( index ) = o; }
	void resetCalculations() { _intersections.clear(), _boundingRect = {}; }
	// -> Wir brauchen - der Reihe nach:
	// - Hinzufügen durch Setzen der Größe
	int	 append( const QSize &sz )
	{
		auto id = size();
		resize( id + 1 );
		operator[]( id ) = sz;
		return id;
	}
	// - Bounding-Rect Bildung und Überlappungsprüfung
	//  -> gibt true zurück, wenn es keine Überlappung gab.  Das Bounding Rect wurde dann auch
	//  angepasst.
	//  -> gibt false zurück, wenn es eine Überlappung gab.  Nur bei "override" wird dann auch das
	//  Bounding Rect angepasst.
	bool setCenter( int index, const QPoint &p, bool override = false )
	{
		operator[]( index ) = p; // erstmal Center setzen
								 // Überlappungstest: nur die beiden Nachbarn bzw. ItemNo_0
		const auto &it		= at( index );
		// Überlappungen zu den Nachbarn bestimmen
		bool		hasI;
		auto		ipp	 = index + 1 == count() ? 0 : index + 1;
		auto		is_p = it._geo.intersected( at( ipp ) );
#if not defined ONLY_SEQUENTIAL_SET_CENTER
		auto imm  = index == 0 ? count() - 1 : index - 1;
		auto is_m = it._geo.intersected( at( imm ) );
		if ( hasI = ( is_p.isValid() || is_m.isValid() ) )
		{
			if ( is_p.isValid() ) _intersections.append( is_p );
			if ( is_m.isValid() ) _intersections.append( is_m );
		}
#else
		if ( hasI = is_p.isValid() ) _intersections.append( is_p );
#endif
		if ( !hasI || override )
			if ( it.operator QRect().isValid() ) _boundingRect |= it;
			else _boundingRect |= { p, p };
		return !hasI;
	}
};


class QPieMenu : public QMenu
{
	Q_OBJECT
	// The showState is used to animate the menu - states:
	// [0]: menu is fully invisible
	// [0..1[: menu is fading in
	// [1]: menu is fully visible
	// ]1..0]: menu is fading out
	// ]1..2[: some secondary animation is running to reach state 2
	// [2]: not yet defined - maybe we will use this to show submenus?
	Q_PROPERTY( qreal showState READ getShowState WRITE setShowState NOTIFY showStateChanged )

  public:
	QPieMenu( QWidget *parent = nullptr )
		: QPieMenu( QString(), parent )
	{}
	QPieMenu( const QString &title, QWidget *parent = nullptr );
	~QPieMenu() override;
	// Properties
	qreal getShowState() const { return _showState; }
	void  setShowState( qreal value );
	// Overridden methods
	QSize sizeHint() const override;

	int	  actionIndexAt( const QPoint &pt ) const;
	int	  actionIndex( QAction *a ) const { return actions().indexOf( a ); }

  signals:
	void showStateChanged( qreal );

  protected:
	// Events we need to override:
	// Master-Event-Handler ("für alles Andere")
	bool event( QEvent *e ) override;
	// The action-event fires after actions were added or removed.  This is our point of
	// recalculation!
	void actionEvent( QActionEvent *event ) override;
	// -> the paintEvent should be called every time the positions were updated.
	//    It shall use QStylePainter-Stuff to draw all the items at the newly
	//    updated positions.
	void paintEvent( QPaintEvent *e ) override;
	// -> the showEvent starts the show animation, which is used to
	//    make the menu readable.
	void showEvent( QShowEvent *e ) override;
	// -> the mouse move event is used to select/highlight the active action
	void mouseMoveEvent( QMouseEvent *e ) override;

	// -> the wheel event could be used to rotate the items around.
	//    It's not a must-have, it's a like to have...
	void wheelEvent( QWheelEvent *e ) override { /*QMenu::wheelEvent( e );*/ }

	void changeEvent( QEvent *e ) override { /*QMenu::changeEvent( e );*/ }
	void enterEvent( QEnterEvent *e ) override { /*QMenu::enterEvent( e );*/ }
	void leaveEvent( QEvent *e ) override { /*QMenu::leaveEvent( e );*/ }
	void hideEvent( QHideEvent *e ) override { /*QMenu::hideEvent( e );*/ }
	void keyPressEvent( QKeyEvent *e ) override { /*QMenu::keyPressEvent( e );*/ }
	void mousePressEvent( QMouseEvent *e ) override { /*QMenu::mousePressEvent( e );*/ }
	void mouseReleaseEvent( QMouseEvent *e ) override { /*QMenu::mouseReleaseEvent( e );*/ }
	void timerEvent( QTimerEvent *e ) override { /*QMenu::timerEvent( e );*/ }

  private:
	//
	// Redesign der privaten Menu-Anteile:
	// -> Typen:
	using RadiusAngles = QList< QPointF >;
	// -> Variablen:
	qreal				_showState{ 0 };

	RadiusAngles		_actionRAs;

	QPropertyAnimation *_anim{ nullptr };

	PieInitData			_initData;
	PieStyleData		_styleData;
	// _actionPieData:  Hier speichern wir dauerhaft die Größen der Items.  Sie werden (erstmal)
	// wirklich nur beim Ändern der Action-Liste neu berechnet. Weiterhin speichern wir den Radius
	// und die Winkel für die "Ruheposition".
	PieData				_actionPieData;
	QList< QRect >		_actionRects;
	HWND				_dropShadowRemoved{ nullptr };
	// Sobald irgend etwas die aktuellen "_actionRects" invalidiert, wird dies gesetzt!
	bool				_actionRectsDirty{ true };

	// -> Methoden:
	// Style-Daten beschaffen (bei init und bei StyleChange)
	void				readStyleData();
	// Die Größen der Boxen berechnen -> Grunddaten
	void				calculatePieDataSizes();
	// Die Ruhepositionen berechnen
	void				calculateStillData();
	// Kleiner Helfer, den ich ggf. an mehreren Stellen brauche
	qreal				startR( int runde ) const;

	// Statusverwaltung!
	// Wir wollen mehrere Animationen haben, ergo gibt es Stati und Übergänge.
	//  Übergang: hidden -> still
	//  -   Menu Fade-In -> alle Elemente werden mit ssst sanft vom Zentrum her hinein
	//      geflogen und geblendet.
	//  -   Mouse Hover -> das Element unter der Mouse wird derart hervorgehoben, dass
	//      es 10% vergrößert dargestellt wird, dazu bildet sich ein gerundetes, transparentes
	//      Auswahlrechteck.
	// Weiterer kleiner Helfer
	qreal				currentAlpha( int actionIndex ) const { return 1.; }

	void				updateActionRects();
	QRect				getNextRect( int actionIndex );
	QSize				getActionSize( QAction *action );
	// -> Method hiding:
	// Tearing our menus off is not allowed -> make setting it private
	using QMenu::setTearOffEnabled;

  private slots:

	// Implementation von Hover- bzw. selection-State:
	// ===============================================
};

#endif // QPIEMENU_H
