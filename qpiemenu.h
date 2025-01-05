#ifndef QPIEMENU_H
#define QPIEMENU_H

#include "Helpers.h"

#include <QMenu>
#include <QPropertyAnimation>
#include <QPushButton>

class QStylePainter;
class QPieMenu;

struct QPieMenuSettings
{
	bool	 previousWasSeparator{ true }; // this is true to allow removing the leading separators
	bool	 contextMenu{ true };
	int		 fw, deskFw, hmarg, vmarg, panelWidth, icone, sp, ih, round{ 0 }, tabWid{ 0 };
	QMargins menuMargins;
	qreal	 startR, startO, dir, maxO;

	QPieMenuSettings( QPieMenu *menu );
	qreal	getStartR( QPieMenu *menu ) const;
	qreal	nextR( QPieMenu *menu ) const;
	QPointF SP() const { return QPointF{ qreal( sp ), qreal( sp ) }; }
};

struct PieRects : public Intersector< QRect, QPoint >
{
	QRect _boundingRect;
	bool  add( R_type r ) override;
	void  addAnyhow( R_type r ) override;
	bool  changeItem( int index, R_type newValue ) override;
	void  reset( int reserved_size );
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
	// Properties
	qreal getShowState() const { return _showState; }
	void  setShowState( qreal value );
	// Overridden methods
	QSize sizeHint() const override;

  signals:
	void showStateChanged( qreal );

  protected:
	// The action-event fires after actions were added or removed.  This is our point of
	// recalculation!
	void actionEvent( QActionEvent *event ) override;
	// -> the paintEvent should be called every time the positions were updated.
	//    It shall use QStylePainter-Stuff to draw all the items at the newly
	//    updated positions.
	void paintEvent( QPaintEvent *e ) override;
	// events we need to override:
	// -> the showEvent starts the show animation, which is used to
	//    make the menu readable.
	void showEvent( QShowEvent *e ) override;
	// -> the hideEvent starts the backwards show animation, which is used to
	//    make the menu fade out/away.
	void hideEvent( QHideEvent *e ) override;
	// -> the wheel event could be used to rotate the items around.
	//    It's not a must-have, it's a like to have...
	void wheelEvent( QWheelEvent *e ) override { QMenu::wheelEvent( e ); }

  private:
	//
	// Redesign of private elements:
	// -> Types:
	using RadiusAngles = QList< QPointF >;
	// -> Members:
	qreal				_showState{ 0 };
	PieRects			_actionRects;
	RadiusAngles		_actionRAs;
	QPieMenuSettings	_settings;
	QPropertyAnimation *_anim{ nullptr };
	// -> Methods:
	// basic calculation of the items -> produce item sizes: calculateSizes()
	void				updateActionRects();
	QRect				getNextRect( int actionIndex );
	QSize				getActionSize( QAction *action );
};

#endif // QPIEMENU_H
