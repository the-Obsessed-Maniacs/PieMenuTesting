/******************************************************************************
 *  PieMenuTesting - main.cpp
 *  written by Stefan <St0fF / Neoplasia ^ the Obsessed Maniacs> Kaps 2024-2025
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
