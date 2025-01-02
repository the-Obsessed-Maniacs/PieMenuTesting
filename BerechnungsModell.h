/**************************************************************************************************
 * Unser BerechnungsModell stellt über die StrategieFactory eine aktuelle Berechnung und Animation
 * zur Verfügung.
 *************************************************************************************************/
#pragma once

#include "Placements.h"

#include <QAbstractItemModel>

class BerechnungsModell : public QAbstractItemModel
{
	Q_OBJECT

  public:
	BerechnungsModell( QObject *parent = nullptr );
	virtual ~BerechnungsModell() override;
	virtual int			rowCount( const QModelIndex &parent = QModelIndex() ) const override;
	virtual int			columnCount( const QModelIndex &parent = QModelIndex() ) const override;
	virtual QVariant	data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
	virtual QModelIndex index( int row, int column,
							   const QModelIndex &parent = QModelIndex() ) const override;
	virtual QModelIndex parent( const QModelIndex &index ) const override;

	// Model - Interface für die UI:
	void				calculateItems( int row, Intersector &items_with_sizes );
	Opacities			animateItems( int row, qreal t, Intersector &items_with_sizes );

	int					defaultRadiusPolicy( const QModelIndex &index ) const;
	int					defaultStrategyID() const { return _strategien.count() - 1; }

	// Model - Interface für die UI: Parameteränderungen
  public slots:
	void setStartAngle( int angle );
	void setMinDelta( int delta );
	void setMaxDelta( int delta );
	void setOpenParam( int v );
	void setDirection( bool clockwise );
	void selectOption( const QModelIndex &index, int no );

  signals:
	void uniformDataChanged();

  private:
	QList< StrategieBasis * > _strategien;
};
