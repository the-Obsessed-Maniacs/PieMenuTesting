// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "mainwindow.h"

#include "qpiemenu.h"

#include <QtWidgets>
#include <ranges>

//! [0]
MainWindow::MainWindow()
	: QMainWindow()
	, anim( this, "showTime", nullptr )
{
	setupUi( this );
	sb_dstart->setRange( d_start->minimum(), d_start->maximum() );
	sb_dfaktor->setRange( d_faktor->minimum(), d_faktor->maximum() );
	sb_rmin->setRange( r_min->minimum(), r_min->maximum() );
	sb_rmax->setRange( r_max->minimum(), r_max->maximum() );

	gv->setScene( new QGraphicsScene );
	auto xb = new QToolButton;
	xb->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit ) );
	connect( xb, &QToolButton::clicked, this, &MainWindow::clearItems );
	x_button = gv->scene()->addWidget( xb );
	x_button->setPos( -0.5 * fromSize( xb->sizeHint() ) );
	gv->scene()->addItem( &c3_group );
	//! [1]

	//! [2]
	createActions();
	createMenus();

	QString message = tr( "A context menu is available by right-clicking" );
	statusBar()->showMessage( message );

	auto wire = [ & ]( auto *v1, auto *v2 )
	{
		v2->setRange( v1->minimum(), v1->maximum() );
		v2->setValue( v1->value() );
		connect( v1, SIGNAL( valueChanged( int ) ), v2, SLOT( setValue( int ) ) );
		connect( v2, SIGNAL( valueChanged( int ) ), v1, SLOT( setValue( int ) ) );
	};
	wire( d_start, sb_dstart );
	wire( d_faktor, sb_dfaktor );
	wire( r_min, sb_rmin );
	wire( r_max, sb_rmax );

	connect( dsb_anistartwert, &QDoubleSpinBox::valueChanged, this, &MainWindow::grabAnimParams );
	connect( dsb_aniendwert, &QDoubleSpinBox::valueChanged, this, &MainWindow::grabAnimParams );
	connect( sl_anidur, &QSlider::valueChanged, this, &MainWindow::grabAnimParams );
	connect( this, &MainWindow::showTimeChanged, this, &MainWindow::animatePositions );

	connect( cb_PlacementPolicies, &QComboBox::currentIndexChanged, this,
			 &MainWindow::placementChanged );
	connect( cb_rstart, &QComboBox::currentIndexChanged, this, &MainWindow::radiusPolicyChanged );

	auto mdl = new BerechnungsModell( this );
	cb_PlacementPolicies->setModel( mdl );
	cb_rstart->setModel( mdl );
	cb_PlacementPolicies->setCurrentIndex( mdl->defaultStrategyID() );

	connect( d_start, &QDial::valueChanged, mdl, &BerechnungsModell::setStartAngle );
	connect( r_min, &QSlider::valueChanged, mdl, &BerechnungsModell::setMinDelta );
	connect( r_max, &QSlider::valueChanged, mdl, &BerechnungsModell::setMaxDelta );
	connect( d_faktor, &QDial::valueChanged, mdl, &BerechnungsModell::setOpenParam );
	connect( rb_minus, &QRadioButton::toggled, mdl, &BerechnungsModell::setDirection );
	connect( mdl, &BerechnungsModell::uniformDataChanged, this, &MainWindow::computePositions );

	mdl->setStartAngle( d_start->value() );
	mdl->setMinDelta( r_min->value() );
	mdl->setMaxDelta( r_max->value() );
	mdl->setOpenParam( d_faktor->value() );
	mdl->setDirection( rb_minus->isChecked() );

	// reflect animation changes
	connect( &anim, &QAbstractAnimation::stateChanged, this, &MainWindow::animStateChange );
	connect( tb_fwd, &QPushButton::toggled, this, &MainWindow::switchAnimFwd );
	connect( tb_rev, &QPushButton::toggled, this, &MainWindow::switchAnimRev );
	connect( tb_playPause, &QPushButton::toggled, this, &MainWindow::toggleAnim );

	sl_anidur->setValue( 4 );
	setWindowTitle( tr( "Menus" ) );
	setMinimumSize( 160, 160 );
	// setWindowState( Qt::WindowFullScreen );
}

void MainWindow::setShowTime( qreal newShowTime )
{
	if ( qAbs( newShowTime - lastTime ) > 0.0001 )
	{
		if ( anim.loopCount() > 1 ) pb_aniTime->setValue( anim.currentLoopTime() );
		pb_aniFullTime->setValue( anim.currentTime() );
		emit showTimeChanged( lastTime = newShowTime );
	}
}

void MainWindow::grabAnimParams()
{
	anim.setStartValue( dsb_anistartwert->value() );
	anim.setEndValue( dsb_aniendwert->value() );
	anim.setDuration( sl_anidur->value() * 100 );
	pb_aniFullTime->setRange( 0, anim.loopCount() * anim.duration() );
	pb_aniTime->setRange( 0, anim.duration() );
	pb_aniTime->setVisible( anim.loopCount() > 1 );
}

void MainWindow::placementChanged( int row )
{
	// Die Strategie wurde geändert.
	// -> radiusPolicy reinitialisieren
	// -> Items animieren
	auto mdl	 = reinterpret_cast< BerechnungsModell * >( cb_PlacementPolicies->model() );
	auto newroot = mdl->index( row, 0 );
	auto nr		 = mdl->rowCount( newroot );
	if ( nr )
	{
		cb_rstart->setRootModelIndex( newroot );
		cb_rstart->setCurrentIndex( mdl->defaultRadiusPolicy( newroot ) );
	} else cb_rstart->clear();
	cb_rstart->setEnabled( nr );
}

void MainWindow::radiusPolicyChanged( int row )
{
	if ( row != -1 ) //
	{
		auto mdl = reinterpret_cast< BerechnungsModell * >( cb_rstart->model() );
		auto idx = cb_rstart->rootModelIndex();
		mdl->selectOption( idx, row );
		update();
	}
}

inline void MainWindow::clearItems( bool onlyRecalculate )
{
	if ( onlyRecalculate )
	{
		data.clear();
		data.resize( items.count(), { -1, 0 } );
	} else {
		while ( items.count() ) delete items.takeFirst();
		_boxes.clear();
	}
	// erst den Zugriff auf die erstellten GraphicsItems entfernen
	while ( c3_items.count() ) delete c3_items.takeLast();
	// dann die Items an sich entfernen.
	while ( c3_group.childItems().count() ) delete c3_group.childItems().takeLast();
}

void MainWindow::animStateChange( QAbstractAnimation::State state )
{
	// reflect animation state in the UI
	QSignalBlocker bf( tb_fwd ), br( tb_rev ), bp( tb_playPause );
	tb_fwd->setChecked( anim.direction() == QAbstractAnimation::Forward );
	tb_rev->setChecked( anim.direction() == QAbstractAnimation::Backward );
	switch ( state )
	{
		case QAbstractAnimation::State::Running: // kind of "started"
			tb_playPause->setChecked( true );
			tb_playPause->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::MediaPlaybackPause ) );
			break;
		default: //  Paused or Stopped
			tb_playPause->setChecked( false );
			tb_playPause->setIcon( QIcon::fromTheme( QIcon::ThemeIcon::MediaPlaybackStart ) );
			break;
	}
}

void MainWindow::animDirectionChange( QAbstractAnimation::Direction newDirection )
{
	QSignalBlocker bf( tb_fwd ), br( tb_rev );
	tb_fwd->setChecked( newDirection == QAbstractAnimation::Forward );
	tb_rev->setChecked( newDirection == QAbstractAnimation::Backward );
}

void MainWindow::toggleAnim( bool on )
{
	// If animation is at end, turn around ...
	if ( on )
	{
		if ( anim.endValue() == anim.currentValue() )
			anim.setDirection( QAbstractAnimation::Backward );
		if ( anim.startValue() == anim.currentValue() )
			anim.setDirection( QAbstractAnimation::Forward );
	}
	// at last, we toggle the animation state.
	switch ( anim.state() )
	{
		case QAbstractAnimation::Paused: anim.resume(); break;
		case QAbstractAnimation::Stopped: anim.start(); break;
		case QAbstractAnimation::Running: anim.pause(); break;
		default: break;
	}
}

void MainWindow::switchAnimFwd( bool on )
{
	auto isFwd{ anim.direction() == QAbstractAnimation::Forward };
	if ( isFwd != on ) // switch directions
		anim.setDirection( on ? QAbstractAnimation::Forward : QAbstractAnimation::Backward );
}

void MainWindow::switchAnimRev( bool on )
{
	switchAnimFwd( !on );
}

void MainWindow::menuAbout2show()
{
	if ( auto m = qobject_cast< QMenu * >( QObject::sender() ) )
	{
		// clear items
		clearItems();
		// create items (in reverse order, so the top item is on top)
		// auto dbg = qDebug() << "menu about to show:" << m->title() << "Items:";
		for ( auto a : std::ranges::reverse_view( m->actions() ) )
		{
			auto pb = new QPushButton( a->icon(), a->text() );
			items.prepend( gv->scene()->addWidget( pb ) );
			pb->setVisible( !a->isSeparator() );
		}
		for ( auto i : items )
			// dbg << i->widget()->property( "text" ).toString() << i->boundingRect().size(),
			_boxes.append( i->boundingRect() );
		// first point where/when calculation would make sense
		reinterpret_cast< BerechnungsModell * >( cb_PlacementPolicies->model() )
			->calculateItems( cb_PlacementPolicies->currentIndex(), _boxes );
		anim.setCurrentTime( 0 );
		anim.setDirection( QAbstractAnimation::Forward );
		anim.start();
	} else qDebug() << "MainWindow::menuAbout2show() fired with sender=" << QObject::sender();
}

void MainWindow::computePositions()
{
	if ( items.isEmpty() || items.count() != _boxes.count() ) return;
	auto mdl = reinterpret_cast< BerechnungsModell * >( cb_PlacementPolicies->model() );
	mdl->calculateItems( cb_PlacementPolicies->currentIndex(), _boxes );
	animatePositions();
}

void MainWindow::animatePositions()
{
	if ( items.isEmpty() ) return;
	if ( auto mdl = reinterpret_cast< BerechnungsModell * >( cb_PlacementPolicies->model() ) )
	{
		auto opas = mdl->animateItems( cb_PlacementPolicies->currentIndex(), lastTime, _boxes );
		if ( _boxes.count() == items.count() && opas.count() == items.count() )
			for ( auto i( 0 ); i < items.count(); ++i )
				items[ i ]->setPos( _boxes[ i ].topLeft() ), items[ i ]->setOpacity( opas[ i ] );
	}
}

void MainWindow::newFile()
{
	infoLabel->setText( tr( "Invoked <b>File|New</b>" ) );
}

void MainWindow::open()
{
	infoLabel->setText( tr( "Invoked <b>File|Open</b>" ) );
}

void MainWindow::save()
{
	infoLabel->setText( tr( "Invoked <b>File|Save</b>" ) );
}

void MainWindow::print()
{
	infoLabel->setText( tr( "Invoked <b>File|Print</b>" ) );
}

void MainWindow::undo()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Undo</b>" ) );
}

void MainWindow::redo()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Redo</b>" ) );
}

void MainWindow::cut()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Cut</b>" ) );
}

void MainWindow::copy()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Copy</b>" ) );
}

void MainWindow::paste()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Paste</b>" ) );
}

void MainWindow::bold()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Bold</b>" ) );
}

void MainWindow::italic()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Italic</b>" ) );
}

void MainWindow::leftAlign()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Left Align</b>" ) );
}

void MainWindow::rightAlign()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Right Align</b>" ) );
}

void MainWindow::justify()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Justify</b>" ) );
}

void MainWindow::center()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Center</b>" ) );
}

void MainWindow::setLineSpacing()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Set Line Spacing</b>" ) );
}

void MainWindow::setParagraphSpacing()
{
	infoLabel->setText( tr( "Invoked <b>Edit|Format|Set Paragraph Spacing</b>" ) );
}

void MainWindow::about()
{
	infoLabel->setText( tr( "Invoked <b>Help|About</b>" ) );
	QMessageBox::about( this,
						tr( "About Menu" ),
						tr( "The <b>Menu</b> example shows how to create "
							"menu-bar menus and context menus." ) );
}

void MainWindow::aboutQt()
{
	infoLabel->setText( tr( "Invoked <b>Help|About Qt</b>" ) );
}

void MainWindow::contextMenuEvent( QContextMenuEvent *event )
{
	QPieMenu menu( this );
	menu.addAction( cutAct );
	menu.addAction( copyAct );
	menu.addAction( pasteAct );
	menu.addSeparator();
	menu.addMenu( formatMenu );
	menu.addSection( tr( "Undo/Redo" ) );
	menu.addAction( undoAct );
	menu.addAction( redoAct );
	connect( &menu, &QPieMenu::aboutToShow, this, &MainWindow::menuAbout2show );
	menu.exec( event->globalPos() );
}

//! [4]
void MainWindow::createActions()
{
	//! [5]
	newAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentNew ), tr( "&New" ), this );
	newAct->setShortcuts( QKeySequence::New );
	newAct->setStatusTip( tr( "Create a new file" ) );
	connect( newAct, &QAction::triggered, this, &MainWindow::newFile );
	//! [4]

	openAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentOpen ), tr( "&Open..." ), this );
	openAct->setShortcuts( QKeySequence::Open );
	openAct->setStatusTip( tr( "Open an existing file" ) );
	connect( openAct, &QAction::triggered, this, &MainWindow::open );
	//! [5]

	saveAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentSave ), tr( "&Save" ), this );
	saveAct->setShortcuts( QKeySequence::Save );
	saveAct->setStatusTip( tr( "Save the document to disk" ) );
	connect( saveAct, &QAction::triggered, this, &MainWindow::save );

	printAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::DocumentPrint ), tr( "&Print..." ), this );
	printAct->setShortcuts( QKeySequence::Print );
	printAct->setStatusTip( tr( "Print the document" ) );
	connect( printAct, &QAction::triggered, this, &MainWindow::print );

	exitAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::ApplicationExit ), tr( "E&xit" ), this );
	exitAct->setShortcuts( QKeySequence::Quit );
	exitAct->setStatusTip( tr( "Exit the application" ) );
	connect( exitAct, &QAction::triggered, this, &QWidget::close );

	undoAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditUndo ), tr( "&Undo" ), this );
	undoAct->setShortcuts( QKeySequence::Undo );
	undoAct->setStatusTip( tr( "Undo the last operation" ) );
	connect( undoAct, &QAction::triggered, this, &MainWindow::undo );

	redoAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditRedo ), tr( "&Redo" ), this );
	redoAct->setShortcuts( QKeySequence::Redo );
	redoAct->setStatusTip( tr( "Redo the last operation" ) );
	connect( redoAct, &QAction::triggered, this, &MainWindow::redo );

	cutAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditCut ), tr( "Cu&t" ), this );
	cutAct->setShortcuts( QKeySequence::Cut );
	cutAct->setStatusTip(
		tr( "Cut the current selection's contents to the "
			"clipboard" ) );
	connect( cutAct, &QAction::triggered, this, &MainWindow::cut );

	copyAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditCopy ), tr( "&Copy" ), this );
	copyAct->setShortcuts( QKeySequence::Copy );
	copyAct->setStatusTip(
		tr( "Copy the current selection's contents to the "
			"clipboard" ) );
	connect( copyAct, &QAction::triggered, this, &MainWindow::copy );

	pasteAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::EditPaste ), tr( "&Paste" ), this );
	pasteAct->setShortcuts( QKeySequence::Paste );
	pasteAct->setStatusTip(
		tr( "Paste the clipboard's contents into the current "
			"selection" ) );
	connect( pasteAct, &QAction::triggered, this, &MainWindow::paste );

	boldAct =
		new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatTextBold ), tr( "&Bold" ), this );
	boldAct->setCheckable( true );
	boldAct->setShortcut( QKeySequence::Bold );
	boldAct->setStatusTip( tr( "Make the text bold" ) );
	connect( boldAct, &QAction::triggered, this, &MainWindow::bold );

	QFont boldFont = boldAct->font();
	boldFont.setBold( true );
	boldAct->setFont( boldFont );

	italicAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatTextItalic ),
							 tr( "&Italic" ), this );
	italicAct->setCheckable( true );
	italicAct->setShortcut( QKeySequence::Italic );
	italicAct->setStatusTip( tr( "Make the text italic" ) );
	connect( italicAct, &QAction::triggered, this, &MainWindow::italic );

	QFont italicFont = italicAct->font();
	italicFont.setItalic( true );
	italicAct->setFont( italicFont );

	setLineSpacingAct = new QAction( tr( "Set &Line Spacing..." ), this );
	setLineSpacingAct->setStatusTip(
		tr( "Change the gap between the lines of a "
			"paragraph" ) );
	connect( setLineSpacingAct, &QAction::triggered, this, &MainWindow::setLineSpacing );

	setParagraphSpacingAct = new QAction( tr( "Set &Paragraph Spacing..." ), this );
	setParagraphSpacingAct->setStatusTip( tr( "Change the gap between paragraphs" ) );
	connect( setParagraphSpacingAct, &QAction::triggered, this, &MainWindow::setParagraphSpacing );

	aboutAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::HelpAbout ), tr( "&About" ), this );
	aboutAct->setStatusTip( tr( "Show the application's About box" ) );
	connect( aboutAct, &QAction::triggered, this, &MainWindow::about );

	aboutQtAct = new QAction( tr( "About &Qt" ), this );
	aboutQtAct->setStatusTip( tr( "Show the Qt library's About box" ) );
	connect( aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt );
	connect( aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt );

	leftAlignAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyLeft ),
								tr( "&Left Align" ), this );
	leftAlignAct->setCheckable( true );
	leftAlignAct->setShortcut( tr( "Ctrl+L" ) );
	leftAlignAct->setStatusTip( tr( "Left align the selected text" ) );
	connect( leftAlignAct, &QAction::triggered, this, &MainWindow::leftAlign );

	rightAlignAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyRight ),
								 tr( "&Right Align" ), this );
	rightAlignAct->setCheckable( true );
	rightAlignAct->setShortcut( tr( "Ctrl+R" ) );
	rightAlignAct->setStatusTip( tr( "Right align the selected text" ) );
	connect( rightAlignAct, &QAction::triggered, this, &MainWindow::rightAlign );

	justifyAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyFill ),
							  tr( "&Justify" ), this );
	justifyAct->setCheckable( true );
	justifyAct->setShortcut( tr( "Ctrl+J" ) );
	justifyAct->setStatusTip( tr( "Justify the selected text" ) );
	connect( justifyAct, &QAction::triggered, this, &MainWindow::justify );

	centerAct = new QAction( QIcon::fromTheme( QIcon::ThemeIcon::FormatJustifyCenter ),
							 tr( "&Center" ), this );
	centerAct->setCheckable( true );
	centerAct->setShortcut( tr( "Ctrl+E" ) );
	centerAct->setStatusTip( tr( "Center the selected text" ) );
	connect( centerAct, &QAction::triggered, this, &MainWindow::center );

	//! [6] //! [7]
	alignmentGroup = new QActionGroup( this );
	alignmentGroup->addAction( leftAlignAct );
	alignmentGroup->addAction( rightAlignAct );
	alignmentGroup->addAction( justifyAct );
	alignmentGroup->addAction( centerAct );
	leftAlignAct->setChecked( true );
	//! [6]
}
//! [7]

//! [8]
void MainWindow::createMenus()
{
	//! [9] //! [10]
	fileMenu = menuBar()->addMenu( tr( "&File" ) );
	fileMenu->addAction( newAct );
	//! [9]
	fileMenu->addAction( openAct );
	//! [10]
	fileMenu->addAction( saveAct );
	fileMenu->addAction( printAct );
	//! [11]
	fileMenu->addSeparator();
	//! [11]
	fileMenu->addAction( exitAct );
	connect( fileMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	editMenu = menuBar()->addMenu( tr( "&Edit" ) );
	editMenu->addAction( undoAct );
	editMenu->addAction( redoAct );
	editMenu->addSeparator();
	editMenu->addAction( cutAct );
	editMenu->addAction( copyAct );
	editMenu->addAction( pasteAct );
	editMenu->addSeparator();
	connect( editMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	helpMenu = menuBar()->addMenu( tr( "&Help" ) );
	helpMenu->addAction( aboutAct );
	helpMenu->addAction( aboutQtAct );
	//! [8]
	connect( helpMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );

	//! [12]
	formatMenu = new QPieMenu( tr( "&Format" ) );
	editMenu->addMenu( formatMenu );
	formatMenu->addAction( boldAct );
	formatMenu->addAction( italicAct );
	formatMenu->addSeparator()->setText( tr( "Alignment" ) );
	formatMenu->addAction( leftAlignAct );
	formatMenu->addAction( rightAlignAct );
	formatMenu->addAction( justifyAct );
	formatMenu->addAction( centerAct );
	formatMenu->addSeparator();
	formatMenu->addAction( setLineSpacingAct );
	formatMenu->addAction( setParagraphSpacingAct );
	connect( formatMenu, &QMenu::aboutToShow, this, &MainWindow::menuAbout2show );
}
//! [12]
