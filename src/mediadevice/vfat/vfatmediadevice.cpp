/****************************************************************************
 * copyright            :(C) 2005 Jeff Mitchell <kde-dev@emailgoeshere.com> *
 *                       (C) 2005 Seb Ruiz <me@sebruiz.net>                 *
 *                                                                          *
 * With some code helpers from KIO_VFAT                                     *
 *                        (c) 2004 Thomas Loeber <vfat@loeber1.de>          *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#define DEBUG_PREFIX "VfatMediaDevice"

#include "vfatmediadevice.h"

AMAROK_EXPORT_PLUGIN( VfatMediaDevice )

#include "debug.h"
#include "medium.h"
#include "metabundle.h"
#include "collectiondb.h"
#include "collectionbrowser.h"
#include "k3bexporter.h"
#include "playlist.h"
#include "statusbar/statusbar.h"
#include "transferdialog.h"

#include <kapplication.h>
#include <kconfig.h>           //download saveLocation
#include <kdiskfreesp.h>
#include <kiconloader.h>       //smallIcon
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kpopupmenu.h>
#include <kurlrequester.h>     //downloadSelectedItems()
#include <kurlrequesterdlg.h>  //downloadSelectedItems()

#include <taglib/audioproperties.h>

#include <unistd.h>            //usleep()

#include <qcstring.h>
#include <qfile.h>
#include <qstringx.h>

namespace amaroK {
    extern KConfig *config( const QString& );
    extern QString cleanPath( const QString&, bool );
}

typedef QPtrList<VfatMediaFile> MediaFileList;

/**
 * VfatMediaItem Class
 */

class VfatMediaItem : public MediaItem
{
    public:
        VfatMediaItem( QListView *parent, QListViewItem *after = 0 )
            : MediaItem( parent, after )
        { }

        VfatMediaItem( QListViewItem *parent, QListViewItem *after = 0 )
            : MediaItem( parent, after )
        { }

        void
        setEncodedName( QString &name )
        {
            m_encodedName = QFile::encodeName( name );
        }

        void
        setEncodedName( QCString &name ) { m_encodedName = name; }

        QCString
        encodedName() { return m_encodedName; }

        // List directories first, always
        int
        compare( QListViewItem *i, int col, bool ascending ) const
        {
            #define i static_cast<VfatMediaItem *>(i)
            switch( type() )
            {
                case MediaItem::DIRECTORY:
                    if( i->type() == MediaItem::DIRECTORY )
                        break;
                    return -1;

                default:
                    if( i->type() == MediaItem::DIRECTORY )
                        return 1;
            }
            #undef i

            return MediaItem::compare(i, col, ascending);
        }

    private:
        bool     m_dir;
        QCString m_encodedName;
};

class VfatMediaFile
{
    public:
        VfatMediaFile( VfatMediaFile *parent, QString basename, VfatMediaDevice *device )
        : m_parent( parent )
        , m_device( device )
        {
            DEBUG_BLOCK
            m_listed = false;
            setNamesFromBase( basename );
            m_children = new MediaFileList();
            debug() << "m_fullName is " << m_fullName << endl;

            if( m_device->getFileMap()[m_fullName] )
            {
                debug() << "Trying to create two VfatMediaFile items with same fullName!" << endl;
                debug() << "name already existing: " << m_device->getFileMap()[m_fullName]->getFullName() << endl;
                return;
            }
            else
            {
                m_device->getFileMap()[m_fullName] = this;
            }

            if( m_parent )
            {
                m_viewItem = new VfatMediaItem( m_parent->getViewItem() );
                m_viewItem->setText( 0, m_baseName );
                m_parent->getChildren()->append( this );
                m_device->getItemMap()[m_viewItem] = this;
            }
            else
            {
                m_viewItem = new VfatMediaItem( m_device->view() );
                m_viewItem->setText( 0, m_fullName );
                m_device->getItemMap()[m_viewItem] = this;
            }

            m_viewItem->setBundle( new MetaBundle( KURL::fromPathOrURL( m_fullName ), true, TagLib::AudioProperties::Fast ) );

        }

        ~VfatMediaFile()
        {
            if( m_parent )
                m_parent->removeChild( this );
            m_device->getItemMap().erase( m_viewItem );
            m_device->getFileMap().erase( m_fullName );
            delete m_children;
            delete m_viewItem;
        }

        VfatMediaFile*
        getParent() { return m_parent; }

        void
        setParent( VfatMediaFile* parent )
        {
            m_device->getFileMap().erase( m_fullName );
            m_parent->getChildren()->remove( this );
            m_parent = parent;
            if( m_parent )
                m_parent->getChildren()->append( this );
            setNamesFromBase( m_baseName );
            m_device->getFileMap()[m_fullName] = this;
        }

        void
        removeChild( VfatMediaFile* childToDelete ) { m_children->remove( childToDelete ); }

        VfatMediaItem*
        getViewItem() { return m_viewItem; }

        bool
        getListed() { return m_listed; }

        void
        setListed( bool listed ) { m_listed = listed; }

        QString
        getFullName() { return m_fullName; }

        QCString
        getEncodedFullName() { return m_encodedFullName; }

        QString
        getBaseName() { return m_baseName; }

        QString
        getEncodedBaseName() { return m_encodedBaseName; }

        //always follow this function with setNamesFromBase()
        void
        setBaseName( QString &name ) { m_baseName = name; }

        void
        setNamesFromBase( const QString &name = QString::null )
        {
            if( name != QString::null )
                m_baseName = name;
            if( m_parent )
                m_fullName = m_parent->getFullName() + '/' + m_baseName;
            else
                m_fullName = m_baseName;
            m_encodedFullName = QFile::encodeName( m_fullName );
            m_encodedBaseName = QFile::encodeName( m_baseName );
        }

        MediaFileList*
        getChildren() { return m_children; }

        void
        deleteAllChildren()
        {
            VfatMediaFile *vmf;
            if( m_children && !m_children->isEmpty() )
            {
                for( vmf = m_children->first(); vmf; vmf = m_children->next() )
                {
                    vmf->deleteAll();
                    m_children->remove( vmf );
                }
            }
        }

        void
        deleteAll()
        {
            VfatMediaFile *vmf;
            debug() << "m_children is " << m_children << endl;
            if( m_children && !m_children->isEmpty() )
            {
                for( vmf = m_children->first(); vmf; vmf = m_children->next() )
                {
                    vmf->deleteAll();
                    m_children->remove( vmf );
                }
            }
            delete this;
        }

    private:
        QString m_fullName;
        QCString m_encodedFullName;
        QString m_baseName;
        QCString m_encodedBaseName;
        VfatMediaFile *m_parent;
        MediaFileList *m_children;
        VfatMediaItem *m_viewItem;
        VfatMediaDevice* m_device;
        bool m_listed;
};

QString
VfatMediaDevice::fileName( const MetaBundle &bundle )
{
    QString result = cleanPath( bundle.artist() );

    if( !result.isEmpty() )
    {
        if( m_spacesToUnderscores )
            result += "_-_";
        else
            result += " - ";
    }

    if( bundle.track() )
    {
        result.sprintf( "%02d", bundle.track() );

        if( m_spacesToUnderscores )
            result += "_";
        else
            result += " ";
    }

    result += cleanPath( bundle.title() + "." + bundle.type() );

    return result;
}


/**
 * VfatMediaDevice Class
 */

VfatMediaDevice::VfatMediaDevice()
    : MediaDevice()
    , m_tmpParent( 0 )
    , m_kBSize( 0 )
    , m_kBAvail( 0 )
    , m_connected( false )
{
    DEBUG_BLOCK
    m_name = "VFAT Device";
    m_td = NULL;
    m_dirLister = new KDirLister();
    m_dirLister->setNameFilter( "*.mp3 *.wav *.asf *.flac *.wma *.ogg *.aac *.m4a" );
    m_dirLister->setAutoUpdate( false );
    m_spacesToUnderscores = false;
    m_isInCopyTrack = false;
    m_stopDirLister = false;
    m_firstSort = "None";
    m_secondSort = "None";
    m_thirdSort = "None";
    connect( m_dirLister, SIGNAL( newItems(const KFileItemList &) ), this, SLOT( newItems(const KFileItemList &) ) );
    connect( m_dirLister, SIGNAL( completed() ), this, SLOT( dirListerCompleted() ) );
    connect( m_dirLister, SIGNAL( clear() ), this, SLOT( dirListerClear() ) );
    connect( m_dirLister, SIGNAL( clear(const KURL &) ), this, SLOT( dirListerClear(const KURL &) ) );
    connect( m_dirLister, SIGNAL( deleteItem(KFileItem *) ), this, SLOT( dirListerDeleteItem(KFileItem *) ) );
}

void
VfatMediaDevice::init( MediaBrowser* parent )
{
    MediaDevice::init( parent );
}

VfatMediaDevice::~VfatMediaDevice()
{
    closeDevice();
}

void
VfatMediaDevice::loadConfig()
{
    MediaDevice::loadConfig();

    m_spacesToUnderscores = configBool("spacesToUnderscores");
    m_firstSort = configString( "firstGrouping", "None" );
    m_secondSort = configString( "secondGrouping", "None" );
    m_thirdSort = configString( "thirdGrouping", "None" );
}

bool
VfatMediaDevice::openDevice( bool /*silent*/ )
{
    DEBUG_BLOCK
    if( !m_medium )
    {
        debug() << "VFAT openDevice:  no Medium present!" << endl;
        return false;
    }
    if( !m_medium->mountPoint() )
    {
        amaroK::StatusBar::instance()->longMessage( i18n( "Devices handled by this plugin must be mounted first.\n"
                                                          "Please mount the device and click \"Connect\" again." ),
                                                    KDE::StatusBar::Sorry );
        return false;
    }
    m_actuallyVfat = m_medium->fsType() == "vfat" ? true : false;
    m_connected = true;
    m_transferDir = m_medium->mountPoint();
    m_initialFile = new VfatMediaFile( 0, m_medium->mountPoint(), this );
    listDir( m_medium->mountPoint() );
    connect( this, SIGNAL( startTransfer() ), MediaBrowser::instance(), SLOT( transferClicked() ) );
    return true;
}

bool
VfatMediaDevice::closeDevice()  //SLOT
{
    if( m_connected )
    {
        m_initialFile->deleteAll();
        m_view->clear();
        m_connected = false;

    }
    //delete these?
    m_mfm.clear();
    m_mim.clear();
    return true;
}

void
VfatMediaDevice::runTransferDialog()
{
    m_td = new TransferDialog( this );
    m_td->exec();
}

/// Renaming

void
VfatMediaDevice::renameItem( QListViewItem *item ) // SLOT
{

    if( !item )
        return;

    #define item static_cast<VfatMediaItem*>(item)

    QCString src = m_mim[item]->getEncodedFullName();
    QCString dst = m_mim[item]->getParent()->getEncodedFullName() + '/' + QFile::encodeName( item->text(0) );

    debug() << "Renaming: " << src << " to: " << dst << endl;

    //do we want a progress dialog?  If so, set last false to true
    if( KIO::NetAccess::file_move( KURL::fromPathOrURL(src), KURL::fromPathOrURL(dst), -1, false, false, false ) )
    {
        m_mfm.erase( m_mim[item]->getFullName() );
        m_mim[item]->setNamesFromBase( item->text(0) );
        m_mfm[m_mim[item]->getFullName()] = m_mim[item];
    }
    else
    {
        debug() << "Renaming FAILED!" << endl;
        //failed, so set the item's text back to how it should be
        item->setText( 0, m_mim[item]->getBaseName() );
    }

    #undef item

}

/// Creating a directory

MediaItem *
VfatMediaDevice::newDirectory( const QString &name, MediaItem *parent )
{
    DEBUG_BLOCK
    if( !m_connected || name.isEmpty() ) return 0;

    debug() << "newDirectory called with name = " << name << ", and parent = " << parent << endl;

    #define parent static_cast<VfatMediaItem*>(parent)

    QString fullName = m_mim[parent]->getFullName();
    QString cleanedName = cleanPath(name);
    QString fullPath = fullName + '/' + cleanedName;
    QCString dirPath = QFile::encodeName( fullPath );
    debug() << "Creating directory: " << dirPath << endl;
    const KURL url( dirPath );

    if( ! KIO::NetAccess::mkdir( url, m_parent ) ) //failed
    {
        debug() << "Failed to create directory " << dirPath << endl;
        return 0;
    }


    //this would be necessary if dirlister wasn't autoupdating; if dirlister *is*, then it causes crashes because of multiple definitions
    //addTrackToList( MediaItem::DIRECTORY, KURL( fullPath ) );

    #undef parent

    return 0;
}

void
VfatMediaDevice::addToDirectory( MediaItem *directory, QPtrList<MediaItem> items )
{
    DEBUG_BLOCK
    debug() << "items.count() is " << items.count() << endl;
    if( !directory || items.isEmpty() ) return;

    VfatMediaFile *dropDir;
    if( directory->type() == MediaItem::TRACK )
    #define directory static_cast<VfatMediaItem *>(directory)
        dropDir = m_mim[directory]->getParent();
    else
        dropDir = m_mim[directory];

    for( QPtrListIterator<MediaItem> it(items); *it; ++it )
    {
        VfatMediaItem *currItem = static_cast<VfatMediaItem *>(*it);
        debug() << "currItem fullname = " << m_mim[currItem]->getFullName() << ", of type " << ((*it)->type() == MediaItem::TRACK ? "track" : ( (*it)->type() == MediaItem::DIRECTORY ? "directory" : "unknown" ) ) << endl;
        QCString src  = m_mim[currItem]->getEncodedFullName();
        QCString dst = dropDir->getEncodedFullName() + "/" + QFile::encodeName( currItem->text(0) );
        debug() << "Moving: " << src << " to: " << dst << endl;

        const KURL srcurl(src);
        const KURL dsturl(dst);

        if ( !KIO::NetAccess::file_move( srcurl, dsturl, -1, false, false, m_parent ) )
            debug() << "Failed moving " << src << " to " << dst << endl;
        else
        {
            //debug() << "Entering first refreshDir" << endl;
            refreshDir( m_mim[currItem]->getParent()->getFullName() );
            //debug() << "Entering second refreshDir" << endl;
            refreshDir( dropDir->getFullName() );
        }
    }
    #undef directory
}

/// Uploading

void
VfatMediaDevice::copyTrackSortHelper( const MetaBundle& bundle, QString& sort, QString& temp, QString& base )
{
    QListViewItem *it;
    if( sort != "None" )
    {
        //debug() << "sort = " << sort << endl;
        temp = bundle.prettyText( bundle.columnIndex(sort) );
        temp = ( temp == QString::null ? "Unknown" : cleanPath(temp) );
        base += temp + "/";

        if( !KIO::NetAccess::exists( KURL(base), true, m_parent ) )
        //   m_tmpParent = static_cast<MediaItem *>(newDirectory( temp, static_cast<MediaItem*>(m_tmpParent) ));
            debug() << "copyTrackSortHelper: stat failed" << endl;
        else
        {
            //debug() << "m_tmpParent (firstSort) " << m_tmpParent << endl;
            if( m_tmpParent)
                it = m_tmpParent->firstChild();
            else
                it = m_view->firstChild();
            while( it && it->text(0) != temp )
            {
                it = it->nextSibling();
                //debug() << "Looking for next in firstSort, temp = " << temp << ", text(0) = " << it->text(0) << endl;
            }
            m_tmpParent = static_cast<MediaItem *>( it );
        }
    }
}


MediaItem *
VfatMediaDevice::copyTrackToDevice( const MetaBundle& bundle )
{
    DEBUG_BLOCK
    debug() << "dirlister autoupdate = " << (m_dirLister->autoUpdate() ? "true" : "false") << endl;
    if( !m_connected ) return 0;

    m_isInCopyTrack = true;

    debug() << "m_tmpParent = " << m_tmpParent << endl;
    MediaItem *previousTmpParent = static_cast<MediaItem *>(m_tmpParent);

    QString  newFilenameSansMountpoint = fileName( bundle );
    QString  base = m_transferDir + "/";
    QString  temp;

    copyTrackSortHelper( bundle, m_firstSort, temp, base);
    copyTrackSortHelper( bundle, m_secondSort, temp, base);
    copyTrackSortHelper( bundle, m_thirdSort, temp, base);

    QString  newFilename = base + newFilenameSansMountpoint;

    const QCString dest = QFile::encodeName( newFilename );
    const KURL desturl = KURL::fromPathOrURL( dest );

    kapp->processEvents( 100 );

    //if( KIO::NetAccess::file_copy( bundle.url(), desturl, -1, false, false, m_parent) ) //success
    if( kioCopyTrack( bundle.url(), desturl ) )
    {
        addTrackToList( MediaItem::TRACK, newFilenameSansMountpoint );
        m_tmpParent = previousTmpParent;
        m_isInCopyTrack = false;
        return m_last;
    }

    m_tmpParent = previousTmpParent;
    m_isInCopyTrack = false;
    return 0;
}

//Somewhat related...

MediaItem *
VfatMediaDevice::trackExists( const MetaBundle& bundle )
{
    QString key;
    QListViewItem *it = view()->firstChild();
    if( m_firstSort != "None")
    {
        key = bundle.prettyText( bundle.columnIndex( m_firstSort ) );
        key = cleanPath( ( key.isEmpty() ? "Unknown" : key ) );
        while( it && it->text( 0 ) != key )
            it = it->nextSibling();
        if( !it )
            return 0;
        if( !it->childCount() )
           expandItem( it );
        it = it->firstChild();
    }

    if( m_secondSort != "None")
    {
        key = bundle.prettyText( bundle.columnIndex( m_secondSort ) );
        key = cleanPath( ( key.isEmpty() ? "Unknown" : key ) );
        while( it && it->text( 0 ) != key )
        {
            it = it->nextSibling();
        }
        if( !it )
            return 0;
        if( !it->childCount() )
           expandItem( it );
        it = it->firstChild();
    }

    if( m_thirdSort != "None")
    {
        key = bundle.prettyText( bundle.columnIndex( m_thirdSort ) );
        key = cleanPath( ( key.isEmpty() ? "Unknown" : key ) );
        while( it && it->text( 0 ) != key )
            it = it->nextSibling();
        if( !it )
            return 0;
        if( !it->childCount() )
           expandItem( it );
        it = it->firstChild();
    }

    key = fileName( bundle );
    while( it && it->text( 0 ) != key )
        it = it->nextSibling();

    return dynamic_cast<MediaItem *>( it );
}

/// File transfer methods


void
VfatMediaDevice::downloadSelectedItems()
{
    KURL::List urls = getSelectedItems();

    CollectionView::instance()->organizeFiles( urls, "Copy Files to Collection", true );

    hideProgress();
}

KURL::List
VfatMediaDevice::getSelectedItems()
{
    while ( !m_downloadList.empty() )
        m_downloadList.pop_front();

    QListViewItemIterator it( m_view, QListViewItemIterator::Selected );
    MediaItem *curritem;
    for( ; it.current(); ++it )
    {
        curritem = static_cast<MediaItem *>(*it);
        debug() << "text(0)=" << curritem->text( 0 ) << endl;
        if( curritem->type() == MediaItem::DIRECTORY )
        {
            debug() << "drill" << curritem->text( 0 ) << endl;
            drillDown( curritem );
        }
        else //file
        {
            debug() << "file: " << curritem->text( 0 )<< ", path: " << getFullPath( curritem, true, true, false ) << endl;
            m_downloadList.append( KURL::fromPathOrURL( getFullPath( curritem, true, true, false ) ) );
        }
    }

    return m_downloadList;
}

void
VfatMediaDevice::drillDown( MediaItem *curritem )
{
    //okay, can recursively call this for directories...
    m_downloadListerFinished  = 0;
    int count = 0;
    m_currentJobUrl = KURL::fromPathOrURL( getFullPath( curritem, true, true, false ) );
    KIO::ListJob * listjob = KIO::listRecursive( m_currentJobUrl, false, false );
    connect( listjob, SIGNAL( result( KIO::Job* ) ), this, SLOT( downloadSlotResult( KIO::Job* ) ) );
    connect( listjob, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ), this, SLOT( downloadSlotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    connect( listjob, SIGNAL( redirection( KIO::Job*, const KURL& ) ), this, SLOT( downloadSlotRedirection( KIO::Job*, const KURL& ) ) );
    while( !m_downloadListerFinished ){
        usleep( 10000 );
        kapp->processEvents( 100 );
        count++;
        if (count > 120){
            debug() << "Taking too long to find files, returning from drillDown in " << m_currentJobUrl << endl;
            return;
        }
    }
}

void
VfatMediaDevice::downloadSlotResult( KIO::Job *job )
{
    if( job->error() )
        debug() << "downloadSlotResult: ListJob reported an error!  Error code = " << job->error() << endl;
    m_downloadListerFinished = true;
}

void
VfatMediaDevice::downloadSlotRedirection( KIO::Job */*job*/, const KURL &url )
{
    m_currentJobUrl = url;
}

void
VfatMediaDevice::downloadSlotEntries(KIO::Job */*job*/, const KIO::UDSEntryList &entries)
{
        KIO::UDSEntryListConstIterator it = entries.begin();
        KIO::UDSEntryListConstIterator end = entries.end();

        for (; it != end; ++it)
        {
                KFileItem file(*it, m_currentJobUrl, false /* no mimetype detection */, true);
                if (!file.isDir())
                        m_downloadList.append(KURL( file.url().path() ) );
        }
}

/// Deleting

int
VfatMediaDevice::deleteItemFromDevice( MediaItem *item, bool /*onlyPlayed*/ )
{
    if( !item || !m_connected ) return -1;

    QString path = getFullPath( item );

    QCString encodedPath = QFile::encodeName( path );
    debug() << "Deleting file: " << encodedPath << endl;
    bool flag = true;
    int count = 0;

    switch( item->type() )
    {
        case MediaItem::DIRECTORY:
            if ( !KIO::NetAccess::del( KURL(encodedPath), m_parent ))
            {
                debug() << "Error deleting directory: " << encodedPath << endl;
                flag = false;
            }
            count++;
            break;

        default:
            if ( !KIO::NetAccess::del( KURL(encodedPath), m_parent ))
            {
                debug() << "Error deleting file: " << encodedPath << endl;
                flag = false;
            }
            count++;
            break;
    }
    if( flag ) //success
        delete item;

    return flag ? count : -1;
}

/// Directory Reading

void
VfatMediaDevice::expandItem( QListViewItem *item ) // SLOT
{
    //DEBUG_BLOCK
    if( !item || !item->isExpandable() ) return;

    #define item static_cast<VfatMediaItem *>(item)
    m_dirListerComplete = false;
    listDir( m_mim[item]->getFullName() );
    #undef item

    while( !m_dirListerComplete )
    {
        kapp->processEvents( 100 );
        usleep(10000);
    }
}

void
VfatMediaDevice::listDir( const QString &dir )
{
    DEBUG_BLOCK
    if( m_mfm[dir]->getListed() )
        m_dirLister->updateDirectory( KURL(dir) );
    else
    {
        //debug() << "in listDir, dir = " << dir << endl;
        m_dirLister->openURL( KURL(dir), true, true );
        m_mfm[dir]->setListed( true );
    }
}

void
VfatMediaDevice::refreshDir( const QString &dir )
{
    DEBUG_BLOCK
    //debug() << "refreshDir, dir = " << dir << endl;
    m_dirLister->updateDirectory( KURL(dir) );
}

void
VfatMediaDevice::newItems( const KFileItemList &items )
{
    DEBUG_BLOCK
    //iterate over items, calling addTrackToList
    //if( m_stopDirLister || m_isInCopyTrack )
    //    return;

    QPtrListIterator<KFileItem> it( items );
    KFileItem *kfi;
    while ( (kfi = it.current()) != 0 ) {
        ++it;
        addTrackToList( kfi->isFile() ? MediaItem::TRACK : MediaItem::DIRECTORY, kfi->url().path(-1), 0 );
    }
}

void
VfatMediaDevice::dirListerCompleted()
{
    DEBUG_BLOCK
    m_dirListerComplete = true;
}

void
VfatMediaDevice::dirListerClear()
{
    DEBUG_BLOCK
    m_initialFile->deleteAll();

    m_view->clear();
    m_mfm.clear();
    m_mim.clear();

    m_initialFile = new VfatMediaFile( 0, m_medium->mountPoint(), this );
}

void
VfatMediaDevice::dirListerClear( const KURL &url )
{
    DEBUG_BLOCK
    QString directory = url.path(-1);
    debug() << "Removing url: " << directory << endl;
    VfatMediaFile *vmf = m_mfm[directory];
    if( vmf )
        vmf->deleteAllChildren();
}

void
VfatMediaDevice::dirListerDeleteItem( KFileItem *fileitem )
{
    DEBUG_BLOCK
    QString filename = fileitem->url().path(-1);
    debug() << "Removing item: " << filename << endl;
    VfatMediaFile *vmf = m_mfm[filename];
    if( vmf )
        vmf->deleteAll();
}

int
VfatMediaDevice::addTrackToList( int type, KURL url, int /*size*/ )
{

    //DEBUG_BLOCK
    //m_tmpParent ?
    //    m_last = new VfatMediaItem( m_tmpParent ):
    //    m_last = new VfatMediaItem( m_view );

    debug() << "addTrackToList: url.path = " << url.path(-1) << endl;

    QString path = url.path( -1 ); //no trailing slash
    int index = path.findRev( '/', -1 );
    QString baseName = path.right( path.length() - index - 1 );
    QString parentName = path.left( index );

    debug() << "index is " << index << ", baseName = " << baseName << ", parentName = " << parentName << endl;

    VfatMediaFile* parent = m_mfm[parentName];
    debug() << "parent's getFullName is: " << parent->getFullName() << endl;
    VfatMediaFile* newItem = new VfatMediaFile( parent, baseName, this );

    if( type == MediaItem::DIRECTORY ) //directory
        newItem->getViewItem()->setType( MediaItem::DIRECTORY );
    //TODO: this logic could maybe be taken out later...or the dirlister shouldn't
    //filter, one or the other...depends if we want to allow viewing any files
    //or just update the list in the plugin as appropriate
    else if( type == MediaItem::TRACK ) //file
    {
        if( baseName.endsWith( "mp3", false ) || baseName.endsWith( "wma", false ) ||
            baseName.endsWith( "wav", false ) || baseName.endsWith( "ogg", false ) ||
            baseName.endsWith( "asf", false ) || baseName.endsWith( "flac", false ) ||
            baseName.endsWith( "aac", false ) || baseName.endsWith( "m4a", false ) )

            newItem->getViewItem()->setType( MediaItem::TRACK );

        else
            newItem->getViewItem()->setType( MediaItem::UNKNOWN );
    }

    refreshDir( parent->getFullName() );

    return 0;
}

/// Capacity, in kB

bool
VfatMediaDevice::getCapacity( KIO::filesize_t *total, KIO::filesize_t *available )
{
    if( !m_connected ) return false;

    KDiskFreeSp* kdf = new KDiskFreeSp( m_parent, "vfat_kdf" );
    kdf->readDF( m_medium->mountPoint() );
    connect(kdf, SIGNAL(foundMountPoint( const QString &, unsigned long, unsigned long, unsigned long )),
                 SLOT(foundMountPoint( const QString &, unsigned long, unsigned long, unsigned long )));

    int count = 0;

    while( m_kBSize == 0 && m_kBAvail == 0){
        usleep( 10000 );
        kapp->processEvents( 100 );
        count++;
        if (count > 120){
            debug() << "KDiskFreeSp taking too long.  Returning false from getCapacity()" << endl;
            return false;
        }
    }

    *total = m_kBSize*1024;
    *available = m_kBAvail*1024;
    unsigned long localsize = m_kBSize;
    m_kBSize = 0;
    m_kBAvail = 0;

    return localsize > 0;
}

void
VfatMediaDevice::foundMountPoint( const QString & mountPoint, unsigned long kBSize, unsigned long /*kBUsed*/, unsigned long kBAvail )
{
    if ( mountPoint == m_medium->mountPoint() ){
        m_kBSize = kBSize;
        m_kBAvail = kBAvail;
    }
}

/// Helper functions

QString
VfatMediaDevice::getFullPath( const QListViewItem *item, const bool getFilename, const bool prependMount, const bool clean )
{
    //DEBUG_BLOCK
    if( !item ) return QString::null;

    QString path;

    if ( getFilename && clean )
        path = cleanPath(item->text(0));
    else if( getFilename )
        path = item->text(0);

    QListViewItem *parent = item->parent();

    while( parent )
    {
        path.prepend( "/" );
        path.prepend( ( clean ? cleanPath(parent->text(0)) : parent->text(0) ) );
        parent = parent->parent();
    }

    //debug() << "path before prependMount = " << path << endl;
    if( prependMount )
        path.prepend( m_medium->mountPoint() + "/" );

    //debug() << "path after prependMount = " << path << endl;

    return path;

}


void
VfatMediaDevice::rmbPressed( QListViewItem* qitem, const QPoint& point, int )
{
    enum Actions { APPEND, LOAD, QUEUE,
        DOWNLOAD,
        BURN_DATACD, BURN_AUDIOCD,
        DIRECTORY, RENAME,
        DELETE, TRANSFER_HERE };

    MediaItem *item = static_cast<MediaItem *>(qitem);
    if ( item )
    {
        KPopupMenu menu( m_view );
        menu.insertItem( SmallIconSet( amaroK::icon( "playlist" ) ), i18n( "&Load" ), LOAD );
        menu.insertItem( SmallIconSet( amaroK::icon( "1downarrow" ) ), i18n( "&Append to Playlist" ), APPEND );
        menu.insertItem( SmallIconSet( amaroK::icon( "fastforward" ) ), i18n( "&Queue Tracks" ), QUEUE );
        menu.insertSeparator();
        menu.insertItem( SmallIconSet( "collection" ), i18n( "&Copy Files to Collection..." ), DOWNLOAD );
        menu.insertItem( SmallIconSet( amaroK::icon( "cdrom_unmount" ) ), i18n( "Burn to CD as Data" ), BURN_DATACD );
        menu.setItemEnabled( BURN_DATACD, K3bExporter::isAvailable() );
        menu.insertItem( SmallIconSet( amaroK::icon( "cdaudio_unmount" ) ), i18n( "Burn to CD as Audio" ), BURN_AUDIOCD );
        menu.setItemEnabled( BURN_AUDIOCD, K3bExporter::isAvailable() );
        menu.insertSeparator();
        menu.insertItem( SmallIconSet( "folder" ), i18n( "Add Directory" ), DIRECTORY );
        menu.insertItem( SmallIconSet( "editclear" ), i18n( "Rename" ), RENAME );
        menu.insertItem( SmallIconSet( "editdelete" ), i18n( "Delete" ), DELETE );
        menu.insertSeparator();
        menu.insertItem( SmallIconSet( "up" ), i18n( "Transfer Queue to Here..." ), TRANSFER_HERE );
        menu.setItemEnabled( TRANSFER_HERE, MediaBrowser::queue()->childCount() );

        int id =  menu.exec( point );
        switch( id )
        {
            case LOAD:
                Playlist::instance()->insertMedia( getSelectedItems(), Playlist::Replace );
                break;
            case APPEND:
                Playlist::instance()->insertMedia( getSelectedItems(), Playlist::Append );
                break;
            case QUEUE:
                Playlist::instance()->insertMedia( getSelectedItems(), Playlist::Queue );
                break;
            case DOWNLOAD:
                downloadSelectedItems();
                break;
            case BURN_DATACD:
                K3bExporter::instance()->exportTracks( getSelectedItems(), K3bExporter::DataCD );
                break;
            case BURN_AUDIOCD:
                K3bExporter::instance()->exportTracks( getSelectedItems(), K3bExporter::AudioCD );
                break;

            case DIRECTORY:
                if( item->type() == MediaItem::DIRECTORY )
                    m_view->newDirectory( static_cast<MediaItem*>(item) );
                else
                    m_view->newDirectory( static_cast<MediaItem*>(item->parent()) );
                break;

            case RENAME:
                m_view->rename( item, 0 );
                break;

            case DELETE:
                deleteFromDevice();
                break;

            case TRANSFER_HERE:
                m_tmpParent = item;
                if( item->type() == MediaItem::DIRECTORY )
                {
                    m_transferDir = getFullPath( item, true );
                }
                else
                {
                    m_transferDir = getFullPath( item, false );
                    if (m_transferDir != QString::null)
                        m_transferDir = m_transferDir.remove( m_transferDir.length() - 1, 1 );
                }
                emit startTransfer();
                break;
        }
        return;
    }

    if( isConnected() )
    {
        KPopupMenu menu( m_view );
        menu.insertItem( SmallIconSet( "folder" ), i18n("Add Directory" ), DIRECTORY );
        if ( MediaBrowser::queue()->childCount())
        {
            menu.insertSeparator();
            menu.insertItem( SmallIconSet( "up" ), i18n(" Transfer queue to here..." ), TRANSFER_HERE );
        }
        int id =  menu.exec( point );
        switch( id )
        {
            case DIRECTORY:
                m_view->newDirectory( 0 );
                break;

            case TRANSFER_HERE:
                m_transferDir = m_medium->mountPoint();
                m_tmpParent = NULL;
                emit startTransfer();
                break;

        }
    }
}


QString VfatMediaDevice::cleanPath( const QString &component )
{
    QString result = component;

    if( m_actuallyVfat )
    {
        result = amaroK::cleanPath( result, true /* replaces weird stuff by '_' */);
    }

    result.simplifyWhiteSpace();
    if( m_spacesToUnderscores )
        result.replace( QRegExp( "\\s" ), "_" );
    if( m_actuallyVfat )
        result = amaroK::vfatPath( result );

    result.replace( "/", "-" );

    return result;
}

#include "vfatmediadevice.moc"
