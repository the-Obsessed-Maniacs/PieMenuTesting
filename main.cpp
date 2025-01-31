#include "mainwindow.h"

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
	w.show();
	return a.exec();
}
