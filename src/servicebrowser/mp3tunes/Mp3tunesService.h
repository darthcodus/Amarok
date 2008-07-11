/***************************************************************************
 *   Copyright (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *   Copyright (c) 2008  Casey Link <unnamedrambler@gmail.com>             *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#ifndef MP3TUNESSERVICE_H
#define MP3TUNESSERVICE_H

#include "../ServiceBase.h"
#include "Mp3tunesServiceCollection.h"
#include "Mp3tunesLocker.h"
#ifndef DEFINE_HARMONY
#define DEFINE_HARMONY
#endif
#include "Mp3tunesHarmonyDaemon.h"



class Mp3tunesServiceFactory: public ServiceFactory
{
    Q_OBJECT

    public:
        explicit Mp3tunesServiceFactory() {}
        virtual ~Mp3tunesServiceFactory() {}

        virtual bool possiblyContainsTrack( const KUrl &url ) const;

        virtual void init();
        virtual QString name();
        virtual KPluginInfo info();
        virtual KConfigGroup config();
};


/**
    A service for displaying, previewing and downloading music from Mp3tunes.com
	@author
*/
class Mp3tunesService : public ServiceBase
{
Q_OBJECT

public:
    explicit Mp3tunesService( const QString &name, const QString &partnerToken, const QString &email = QString(), const QString &password = QString(), bool harmonyEnabled = false);

    ~Mp3tunesService();

    void polish();

    virtual Collection * collection() { return m_collection; }

private slots:
    void authenticate( const QString & uname = "", const QString & passwd = "" );
    void authenticationComplete(  const QString & sessionId );

    /**
     * the daemon received the PIN. now that pin has to be presented to the user,
     * so he/she (comments must be gender neutral) can add it to his/her mp3tunes
     * account.
     */
    void harmonyWaitingForEmail();
    void harmonyConnected();
    void harmonyDisconnected();
    void harmonyError( const QString &error );
    void harmonyDownloadReady( Mp3tunesHarmonyDownload *download );
    void harmonyDownloadPending( Mp3tunesHarmonyDownload *download );

private:
    char *convertToChar( const QString &source ) const;
    QString m_email;
    QString m_password;
    bool m_harmonyEnabled;
    QString m_partnerToken;

    bool m_authenticated;
    QString m_sessionId;

    Mp3tunesServiceCollection *  m_collection;

    Mp3tunesLocker * m_locker;
    Mp3tunesHarmonyDaemon * m_daemon;
};

#endif
