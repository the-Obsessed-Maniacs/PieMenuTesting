#pragma once
/***************************************************************************************************
 * Der Super-polator. .. ... .. . Eigentlich nur eine einfache Listen-Interpolator-Klasse.
 * ---------------------------------------------------------------------------------------
 * Author: Stefan <St0fF / Neoplasia ^ the 0bsessed Maniacs> Kaps, 10.Jan.2025
 **************************************************************************************************
 * Ich habe festgestellt, dass meine Pie-Menüs für jegliche Dynamik mit 3 Grundgrößen und 4 Anima-
 * tionsgrößen dargestellt werden können.
 *
 * Je Element: Größe und Winkel in Ruheposition:
 * Für alle Elemente gemeinsam gilt der Ruhe-Radius.
 *
 * Das Rendern dieser Daten ist klar und einfach.  Ich brauche noch einen Mittelpunkt, da bietet
 * sich ja sowas wie "{}" an ;)  Irgendwoher möchte ich noch die Deckkraft nehmen und die Skala ist
 * ziemlich logisch bei 1 angesiedelt, taucht deshalb im Rendering nicht auf.
 *
 * Der Gedanke, hier jetzt alles in eine Klasse zu packen ist recht einfach.  Ich habe schon so
 * viele Arrays in der Basisklasse, dass eine "standard Array mit intergrierterter Verwaltung" -
 * Klasse sinnvoll erscheint, um den Überblick zu bewahren.  Damit kann ich die gesamte Geometrie-
 * Generierung hier her auslagern und dem QPieMenu lediglich die Listenklasse zum Verwalten präsen-
 * tieren.
 *
 * Ich stolpere schon über die erste Frage: wie soll ich die Stilldaten genau ablegen?
 * Radius, Winkel, Breite, Höhe?  Dann kann ich den Pointer darauf auch als QRectF interpretieren.
 * Eine noch schöner erscheinende Lösung: ich speichere die Smoothstep-Parameter anstelle des
 * (globalen) Radius.  Weil ich die "magischen" 128 Bytes nicht überschreiten möchte und nur einen
 * double-Wert durch 2 Werte ersetzen muss, kann ich die t0 und t1 nur als floats abspeichern.  Den
 * Radius packe ich wieder global in die Listenklasse ...
 *
 * Die Animationsdaten hatte ich ja schon verbal aufgezählt:
 *  -   Radius
 *  -   Winkel
 *  -   Skalierung
 *  -   Deckkraft
 * ...
 * Der Gedankengang geht weiter.  Diese Animationsdaten benötige ich jetzt genau 3 Mal.  Start, Ziel
 * und Aktuell.  Wenn ich die Init-Daten in den gleichen "Chunk" packe, bin ich wieder beim Faktor 4
 * für 4 doubles und habe am Ende eine Datenstruktur von 128 Bytes je Element.  Das müsste - so
 * hoffe ich - Cache-freundlich genug sein.
 *
 * Ich habe gerade mal nachgesehen, was die bisherige Infrastruktur braucht:
 *  -   Stillberechnung:
 *      - append(QSize)
 *      - setAngle(qreal)
 *      - radius - weil global - wird manuell gesetzt.
 *      - setCenter ->  nicht benötigt, der Intersector für die Berechnung muss dann lokal erstellt
 *                      werden -> bei geringen Elementanzahlen vermutlich ok.
 *  -   Darstellung:
 *      -   muss im Endeffekt nur mit Bezug auf "dieses hier" umgeschrieben werden - ergo gar nicht
 *          oder nur minimal, denn die Zentren müssen immer neu ausgerechnet werden, um valide
 *          Action-Rects zur Verfügung zu stellen.  Ich kann also einfach ActionRects und
 *          ActionRenderData als Pointer-Parameter in "update" hineingeben ...
 *
 *  -   Animation:
 *      -   muss sorgfältig initialisiert werden
 *      -   als erster Test das einfachste Beispiel: die show-up-Animation -> initShowUp
 *      -   Vorgehensweise bei Start einer Animation (in der Regel zumindest)
 *          -   die aktuellen Werte zu den Quellwerten machen ( copyCurrent2Source() )
 *          -   die neuen Zielwerte berechnen und bereitstellen
 *          -   Dauer festlegen und starten ( startAnimation( ms ) )
 *
 * Damit sollte die rudimentäre Erstimplementation soweit lauffähig sein, dass sie das Menü auf den
 * Bildschirm zaubern kann...
 **************************************************************************************************/
#include <QList>
#include <QRectF>
#include <QTime>

struct alignas( 128 ) SPElem
{
	int			   w, h;									// Standard-Größe des Elementes
	qreal		   a{ 0. }, t0{ 0. }, t1{ 1. };				// Ruhewinkel, sst-parameter
	qreal		   sr{ 0. }, sa{ 0. }, so{ 0. }, ss{ 0.5 }; // Start
	qreal		   er{ 0. }, ea{ 0. }, eo{ 0. }, es{ 0.5 }; // Ende
	qreal		   cr{ 0. }, ca{ 0. }, co{ 0. }, cs{ 0.5 }; // Current
	// Zugriffshelfer:
	// -> Initialisierung durch Setzen der Größe
	constexpr void operator=( const QSize s ) { w = s.width(), h = s.height(); }
	constexpr void operator=( const qreal o ) { a = o; }
	constexpr	   operator QSize() const { return { w, h }; }
	constexpr	   operator QSizeF() const { return { ( qreal ) w, ( qreal ) h }; }
	__m128d&	   t01() { return *( &as128d() + 1 ); }
	__m128d&	   as128d() { return *reinterpret_cast< __m128d* >( this ); }
	__m256d&	   stillData() { return *reinterpret_cast< __m256d* >( this ); }
	__m256d&	   quelle() { return *( reinterpret_cast< __m256d* >( this ) + 1 ); }
	__m256d&	   ziel() { return *( reinterpret_cast< __m256d* >( this ) + 2 ); }
	__m256d&	   aktuell() { return *( reinterpret_cast< __m256d* >( this ) + 3 ); }
	const __m256d& quelle() const { return *( reinterpret_cast< const __m256d* >( this ) + 1 ); }
	const __m256d& ziel() const { return *( reinterpret_cast< const __m256d* >( this ) + 2 ); }
	const __m256d& aktuell() const { return *( reinterpret_cast< const __m256d* >( this ) + 3 ); }
};

inline QDebug operator<<( QDebug d, const __m256d& o )
{
	QDebugStateSaver s( d );
	d.nospace() << Qt::fixed << qSetRealNumberPrecision( 3 ) << "{ " << o.m256d_f64[ 0 ] << ", "
				<< o.m256d_f64[ 1 ] << ", " << o.m256d_f64[ 2 ] << ", " << o.m256d_f64[ 3 ] << " }";
	return d;
}

inline QDebug operator<<( QDebug d, const SPElem& o )
{
	QDebugStateSaver s( d );
	d.nospace() << Qt::fixed << qSetRealNumberPrecision( 2 ) << "defAngle=" << o.a
				<< ", target=" << o.ziel();
	return d;
}

class SuperPolator : public QList< SPElem >
{
  public:
	friend inline QDebug operator<<( QDebug& d, SuperPolator& s );
	// Das sollte "PieData" erstmal ersetzen und kann dem "Algorithmus" vorgelegt werden ...
	void				 clear( int reserveSize );
	constexpr void		 setR( qreal r ) { r0 = r; }
	constexpr qreal		 r() const { return r0; }
	int					 append( QSize elementSize );
	void				 setAngle( int index, qreal radians );

	// Animationen müssen gut vorbereitet werden.  Vermutlich macht es am meisten Sinn, für jede
	// Animation eine eigene Vorbereitung zu kreieren, schliesslich gibt es immer aktuelle Werte
	// zu sichern oder Ähnliches.  Ich beginne mal mit der einfachsten Animation ...
	void				 initShowUp( int duration_ms, qreal startO = 0.f );
	void				 initHideAway( int duration_ms, int currentActiveItem = -1 );
	void				 initStill( int duration_ms );

	// Und nun zur Interpolation ...
	// -> Im SuperPolator erstelle ich die aktuellen actionRects und renderDaten
	// Die Funktion gibt "true" zurück, wenn seine interne Animation abgeschlossen ist.
	bool				 update( QList< QRect >& actions, QList< QPointF >& opaScale );
	void				 startAnimation( int ms ) { durMs = ms, started = QTime::currentTime(); }

	void				 copyCurrent2Source()
	{
		for ( auto& i : *this ) i.quelle() = i.aktuell();
	}

  private:
	// Init-Helfer - wird fast überall benötigt und ist dank "Zugriffshelfer" inlinebar ;)
	void  debugInitialValues( const char* dsc ) const;

	// Variablen...
	qreal r0{ 1. };		// der globale "Ruhe-Radius"
	QTime started;		// falls gerade animiert wird, ist dies die gültige Startzeit
	int	  durMs{ 100 }; // und dies hier wird die geplante Dauer der Animation sein.
};
inline QDebug operator<<( QDebug& d, SuperPolator& i );
