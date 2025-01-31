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

#include "Helpers.h"
#include "Superpolator.h"

#include <QBasicTimer>
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
	void hideEvent( QHideEvent *e ) override { /*QMenu::hideEvent( e );*/ }
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
	qreal			 _lastDm, _lastDi;
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
