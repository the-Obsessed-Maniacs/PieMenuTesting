#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QStyleFactory>
#include <QTranslator>

int main( int argc, char *argv[] )
{
	QApplication	  a( argc, argv );
	MainWindow		  w;
	QTranslator		  translator;
	const QStringList uiLanguages = QLocale::system().uiLanguages();
	for ( const QString &locale : uiLanguages )
	{
		const QString baseName = "PieMenuTesting_" + QLocale( locale ).name();
		if ( translator.load( ":/i18n/" + baseName ) )
		{
			a.installTranslator( &translator );
			break;
		}
	}
	QWidget s( nullptr, Qt::WindowType::Popup );
	auto   *l = new QVBoxLayout;
	auto   *h = new QHBoxLayout;
	for ( auto i : QStyleFactory::keys() )
	{
		auto *b = new QPushButton( i );
		b->setStyle( QStyleFactory::create( i ) );
		b->connect( b, &QPushButton::clicked,
					[ &a, &w, &s, i ]()
					{
						a.setStyle( QStyleFactory::create( i ) );
						w.show();
						s.close();
					} );
		h->addWidget( b );
	}
	l->addWidget( new QLabel( s.tr( "Bitte Style für die Application auswählen!" ) ) );
	l->addLayout( h );
	s.setLayout( l );
	auto s0 = l->sizeHint().grownBy( s.contentsMargins() );
	auto r0 = QRect{ {}, s0 };
	r0.moveCenter( a.screenAt( QCursor::pos() )->availableGeometry().center() );
	s.setGeometry( r0 );
	s.show();
	return a.exec();
}
