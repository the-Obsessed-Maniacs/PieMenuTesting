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
#include "Helpers.h"
#include "SelfRegFactory.h"

#include <QList>
#include <QObject>
#include <QRectF>

using Opacities	   = QList< qreal >;
using RadiusAngles = QList< QPair< qreal, qreal > >;

struct StrategieBasis : Factory< StrategieBasis >
{
	StrategieBasis( Key ) {}
	// Austauschformat:
	using Items													= Intersector< QRectF, QPointF >;
	// Soll die Berechnung initial nach Änderung von Parametern durchführen
	virtual void	  calculateItems( Items &items_with_sizes ) = 0;
	// Soll die items auf ihre animierten Positionen setzen
	virtual Opacities animateItems( Items &items_with_sizes, qreal progress ) = 0;

	// Pie Menu Basisparameter (muss mglw. nochmal verändert werden):
	enum Direction { clockwise = -1, counterclockwise = 1 };
	void setDirection( Direction direction ) { _direction = direction; }
	void setStartAngle( qreal angle ) { _startAngle = angle; }
	void setMinDelta( qreal delta ) { _minDelta = delta; }
	void setMaxDelta( qreal delta ) { _maxDelta = delta; }
	void setOpenParam( qreal v ) { _openParam = v; }
	void selectOption( int no ) { _selectedOption = no > -1 && no < _options.count() ? no : -1; }
	Direction		   direction() const { return _direction; }
	qreal			   startAngle() const { return _startAngle; }
	qreal			   minDelta() const { return _minDelta; }
	qreal			   maxDelta() const { return _maxDelta; }
	qreal			   openParam() const { return _openParam; }
	const QString	  &description() const { return _description; }
	const QStringList &options() const { return _options; }
	virtual int		   defaultOption() const { return -1; }
	int				   selectedOption() const { return _selectedOption; }

	QString			   _description;
	QStringList		   _options;
	int				   _selectedOption{ -1 };
	Direction		   _direction{ counterclockwise };
	qreal			   _startAngle{ 0. }, _minDelta{ -20. }, _maxDelta{ -240. }, _openParam{ 0. };
};
typedef Factory< StrategieBasis > StrategieFactory;


class ersterVersuch : public StrategieFactory::Registrar< ersterVersuch >
{
  public:
	ersterVersuch();
	void	  calculateItems( Items &items_with_sizes ) override;
	Opacities animateItems( Items &items_with_sizes, qreal progress ) override;
};

class StrategieNo2 : public StrategieFactory::Registrar< StrategieNo2 >
{
  public:
	StrategieNo2();

	void	  calculateItems( Items &items_with_sizes ) override;
	Opacities animateItems( Items &items_with_sizes, qreal progress ) override;
	int		  defaultOption() const override { return 2; }

  protected:
	RadiusAngles data;
};

class StrategieNo3 : public StrategieFactory::Registrar< StrategieNo3 >
{
  public:
	StrategieNo3();

	void	  calculateItems( Items &items_with_sizes ) override;
	Opacities animateItems( Items &items_with_sizes, qreal progress ) override;

  protected:
	RadiusAngles data;
};

class StrategieNo3plus : public StrategieFactory::Registrar< StrategieNo3plus >
{
  public:
	StrategieNo3plus();

	void	  calculateItems( Items &items_with_sizes ) override;
	Opacities animateItems( Items &items_with_sizes, qreal progress ) override;

  protected:
	RadiusAngles data;
};
