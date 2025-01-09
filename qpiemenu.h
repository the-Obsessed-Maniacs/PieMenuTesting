/**************************************************************************************************
 * QPieMenu - eine Implementation radialer Kontextmenüs in Qt
 * ==========================================================
 * Author: Stefan <St0fF / the0bsessedManiacs> Kaps
 * Entwicklungsbeginn: 31.12.2024
 *************************************************************************************************/
#pragma once

#include "Helpers.h"

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
	QPoint _execPoint;
	qreal  _start0{ qDegreesToRadians( 175 ) }, _max0{ qDegreesToRadians( 285 ) };
	qreal  _selRectAlpha{ 0.5 }, _selRectDuration{ 0.5 };
	bool   _negativeDirection{ true }, _isContext{ true }, _isSubMenu{ false };

	void   init( QPoint menuExecPoint, bool isContextMenu = true )
	{
		_execPoint = menuExecPoint, _isContext = isContextMenu;
	}
};
#pragma endregion
#pragma region( Interne_Datenstrukturen_Actiondata )
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
	constexpr auto operator=( const qreal angle ) { _angle = angle, _a = true; }
};
QDebug operator<<( QDebug dbg, const PieDataItem &pd );

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
inline QDebug operator<<( QDebug dbg, const PieData &pd );
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
// In der Theorie sollte der Compiler dieser Klasse keinerlei Speicher zuweisen, weil sie ja
// wirklich komplett auf constexprs beruht und lediglich einen Zugriffswrapper darstellt.
class DistArc
{
  public:
	// Immer direkt auf einer Referenz arbeiten.  Das Objekt soll nur lokale Lebensdauer haben!
	explicit DistArc( QRectF &rect )
		: r( rect )
	{}
	DistArc()				   = delete;
	DistArc( const DistArc & ) = delete;
	qreal		   &a() { return ( *this )( 2 ); }
	qreal		   &d() { return ( *this )( 3 ); }
	qreal		   &r0() { return ( *this )( 0 ); }
	qreal		   &r1() { return ( *this )( 1 ); }

	constexpr qreal a0() const { return r.left() - r.top(); }
	constexpr qreal a1() const { return r.left() + r.top(); }

  private:
	qreal  &operator()( int id ) { return reinterpret_cast< qreal * >( &r )[ id ]; }
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
	QSize sizeHint() const override;

	int	  actionIndexAt( const QPoint &pt ) const;
	int	  actionIndex( QAction *a ) const { return actions().indexOf( a ); }

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
	void timerEvent( QTimerEvent *e ) override;
#pragma endregion
#pragma region( private_redesign )
  private:
	//
	// Redesign der privaten Menu-Anteile:
	// -> Typen:
	//      Implementation von Hover- bzw. selection-State:
	//      ===============================================
	enum class PieMenuStatus { hidden = 0, still, closeby, hover, openSub, trigger };
	enum class PieMenuEreignis {
		// Ereignisse: Mousezeigerposition vs. _actionRects
		hover_Item_start,
		hover_Item_end,
		// Ereignisse: Mousezeigerposition vs. Mittelpunkt vs. kürzeste Distanz zu einem Element
		distance_closing_in,
		distance_leaving,
		// Ereignisse: andere
		getsShown,
		animate,
		selecting_item,
		open_sub_menu,
		execute_action,
		// ToDo: mehr davon ...
		folgeanimation_beginnt, // intern zum sparen von code wird ereignis rekursiv aufgerufen
		zeitanimation_beginnt,
		zeitanimation_abgeschlossen, // damit zentral geprüft wird, ob der timer zu stoppen ist
	};
	// -> Variablen:
	// Die Init-Daten müssen wir zum Teil noch zu beschaffen herausfinden.
	PieInitData		 _initData;
	// Für die Berechnung nötige, relativ konstante, Style-Daten
	PieStyleData	 _styleData;
	// _actionPieData:  Hier speichern wir dauerhaft die Größen der Items.  Sie werden (erstmal)
	// wirklich nur beim Ändern der Action-Liste neu berechnet. Weiterhin speichern wir den Radius
	// und die Winkel für die "Ruheposition".
	PieData			 _actionPieData;
	// Dies werden die "immer aktuellen" Action-Rects.  Dort hin werden die Actions gerendert.
	QList< QRect >	 _actionRects;
	// Wir brauchen zum Rendern allerdings noch eine weitere Information: die Opacity.  Da ggf.
	// auch die Scale eine Rolle spielen wird, machen wir gleich einen Punkt draus.
	QList< QPointF > _actionRenderData;
	// Sobald irgend etwas die aktuellen "_actionRects" invalidiert, wird dies gesetzt!
	bool			 _actionRectsDirty{ true };
	// Das Selection Rect
	PieSelectionRect _selRect, _srS, _srE;
	bool			 _selRectDirty{ false };
	// Animationsvariablen:
	int				 _timerId{ 0 }, _hoverId{ -1 };
	bool			 _selRectAnimiert{ false }, _raAnimiert{ false };
	QTime			 _selRectStart, _raStart;
	int				 _raDauerMS;
	// Die Folge-Animation hab ich mir bisher nicht so recht überlegt.  Sie soll den Kreissektor
	// des nächsten Items beleuchten.  Die Beleuchtungsstärke in Abhängigkeit von der Entfernung
	// des Zeigers zum Element.  Das AUslösen funktioniert schonmal.  Auch ein Wechsel der IDs.
	// Im Ein-, Aus- und Umschaltmoment sollte eine Zeitanimation ausgelöst werden.
	// Die Farbe des Zielwertes sollte dynamisch angepasst werden, sofern die Animation läuft.
	// Wir haben es hier genau genommen mit 2 verschiedenen Spielen zu tun:
	// mouseMoveEvent:  - die aktuellen Folgedaten ausrechnen
	//                  - entscheiden, ob ein Wechsel stattgefunden hat
	//                    -> ja:    Zeitanimation starten
	//                  - aktuelle Daten als Zieldaten bereitstellen
	// paintEvent:  - prüfen, ob entweder eine _folgeId gesetzt ist (> -1), oder _folgenAnimiert
	//                ist, wenn ja: den Sektor zeichnen.  Wenn animiert und t>=1, animationBeenden.
	// ereignis:    - Zeitanimation_starten: letzte Daten als Quelle übernehmen bzw. bei (id==-1)
	//                das Teil irgendwie aus der Mitte heraus animieren.
	//              - animate: wird durch den timer zur invalidisierung aufgerufen - hat NICHTS
	//                im mouseMoveEvent verloren!!!  Stattdessen wird "update()" aufgerufen, wenn
	//                ein Update nötig ist.
	//
	// Welche Daten brauche ich, um vglw. schnell das Teil zeichnen zu können?
	//  Radien und Winkel sind am leichtesten zu interpolieren -> ergo bauen wir uns ein QRectF
	//  aus Radien und Winkeln.  Dazu die Farbe, weil die soll ja auch Alpha-Animiert werden, und -
	//  schwupps - sind wir wieder bei PieSelectionRect, nur dass wir es jetzt als
	//      { { kleiner Radius, kleiner Winkel }, { großer Radius, großer Winkel } } + QColor
	//  interpretieren werden.

	bool			 _folgenAnimiert{ false };
	int				 _folgeId{ -1 }, _folgeDauer{ 300 };
	PieSelectionRect _folge{ { 0, 0, 0, 0 }, Qt::GlobalColor::transparent }, _folgeStart,
		_folgeStop;
	QTime _folgeBeginn;

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
	void  updateCurrentVisuals();
	bool  hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID );
	// Ein kluger Hit-Test rechnet einfach - wir nutzen diese Signed Distance Function:
	auto  boxDistance( const auto &p, const auto &b ) const
	{
		return qMax( qMax( b.left() - p.x(), p.x() - b.right() ),
					 qMax( b.top() - p.y(), p.y() - b.bottom() ) );
	}

	// -> Variablen - Windows-spezifisch:
	//      das transparente Fenster sollte keinen Schatten werfen !0 => geschafft!
	HWND _dropShadowRemoved{ nullptr };
	// -> Method hiding:
	// Tearing our menus off is not allowed -> make setting it private
	using QMenu::setTearOffEnabled;
#pragma endregion
};
