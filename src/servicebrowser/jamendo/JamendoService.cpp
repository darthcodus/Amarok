/***************************************************************************
 *   Copyright (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "JamendoService.h"

#include "CollectionManager.h"
#include "Debug.h"
#include "EngineController.h"
#include "JamendoInfoParser.h"
#include "JamendoXmlParser.h"
#include "ServiceSqlRegistry.h"
#include "TheInstances.h"

#include <KTemporaryFile>
#include <KRun>
#include <KShell>
#include <threadweaver/ThreadWeaver.h>
#include <KMenuBar>

#include <QAction>
#include <typeinfo>

using namespace Meta;

AMAROK_EXPORT_PLUGIN( JamendoServiceFactory )

void JamendoServiceFactory::init()
{
    ServiceBase* service = new JamendoService( "Jamendo.com" );
    m_activeServices << service;
    emit newService( service );
}


QString JamendoServiceFactory::name()
{
    return "Jamendo.com";
}

KPluginInfo JamendoServiceFactory::info()
{
    KPluginInfo pluginInfo(  "amarok_service_jamendo.desktop", "services" );
    pluginInfo.setConfig( config() );
    return pluginInfo;
}


KConfigGroup JamendoServiceFactory::config()
{
    return Amarok::config( "Service_Jamendo" );
}


JamendoService::JamendoService(const QString & name)
 : ServiceBase( name )
 , m_currentAlbum( 0 )
{

    setShortDescription(  i18n( "A site where artists can showcase their creations to the world" ) );
    setIcon( KIcon( "view-services-jamendo-amarok" ) );

    ServiceMetaFactory * metaFactory = new JamendoMetaFactory( "jamendo", this );
    ServiceSqlRegistry * registry = new ServiceSqlRegistry( metaFactory );
    m_collection = new ServiceSqlCollection( "jamendo", "Jamendo.com", metaFactory, registry );

}


JamendoService::~JamendoService()
{
}

void JamendoService::polish()
{

    generateWidgetInfo();
    if ( m_polished ) return;

    KHBox * bottomPanelLayout = new KHBox;
    bottomPanelLayout->setParent( m_bottomPanel );

    m_updateListButton = new QPushButton;
    m_updateListButton->setParent( bottomPanelLayout );
    m_updateListButton->setText( i18nc( "Fetch new information from the website", "Update" ) );
    m_updateListButton->setObjectName( "updateButton" );
    m_updateListButton->setIcon( KIcon( "view-refresh-amarok" ) );


    m_downloadButton = new QPushButton;
    m_downloadButton->setParent( bottomPanelLayout );
    m_downloadButton->setText( i18n( "Download" ) );
    m_downloadButton->setObjectName( "downloadButton" );
    m_downloadButton->setIcon( KIcon( "get-hot-new-stuff-amarok" ) );

    m_downloadButton->setEnabled( false );

    connect( m_updateListButton, SIGNAL( clicked() ), this, SLOT( updateButtonClicked() ) );
    connect( m_downloadButton, SIGNAL( clicked() ), this, SLOT( download() ) );

    setInfoParser( new JamendoInfoParser() );

    //m_model = new DatabaseDrivenContentModel();
    //m_dbHandler = new JamendoDatabaseHandler();
    //m_model->setDbHandler( m_dbHandler );

    QList<int> levels;
    //levels << CategoryId::Artist << CategoryId::Album << CategoryId::None;
    levels << CategoryId::Genre << CategoryId::Artist << CategoryId::Album;


    setModel( new SingleCollectionTreeItemModel( m_collection, levels ) );

    connect( m_contentView, SIGNAL( itemSelected( CollectionTreeItem * ) ), this, SLOT( itemSelected( CollectionTreeItem * ) ) );

    QAction *action = new QAction( i18n("Artist"), m_menubar );
    connect( action, SIGNAL(triggered(bool)), SLOT(sortByArtist() ) );
    m_filterMenu->addAction( action );

    action = new QAction( i18n( "Artist / Album" ), m_menubar );
    connect( action, SIGNAL(triggered(bool)), SLOT(sortByArtistAlbum() ) );
    m_filterMenu->addAction( action );

    action = new QAction( i18n( "Album" ), m_menubar );
    connect( action, SIGNAL(triggered(bool)), SLOT( sortByAlbum() ) );
    m_filterMenu->addAction( action );

    action = new QAction( i18n( "Genre / Artist" ), m_menubar );
    connect( action, SIGNAL(triggered(bool)), SLOT( sortByGenreArtist() ) );
    m_filterMenu->addAction( action );

    action = new QAction( i18n( "Genre / Artist / Album" ), m_menubar );
    connect( action, SIGNAL(triggered(bool)), SLOT(sortByGenreArtistAlbum() ) );
    m_filterMenu->addAction( action );

    m_menubar->show();


    m_polished = true;



}

void JamendoService::updateButtonClicked()
{
    m_updateListButton->setEnabled( false );

    debug() << "JamendoService: start downloading xml file";

    KTemporaryFile tempFile;
    tempFile.setSuffix( ".gz" );
    tempFile.setAutoRemove( false );  //file will be removed in JamendoXmlParser
    if( !tempFile.open() )
    {
        return; //error
    }

    m_tempFileName = tempFile.fileName();
    m_listDownloadJob = KIO::file_copy( KUrl( "http://img.jamendo.com/data/dbdump.en.xml.gz" ), KUrl( m_tempFileName ), 0700 , KIO::HideProgressInfo | KIO::Overwrite );
    Amarok::ContextStatusBar::instance() ->newProgressOperation( m_listDownloadJob )
    .setDescription( i18n( "Downloading Jamendo.com Database" ) )
    .setAbortSlot( this, SLOT( listDownloadCancelled() ) );

    connect( m_listDownloadJob, SIGNAL( result( KJob * ) ),
            this, SLOT( listDownloadComplete( KJob * ) ) );


  /* KIO::StoredTransferJob * job =  KIO::storedGet(  KUrl( "http://img.jamendo.com/data/dbdump.en.xml.gz" ) );
    Amarok::ContextStatusBar::instance() ->newProgressOperation( job )
    .setDescription( i18n( "Downloading Jamendo.com Database" ) )
    .setAbortSlot( this, SLOT( listDownloadCancelled() ) );
*/
    //return true;
}

void JamendoService::listDownloadComplete(KJob * downloadJob)
{


    if ( downloadJob != m_listDownloadJob )
        return ; //not the right job, so let's ignore it
    debug() << "JamendoService: xml file download complete";


    //testing



    if ( !downloadJob->error() == 0 )
    {
        //TODO: error handling here
        return ;
    }

    //system( "gzip -df /tmp/dbdump.en.xml.gz" ); //FIXME!!!!!!!!!

    Amarok::ContextStatusBar::instance()->shortMessage( i18n( "Updating the local Jamendo database."  ) );
    debug() << "JamendoService: create xml parser";
    JamendoXmlParser * parser = new JamendoXmlParser( m_tempFileName );
    connect( parser, SIGNAL( doneParsing() ), SLOT( doneParsing() ) );

    ThreadWeaver::Weaver::instance()->enqueue( parser );
    downloadJob->deleteLater();
    m_listDownloadJob = 0;

}

void JamendoService::listDownloadCancelled()
{

    Amarok::ContextStatusBar::instance()->endProgressOperation( m_listDownloadJob );
    m_listDownloadJob->kill();
    delete m_listDownloadJob;
    m_listDownloadJob = 0;
    debug() << "Aborted xml download";

    m_updateListButton->setEnabled( true );
}

void JamendoService::doneParsing()
{
    debug() << "JamendoService: done parsing";
    m_updateListButton->setEnabled( true );
    // getModel->setGenre("All");
    //delete sender
    sender()->deleteLater();
    m_collection->emitUpdated();
}

void JamendoService::itemSelected( CollectionTreeItem * selectedItem ){

    DEBUG_BLOCK

    //we only enable the download button if there is only one item selected and it happens to
    //be an album or a track
    DataPtr dataPtr = selectedItem->data();

    if ( typeid( * dataPtr.data() ) == typeid( JamendoTrack ) )  {

        debug() << "is right type (track)";
        JamendoTrack * track = static_cast<JamendoTrack *> ( dataPtr.data() );
        m_currentAlbum = static_cast<JamendoAlbum *> ( track->album().data() );
        m_downloadButton->setEnabled( true );

    } else if ( typeid( * dataPtr.data() ) == typeid( JamendoAlbum ) ) {

        m_currentAlbum = static_cast<JamendoAlbum *> ( dataPtr.data() );
        debug() << "is right type (album) named " << m_currentAlbum->name();

        m_downloadButton->setEnabled( true );
    } else {

        debug() << "is wrong type";
        m_downloadButton->setEnabled( false );

    }

    return;
}

void JamendoService::download()
{

    if ( m_currentAlbum ){
        download( m_currentAlbum );
    }

}

void JamendoService::download( JamendoAlbum * album )
{

    if ( !m_polished )
        polish();

    m_downloadButton->setEnabled( false );

    KTemporaryFile tempFile;
    tempFile.setSuffix( ".torrent" );
    tempFile.setAutoRemove( false );
    if( !tempFile.open() )
    {
        return;
    }


    m_torrentFileName = tempFile.fileName();
    m_torrentDownloadJob = KIO::file_copy( KUrl( album->oggTorrentUrl() ), KUrl( m_torrentFileName ), 0774 , KIO::Overwrite );
    connect( m_torrentDownloadJob, SIGNAL( result( KJob * ) ),
             this, SLOT( torrentDownloadComplete( KJob * ) ) );
}

void JamendoService::torrentDownloadComplete(KJob * downloadJob)
{


    if ( downloadJob != m_torrentDownloadJob )
        return ; //not the right job, so let's ignore it

    if ( !downloadJob->error() == 0 )
    {
        //TODO: error handling here
        return ;
    }

    debug() << "Torrent downloaded";

    KRun::runUrl( KShell::quoteArg( m_torrentFileName ), "application/x-bittorrent", 0, true );

    downloadJob->deleteLater();
    m_torrentDownloadJob = 0;
}


void JamendoService::downloadCurrentTrackAlbum()
{
        //get current track
    Meta::TrackPtr track = The::engineController()->currentTrack();

    //check if this is indeed a Jamendo track
    Meta::SourceInfoCapability *sic = track->as<Meta::SourceInfoCapability>();
    if( sic )
    {
        //is the source defined
        QString source = sic->sourceName();
        if ( source != "Jamendo.com" ) {
            //not a Jamendo track, so don't bother...
            delete sic;
            return;
        }
        delete sic;
    } else {
        //not a Jamendo track, so don't bother...
        return;
    }

    //so far so good...
    //now the casting begins:

    JamendoTrack * jamendoTrack = dynamic_cast<JamendoTrack *> ( track.data() );
    if ( !jamendoTrack )
        return;

    JamendoAlbum * jamendoAlbum = dynamic_cast<JamendoAlbum *> ( jamendoTrack->album().data() );
    if ( !jamendoAlbum )
        return;

    download( jamendoAlbum );
}




#include "JamendoService.moc"
