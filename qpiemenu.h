#ifndef QPIEMENU_H
#define QPIEMENU_H

#include <QMenu>
#include <QPropertyAnimation>
#include <QPushButton>

class QStylePainter;

inline QPointF qSinCos( qreal radians )
{
	return QPointF{ qSin( radians ), qCos( radians ) };
}

class QPieMenu : public QMenu
{
	Q_OBJECT
	// Q_PROPERTY( qreal showTime READ getShowTime WRITE setShowTime NOTIFY showTimeChanged )
	Q_PROPERTY( qreal spread READ getSpread WRITE setSpread NOTIFY spreadChanged )

  public:
	QPieMenu( QWidget *parent = nullptr );
	QPieMenu( const QString &title, QWidget *parent = nullptr );

	void  setSpread( qreal newSpread );
	qreal getSpread() const;

  signals:
	void spreadChanged( qreal newSpread );

  protected:
	// events we need to override:
	// -> the showEvent starts the show animation, which is used to
	//    make the menu readable.
	void showEvent( QShowEvent *e ) override;
	// -> the hideEvent starts the backwards show animation, which is used to
	//    make the menu fade out/away.
	void hideEvent( QHideEvent *e ) override;
	// -> the paintEvent should be called every time the positions were updated.
	//    It shall use QStylePainter-Stuff to draw all the items at the newly
	//    updated positions.
	void paintEvent( QPaintEvent *e ) override;
	// -> the wheel event could be used to rotate the items around.
	//    It's not a must-have, it's a like to have...
	void wheelEvent( QWheelEvent *e ) override { QMenu::wheelEvent( e ); }

  private:
	void			 calculateItems();
	QList< QPointF > baseCalcPositions( QList< QRectF > & );
	void			 drawMenuBackground( QStylePainter & );
	// For each menu entry we create a button and check its path
	// -> angle, "t", still_moving, QPushButton*
	using entryPath = std::tuple< qreal, qreal, bool, QPushButton * >;
	QList< entryPath >												   _offsets;
	QList< QRectF >													   _itemRects;
	QList< QPointF >												   _itemRA;
	QPropertyAnimation												  *_anim{ nullptr };
	std::function< QList< QPointF >( QPieMenu *, QList< QRectF > & ) > _calculator{
		&QPieMenu::baseCalcPositions };
};

#endif // QPIEMENU_H
