/**************************************************************************************************
 * QPieMenu - eine Implementation radialer Kontextmenüs in Qt
 * ==========================================================
 * Author: Stefan <St0fF / the0bsessedManiacs> Kaps
 * Entwicklungsbeginn: 31.12.2024
 *************************************************************************************************/
#pragma once

#include "Helpers.h"
#include "Superpolator.h"

#include <QMenu>
#include <QTime>

class QStylePainter;
class QPieMenu;

#pragma region( konfigurierbare_Daten )
struct PieStyleData
{
	qint32	 fw, deskFw, hmarg, vmarg, panelWidth, icone, sp{ 0 };
	QMargins menuMargins;
	QColor	 HL, HLtransparent;
};

struct PieInitData
{
	QPoint	_execPoint;
	qreal	_start0{ qDegreesToRadians( 175 ) }, _max0{ qDegreesToRadians( 285 ) };
	qreal	_selRectAlpha{ 0.5 };
	quint32 _animBaseDur{ 250 }, _subMenuDelayMS{ 500 };
	bool	_negativeDirection{ true }, _isContext{ true }, _isSubMenu{ false };

	void	init( QPoint menuExecPoint, bool isContextMenu = true )
	{
		_execPoint = menuExecPoint, _isContext = isContextMenu;
	}
};
#pragma endregion
#pragma region( Animationshelfer_SelectionRect )
struct PieSelectionRect
{
	QRectF					 first;
	QColor					 second;
	inline PieSelectionRect &operator()( const qreal f, const PieSelectionRect &a,
										 const PieSelectionRect &b );
};

inline QDebug operator<<( QDebug dbg, const PieSelectionRect &r )
{
	QDebugStateSaver s( dbg );
	dbg << Qt::fixed << qSetRealNumberPrecision( 2 );
	dbg.nospace() << "PSR( " << r.first << ", " << r.second << " )";
	return dbg;
}

// Helferklasse für Animationen für schöneren Zugriff auf den Distance-Arc.  Ich nutze den Fakt
// aus, dass QRectF intern aus 4 qreal-Werten besteht - so lerpe ich ja zwischen 2 QRectFs hin-
// und her ;) - und definiere einfach neue Zugriffsfunktionen.
class DistArc
{
  public:
	// Immer direkt auf einer Referenz arbeiten.  Das Objekt soll nur lokale Lebensdauer haben!
	explicit DistArc( QRectF &rect )
		: r( rect )
	{}
	DistArc()				   = delete;
	DistArc( const DistArc & ) = delete;
	qreal &a() { return ( *this )( 2 ); }
	qreal &d() { return ( *this )( 3 ); }
	qreal &r0() { return ( *this )( 0 ); }
	qreal &r1() { return ( *this )( 1 ); }
	qreal  a() const { return ( *this )( 2 ); }
	qreal  d() const { return ( *this )( 3 ); }
	qreal  r0() const { return ( *this )( 0 ); }
	qreal  r1() const { return ( *this )( 1 ); }
	qreal  a0() const { return a() - d(); }
	qreal  a1() const { return a() + d(); }

  private:
	qreal  &operator()( int id ) { return reinterpret_cast< qreal * >( &r )[ id ]; }
	qreal  &operator()( int id ) const { return reinterpret_cast< qreal * >( &r )[ id ]; }
	QRectF &r;
};
#pragma endregion

class QPieMenu : public QMenu
{
	Q_OBJECT
#pragma region( public_overrides_of_QMenu )
  public:
	QPieMenu( QWidget *parent = nullptr )
		: QPieMenu( QString(), parent )
	{}
	QPieMenu( const QString &title, QWidget *parent = nullptr );
	~QPieMenu() override;
	// Overridden methods
	QSize	 sizeHint() const override;

	QAction *actionAt( const QPoint &pt ) const;
	int		 actionIndexAt( const QPoint &pt ) const;
	int		 actionIndex( QAction *a ) const { return actions().indexOf( a ); }

  signals:
#pragma endregion
#pragma region( protected_overrides_of_QMenu )
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
	// -> Animationen weiterschreiben, verzögertes Öffnen von Submenüs
	void timerEvent( QTimerEvent *e ) override;
	// -> ...
	void mousePressEvent( QMouseEvent *e ) override;
	void mouseReleaseEvent( QMouseEvent *e ) override;

	// -> the wheel event could be used to rotate the items around.
	//    It's not a must-have, it's a like to have...
	void wheelEvent( QWheelEvent *e ) override { /*QMenu::wheelEvent( e );*/ }

	void changeEvent( QEvent *e ) override { /*QMenu::changeEvent( e );*/ }
	void enterEvent( QEnterEvent *e ) override { /*QMenu::enterEvent( e );*/ }
	void leaveEvent( QEvent *e ) override { /*QMenu::leaveEvent( e );*/ }
	void hideEvent( QHideEvent *e ) override { /*QMenu::hideEvent( e );*/ }
	void keyPressEvent( QKeyEvent *e ) override { /*QMenu::keyPressEvent( e );*/ }
#pragma endregion
#pragma region( private_redesign )
  private:
	//
	// Redesign der privaten Menu-Anteile:
	// -> Typen:
	//      Implementation von Hover- bzw. selection-State:
	//      ===============================================
	enum class PieMenuStatus { hidden = 0, still, closeby, hover, item_active };
	enum class PieMenuEreignis {
		// Ereignisse, der Reihe nach, wie sie auftreten dürften
		show_up,
		hide_away,
		closeBy_start,
		closeBy_switch,
		closeBy_leave,
		hover_start,
		hover_switch,
		hover_leave,
		hover_activate,
		key_select_current,
		release_current,
		activate_action,
		activate_submenu,
		submenu_activate_action,
		submenu_hide,

		// ein paar einfache Ereignisse, die nichts mit dem "Grundstatus" zu tun haben, sondern eher
		// im Hintergrund laufen:
		time_out,
		animate,
		zeitanimation_beginnt,
		zeitanimation_abgeschlossen, // damit zentral geprüft wird, ob der timer zu stoppen ist
		timer_startet,
		timer_abbruch,
	};
	// -> Variablen:
	// Die Init-Daten (wie beschaffen?)
	PieInitData		 _initData;
	// Für die Berechnung nötige, relativ konstante, Style-Daten
	PieStyleData	 _styleData;
	// DIE DATEN an sich ...
	SuperPolator	 _data;
	QSize			 _avgSz;
	// Dies werden die "immer aktuellen" Action-Rects.  Dort hin werden die Actions gerendert.
	QList< QRect >	 _actionRects;
	// Zum Rendern brauche ich allerdings noch eine weitere Information: die Opacity.  Da ggf.
	// auch die Scale eine Rolle spielen wird, mache ich gleich einen Punkt!
	QList< QPointF > _actionRenderData;
	// Sobald irgend etwas die aktuellen "_actionRects" invalidiert, wird dies gesetzt!
	bool			 _actionRectsDirty{ true };
	// Das Bounding-Rect wird beim Hinzufügen von Aktionen neu berechnet.  Da das Ergebnis dieser
	// Berechnungen vom "Still" - also Ruhezustand - ausgeht, werden klare Margins hinzugefügt.
	QRect			 _boundingRect;
	// Das Selection Rect
	bool			 _selRectDirty{ false };
	PieSelectionRect _selRect, _srS, _srE;
	// Variablen des Zustandsautomaten
	PieMenuStatus	 _state{ PieMenuStatus::hidden };
	// Mouse / Pointer Device:
	QPoint			 _lastPos;
	qreal			 _lastDm, _lastDi;
	// verschiedene "current IDs":
	int				 _timerId{ 0 }, _alertId{ 0 }; // ich lasse 2 Timer laufen
	int				 _hoverId{ -1 }, _folgeId{ -1 }, _activeId{ -1 };
	// Zeitanimationen (mglw. kann ich mir die Booleans sparen und nutze QTime::isValid() ?)
	bool			 _rectsAnimiert{ false }, _selRectAnimiert{ false };
	QTime			 _selRectStart, _hoverStart, _folgeBeginn;
	int	  _folgeDauerMS; // beim Folgen kann es schnellere und langsamere Transitionen geben ...

	qreal _folgeDW{ 0. };
	QList< qreal >	 _hitDist;
	PieSelectionRect _folge{ { 0, 0, 0, 0 }, Qt::GlobalColor::transparent }, _folgeStart,
		_folgeStop;

	// -> Methoden:
	// Style-Daten beschaffen (bei init und bei StyleChange)
	void  readStyleData();
	// Die Größen der Boxen berechnen -> Grunddaten
	void  calculatePieDataSizes();
	// Die Ruhepositionen berechnen
	void  calculateStillData();
	// Kleiner Helfer, den ich ggf. an mehreren Stellen brauche
	qreal startR( int runde ) const;

	// Statusverwaltung!
	void  ereignis( PieMenuEreignis e, qint64 parameter = 0ll, qint64 parameter2 = 0ll );
	void  makeState( PieMenuStatus s );
	void  updateCurrentVisuals();
	bool  hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID );
	// Ein kluger Hit-Test rechnet einfach - wir nutzen diese Signed Distance Function:
	auto  boxDistance( const auto &p, const auto &b ) const
	{
		return qMax( qMax( b.left() - p.x(), p.x() - b.right() ),
					 qMax( b.top() - p.y(), p.y() - b.bottom() ) );
	}

	void initHover();

	// -> Variablen - Windows-spezifisch:
	//      das transparente Fenster sollte keinen Schatten werfen !0 => geschafft!
	HWND _dropShadowRemoved{ nullptr };
	// -> Method hiding:
	// Tearing our menus off is not allowed -> make setting it private
	using QMenu::setTearOffEnabled;
#pragma endregion

	friend QDebug operator<<( QDebug d, const QPieMenu::PieMenuStatus s );
};

inline QDebug operator<<( QDebug d, const QPieMenu::PieMenuStatus s )
{
	constexpr const char *c[] = { "hidden", "still", "closeby", "hover", "active", "child" };
	return ( d << c[ int( s ) ] );
}
