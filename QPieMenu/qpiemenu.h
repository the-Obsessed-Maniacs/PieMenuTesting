/******************************************************************************
 * QPieMenu.h - eine Implementation radialer Kontextmenüs in Qt
 * ============================================================
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
#pragma once

#include <QBasicTimer>
#include <QMenu>
#include <QTime>

#define SCALE_MAX 1.35

class QStylePainter;
class QPieMenu;

#pragma region( Template_Geschichten )
template < qreal halfCircle >
constexpr qreal normalize( qreal a )
{
	auto s = a < 0;
	auto b = ( !s && a < 2 * halfCircle ) ? a : a + 2 * halfCircle * trunc( 0.5 * a / halfCircle );
	return b + s * 2 * halfCircle;
}
template < qreal halfCircle >
qreal distance( qreal from, qreal to )
{
	constexpr auto w0 = 2 * halfCircle, w1 = 0.5 / halfCircle, w2 = halfCircle;
	auto		   d = to - from + w0 * ( trunc( from * w1 ) - trunc( to * w1 ) );
	return d + ( d > w2 ? -w0 : d < -w2 ? w0 : 0 );
}
#pragma endregion
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
	qreal	_start0{ qDegreesToRadians( 175 ) }, _max0{ qDegreesToRadians( 285 ) }, _minR{ 0. };
	qreal	_selRectAlpha{ 0.5 };
	quint32 _animBaseDur{ 250 }, _subMenuDelayMS{ 750 };
	bool	_negativeDirection{ true }, _isContext{ true }, _isSubMenu{ false };

	void	init( QPoint menuExecPoint, bool isContextMenu = true )
	{
		_execPoint = menuExecPoint, _isContext = isContextMenu;
	}
	constexpr qreal dir( qreal f = 1.f ) const { return ( _negativeDirection ? -f : f ); }
};
#pragma endregion
#pragma region( Animationshelfer )
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

struct alignas( 128 ) SPElem
{
	int			   w, h;									// Standard-Größe des Elementes
	qreal		   a{ 0. }, t0{ 0. }, t1{ 1. };				// Ruhewinkel, sst-parameter
	qreal		   sr{ 0. }, sa{ 0. }, so{ 0. }, ss{ 0.5 }; // Start
	qreal		   er{ 0. }, ea{ 0. }, eo{ 0. }, es{ 0.5 }; // Ende
	qreal		   cr{ 0. }, ca{ 0. }, co{ 0. }, cs{ 0.5 }; // Current
	// Zugriffshelfer:
	// -> Initialisierung durch Setzen der Größe
	constexpr void operator=( const QSize s ) { w = s.width(), h = s.height(); }
	constexpr void operator=( const qreal o ) { a = o; }
	constexpr	   operator QSize() const { return { w, h }; }
	constexpr	   operator QSizeF() const { return { ( qreal ) w, ( qreal ) h }; }
	__m128d		  &t01() { return *( &as128d() + 1 ); }
	__m128d		  &as128d() { return *reinterpret_cast< __m128d * >( this ); }
	__m256d		  &stillData() { return *reinterpret_cast< __m256d * >( this ); }
	__m256d		  &quelle() { return *( reinterpret_cast< __m256d * >( this ) + 1 ); }
	__m256d		  &ziel() { return *( reinterpret_cast< __m256d * >( this ) + 2 ); }
	__m256d		  &aktuell() { return *( reinterpret_cast< __m256d * >( this ) + 3 ); }
	const __m256d &quelle() const { return *( reinterpret_cast< const __m256d * >( this ) + 1 ); }
	const __m256d &ziel() const { return *( reinterpret_cast< const __m256d * >( this ) + 2 ); }
	const __m256d &aktuell() const { return *( reinterpret_cast< const __m256d * >( this ) + 3 ); }
};

inline QDebug operator<<( QDebug d, const __m256d &o )
{
	QDebugStateSaver s( d );
	d.nospace() << Qt::fixed << qSetRealNumberPrecision( 3 ) << "{ " << o.m256d_f64[ 0 ] << ", "
				<< o.m256d_f64[ 1 ] << ", " << o.m256d_f64[ 2 ] << ", " << o.m256d_f64[ 3 ] << " }";
	return d;
}

inline QDebug operator<<( QDebug d, const SPElem &o )
{
	QDebugStateSaver s( d );
	d.nospace() << Qt::fixed << qSetRealNumberPrecision( 2 ) << "defAngle=" << o.a
				<< ", target=" << o.ziel();
	return d;
}

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
 **************************************************************************************************
 * Ich habe festgestellt, dass meine Pie-Menüs für jegliche Dynamik mit 3 Grundgrößen und 4 Anima-
 * tionsgrößen dargestellt werden können.
 *
 * Je Element: Größe und Winkel in Ruheposition:
 * Für alle Elemente gemeinsam gilt der Ruhe-Radius.
 *
 * Das Rendern dieser Daten ist klar und einfach.  Ich brauche noch einen Mittelpunkt, da bietet
 * sich ja sowas wie "{}" an ;)  Irgendwoher möchte ich noch die Deckkraft nehmen und die Skala ist
 * ziemlich logisch bei 1 angesiedelt, taucht deshalb im Rendering nicht auf.
 *
 * Der Gedanke, hier jetzt alles in eine Klasse zu packen ist recht einfach.  Ich habe schon so
 * viele Arrays in der Basisklasse, dass eine "standard Array mit intergrierterter Verwaltung" -
 * Klasse sinnvoll erscheint, um den Überblick zu bewahren.  Damit kann ich die gesamte Geometrie-
 * Generierung hier her auslagern und dem QPieMenu lediglich die Listenklasse zum Verwalten präsen-
 * tieren.
 *
 * Ich stolpere schon über die erste Frage: wie soll ich die Stilldaten genau ablegen?
 * Radius, Winkel, Breite, Höhe?  Dann kann ich den Pointer darauf auch als QRectF interpretieren.
 * Eine noch schöner erscheinende Lösung: ich speichere die Smoothstep-Parameter anstelle des
 * (globalen) Radius.  Weil ich die "magischen" 128 Bytes nicht überschreiten möchte und nur einen
 * double-Wert durch 2 Werte ersetzen muss, kann ich die t0 und t1 nur als floats abspeichern.  Den
 * Radius packe ich wieder global in die Listenklasse ...
 *
 * Die Animationsdaten hatte ich ja schon verbal aufgezählt:
 *  -   Radius
 *  -   Winkel
 *  -   Skalierung
 *  -   Deckkraft
 * ...
 * Der Gedankengang geht weiter.  Diese Animationsdaten benötige ich jetzt genau 3 Mal.  Start, Ziel
 * und Aktuell.  Wenn ich die Init-Daten in den gleichen "Chunk" packe, bin ich wieder beim Faktor 4
 * für 4 doubles und habe am Ende eine Datenstruktur von 128 Bytes je Element.  Das müsste - so
 * hoffe ich - Cache-freundlich genug sein.
 *
 * Ich habe gerade mal nachgesehen, was die bisherige Infrastruktur braucht:
 *  -   Stillberechnung:
 *      - append(QSize)
 *      - setAngle(qreal)
 *      - radius - weil global - wird manuell gesetzt.
 *      - setCenter ->  nicht benötigt, der Intersector für die Berechnung muss dann lokal erstellt
 *                      werden -> bei geringen Elementanzahlen vermutlich ok.
 *  -   Darstellung:
 *      -   muss im Endeffekt nur mit Bezug auf "dieses hier" umgeschrieben werden - ergo gar nicht
 *          oder nur minimal, denn die Zentren müssen immer neu ausgerechnet werden, um valide
 *          Action-Rects zur Verfügung zu stellen.  Ich kann also einfach ActionRects und
 *          ActionRenderData als Pointer-Parameter in "update" hineingeben ...
 *
 *  -   Animation:
 *      -   muss sorgfältig initialisiert werden
 *      -   als erster Test das einfachste Beispiel: die show-up-Animation -> initShowUp
 *      -   Vorgehensweise bei Start einer Animation (in der Regel zumindest)
 *          -   die aktuellen Werte zu den Quellwerten machen ( copyCurrent2Source() )
 *          -   die neuen Zielwerte berechnen und bereitstellen
 *          -   Dauer festlegen und starten ( startAnimation( ms ) )
 *
 * Damit sollte die rudimentäre Erstimplementation soweit lauffähig sein, dass sie das Menü auf den
 * Bildschirm zaubern kann...
 **************************************************************************************************/
class SuperPolator : public QList< SPElem >
{
  public:
	friend inline QDebug operator<<( QDebug &d, SuperPolator &s );
	// Das sollte "PieData" erstmal ersetzen und kann dem "Algorithmus" vorgelegt werden ...
	void				 clear( int reserveSize );
	constexpr void		 setR( qreal r ) { r0 = r; }
	constexpr qreal		 r() const { return r0; }
	int					 append( QSize elementSize );
	void				 setAngle( int index, qreal radians );

	// Animationen müssen gut vorbereitet werden.  Vermutlich macht es am meisten Sinn, für jede
	// Animation eine eigene Vorbereitung zu kreieren, schliesslich gibt es immer aktuelle Werte
	// zu sichern oder Ähnliches.  Ich beginne mal mit der einfachsten Animation ...
	void				 initShowUp( int duration_ms, qreal startO = 0.f );
	void				 initHideAway( int duration_ms, int currentActiveItem = -1 );
	void				 initStill( int duration_ms );

	// Und nun zur Interpolation ...
	// -> Im SuperPolator erstelle ich die aktuellen actionRects und renderDaten
	// Die Funktion gibt "true" zurück, wenn seine interne Animation abgeschlossen ist.
	bool				 update( QList< QRect > &actions, QList< QPointF > &opaScale );
	void				 startAnimation( int ms ) { durMs = ms, started = QTime::currentTime(); }

	void				 copyCurrent2Source()
	{
		for ( auto &i : *this ) i.quelle() = i.aktuell();
	}
	QDebug debug();

  private:
	// Init-Helfer - wird fast überall benötigt und ist dank "Zugriffshelfer" inlinebar ;)
	void  debugInitialValues( const char *dsc ) const;

	// Variablen...
	qreal r0{ 1. };		// der globale "Ruhe-Radius"
	QTime started;		// falls gerade animiert wird, ist dies die gültige Startzeit
	int	  durMs{ 100 }; // und dies hier wird die geplante Dauer der Animation sein.
};

// Der Intersektor ist eine Rect(F)-Liste, die beim Hinzufügen mit den "neuen Funktionen"
// (add(),addAnyhow()...) auch einen Überlappungsstatus der hinzugefügten Rects speichert.
template < typename R, typename P >
	requires( std::is_same_v< R, QRect > && std::is_same_v< P, QPoint > )
			|| ( std::is_same_v< R, QRectF > && std::is_same_v< P, QPointF > )
struct Intersector : public QList< R >
{
	using R_type = R;
	using P_type = P;

	R	 _lastIntersection, _br;
	bool _hasIntersection{ false };
	// overwrite QList::clear()
	void clear() { _br = {}, resetIntersection(), QList< R >::clear(); }
	void resetIntersection() { _lastIntersection = {}, _hasIntersection = false; }
	bool checkIntersections()
	{
		int i( QList< R >::count() - 1 );
		while ( i > 0 )
		{
			auto j( i - 1 );
			while ( j >= 0 )
			{
				if ( QList< R >::at( i ).intersects( QList< R >::at( j ) ) )
				{
					_lastIntersection = QList< R >::at( i ).intersected( QList< R >::at( j ) );
					return ( _hasIntersection = true );
				}
				--j;
			}
			--i;
		}
		_lastIntersection = {};
		return ( _hasIntersection = false );
	}
	// add() returns true, if the item can be added without intersections.
	// If there is an intersection, lastIntersection will be set to the intersection rectangle and
	// the item is not added.
	virtual bool add( R_type r )
	{
		for ( auto i : *this )
			if ( ( _lastIntersection = r.intersected( i ) ).isValid() )
				return !( _hasIntersection = true );
		QList< R >::append( r );
		_br |= r;
		return true;
	}
	virtual void addAnyhow( R_type r )
	{
		int i( QList< R >::count() );
		while ( !_hasIntersection && ( --i >= 0 ) )
			_hasIntersection =
				( _lastIntersection = r.intersected( QList< R >::at( i ) ) ).isValid();
		QList< R >::append( r );
		_br |= r;
	}
	// changeItem() returns true, if the item at index can be changed without intersections.
	virtual bool changeItem( int index, R_type newValue )
	{
		if ( QList< R >::at( index ) != newValue )
		{
			// assign and recheck bounds
			QList< R >::operator[]( index ) = newValue;
			int i( QList< R >::count() );
			while ( !_hasIntersection && ( --i >= 0 ) )
				if ( i != index )
					_hasIntersection =
						( _lastIntersection = newValue.intersected( QList< R >::at( i ) ) )
							.isValid();
			_br |= newValue;
		}
		return !_hasIntersection;
	}
	virtual bool changeItem( int index, P_type newCenter )
	{
		return changeItem( index, QList< R >::at( index ).translated(
									  newCenter - QList< R >::at( index ).center() ) );
	}
	virtual void resetToFirst()
	{
		QList< R >::resize( 1 );
		_br = QList< R >::first();
		resetIntersection();
	}
};

// Mal ein Versuch, die Winkeldifferenzen-Geschichte aus Strategie 3 zu vereinfachen:
class BestDelta
{
  public:
	BestDelta() = default;
	BestDelta( bool negAngles )
		: dir( negAngles ? -1 : 1 )
	{}
	__forceinline void init( qreal direction, qreal reference_angle )
	{
		bad = -( good = std::numeric_limits< qreal >::max() );
		dir = direction;
		w0	= reference_angle;
	}
	__forceinline void init( qreal reference_angle )
	{
		bad = -( good = std::numeric_limits< qreal >::max() );
		w0	= reference_angle;
	}
	constexpr auto	reference() const { return w0; }
	constexpr bool	hasGood() const { return good < std::numeric_limits< qreal >::max(); }
	constexpr bool	hasBad() const { return bad > -std::numeric_limits< qreal >::max(); }
	constexpr qreal best() const { return dir * ( hasGood() ? good : hasBad() ? bad : 0.0 ); }
	template < double halfCircle >
	__forceinline void addAngle( qreal a )
	{
		auto d = distance< halfCircle >( w0, a ) * dir;
		if ( d > 0 ) good = qMin( good, d );
		else if ( d < 0 ) bad = qMax( bad, d );
	}
	template < double halfCircle >
	__forceinline void addBoth( qreal a, bool halfOffs )
	{
		addAngle< halfCircle >( a ), addAngle< halfCircle >( qreal( halfOffs ) * halfCircle - a );
	}
	// Instanziierungen für die beiden Winkelmaße:
	__forceinline void addDeg( qreal a ) { addAngle< 180.0 >( a ); }
	__forceinline void addRad( qreal a ) { addAngle< M_PI >( a ); }
	__forceinline void addRad2( qreal a, bool isAsin = false ) { addBoth< M_PI >( a, isAsin ); }

  private:
	qreal good{ std::numeric_limits< qreal >::max() }, bad{ -std::numeric_limits< qreal >::max() },
		dir{ -1 }, w0{ M_PI_2 };
};
#pragma endregion

class QPieMenu : public QMenu
{
	Q_OBJECT
#pragma region( public_overrides_of_QMenu )
  public:
	QPieMenu( QWidget *parent = nullptr )
		: QMenu( parent )
	{}
	QPieMenu( const QString &title, QWidget *parent = nullptr );
	QPieMenu( const QString &title, const QList< QAction * > &actions, QWidget *parent = nullptr );
	virtual ~QPieMenu();
	QAction *execPieStartRange( QPoint pos, float phi0, float dphimax );

	// Overridden methods
	QSize	 sizeHint() const override;
	void	 setVisible( bool vis = true ) override;
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
	void mousePressEvent( QMouseEvent *e ) override;
	void mouseReleaseEvent( QMouseEvent *e ) override;
	// -> ...
	void keyPressEvent( QKeyEvent *e ) override;
	// -> Animationen weiterschreiben, verzögertes Öffnen von Submenüs
	void timerEvent( QTimerEvent *e ) override;

	// -> the wheel event could be used to rotate the items around.
	//    It's not a must-have, it's a like to have...
	void wheelEvent( QWheelEvent *e ) override { /*QMenu::wheelEvent( e );*/ }

	void changeEvent( QEvent *e ) override { /*QMenu::changeEvent( e );*/ }
	void enterEvent( QEnterEvent *e ) override { /*QMenu::enterEvent( e );*/ }
	void leaveEvent( QEvent *e ) override { /*QMenu::leaveEvent( e );*/ }
	void hideEvent( QHideEvent *e ) override;
#pragma endregion
#pragma region( private_redesign )
  private:
	//
	// Redesign der privaten Menu-Anteile:
	// -> Typen:
	enum class PieMenuStatus { hidden = 0, still, closeby, hover, item_active };
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
	// Zum Rendern brauche ich allerdings noch weitere Informationen: Opacity und Scale
	QList< QPointF > _actionRenderData;
	// Das Bounding-Rect wird beim Hinzufügen von Aktionen neu berechnet.  Da das Ergebnis dieser
	// Berechnungen vom "Still" - also Ruhezustand - ausgeht, werden klare Margins hinzugefügt.
	QRect			 _boundingRect;
	// Das Selection Rect
	PieSelectionRect _selRect, _srS, _srE;

	PieMenuStatus	 _state{ PieMenuStatus::hidden };
	// Sobald irgend etwas die aktuellen "_actionRects" invalidiert, wird dies gesetzt!
	bool			 _actionRectsDirty{ true };
	bool			 _selRectDirty{ false };
	bool			 _mouseDown{ false };
	// Mouse / Pointer Device:
	QPoint			 _lastPos;
	qreal			 _lastDm{ 0. }, _lastDi{ 1000. };
	// verschiedene "current IDs":
	//  _hoverId: ist im Endeffekt, was gerade gewählt ist - egal ob via KBD oder Mouse.
	//  _folgeId: verfolgen wir im Moment -> muss vorgehalten werden falls die ID wechselt.
	//  _alertId: bei Mouse-Hover mache ich Submenüs bei längerem Hovern auf, aber nur wenn die
	//            _hoverId zwischendurch nicht gesprungen ist.
	int				 _hoverId{ -1 }, _folgeId{ -1 }, _alertId{ -1 }, _lastWi{ -1 };
	// Zeitanimationen
	QBasicTimer		 _rectsAnimiert, _selRectAnimiert, _alertTimer, _kbdOvr;
	QTime			 _selRectStart;
	// showAsChild: quellmenu
	QPieMenu		*_causedMenu{ nullptr };

	// -> Methoden:
	// Style-Daten beschaffen (bei init und bei StyleChange)
	void			 readStyleData();
	// Die Größen der Boxen berechnen -> Grunddaten
	void			 calculatePieDataSizes();
	// Kleiner Helfer, den ich ggf. an mehreren Stellen brauche
	qreal			 startR( int runde ) const;
	// Die Ruhepositionen berechnen
	// Neuer Algorithmus: nutze StepBox, um die still-Daten zu berechnen
	void			 createStillData();
	// Spezialberechnungen:
	void			 createZoom();
	// Großer Helfer: berechne die nächste Box, gib das Delta zurück
	qreal			 stepBox( int index, QRectF &rwsd, QSizeF &lastSz );
	// Grundsätzlich werden mit stepBox Zieldaten berechnet.
	// Diese Funktion leitet aus den Zieldaten still-Daten ab.
	void			 makeZielStill( qreal r0 );

	void			 setState( PieMenuStatus s )
	{
		qDebug() << "[[" << ( _state = s ) << "]]" << _folgeId << _hoverId;
	}
	void showChild( int index );
	void initVisible( bool show );
	void initStill();
	void initCloseBy( int newFID = -1 );
	void initHover( int newHID = -1 );
	void initActive();
	void updateCurrentVisuals();
	bool hitTest( const QPoint &p, qreal &mindDistance, qint32 &minDistID );
	// Ein kluger Hit-Test rechnet einfach - wir nutzen diese Signed Distance Function:
	auto boxDistance( const auto &p, const auto &b ) const
	{
		return qMax( qMax( b.left() - p.x(), p.x() - b.right() ),
					 qMax( b.top() - p.y(), p.y() - b.bottom() ) );
	}

	void startSelRect( QRect tr = QRect() );
	void startSelRectColorFade( const QColor &target_color );
	void showAsChild( QPieMenu *source, QPoint pos, qreal minRadius, qreal startAngle,
					  qreal endAngle );
	void childHidden( QPieMenu *child, bool hasTriggered );
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
	constexpr const char *c[] = { "  hidden   ", "   still   ", "  closeby  ", "   hover   ",
								  "item_active" };
	return ( d << c[ int( s ) ] );
}
