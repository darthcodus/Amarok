// (c) 2004 Mark Kretschmann <markey@web.de>
// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// (c) 2005 Gábor Lehel <illissius@gmail.com>
// (c) 2005 Alexandre Pereira de Oliveira <aleprj@gmail.com>
// See COPYING file for licensing information.

#include <config.h>

#include "amarokconfig.h"
#include "browserToolBar.h"
#include "clicklineedit.h"
#include "debug.h"
#include "collectionbrowser.h"

#include "collectiondb.h"
#include "collectionreader.h"

#include "directorylist.h"
#include "k3bexporter.h"
#include "mediabrowser.h"
#include "metabundle.h"
#include "playlist.h"       //insertMedia()
#include "playlistbrowser.h"
#include "statusbar.h"
#include "tagdialog.h"

#include <unistd.h>         //CollectionView ctor

#include <qapplication.h>
#include <qcstring.h>
#include <qdragobject.h>
#include <qlayout.h>        //infobox
#include <qpainter.h>
#include <qptrlist.h>
#include <qpushbutton.h>
#include <qsimplerichtext.h>
#include <qtimer.h>
#include <qtooltip.h>       //QToolTip::add()

#include <kactioncollection.h>
#include <kapplication.h>   //kapp
#include <kconfig.h>
#include <kdialogbase.h>
#include <kglobal.h>
#include <kiconloader.h>    //renderView()
#include <klocale.h>
#include <kpopupmenu.h>
#include <ktoolbarbutton.h> //ctor
#include <kurldrag.h>       //dragObject()

namespace amaroK { extern KConfig *config( const QString& ); }

CollectionBrowser::CollectionBrowser( const char* name )
    : QVBox( 0, name )
    , m_cat1Menu( new KPopupMenu( this ) )
    , m_cat2Menu( new KPopupMenu( this ) )
    , m_cat3Menu( new KPopupMenu( this ) )
    , m_infoA( 0 )
    , m_infoB( 0 )
    , m_timer( new QTimer( this ) )
{
    setSpacing( 4 );

    KToolBar* toolbar = new Browser::ToolBar( this );

    { //<Search LineEdit>
        KToolBarButton *button;
        KToolBar* searchToolBar = new Browser::ToolBar( this );


        button       = new KToolBarButton( "locationbar_erase", 0, searchToolBar );
        m_searchEdit = new ClickLineEdit( i18n( "Filter here..." ), searchToolBar );
        m_searchEdit->installEventFilter( this );
        searchToolBar->setStretchableWidget( m_searchEdit );

        m_searchEdit->setFrame( QFrame::Sunken );
        connect( button, SIGNAL( clicked() ), m_searchEdit, SLOT( clear() ) );

        QToolTip::add( button, i18n( "Clear filter" ) );
        QToolTip::add( m_searchEdit, i18n( "Enter space-separated terms to filter collection" ) );
    } //</Search LineEdit>

    KActionCollection* ac = new KActionCollection( this );
    m_scanAction = new KAction( i18n( "Update" ), "reload", 0, CollectionDB::instance(), SLOT( scanMonitor() ), ac, "Start Scan" );

    // we need m_scanAction to be initialized before CollectionView's CTOR
    m_view = new CollectionView( this );
    m_view->installEventFilter( this );

    m_configureAction = new KAction( i18n( "Configure Folders" ), "configure", 0, this, SLOT( setupDirs() ), ac, "Configure" );
    m_treeViewAction = new KRadioAction( i18n( "Tree View" ), "view_tree", 0, m_view, SLOT( setTreeMode() ), ac, "Tree View" );
    m_flatViewAction = new KRadioAction( i18n( "Flat View" ), "view_detailed", 0, m_view, SLOT( setFlatMode() ), ac, "Flat View" );
    m_treeViewAction->setExclusiveGroup("view mode");
    m_flatViewAction->setExclusiveGroup("view mode");
    if(m_view->m_viewMode == CollectionView::modeTreeView)
        m_treeViewAction->setChecked(true);
    else
        m_flatViewAction->setChecked(true);


    KActionMenu* tagfilterMenuButton = new KActionMenu( i18n( "Group By" ), "filter", ac );
    tagfilterMenuButton->setDelayed( false );
    m_categoryMenu = tagfilterMenuButton->popupMenu();

    toolbar->setIconText( KToolBar::IconTextRight, false );
    tagfilterMenuButton->plug( toolbar );
    toolbar->insertLineSeparator();

    toolbar->setIconText( KToolBar::IconOnly, false );
    m_treeViewAction->plug( toolbar );
    m_flatViewAction->plug( toolbar );
    toolbar->insertLineSeparator();

    toolbar->setIconText( KToolBar::IconOnly, false );
    m_scanAction->plug( toolbar );
    m_scanAction->setEnabled( !AmarokConfig::monitorChanges() );
    m_configureAction->plug( toolbar );

    m_categoryMenu->insertItem( i18n( "Artist" ), m_view, SLOT( presetMenu( int ) ), 0, IdArtist );
    m_categoryMenu->insertItem( i18n( "Artist / Album" ), m_view, SLOT( presetMenu( int ) ), 0, IdArtistAlbum );
    m_categoryMenu->insertItem( i18n( "Artist" )+" / "+ i18n( "Year" ) + i18n( " - " ) + i18n( "Album" ), m_view, SLOT( presetMenu( int ) ), 0, IdArtistVisYearAlbum );
    m_categoryMenu->insertItem( i18n( "Album" ), m_view, SLOT( presetMenu( int ) ), 0, IdAlbum );
    m_categoryMenu->insertItem( i18n( "Genre / Artist" ), m_view, SLOT( presetMenu( int ) ), 0, IdGenreArtist );
    m_categoryMenu->insertItem( i18n( "Genre / Artist / Album" ), m_view, SLOT( presetMenu( int ) ), 0, IdGenreArtistAlbum );

    m_categoryMenu->insertSeparator();

    m_categoryMenu->insertItem( i18n( "&First Level" ), m_cat1Menu );
    m_categoryMenu->insertItem( i18n( "&Second Level"), m_cat2Menu );
    m_categoryMenu->insertItem( i18n( "&Third Level" ), m_cat3Menu );

    m_cat1Menu ->insertItem( i18n( "&Album" ), m_view, SLOT( cat1Menu( int ) ), 0, IdAlbum );
    m_cat1Menu ->insertItem( i18n( "(Y&ear) - Album" ), m_view, SLOT( cat1Menu( int ) ), 0, IdVisYearAlbum);
    m_cat1Menu ->insertItem( i18n( "A&rtist"), m_view, SLOT( cat1Menu( int ) ), 0, IdArtist );
    m_cat1Menu ->insertItem( i18n( "&Genre" ), m_view, SLOT( cat1Menu( int ) ), 0, IdGenre );
    m_cat1Menu ->insertItem( i18n( "&Year" ), m_view, SLOT( cat1Menu( int ) ), 0, IdYear );

    m_cat2Menu ->insertItem( i18n( "&None" ), m_view, SLOT( cat2Menu( int ) ), 0, IdNone );
    m_cat2Menu ->insertSeparator();
    m_cat2Menu ->insertItem( i18n( "&Album" ), m_view, SLOT( cat2Menu( int ) ), 0, IdAlbum );
    m_cat2Menu ->insertItem( i18n( "(Y&ear) - Album" ), m_view, SLOT( cat2Menu( int ) ), 0, IdVisYearAlbum);
    m_cat2Menu ->insertItem( i18n( "A&rtist" ), m_view, SLOT( cat2Menu( int ) ), 0, IdArtist );
    m_cat2Menu ->insertItem( i18n( "&Genre" ), m_view, SLOT( cat2Menu( int ) ), 0, IdGenre );
    m_cat2Menu ->insertItem( i18n( "&Year" ), m_view, SLOT( cat2Menu( int ) ), 0, IdYear );

    m_cat3Menu ->insertItem( i18n( "&None" ), m_view, SLOT( cat3Menu( int ) ), 0, IdNone );
    m_cat3Menu ->insertSeparator();
    m_cat3Menu ->insertItem( i18n( "A&lbum" ), m_view, SLOT( cat3Menu( int ) ), 0, IdAlbum );
    m_cat3Menu ->insertItem( i18n( "(Y&ear) - Album" ), m_view, SLOT( cat3Menu( int ) ), 0, IdVisYearAlbum);
    m_cat3Menu ->insertItem( i18n( "A&rtist" ), m_view, SLOT( cat3Menu( int ) ), 0, IdArtist );
    m_cat3Menu ->insertItem( i18n( "&Genre" ), m_view, SLOT( cat3Menu( int ) ), 0, IdGenre );
    m_cat3Menu ->insertItem( i18n( "&Year" ), m_view, SLOT( cat3Menu( int ) ), 0, IdYear );

    m_view->cat1Menu( m_view->m_cat1, false );
    m_view->cat2Menu( m_view->m_cat2, false );
    m_view->cat3Menu( m_view->m_cat3, false );
    m_view->setViewMode( m_view->m_viewMode );

    connect( m_timer, SIGNAL( timeout() ), SLOT( slotSetFilter() ) );
    connect( m_searchEdit, SIGNAL( textChanged( const QString& ) ), SLOT( slotSetFilterTimeout() ) );
    connect( m_searchEdit, SIGNAL( returnPressed() ), SLOT( slotSetFilter() ) );

    QHBox *hb = new QHBox( this );
    m_infoA = new QLabel( hb );
    m_infoB = new QLabel( hb );
    m_infoA->setAlignment( Qt::AlignCenter );
    m_infoB->setAlignment( Qt::AlignCenter );

    refreshInfo(); // Need to call this here because CollectionView is created before the labels

    setFocusProxy( m_view ); //default object to get focus
    setMinimumWidth( toolbar->sizeHint().width() + 2 ); //set a reasonable minWidth
}

void
CollectionBrowser::refreshInfo()
{
    if( !m_infoA || !m_infoB )
        return;

    QStringList a = CollectionDB::instance()->query( "SELECT COUNT( url ) FROM tags;" );

    QStringList b;
    QString descriptor;
    if( m_view )
    {
        switch( m_view->m_cat1 )
        {
            case IdArtist:
                b = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM artist;" );
                descriptor = i18n( "1 Artist", "%n Artists", b[0].toInt() );
                break;
            case IdAlbum:
                b = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM album;" );
                descriptor = i18n( "1 Album", "%n Albums", b[0].toInt() );
                break;
            case IdGenre:
                b = CollectionDB::instance()->query( "SELECT COUNT( id ) FROM genre;" );
                descriptor = i18n( "1 Genre", "%n Genres", b[0].toInt() );
                break;
        }
    }

    if( a.isEmpty() )
    {
        m_infoA->hide();
    }
    else
    {
        m_infoA->setText( i18n("1 Track","%n Tracks", a[0].toInt()) );
	m_infoA->show();
    }
    if( b.isEmpty() )
    {
        m_infoB->hide();
    }
    else
    {
	m_infoB->setText( descriptor );
        m_infoB->show();
    }
}

void
CollectionBrowser::slotSetFilterTimeout() //SLOT
{
    m_timer->start( 280, true ); //stops the timer for us first
}

void
CollectionBrowser::slotSetFilter() //SLOT
{
    m_timer->stop();

    m_view->setFilter( m_searchEdit->text() );
    m_view->renderView();
}

void
CollectionBrowser::setupDirs()  //SLOT
{
    m_view->setupDirs();
}

bool CollectionBrowser::eventFilter( QObject *o, QEvent *e )
{
    typedef QListViewItemIterator It;

    switch( e->type() )
    {
    case 6/*QEvent::KeyPress*/:

        //there are a few keypresses that we intercept

        #define e static_cast<QKeyEvent*>(e)

        if( o == m_searchEdit ) //the search lineedit
        {
            QListViewItem *item;
            switch( e->key() )
            {
            case Key_Up:
                item = *It( m_view, It::Visible );
                if( item )
                    while( item->itemBelow() )
                        item = item->itemBelow();
                if( item )
                {
                    m_view->setFocus();
                    m_view->setCurrentItem( item );
                    item->setSelected( true );
                    m_view->ensureItemVisible( item );
                    return true;
                }
                return false;
            case Key_Down:
                if( ( item = *It( m_view, It::Visible ) ) )
                {
                    m_view->setFocus();
                    m_view->setCurrentItem( item );
                    item->setSelected( true );
                    m_view->ensureItemVisible( item );
                    return true;
                }
                return false;

            case Key_PageDown:
            case Key_PageUp:
                QApplication::sendEvent( m_view, e );
                return true;

            case Key_Escape:
                m_searchEdit->clear();
                return true;

            default:
                return false;
            }
        }

        if( m_view->currentItem() && ( ( e->key() == Key_Up   && m_view->currentItem()->itemAbove() == 0 )
                                || ( e->key() == Key_Down && m_view->currentItem()->itemBelow() == 0 ) ) )
        {
            m_view->currentItem()->setSelected( false );
            m_searchEdit->setFocus();
            m_view->ensureItemVisible( *It( m_view, It::Visible ) );
            return true;
        }

        if( ( e->key() >= Key_0 && e->key() <= Key_Z ) || e->key() == Key_Backspace )
        {
            m_searchEdit->setFocus();
            QApplication::sendEvent( m_searchEdit, e );
            return true;
        }
        #undef e
        break;

    default:
        break;
    }

    return QVBox::eventFilter( o, e );
}



//////////////////////////////////////////////////////////////////////////////////////////
// CLASS CollectionView
//////////////////////////////////////////////////////////////////////////////////////////

CollectionView* CollectionView::m_instance = 0;


CollectionView::CollectionView( CollectionBrowser* parent )
        : KListView( parent )
        , m_parent( parent )
{
    DEBUG_FUNC_INFO
    m_instance = this;

    setSelectionMode( QListView::Extended );
    setItemsMovable( false );
    setShowSortIndicator( true );
    setAcceptDrops( false );
    setAllColumnsShowFocus( true );

    //<READ CONFIG>
        KConfig* config = amaroK::config( "Collection Browser" );
        m_cat1 = config->readNumEntry( "Category1", CollectionBrowser::IdArtist );
        m_cat2 = config->readNumEntry( "Category2", CollectionBrowser::IdAlbum );
        m_cat3 = config->readNumEntry( "Category3", CollectionBrowser::IdNone );
        m_viewMode = config->readNumEntry( "ViewMode", modeTreeView );
    //</READ CONFIG>
     KActionCollection* ac = new KActionCollection( this );
     KStdAction::selectAll( this, SLOT( selectAll() ), ac, "collectionview_select_all" );

    connect( CollectionDB::instance(), SIGNAL( scanStarted() ),
             this,                      SLOT( scanStarted() ) );
    connect( CollectionDB::instance(), SIGNAL( scanDone( bool ) ),
             this,                      SLOT( scanDone( bool ) ) );

    connect( this,           SIGNAL( expanded( QListViewItem* ) ),
             this,             SLOT( slotExpand( QListViewItem* ) ) );
    connect( this,           SIGNAL( collapsed( QListViewItem* ) ),
             this,             SLOT( slotCollapse( QListViewItem* ) ) );
    connect( this,           SIGNAL( returnPressed( QListViewItem* ) ),
             this,             SLOT( invokeItem( QListViewItem* ) ) );
    connect( this,           SIGNAL( doubleClicked( QListViewItem*, const QPoint&, int ) ),
             this,             SLOT( invokeItem( QListViewItem* ) ) );
    connect( this,           SIGNAL( rightButtonPressed( QListViewItem*, const QPoint&, int ) ),
             this,             SLOT( rmbPressed( QListViewItem*, const QPoint&, int ) ) );
}


CollectionView::~CollectionView() {
    DEBUG_FUNC_INFO

    KConfig* const config = amaroK::config( "Collection Browser" );
    config->writeEntry( "Category1", m_cat1 );
    config->writeEntry( "Category2", m_cat2 );
    config->writeEntry( "Category3", m_cat3 );
    config->writeEntry( "ViewMode", m_viewMode );
}


//////////////////////////////////////////////////////////////////////////////////////////
// public slots
//////////////////////////////////////////////////////////////////////////////////////////

void
CollectionView::renderView()  //SLOT
{
    DEBUG_FUNC_INFO

    m_parent->refreshInfo();

    if ( childCount() )
        cacheView();

    clear();
    QPixmap pixmap = iconForCategory( m_cat1 );

    //query database for all records with the specified category
    QStringList values;
    QueryBuilder qb;

    int VisYearAlbum = -1;
    int q_cat1=m_cat1;
    int q_cat2=m_cat2;
    int q_cat3=m_cat3;
    if( m_cat1 == CollectionBrowser::IdVisYearAlbum ||
        m_cat2 == CollectionBrowser::IdVisYearAlbum ||
        m_cat3 == CollectionBrowser::IdVisYearAlbum )
    {
        if( m_cat1==CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 1;
            q_cat1 = CollectionBrowser::IdAlbum;
        }
        if( m_cat2==CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 2;
            q_cat2 = CollectionBrowser::IdAlbum;
        }
        if( m_cat3==CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 3;
            q_cat3 = CollectionBrowser::IdAlbum;
        }
    }



    // MODE FLATVIEW
    if ( m_viewMode == modeFlatView )
    {
        if ( m_filter.length() < 3 ) {
            // Redraw bubble help
            triggerUpdate();
            return;
        }

        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
        qb.addReturnValue( q_cat1, QueryBuilder::valName );

        if( q_cat2 != CollectionBrowser::IdNone )
            qb.addReturnValue( q_cat2, QueryBuilder::valName );

        if( q_cat3 != CollectionBrowser::IdNone )
            qb.addReturnValue( q_cat3, QueryBuilder::valName );

        if( VisYearAlbum != 0 )
            qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );

        if( VisYearAlbum == 1 )
            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );

        qb.sortBy( m_cat1, QueryBuilder::valName );

        if( VisYearAlbum == 2 )
            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );

        if( q_cat2 != CollectionBrowser::IdNone )
            qb.sortBy( q_cat2, QueryBuilder::valName );

        if( VisYearAlbum == 3 )
            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );

        if( q_cat3 != CollectionBrowser::IdNone )
            qb.sortBy( q_cat3, QueryBuilder::valName );

        setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );
        qb.setOptions( QueryBuilder::optRemoveDuplicates );

        values = qb.run();

        //add items to the view
        for ( int i = values.count() - qb.countReturnValues(); i >= 0; i -= qb.countReturnValues() )
        {
            if( values[i].stripWhiteSpace().isEmpty() )
                values[i] = i18n( "Unknown" );

            Item* item = new Item( this );
            item->setDragEnabled( true );
            item->setDropEnabled( false );

            item->setUrl( values[ i ] );

            if( VisYearAlbum != 0 )
            {
                 for ( uint j = 1; j < qb.countReturnValues()-1; j++ )
                 {
                    QString value;
                    if( j == qb.countReturnValues() - 2 )
                        value = ( values[ i+j+1 ].isEmpty() ? "?" : values [i+j+1] ) + i18n( " - " ) +
                                ( values[ i+j ]  .isEmpty() ? i18n( "Unknown" ) : values[i+j] );
                    else
                        value = values[i + j];
                    item->setText( j - 1, value );
                 }
            }
            else
            {
                for ( uint j = 1; j < qb.countReturnValues(); j++ )
                    item->setText( j - 1, values[ i + j ] );
            }
        }
    }

    // MODE TREEVIEW
    if( m_viewMode == modeTreeView )
    {
        qb.addReturnValue( q_cat1, QueryBuilder::valName );

        if( VisYearAlbum == 1 )
            qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );

        setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );

        if( VisYearAlbum == 1 )
            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );

        qb.sortBy( q_cat1, QueryBuilder::valName );
        qb.setOptions( QueryBuilder::optRemoveDuplicates );

        if( q_cat1 == QueryBuilder::tabArtist )
            qb.setOptions( QueryBuilder::optNoCompilations );

        values = qb.run();

        //add items to the view

        // if year - artist is cat 1 rebuild the list
        if( VisYearAlbum == 1 )
        {
            QStringList tmpvalues;
            for( unsigned int i=0; i < values.count()-1 ; i += 2 )
            {
                tmpvalues += ( values[i+1].isEmpty() ? "?" : values[i+1]) +
                               i18n( " - " ) +
                             ( values[i].isEmpty() ? i18n( "Unknown" ) : values[i] );
            }
            values = tmpvalues;
        }

        if( values.count() )
            for ( QStringList::Iterator it = values.fromLast(), begin = values.begin(); true; --it )
            {
                if ( (*it).stripWhiteSpace().isEmpty() )
                    (*it) = i18n( "Unknown" );

                if ( (*it).startsWith( "the ", false ) )
                    manipulateThe( *it, true );

                KListViewItem* item = new KListViewItem( this );
                item->setExpandable( true );
                item->setDragEnabled( true );
                item->setDropEnabled( false );
                item->setText( 0, *it );
                item->setPixmap( 0, pixmap );

                if ( it == begin )
                    break;
            }

        //check if we need to add a Various Artists node
        if ( q_cat1 == QueryBuilder::tabArtist )
        {
            qb.clear();
            qb.addReturnValue( q_cat1, QueryBuilder::valName );
            setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );
            qb.setOptions( QueryBuilder::optOnlyCompilations | QueryBuilder::optRemoveDuplicates );
            qb.setLimit( 0, 1 );
            values = qb.run();

            if ( values.count() )
            {
                KListViewItem* item = new KListViewItem( this, lastItem() );
                item->setExpandable( true );
                item->setDragEnabled( true );
                item->setDropEnabled( false );
                item->setText( 0, i18n( "Various Artists" ) );
                item->setPixmap( 0, pixmap );
            }
        }

        // we sort because of the items which should begin with 'the' are at the 't' location
        setSorting( 0 );
        sort();
        // we then disable it, as we don't want the children being sorted later as well
        setSorting( -1 );
    }
    restoreView();
}


//////////////////////////////////////////////////////////////////////////////////////////
// private slots
//////////////////////////////////////////////////////////////////////////////////////////

void
CollectionView::setupDirs()  //SLOT
{
    KDialogBase dialog( this, 0, false );
    kapp->setTopWidget( &dialog );
    dialog.setCaption( kapp->makeStdCaption( i18n("Configure Collection") ) );

    CollectionSetup *setup = new CollectionSetup( &dialog );
    dialog.setMainWidget( setup );
    dialog.showButtonApply( false );
    dialog.adjustSize();
    // Make the dialog a bit wider, default is too narrow to be useful
    dialog.resize( dialog.width() + 50, dialog.height() );

    if ( dialog.exec() != QDialog::Rejected )
    {
        const bool rescan = ( AmarokConfig::collectionFolders() != setup->dirs() );
        setup->writeConfig();

        if ( rescan )
            CollectionDB::instance()->startScan();

        m_parent->m_scanAction->setEnabled( !AmarokConfig::monitorChanges() );
    }
}


void
CollectionView::scanStarted() // SLOT
{
    m_parent->m_scanAction->setEnabled( false );
}


void
CollectionView::scanDone( bool changed ) //SLOT
{
    DEBUG_BLOCK

    if ( changed )
    {
        renderView();
        m_parent->refreshInfo();
    }
    m_parent->m_scanAction->setEnabled( !AmarokConfig::monitorChanges() );
}


void
CollectionView::slotExpand( QListViewItem* item )  //SLOT
{
    DEBUG_FUNC_INFO
    if ( !item || !item->isExpandable() ) return;

    int category = 0;
    QStringList values;

    QueryBuilder qb;
    bool c = false;

    // initalization for year - album mode
    QString tmptext;
    int VisYearAlbum = -1;
    int q_cat1=m_cat1;
    int q_cat2=m_cat2;
    int q_cat3=m_cat3;
    if( m_cat1 == CollectionBrowser::IdVisYearAlbum ||
        m_cat2 == CollectionBrowser::IdVisYearAlbum ||
        m_cat3 == CollectionBrowser::IdVisYearAlbum )
    {
        if( m_cat1 == CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 1;
            q_cat1 = CollectionBrowser::IdAlbum;
        }
        if( m_cat2 == CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 2;
            q_cat2 = CollectionBrowser::IdAlbum;
        }
        if( m_cat3 == CollectionBrowser::IdVisYearAlbum )
        {
            VisYearAlbum = 3;
            q_cat3 = CollectionBrowser::IdAlbum;
        }
    }

    switch ( item->depth() )
    {
        case 0:
            if( endsInThe( item->text( 0 ) ) )
            {
                tmptext = item->text( 0 );
                manipulateThe( tmptext );

                QStringList matches;
                matches << item->text( 0 ) << tmptext;

                qb.addMatches( q_cat1, matches );
            }
            else if ( item->text( 0 ) != i18n( "Various Artists" ) )
            {
                QString tmptext = item->text( 0 );
                if( VisYearAlbum == 1 )
                {
                    QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                    yearAlbumCalc( year, tmptext );
                    qb.addMatch( QueryBuilder::tabYear, year );
                }
                qb.addMatch( q_cat1, tmptext );
            }
            else
            {
                qb.setOptions( QueryBuilder::optOnlyCompilations );
                c = true;
            }

            if ( m_cat2 == QueryBuilder::tabSong )
            {
                qb.addReturnValue( q_cat2, QueryBuilder::valTitle );
                qb.addReturnValue( q_cat2, QueryBuilder::valURL );
                if ( c ) qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
                qb.sortBy( q_cat2, QueryBuilder::valTrack );
                qb.sortBy( q_cat2, QueryBuilder::valURL );
            }
            else
            {
                c = false;
                qb.addReturnValue( q_cat2, QueryBuilder::valName );
                if( VisYearAlbum == 2 )
                {
                    qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );
                    qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );
                }
                qb.sortBy( q_cat2, QueryBuilder::valName );
            }

            category = m_cat2;
            break;

        case 1:
            if( endsInThe( item->text( 0 ) ) )
            {
                tmptext = item->text( 0 );
                manipulateThe( tmptext );

                QStringList matches;
                matches << item->text( 0 ) << tmptext;

                qb.addMatches( q_cat1, matches );
            }
            else if( item->parent()->text( 0 ) != i18n( "Various Artists" ) )
            {
                tmptext = item->parent()->text( 0 );
                QStringList matches( item->parent()->text( 0 ) ) ;

                if( endsInThe( tmptext ) ) {
                    manipulateThe( tmptext );
                    matches << tmptext;
                }

                if( VisYearAlbum == 1 )
                {
                    QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                    yearAlbumCalc( year, tmptext );
                    qb.addMatch( QueryBuilder::tabYear, year );
                }

                qb.addMatches( q_cat1, matches );
            }
            else
            {
                qb.setOptions( QueryBuilder::optOnlyCompilations );
                c = true;
            }

            tmptext = item->text( 0 );

            if( VisYearAlbum == 2 )
            {
                QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                yearAlbumCalc( year, tmptext );
                qb.addMatch( QueryBuilder::tabYear, year );
            }
            qb.addMatch( q_cat2, tmptext );

            if( m_cat3 == QueryBuilder::tabSong )
            {
                qb.addReturnValue( q_cat3, QueryBuilder::valTitle );
                qb.addReturnValue( q_cat3, QueryBuilder::valURL );
                if ( c ) qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );

                qb.sortBy( q_cat3, QueryBuilder::valTrack );
                qb.sortBy( q_cat3, QueryBuilder::valURL );
            }
            else
            {
                c = false;
                qb.addReturnValue( q_cat3, QueryBuilder::valName );
                if( VisYearAlbum == 3 )
                {
                    qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );
                    qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName );
                }
                qb.sortBy( q_cat3, QueryBuilder::valName );
            }

            category = m_cat3;
            break;

        case 2:
            // check for compilations
            if( endsInThe( item->text( 0 ) ) )
            {
                tmptext = item->text( 0 );
                manipulateThe( tmptext );
                QStringList matches;
                matches << item->text( 0 ) << tmptext;

                qb.addMatches( q_cat1, matches );
            }
            else if ( item->parent()->parent()->text( 0 ) != i18n( "Various Artists" ) )
            {
                tmptext = item->parent()->parent()->text( 0 );
                QStringList matches( item->parent()->parent()->text( 0 ) ) ;

                if( endsInThe( tmptext ) ) {
                    manipulateThe( tmptext );
                    matches << tmptext;
                }

                if (VisYearAlbum==1)
                {
                    QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                    yearAlbumCalc( year, tmptext );
                    qb.addMatch( QueryBuilder::tabYear, year );
                }

                qb.addMatches( q_cat1, matches );
            }
            else
            {
                qb.setOptions( QueryBuilder::optOnlyCompilations );
                c = true;
            }

            tmptext = item->parent()->text( 0 );
            QStringList matches( item->parent()->text( 0 ) ) ;

            if( endsInThe( tmptext ) ) {
                manipulateThe( tmptext );
                matches << tmptext;
            }

            if( VisYearAlbum == 2 )
            {
                QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                yearAlbumCalc( year, tmptext );
                qb.addMatch( QueryBuilder::tabYear, year );
            }

            qb.addMatches( q_cat2, matches );

            tmptext = item->text( 0 );
            matches.clear();

            if( VisYearAlbum == 3 )
            {
                QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                yearAlbumCalc( year, tmptext );
                qb.addMatch( QueryBuilder::tabYear, year );
            }

            matches << tmptext;

            if( endsInThe( tmptext ) ) {
                manipulateThe( tmptext );
                matches << tmptext;
            }

            qb.addMatches( q_cat3, matches );

            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );

            if( c )
                qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );

            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valURL );

            category = CollectionBrowser::IdNone;
            break;
    }

    setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );
    qb.setOptions( QueryBuilder::optRemoveDuplicates );
    values = qb.run();
    int countReturnValues = qb.countReturnValues();


    if( category == CollectionBrowser::IdVisYearAlbum )
    {
        QStringList tmpvalues;
        for( unsigned int i=0; i<=values.count() - countReturnValues; i += countReturnValues )
        {
            tmpvalues += ( values[i+1].isEmpty() ? "?" : values[i+1]) +
                           i18n( " - " ) +
                         ( values[i].isEmpty() ? i18n( "Unknown" ) : values[i] );
        }
        values = tmpvalues;
        countReturnValues--;
    }

    QPixmap pixmap;
    bool expandable = category != CollectionBrowser::IdNone;
    if ( expandable )
        pixmap = iconForCategory( category );

    //this check avoid possible problems on database errors. FIXME: Should we add some real error handling here,
    //like calling a collection update or something?
    if ( values.isEmpty() ) { return; }

    for ( int i = values.count() - countReturnValues; i >= 0; i -= countReturnValues )
    {
        QString text;

        //show "artist - title" for compilations
        if ( c )
            text = values[ i + 2 ].stripWhiteSpace().isEmpty() ? i18n( "Unknown" ) : values[ i + 2 ] + i18n(" - ");
        text += values[ i ].stripWhiteSpace().isEmpty() ? i18n( "Unknown" ) : values[ i ];

        Item* child = new Item( item );
        child->setDragEnabled( true );
        child->setDropEnabled( false );
        child->setText( 0, text );
        if ( expandable )
            child->setPixmap( 0, pixmap );
        else
            child->setUrl( values[ i + 1 ] );
        child->setExpandable( expandable );
    }
}


void
CollectionView::slotCollapse( QListViewItem* item )  //SLOT
{
    DEBUG_FUNC_INFO

    QListViewItem* child = item->firstChild();
    QListViewItem* childTmp;

    //delete all children
    while ( child )
    {
        childTmp = child;
        child = child->nextSibling();
        delete childTmp;
    }
}


void
CollectionView::presetMenu( int id )  //SLOT
{
    switch ( id )
    {
        case CollectionBrowser::IdArtist:
            cat1Menu( CollectionBrowser::IdArtist, false );
            cat2Menu( CollectionBrowser::IdNone, false );
            cat3Menu( CollectionBrowser::IdNone, false );
            break;
        case CollectionBrowser::IdAlbum:
            cat1Menu( CollectionBrowser::IdAlbum, false );
            cat2Menu( CollectionBrowser::IdNone, false );
            cat3Menu( CollectionBrowser::IdNone, false );
            break;
        case CollectionBrowser::IdArtistAlbum:
            cat1Menu( CollectionBrowser::IdArtist, false );
            cat2Menu( CollectionBrowser::IdAlbum, false );
            cat3Menu( CollectionBrowser::IdNone, false );
            break;
        case CollectionBrowser::IdArtistVisYearAlbum:
            cat1Menu( CollectionBrowser::IdArtist, false );
            cat2Menu( CollectionBrowser::IdVisYearAlbum, false );
            cat3Menu( CollectionBrowser::IdNone, false );
            break;
        case CollectionBrowser::IdGenreArtist:
            cat1Menu( CollectionBrowser::IdGenre, false );
            cat2Menu( CollectionBrowser::IdArtist, false );
            cat3Menu( CollectionBrowser::IdNone, false );
            break;
        case CollectionBrowser::IdGenreArtistAlbum:
            cat1Menu( CollectionBrowser::IdGenre, false );
            cat2Menu( CollectionBrowser::IdArtist, false );
            cat3Menu( CollectionBrowser::IdAlbum, false );
            break;
    }

    renderView();
}


void
CollectionView::cat1Menu( int id, bool rerender )  //SLOT
{
    m_parent->m_cat1Menu->setItemChecked( m_cat1, false ); //uncheck old item
    m_parent->m_cat2Menu->setItemEnabled( m_cat1, true );  //enable old items
    m_parent->m_cat3Menu->setItemEnabled( m_cat1, true );
    m_cat1 = id;
    updateColumnHeader();
    m_parent->m_cat1Menu->setItemChecked( m_cat1, true );

    //prevent choosing the same category in both menus
    m_parent->m_cat2Menu->setItemEnabled( id , false );
    m_parent->m_cat3Menu->setItemEnabled( id , false );

    //if this item is checked in second menu, uncheck it
    if ( m_parent->m_cat2Menu->isItemChecked( id ) ) {
        m_parent->m_cat2Menu->setItemChecked( id, false );
        m_parent->m_cat2Menu->setItemChecked( CollectionBrowser::IdNone, true );
        m_cat2 = CollectionBrowser::IdNone;
        enableCat3Menu( false );
    }
    //if this item is checked in third menu, uncheck it
    if ( m_parent->m_cat3Menu->isItemChecked( id ) ) {
        m_parent->m_cat3Menu->setItemChecked( id, false );
        m_parent->m_cat3Menu->setItemChecked( CollectionBrowser::IdNone, true );
        m_cat3 = CollectionBrowser::IdNone;
    }

    if ( rerender )
        renderView();
}


void
CollectionView::cat2Menu( int id, bool rerender )  //SLOT
{
    m_parent->m_cat2Menu->setItemChecked( m_cat2, false ); //uncheck old item
    m_parent->m_cat3Menu->setItemEnabled( m_cat3, true );  //enable old item
    m_cat2 = id;
    m_parent->m_cat2Menu->setItemChecked( m_cat2, true );
    updateColumnHeader();

    enableCat3Menu( id != CollectionBrowser::IdNone );

    //prevent choosing the same category in both menus
    m_parent->m_cat3Menu->setItemEnabled( m_cat1 , false );
    if( id != CollectionBrowser::IdNone )
        m_parent->m_cat3Menu->setItemEnabled( id , false );

    //if this item is checked in third menu, uncheck it
    if ( m_parent->m_cat3Menu->isItemChecked( id ) ) {
        m_parent->m_cat3Menu->setItemChecked( id, false );
        enableCat3Menu( false );
    }

    if ( rerender )
        renderView();
}


void
CollectionView::cat3Menu( int id, bool rerender )  //SLOT
{
    m_parent->m_cat3Menu->setItemChecked( m_cat3, false ); //uncheck old item
    m_cat3 = id;
    m_parent->m_cat3Menu->setItemChecked( m_cat3, true );
    updateColumnHeader();

    if ( rerender )
        renderView();
}


void
CollectionView::enableCat3Menu( bool enable )
{
    m_parent->m_cat3Menu->setItemEnabled( CollectionBrowser::IdAlbum, enable );
    m_parent->m_cat3Menu->setItemEnabled( CollectionBrowser::IdVisYearAlbum, enable );
    m_parent->m_cat3Menu->setItemEnabled( CollectionBrowser::IdArtist, enable );
    m_parent->m_cat3Menu->setItemEnabled( CollectionBrowser::IdGenre, enable );
    m_parent->m_cat3Menu->setItemEnabled( CollectionBrowser::IdYear, enable );

    if( !enable ) {
        m_parent->m_cat3Menu->setItemChecked( m_cat3, false );
        m_parent->m_cat3Menu->setItemChecked( CollectionBrowser::IdNone, true );
        m_cat3 = CollectionBrowser::IdNone;
    }
}


void
CollectionView::invokeItem( QListViewItem* item ) //SLOT
{
    if ( !item )
        return;

    if( item->isExpandable() )
        item->setOpen( !item->isOpen() );
    else
        //direct play & prevent doubles in playlist
        Playlist::instance()->insertMedia( static_cast<Item*>( item )->url(), Playlist::Unique | Playlist::DirectPlay );

}


void
CollectionView::rmbPressed( QListViewItem* item, const QPoint& point, int ) //SLOT
{
    if ( item ) {
        KPopupMenu menu( this );

        int cat = 0;
        switch ( item->depth() )
        {
            case 0:
                cat = m_cat1;
                break;
            case 1:
                cat = m_cat2;
                break;
            case 2:
                cat = m_cat3;
                break;
        }

        #ifdef AMAZON_SUPPORT
        enum Actions { APPEND, QUEUE, MAKE, SAVE, MEDIA_DEVICE, BURN_ARTIST, BURN_ALBUM,
                       BURN_DATACD, BURN_AUDIOCD, COVER, INFO,
                       COMPILATION_SET, COMPILATION_UNSET  };
        #else
        enum Actions { APPEND, QUEUE, MAKE, SAVE, BURN_ARTIST, BURN_ALBUM,
                       BURN_DATACD, BURN_AUDIOCD, INFO,
                       COMPILATION_SET, COMPILATION_UNSET, MEDIA_DEVICE };
        #endif
        KURL::List selection = listSelected();
        menu.insertItem( SmallIconSet( "1downarrow" ), i18n( "&Append to Playlist" ), APPEND );
        menu.insertItem( SmallIconSet( "2rightarrow" ), selection.count() == 1 ? i18n( "&Queue Track" )
            : i18n( "&Queue Tracks" ), QUEUE );

        menu.insertItem( SmallIconSet( "player_playlist_2" ), i18n( "Set as &Playlist" ), MAKE );
        if( selection.count() > 1 )
            menu.insertItem( SmallIconSet( "filesave" ), i18n( "&Save as Playlist..." ), SAVE );
        menu.insertItem( SmallIconSet( "usbpendrive_unmount" ), i18n( "Add to Media Device &Transfer Queue" ), MEDIA_DEVICE );

        menu.insertSeparator();

        if( cat == CollectionBrowser::IdArtist )
        {
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n("Burn All Tracks by This Artist"), BURN_ARTIST );
            menu.setItemEnabled( BURN_ARTIST, K3bExporter::isAvailable() );
        }
        else if( cat == CollectionBrowser::IdAlbum || cat == CollectionBrowser::IdVisYearAlbum )
        {
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n("Burn This Album"), BURN_ALBUM );
            menu.setItemEnabled( BURN_ALBUM, K3bExporter::isAvailable() );
        }
        else if( !item->isExpandable() )
        {
            menu.insertItem( SmallIconSet( "cdrom_unmount" ), i18n("Burn to CD as Data"), BURN_DATACD );
            menu.setItemEnabled( BURN_DATACD, K3bExporter::isAvailable() );
            menu.insertItem( SmallIconSet( "cdaudio_unmount" ), i18n("Burn to CD as Audio"), BURN_AUDIOCD );
            menu.setItemEnabled( BURN_AUDIOCD, K3bExporter::isAvailable() );
        }

        menu.insertSeparator();

        #ifdef AMAZON_SUPPORT
        menu.insertItem( SmallIconSet( "www" ), i18n( "&Fetch Cover Images" ), this, SLOT( fetchCover() ), 0, COVER );
        menu.setItemEnabled(COVER, cat == CollectionBrowser::IdAlbum );
        #endif
        menu.insertItem( SmallIconSet( "info" )
            , i18n( "Edit Track &Information...",  "Edit &Information for %n Tracks...", selection.count())
            , this, SLOT( showTrackInfo() ), 0, INFO );

        if ( cat == CollectionBrowser::IdAlbum )
        {
            menu.insertSeparator();
            menu.insertItem( SmallIconSet( "ok" ), i18n( "&Mark as Compilation" ), COMPILATION_SET );
            menu.insertItem( SmallIconSet( "cancel" ), i18n( "&Unmark as Compilation" ), COMPILATION_UNSET );
        }

        switch( menu.exec( point ) )
        {
            case APPEND:
                Playlist::instance()->insertMedia( selection, Playlist::Append );
                break;
            case MAKE:
                Playlist::instance()->insertMedia( selection, Playlist::Replace );
                break;
            case SAVE:
                playlistFromURLs( selection );
                break;
            case QUEUE:
                Playlist::instance()->insertMedia( selection, Playlist::Queue );
                break;
            case MEDIA_DEVICE:
                MediaDevice::instance()->addURLs( selection );
                break;
            case BURN_ARTIST:
                K3bExporter::instance()->exportArtist( item->text(0) );
                break;
            case BURN_ALBUM:
                if ( cat == CollectionBrowser::IdVisYearAlbum )
                    K3bExporter::instance()->exportAlbum(
                        item->text(0).right( item->text(0).length() - item->text(0).find( i18n(" - ") ) - i18n(" - ").length() )
                    );
                else
                    K3bExporter::instance()->exportAlbum( item->text(0) );
                break;
            case BURN_DATACD:
                K3bExporter::instance()->exportTracks( selection, K3bExporter::DataCD );
                break;
            case BURN_AUDIOCD:
                K3bExporter::instance()->exportTracks( selection, K3bExporter::AudioCD );
                break;
            case COMPILATION_SET:
                CollectionDB::instance()->setCompilation( item->text(0), true );
                break;
            case COMPILATION_UNSET:
                CollectionDB::instance()->setCompilation( item->text(0), false );
                break;
        }
    }
}


void
CollectionView::setViewMode( int mode, bool rerender )
{
    clear();

    // remove all columns
    for ( int i = columns() - 1; i >= 0 ; --i )
        removeColumn( i );

    if ( mode == modeTreeView )
    {
        QString headerText = captionForCategory( m_cat1 );
        if ( m_cat2 != CollectionBrowser::IdNone )
            headerText += " / " + captionForCategory( m_cat2 );
        if ( m_cat3 != CollectionBrowser::IdNone )
            headerText += " / " + captionForCategory( m_cat3 );

        addColumn( headerText );
        setResizeMode( QListView::LastColumn );
        setRootIsDecorated( true );
        setFullWidth( true );
        setSorting( -1 ); //disables sorting
    }
    else
    {
        addColumn( i18n( "Title" ) );
        addColumn( captionForCategory( m_cat1 ) );
        if( m_cat2 != CollectionBrowser::IdNone ) addColumn( captionForCategory( m_cat2 ) );
        if( m_cat3 != CollectionBrowser::IdNone ) addColumn( captionForCategory( m_cat3 ) );

        setResizeMode( QListView::AllColumns );
        setRootIsDecorated( false );
        setSorting( 0 );
    }

    m_viewMode = mode;
    if ( rerender )
        renderView();
}


void
CollectionView::fetchCover() //SLOT
{
    #ifdef AMAZON_SUPPORT
    Item* item = static_cast<Item*>( currentItem() );
    if ( !item ) return;

    QString album = item->text(0);

    // find the first artist's name
    QStringList values =
        CollectionDB::instance()->query( QString (
            "SELECT DISTINCT artist.name FROM artist, album, tags "
            "WHERE artist.id = tags.artist AND tags.album = album.id "
            "AND album.name = '%1';" )
            .arg( CollectionDB::instance()->escapeString( album ) ) );

    if ( !values.isEmpty() )
        CollectionDB::instance()->fetchCover( this, values[0], album, false );
    #endif
}

void
CollectionView::showTrackInfo() //SLOT
{
     KURL::List urls = listSelected();
     int selectedTracksNumber = urls.count();

     //If we have only one, call the full dialog. Otherwise, the multiple tracks one.
     if (selectedTracksNumber == 1) {
          TagDialog* dialog = new TagDialog( urls.first() );
          dialog->show();
     }
     else if (selectedTracksNumber)  {
          TagDialog* dialog = new TagDialog( urls, instance() );
          dialog->show();
     }
}


//////////////////////////////////////////////////////////////////////////////////////////
// private
//////////////////////////////////////////////////////////////////////////////////////////

void
CollectionView::updateColumnHeader()
{
    if ( m_viewMode == modeFlatView )
    {
        // remove all columns
        for ( int i = columns() - 1; i >= 0 ; --i )
            removeColumn( i );

        addColumn( i18n( "Title" ) );
        addColumn( captionForCategory( m_cat1 ) );
        if( m_cat2 != CollectionBrowser::IdNone ) addColumn( captionForCategory( m_cat2 ) );
        if( m_cat3 != CollectionBrowser::IdNone ) addColumn( captionForCategory( m_cat3 ) );

        setResizeMode( QListView::AllColumns );
    }
    else
    {
        setColumnText( 0, captionForCategory( m_cat1 ) );
        int catArr[2] = {m_cat2, m_cat3};

        for(int i = 0; i < 2; i++) {
            if (catArr[i] != CollectionBrowser::IdNone ) {
                setColumnText( 0, columnText(0) + " / " + captionForCategory( catArr[i] ) );
            }
        }
    }

    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdArtist, m_cat1 == CollectionBrowser::IdArtist && m_cat2 == CollectionBrowser::IdNone );
    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdAlbum, m_cat1 == CollectionBrowser::IdAlbum && m_cat2 == CollectionBrowser::IdNone );
    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdArtistAlbum, m_cat1 == CollectionBrowser::IdArtist && m_cat2 == CollectionBrowser::IdAlbum && m_cat3 == CollectionBrowser::IdNone );
    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdArtistVisYearAlbum, m_cat1 == CollectionBrowser::IdArtist && m_cat2 == CollectionBrowser::IdVisYearAlbum && m_cat3 == CollectionBrowser::IdNone );
    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdGenreArtist, m_cat1 == CollectionBrowser::IdGenre && m_cat2 == CollectionBrowser::IdArtist && m_cat3 == CollectionBrowser::IdNone );
    m_parent->m_categoryMenu->setItemChecked( CollectionBrowser::IdGenreArtistAlbum, m_cat1 == CollectionBrowser::IdGenre && m_cat2 == CollectionBrowser::IdArtist && m_cat3 == CollectionBrowser::IdAlbum );
}


void
CollectionView::startDrag()
{
    KURLDrag* d = new KURLDrag( listSelected(), this );
    d->dragCopy();
}


KURL::List
CollectionView::listSelected()
{
    //Here we determine the URLs of all selected items. We use two passes, one for the parent items,
    //and another one for the children.

    KURL::List list;
    QListViewItem* item;
    QStringList values;
    QueryBuilder qb;

    // initalization for year - album mode
    QString tmptext;
    int VisYearAlbum = -1;
    int q_cat1=m_cat1;
    int q_cat2=m_cat2;
    int q_cat3=m_cat3;
    if (m_cat1 == CollectionBrowser::IdVisYearAlbum || m_cat2 == CollectionBrowser::IdVisYearAlbum || m_cat3 == CollectionBrowser::IdVisYearAlbum)
    {
        if (m_cat1==CollectionBrowser::IdVisYearAlbum)
        {
            VisYearAlbum = 1;
            q_cat1 = CollectionBrowser::IdAlbum;
        }
        if (m_cat2==CollectionBrowser::IdVisYearAlbum)
        {
            VisYearAlbum = 2;
            q_cat2 = CollectionBrowser::IdAlbum;
        }
        if (m_cat3==CollectionBrowser::IdVisYearAlbum)
        {
            VisYearAlbum = 3;
            q_cat3 = CollectionBrowser::IdAlbum;
        }
    }

    if ( m_viewMode == modeFlatView )
    {
        for ( item = firstChild(); item; item = item->nextSibling() )
            if ( item->isSelected() )
                list << static_cast<Item*>( item ) ->url();

        return list;
    }

    //first pass: parents
    for ( item = firstChild(); item; item = item->nextSibling() )
        if ( item->isSelected() )
        {
            bool sampler = item->text( 0 ) == i18n( "Various Artists" );
            qb.clear();
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );

            tmptext = item->text( 0 );
            QStringList matches( item->text( 0 ) );

            if( endsInThe( tmptext ) ) {
                manipulateThe( tmptext );
                matches << tmptext;
            }

            if ( !sampler )
            {
                if( VisYearAlbum == 1 )
                {
                    QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                    yearAlbumCalc( year, tmptext );
                    tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                    qb.addMatch( QueryBuilder::tabYear, year );
                    qb.addMatch( q_cat1, tmptext );
                }
                else
                    qb.addMatches( q_cat1, matches );
            }
            else
                qb.setOptions( QueryBuilder::optOnlyCompilations );

            setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );

            if( VisYearAlbum == 1 )
                qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

            if( !sampler )    qb.sortBy( q_cat1, QueryBuilder::valName );

            if( VisYearAlbum == 2 )
                qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

            if( q_cat2 != QueryBuilder::tabSong )
                qb.sortBy( q_cat2, QueryBuilder::valName );

            if( VisYearAlbum == 3 )
                qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

            if( q_cat3 != QueryBuilder::tabSong )
                qb.sortBy( q_cat3, QueryBuilder::valName );

            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valURL );

            qb.setOptions( QueryBuilder::optRemoveDuplicates );
            values = qb.run();

            for ( uint i = 0; i < values.count(); i++ )
            {
                KURL tmp;
                tmp.setPath( values[i] );
                list << tmp;
            }
        }

    //second pass: category 1
    if ( m_cat2 == CollectionBrowser::IdNone )
    {
        for ( item = firstChild(); item; item = item->nextSibling() )
            for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
                if ( child->isSelected() && !child->parent()->isSelected() )
                    list << static_cast<Item*>( child ) ->url();
    }
    else {
        for ( item = firstChild(); item; item = item->nextSibling() )
            for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
                if ( child->isSelected() && !child->parent()->isSelected() )
                {
                    bool sampler = item->text( 0 ) == i18n( "Various Artists" );
                    qb.clear();
                    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );

                    if ( !sampler )
                    {

                        tmptext = item->text( 0 );
                        QStringList matches( item->text( 0 ) );

                        if( endsInThe( tmptext ) ) {
                            manipulateThe( tmptext );
                            matches << tmptext;
                        }

                        if( VisYearAlbum == 1 )
                        {
                            QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                            yearAlbumCalc( year, tmptext );
                            tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                            qb.addMatch( QueryBuilder::tabYear, year );
                            qb.addMatch( q_cat1, tmptext );
                        }
                        else
                            qb.addMatches( q_cat1, matches );
                    }
                    else
                        qb.setOptions( QueryBuilder::optOnlyCompilations );


                    tmptext = child->text( 0 );
                    QStringList matches( child->text( 0 ) );

                    if( endsInThe( tmptext ) ) {
                        manipulateThe( tmptext );
                        matches << tmptext;
                    }

                    if( VisYearAlbum == 2 )
                    {
                        QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                        yearAlbumCalc( year, tmptext );
                        tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                        qb.addMatch( QueryBuilder::tabYear, year );
                        qb.addMatch( q_cat2, tmptext );
                    }
                    else
                        qb.addMatches( q_cat2, matches );

                    setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );

                    if( VisYearAlbum == 1 )
                        qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                    if( !sampler )
                        qb.sortBy( q_cat1, QueryBuilder::valName );

                    if( VisYearAlbum == 2 )
                        qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                    qb.sortBy( q_cat2, QueryBuilder::valName );

                    if( VisYearAlbum == 3 )
                        qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                    if( q_cat3 != QueryBuilder::tabSong )
                        qb.sortBy( q_cat3, QueryBuilder::valName );

                    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
                    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valURL );

                    qb.setOptions( QueryBuilder::optRemoveDuplicates );
                    values = qb.run();

                    for ( uint i = 0; i < values.count(); i++ )
                    {
                        KURL tmp;
                        tmp.setPath( values[i] );
                        list << tmp;
                    }
                }
    }

    //third pass: category 2
    for ( item = firstChild(); item; item = item->nextSibling() )
        for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
            for ( QListViewItem* grandChild = child->firstChild(); grandChild; grandChild = grandChild->nextSibling() )
                if ( grandChild->isSelected() && !grandChild->isExpandable() && !child->parent()->isSelected() && !child->isSelected() )
                    list << static_cast<Item*>( grandChild ) ->url();

    //category 3
    if ( m_cat3 == CollectionBrowser::IdNone )
    {
        for ( item = firstChild(); item; item = item->nextSibling() )
            for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
                for ( QListViewItem* grandChild = child->firstChild(); grandChild; grandChild = grandChild->nextSibling() )
                    for ( QListViewItem* grandChild2 = grandChild->firstChild(); grandChild2; grandChild2 = grandChild2->nextSibling() )
                        if ( grandChild2->isSelected() && !child->parent()->isSelected() && !child->isSelected() && !grandChild->isSelected() )
                            list << static_cast<Item*>( grandChild2 ) ->url();
    }
    else {
        for ( item = firstChild(); item; item = item->nextSibling() )
            for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
                for ( QListViewItem* grandChild = child->firstChild(); grandChild; grandChild = grandChild->nextSibling() )
                    if ( grandChild->isSelected() && !grandChild->parent()->isSelected() )
                    {
                        bool sampler = item->text( 0 ) == i18n( "Various Artists" );
                        qb.clear();
                        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );

                        if ( !sampler )
                        {

                            tmptext = item->text( 0 );
                            QStringList matches( item->text( 0 ) );

                            if( endsInThe( tmptext ) ) {
                                manipulateThe( tmptext );
                                matches << tmptext;
                            }

                            if( VisYearAlbum == 1 )
                            {
                                QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                                yearAlbumCalc( year, tmptext );
                                tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                                qb.addMatch( QueryBuilder::tabYear, year );
                                qb.addMatch( q_cat1, tmptext );
                            }
                            else
                                qb.addMatches( q_cat1, matches );
                        }
                        else
                            qb.setOptions( QueryBuilder::optOnlyCompilations );

                        tmptext = child->text( 0 );
                        QStringList matches( child->text( 0 ) );

                        if( endsInThe( tmptext ) ) {
                            manipulateThe( tmptext );
                            matches << tmptext;
                        }

                        if( VisYearAlbum == 2 )
                        {
                            QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                            yearAlbumCalc( year, tmptext );
                            tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                            qb.addMatch( QueryBuilder::tabYear, year );
                            qb.addMatch( q_cat2, tmptext );
                        }
                        else
                            qb.addMatches( q_cat2, matches );

                        matches.clear();
                        tmptext = grandChild->text( 0 );
                        matches << grandChild->text( 0 );

                        if( endsInThe( tmptext ) ) {
                            manipulateThe( tmptext );
                            matches << tmptext;
                        }

                        if( VisYearAlbum == 3 )
                        {
                            QString year = tmptext.left( tmptext.find( i18n(" - ") ) );
                            yearAlbumCalc( year, tmptext );
                            tmptext = tmptext.right(tmptext.find(i18n(" - ")));
                            qb.addMatch( QueryBuilder::tabYear, year );
                            qb.addMatch( q_cat3, tmptext );
                        }
                        else {
                            matches.clear();
                            matches << grandChild->text( 0 ) << tmptext;

                            qb.addMatches( q_cat3, matches );
                        }
                        setQBFilters( qb, m_filter, q_cat1 | q_cat2 | q_cat3 | QueryBuilder::tabSong );

                        if( VisYearAlbum == 1 )
                            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                        if( !sampler )
                            qb.sortBy( q_cat1, QueryBuilder::valName );

                        if( VisYearAlbum == 2 )
                            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                        qb.sortBy( q_cat2, QueryBuilder::valName );

                        if( VisYearAlbum == 3 )
                            qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName);

                        qb.sortBy( q_cat3, QueryBuilder::valName );
                        qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
                        qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valURL );

                        qb.setOptions( QueryBuilder::optRemoveDuplicates );
                        values = qb.run();

                        for ( uint i = 0; i < values.count(); i++ )
                        {
                            KURL tmp;
                            tmp.setPath( values[i] );
                            list << tmp;
                        }
                    }
    }

    //category 3
    for ( item = firstChild(); item; item = item->nextSibling() )
            for ( QListViewItem* child = item->firstChild(); child; child = child->nextSibling() )
                for ( QListViewItem* grandChild = child->firstChild(); grandChild; grandChild = grandChild->nextSibling() )
                    for ( QListViewItem* grandChild2 = grandChild->firstChild(); grandChild2; grandChild2 = grandChild2->nextSibling() )
                        if ( grandChild2->isSelected() && !child->parent()->isSelected() && !child->isSelected() && !grandChild->isSelected() )
                            list << static_cast<Item*>( grandChild2 ) ->url();

    return list;
}

void
CollectionView::playlistFromURLs( const KURL::List &urls )
{
    const QString path = PlaylistDialog::getSaveFileName( i18n( "Untitled" ) );

    if( path.isEmpty() )
        return;

    CollectionDB* db = CollectionDB::instance();

    QValueList<QString> titles;
    QValueList<QString> seconds;
    for( KURL::List::ConstIterator it = urls.constBegin(), end = urls.constEnd(); it != end; ++it )
    {
        const QString query = QString("SELECT title, length FROM tags WHERE url = '%1';")
                              .arg( db->escapeString( (*it).path() ) ); //no operator->, how suck!
        QStringList result = db->query( query );
        titles << result[0];
        seconds << result[1];
    }

    if( PlaylistBrowser::savePlaylist( path, urls, titles, seconds ) )
        PlaylistWindow::self()->showBrowser( "PlaylistBrowser" );
}

QPixmap
CollectionView::iconForCategory( const int cat ) const
{
    QString icon;
    switch( cat )
    {
        case CollectionBrowser::IdAlbum:
            icon = "cdrom_unmount";
            break;
        case CollectionBrowser::IdVisYearAlbum:
            icon = "cdrom_unmount";
            break;
        case CollectionBrowser::IdArtist:
            icon = "personal";
            break;

        case CollectionBrowser::IdGenre:
            icon = "kfm";
            break;

        case CollectionBrowser::IdYear:
            icon = "history";
            break;
    }

    return KGlobal::iconLoader()->loadIcon( icon, KIcon::Toolbar, KIcon::SizeSmall );
}


QString
CollectionView::captionForCategory( const int cat ) const
{
    switch( cat )
    {
        case CollectionBrowser::IdAlbum:
            return i18n( "Album" );
            break;
        case CollectionBrowser::IdVisYearAlbum:
            return i18n( "Year" ) + i18n( " - " ) + i18n( "Album" );
            break;
        case CollectionBrowser::IdArtist:
            return i18n( "Artist" );
            break;

        case CollectionBrowser::IdGenre:
            return i18n( "Genre" );
            break;

        case CollectionBrowser::IdYear:
            return i18n( "Year" );
            break;
    }

    return QString::null;
}

void
CollectionView::cacheView()
{
    //free cache
    m_cacheOpenItemPaths.clear();
    m_cacheViewportTopItem = QString::null;

    m_cacheCurrentItem = currentItem() ? currentItem()->text( 0 ) : QString::null;

    //cache expanded/open items
    if ( m_viewMode == modeTreeView ) {
        QListViewItemIterator it( this );
        while ( it.current() ) {
            QListViewItem *item = it.current();
            if ( item->isOpen() ) {
                //construct path to item
                QStringList itemPath;
                for( const QListViewItem *i = item; i; i = i->parent() )
                    itemPath.prepend( i->text( 0 ) );

                m_cacheOpenItemPaths.append ( itemPath );
            }
            ++it;
        }
    }

    //cache viewport's top item
    QListViewItem* item = itemAt( QPoint(0, 0) );
    if ( item )
        m_cacheViewportTopItem = item->text(0);
}


void
CollectionView::restoreView()
{
    //expand cached items
    if ( m_viewMode == modeTreeView ) {
        QValueList<QStringList>::const_iterator it;
        for ( it = m_cacheOpenItemPaths.begin(); it != m_cacheOpenItemPaths.end(); ++it ) {
            QListViewItem* item = findItem( (*it)[0], 0 );
            if ( item )
                item->setOpen ( true );

            for ( uint i = 1; i < (*it).count() && item; ++i ) {
                item = item->firstChild();
                while ( item ) {
                    if ( item->text(0) == (*it)[ i ] )
                        item->setOpen ( true );
                    item = item->nextSibling();
                }
            }
        }
    }

    //scroll to previous viewport top item
    QListViewItem* item = findItem( m_cacheViewportTopItem, 0 );
    if ( item )
        setContentsPos( 0, itemPos(item) );

    item = findItem( m_cacheCurrentItem, 0 );
    if ( item ) {
        setCurrentItem( item );
        item->setSelected( true );
    }

    //free cache
    m_cacheOpenItemPaths.clear();
    m_cacheViewportTopItem = QString::null;
    m_cacheCurrentItem = QString::null;
}


// Small function aimed to convert Eagles, The -> The Eagles
void
CollectionView::manipulateThe( QString &str, bool reverse )
{
    if( reverse )
    {
        QString begin = str.left( 3 );
        str = str.append( ", %1" ).arg( begin );
        str = str.mid( 4 );
        return;
    }

    if( !endsInThe( str ) )
        return;

    QString end = str.right( 3 );
    str = str.prepend( "%1 " ).arg( end );

    uint newLen = str.length() - end.length() - 2;

    str.truncate( newLen );
}

bool
CollectionView::endsInThe( const QString & text )
{
    return text.endsWith( ", the", false );
}

// avoid code duplication
void
CollectionView::yearAlbumCalc( QString &year, QString &text )
{
    if( year == "?" )
        year = "";

    text = text.right( text.length() -
                       text.find( i18n(" - ") ) -
                       i18n(" - ").length() );
}


void
CollectionView::setQBFilters( QueryBuilder &qb, QString query, const int &defaults )
{
    if( query.contains( "\"" ) % 2 == 1 ) query += "\""; //make an even number of "s

    //something like thingy"bla"stuff -> thingy "bla" stuff
    bool odd = false;
    for( int pos = query.find( "\"" );
         pos >= 0 && pos <= (int)query.length();
         pos = query.find( "\"", pos + 1 ) )
    {
        query = query.insert( odd ? ++pos : pos++, " " );
        odd = !odd;
    }
    query = query.simplifyWhiteSpace();

    int x; //position in string of the end of the next element
    bool OR = false, minus = false; //whether the next element is to be OR, and/or negated
    QString tmp, s = "", field = ""; //the current element, a tempstring, and the field: of the next element
    QStringList tmpl; //list of elements of which at least one has to match (OR)
    QValueList<QStringList> allof; //list of all the tmpls, of which all have to match
    while( !query.isEmpty() )  //seperate query into parts which all have to match
    {
        if( query.startsWith( " " ) )
            query = query.mid( 1 ); //cuts off the first character
        if( query.startsWith( "\"" ) ) //take stuff in "s literally (basically just ends up ignoring spaces)
        {
            query = query.mid( 1 );
            x = query.find( "\"" );
        }
        else
            x = query.find( " " );
        if( x < 0 )
            x = query.length();
        s = query.left( x ); //get the element
        query = query.mid( x + 1 ); //move on

        if( !field.isEmpty() || ( s != "-" && s != "AND" && s != "OR" &&
                                  !s.endsWith( ":" ) && !s.endsWith( ":>" ) && !s.endsWith( ":<" ) ) )
        {
            if( !OR && !tmpl.isEmpty() ) //add the OR list to the AND list
            {
                allof += tmpl;
                tmpl.clear();
            }
            else
                OR = false;
            tmp = field + s;
            if( minus )
            {
                tmp = "-" + tmp;
                minus = false;
            }
            tmpl += tmp;
            tmp = field = "";
        }
        else if( s.endsWith( ":" ) || s.endsWith( ":>" ) || s.endsWith( ":<" ) )
            field = s;
        else if( s == "OR" )
            OR = true;
        else if( s == "-" )
            minus = true;
        else
            OR = false;
    }
    if( !tmpl.isEmpty() )
        allof += tmpl;

    const uint allofcount = allof.count();
    for( uint i = 0; i < allofcount; ++i ) //check each part for matchiness
    {
        qb.beginOR();
        uint count = allof[i].count();
        for( uint ii = 0; ii < count; ++ii )
        {
            field = QString::null;
            s = allof[i][ii];
            bool neg = s.startsWith( "-" );
            if ( neg )
                s = s.mid( 1 ); //cut off the -
            x = s.find( ":" ); //where the field ends and the thing-to-match begins
            if( x > 0 )
            {
                field = s.left( x ).lower();
                s = s.mid( x + 1 );
            }

            int mode = QueryBuilder::modeNormal;
            if( !field.isEmpty() && s.startsWith( ">" ) )
            {
                s = s.mid( 1 );
                mode = QueryBuilder::modeGreater;
            }
            else if( !field.isEmpty() && s.startsWith( "<" ) )
            {
                s = s.mid( 1 );
                mode = QueryBuilder::modeLess;
            }

            int table = -1, value = -1;
            if( field == "artist" )
                table = QueryBuilder::tabArtist;
            else if( field == "album" )
                table = QueryBuilder::tabAlbum;
            else if( field == "title" )
                table = QueryBuilder::tabSong;
            else if( field == "genre" )
                table = QueryBuilder::tabGenre;
            else if( field == "year" )
                table = QueryBuilder::tabYear;
            else if( field == "score" )
            {
                table = QueryBuilder::tabStats;
                value = QueryBuilder::valScore;
            }
            else if( field == "directory" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valDirectory;
            }
            else if( field == "length" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valLength;
            }
            else if( field == "playcount" )
            {
                table = QueryBuilder::tabStats;
                value = QueryBuilder::valPlayCounter;
            }
            else if( field == "samplerate" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valSamplerate;
            }
            else if( field == "track" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valTrack;
            }
            else if( field == "filename" || field == "url" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valURL;
            }
            else if( field == "bitrate" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valBitrate;
            }
            else if( field == "comment" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valComment;
            }
            else if( field == "lyrics" )
            {
                table = QueryBuilder::tabSong;
                value = QueryBuilder::valLyrics;
            }

            if( neg )
            {
                if( value >= 0 )
                    qb.excludeFilter( table, value, s, mode );
                else
                    qb.excludeFilter( table >= 0 ? table : defaults, s );
            }
            else
            {
                if( value >= 0 )
                    qb.addFilter( table, value, s, mode );
                else
                    qb.addFilter( table >= 0 ? table : defaults, s );
            }
        }
        qb.endOR();
    }
}


void
CollectionView::viewportPaintEvent( QPaintEvent *e )
{
    KListView::viewportPaintEvent( e );

    // Superimpose bubble help for Flat-View mode:

    if ( m_viewMode == modeFlatView && childCount() == 0 )
    {
        QPainter p( viewport() );

        QSimpleRichText t( i18n(
                "<div align=center>"
                  "<h3>Flat-View Mode</h3>"
                    "To enable the Flat-View mode, please enter search terms in the filter line above."
                "</div>" ), QApplication::font() );

        t.setWidth( width() - 50 );

        const uint w = t.width() + 20;
        const uint h = t.height() + 20;

        p.setBrush( colorGroup().background() );
        p.drawRoundRect( 15, 15, w, h, (8*200)/w, (8*200)/h );
        t.draw( &p, 20, 20, QRect(), colorGroup() );
    }
}


void
CollectionView::viewportResizeEvent( QResizeEvent* )
{
    // Needed for correct redraw of bubble help
    triggerUpdate();
}


#include "collectionbrowser.moc"
