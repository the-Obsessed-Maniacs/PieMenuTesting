// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BerechnungsModell.h"
#include "Helpers.h"
#include "Placements.h"
#include "ui_mainwindow.h"

#include <QAbstractGraphicsShapeItem>
#include <QAbstractItemModel>
#include <QMainWindow>
#include <QPropertyAnimation>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QGraphicsView;
QT_END_NAMESPACE

//! [0]
class MainWindow
	: public QMainWindow
	, public Ui::MainWindow
{
	Q_OBJECT
	Q_PROPERTY( qreal showTime READ getShowTime WRITE setShowTime NOTIFY showTimeChanged )

  public:
	MainWindow();

	qreal getShowTime() const { return lastTime; }
	void  setShowTime( qreal newShowTime );

  signals:
	void showTimeChanged( qreal newShowTime );

  private slots:
	void		grabAnimParams();
	void		placementChanged( int );
	void		radiusPolicyChanged( int );
	inline void clearItems( bool onlyRecalculate = false );
	void		animStateChange( QAbstractAnimation::State );
	void		animDirectionChange( QAbstractAnimation::Direction newDirection );
	void		toggleAnim( bool );
	void		switchAnimFwd( bool );
	void		switchAnimRev( bool );
	void		menuAbout2show();
	void		computePositions();
	void		animatePositions();

	void		newFile();
	void		open();
	void		save();
	void		print();
	void		undo();
	void		redo();
	void		cut();
	void		copy();
	void		paste();
	void		bold();
	void		italic();
	void		leftAlign();
	void		rightAlign();
	void		justify();
	void		center();
	void		setLineSpacing();
	void		setParagraphSpacing();
	void		about();
	void		aboutQt();

  protected:
	void contextMenuEvent( QContextMenuEvent *event ) override;

  private:
	void							createActions();
	void							createMenus();

	QMenu						   *fileMenu;
	QMenu						   *editMenu;
	QMenu						   *formatMenu;
	QMenu						   *helpMenu;
	QActionGroup				   *alignmentGroup;
	QAction						   *newAct;
	QAction						   *openAct;
	QAction						   *saveAct;
	QAction						   *printAct;
	QAction						   *exitAct;
	QAction						   *undoAct;
	QAction						   *redoAct;
	QAction						   *cutAct;
	QAction						   *copyAct;
	QAction						   *pasteAct;
	QAction						   *boldAct;
	QAction						   *italicAct;
	QAction						   *leftAlignAct;
	QAction						   *rightAlignAct;
	QAction						   *justifyAct;
	QAction						   *centerAct;
	QAction						   *setLineSpacingAct;
	QAction						   *setParagraphSpacingAct;
	QAction						   *aboutAct;
	QAction						   *aboutQtAct;

	qreal							lastTime{ 0. };
	QPropertyAnimation				anim;
	QGraphicsWidget				   *x_button;
	QList< QGraphicsProxyWidget * > items;
	Intersector						_boxes;
	struct c3_item
	{
		QAbstractGraphicsShapeItem *item{ nullptr };
		c3_item					   *next{ nullptr };
		c3_item					   *back{ nullptr };
		bool						good{ false };
		QList< QPointF >			centers;
		// Swap CTor (Needed by QList realloc)
		c3_item() noexcept
			: centers()
		{}
		c3_item( c3_item &other ) = delete;
		c3_item( c3_item &&other ) { *this = std::move( other ); }
		c3_item &operator=( c3_item &other ) = delete;
		c3_item &operator=( c3_item &&other )
		{
			good = other.good;
			std::swap( item, other.item );
			std::swap( next, other.next );
			std::swap( back, other.back );
			centers.swap( other.centers );
			if ( back ) back->next = this;
			if ( next ) next->back = this;
			return *this;
		}
		~c3_item()
		{
			if ( next ) delete next, next = nullptr;
		}
		c3_item *addItem( QAbstractGraphicsShapeItem *_item )
		{
			if ( item )
			{
				if ( next == nullptr ) next = new c3_item;
				return next->addItem( item );
			}
			item = _item;
			return this;
		}
		int count() const { return next ? 1 + next->count() : 1; }
		int countSimples() const
		{
			return next ? int( centers.isEmpty() ) + next->countSimples()
						: int( centers.isEmpty() );
		}
	};
	QList< c3_item * >			   c3_items;
	QGraphicsItemGroup			   c3_group;
	QList< QPair< qreal, qreal > > data;
};
//! [3]

#endif
