#include "BerechnungsModell.h"

BerechnungsModell::BerechnungsModell( QObject *parent )
	: QAbstractItemModel( parent )
{
	for ( auto i : StrategieFactory::Classes() ) _strategien.append( i.first() );
}

BerechnungsModell::~BerechnungsModell() {}

int BerechnungsModell::rowCount( const QModelIndex &parent ) const
{
	// Top Level Items:
	if ( !parent.isValid() ) return _strategien.count();
	else if ( parent.row() < _strategien.count() )
		return _strategien.at( parent.row() )->options().count();
	return 0;
}

int BerechnungsModell::columnCount( const QModelIndex &parent ) const
{
	return 1; // always only one level of information to display
}

QVariant BerechnungsModell::data( const QModelIndex &index, int role ) const
{
	switch ( role )
	{
		case Qt::DisplayRole:
			if ( index.isValid() )
			{
				if ( index.parent().isValid() )
				{
					if ( index.parent().row() < _strategien.count() )
					{
						auto &strat = _strategien.at( index.parent().row() );
						if ( index.row() < strat->options().count() )
							return strat->options().at( index.row() );
					}
				} else if ( index.row() < _strategien.count() )
					return _strategien.at( index.row() )->description();
			}
			[[fallthrough]];
		default: return {};
	}
}

QModelIndex BerechnungsModell::index( int row, int column, const QModelIndex &parent ) const
{
	if ( column == 0 )
		if ( !parent.isValid() )
		{
			if ( row < _strategien.count() ) return createIndex( row, column, nullptr );
		} else if ( parent.row() < _strategien.count() ) {
			auto &strat = _strategien.at( parent.row() );
			if ( row < strat->options().count() )
				return createIndex( row, column, _strategien.at( parent.row() ) );
		}
	return {};
}

QModelIndex BerechnungsModell::parent( const QModelIndex &index ) const
{
	if ( index.isValid() )
	{
		if ( auto strat = static_cast< StrategieBasis * >( index.internalPointer() ) )
		{
			auto i = _strategien.indexOf( strat );
			if ( i >= 0 ) return createIndex( i, 0, nullptr );
		}
	}
	return {};
}

void BerechnungsModell::calculateItems( int row, Intersector &items_with_sizes )
{
	if ( row >= 0 && row < _strategien.count() )
		_strategien.at( row )->calculateItems( items_with_sizes );
}

Opacities BerechnungsModell::animateItems( int row, qreal t, Intersector &items_with_sizes )
{
	if ( row >= 0 && row < _strategien.count() )
		return _strategien.at( row )->animateItems( items_with_sizes, t );
	return {};
}

int BerechnungsModell::defaultRadiusPolicy( const QModelIndex &index ) const
{
	auto r = index.parent().isValid() ? index.parent().row() : index.row();
	if ( r >= 0 && r < _strategien.count() ) return _strategien[ r ]->defaultOption();
	else return -1;
}

void BerechnungsModell::setStartAngle( int angle )
{
	for ( auto i : _strategien ) i->setStartAngle( angle );
	emit uniformDataChanged();
}

void BerechnungsModell::setMinDelta( int delta )
{
	for ( auto i : _strategien ) i->setMinDelta( delta );
	emit uniformDataChanged();
}

void BerechnungsModell::setMaxDelta( int delta )
{
	for ( auto i : _strategien ) i->setMaxDelta( delta );
	emit uniformDataChanged();
}

void BerechnungsModell::setOpenParam( int v )
{
	for ( auto i : _strategien ) i->setOpenParam( v );
	emit uniformDataChanged();
}

void BerechnungsModell::setDirection( bool clockwise )
{
	for ( auto i : _strategien )
		i->setDirection( clockwise ? StrategieBasis::clockwise : StrategieBasis::counterclockwise );
	emit uniformDataChanged();
}

void BerechnungsModell::selectOption( const QModelIndex &index, int no )
{
	auto r = index.parent().isValid() ? index.parent().row() : index.row();
	if ( r >= 0 && r < _strategien.count() )
		_strategien.at( r )->selectOption( no ), emit uniformDataChanged();
}
