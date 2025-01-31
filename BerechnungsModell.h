/**************************************************************************************************
 * Datei: BerechnungsModell.h
 * Autor: Stefan <St0fF / Neoplasia ^ the Obsessed Maniacs> Kaps, 2024-2025
 *
 * Dieses BerechnungsModell stellt über die StrategieFactory eine aktuelle Berechnung und Animation
 * zur Verfügung.
 **************************************************************************************************
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
	void				calculateItems( int row, StrategieBasis::Items &items_with_sizes );
	Opacities			animateItems( int row, qreal t, StrategieBasis::Items &items_with_sizes );

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
