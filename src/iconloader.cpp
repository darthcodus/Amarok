/***************************************************************************
 *   Copyright (C) 2006 by Mark Kretschmann <markey@web.de>                *
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
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/

#include "amarok.h"
#include "amarokconfig.h"

#include <qmap.h>


QString
Amarok::icon( const QString& name ) //declared in amarok.h
{
    // We map our Amarok icon theme names to system icons, instead of using the same
    // naming scheme. This has two advantages:
    // 1. Our icons can have simpler and more meaningful names
    // 2. We can map several of our icons to one system icon, if necessary
    static QMap<QString, QString> iconMap;

    if( iconMap.empty() ) {
        iconMap["add_lyrics"]           = "edit_add";
        iconMap["add_playlist"]         = "arrow_down";
        iconMap["album"]                = "cdrom_unmount";
        iconMap["artist"]               = "personal";
        iconMap["audioscrobbler"]       = "audioscrobbler";
        iconMap["love"]                 = "bookmark";
        iconMap["back"]                 = "media-skip-backward";
        iconMap["burn"]                 = "cdrom_unmount";
        iconMap["change_language"]      = "configure";
        iconMap["clock"]                = "history";
        iconMap["collection"]           = "collection";
        iconMap["configure"]            = "configure";
        iconMap["covermanager"]         = "covermanager";
        iconMap["device"]               = "usbpendrive_unmount";
        iconMap["download"]             = "khtml_kget";
        iconMap["dynamic"]              = "dynamic";
        iconMap["edit"]                 = "edit";
        iconMap["editcopy"]             = "editcopy";
        iconMap["equalizer"]            = "equalizer";
        iconMap["external"]             = "exec";
        iconMap["fastforward"]          = "2rightarrow";
        iconMap["favourite_genres"]     = "kfm";
        iconMap["files"]                = "folder";
        iconMap["files2"]               = "folder_red";
        iconMap["info"]                 = "info";
        iconMap["lyrics"]               = "document";
        iconMap["magnatune"]            = "cd";
        iconMap["mostplayed"]           = "favorites";
        iconMap["music"]                = "today";
        iconMap["next"]                 = "media-skip-forward";
        iconMap["pause"]                = "media-playback-pause";
        iconMap["play"]                 = "media-playback-start";
        iconMap["playlist"]             = "player_playlist_2";
        iconMap["playlist_clear"]       = "edit-clear";
        iconMap["playlist_refresh"]     = "rebuild";
        iconMap["queue"]                = "goto";
        iconMap["queue_track"]          = "2rightarrow";
        iconMap["dequeue_track"]        = "2leftarrow";
        iconMap["random"]               = "random";
        iconMap["random_album"]         = "cd";
        iconMap["random_no"]            = "forward";
        iconMap["random_track"]         = "random";
        iconMap["redo"]                 = "edit-redo";
        iconMap["refresh"]              = "view-refresh";
        iconMap["remove"]               = "edit-delete";
        iconMap["remove_from_playlist"] = "remove";
        iconMap["repeat_album"]         = "cdrom_unmount";
        iconMap["repeat_no"]            = "bottom";
        iconMap["repeat_playlist"]      = "repeat_playlist";
        iconMap["repeat_track"]         = "repeat_track";
        iconMap["rescan"]               = "edit-refresh";
        iconMap["rewind"]               = "2leftarrow";
        iconMap["save"]                 = "document-save";
        iconMap["scripts"]              = "pencil";
        iconMap["search"]               = "find";
        iconMap["settings_engine"]      = "amarok";
        iconMap["settings_general"]     = "misc";
        iconMap["settings_indicator"]   = "tv";
        iconMap["settings_playback"]    = "kmix";
        iconMap["settings_view"]        = "colors";
        iconMap["stop"]                 = "media-playback-stop";
        iconMap["podcast"]              = "podcast";
        iconMap["podcast2"]             = "podcast_new";
        iconMap["track"]                = "sound";
        iconMap["undo"]                 = "edit-undo";
        iconMap["visualizations"]       = "visualizations";
        iconMap["zoom"]                 = "edit-find";
    }

    static QMap<QString, QString> amarokMap;
    if( amarokMap.empty() ) {
        amarokMap["queue_track"]          = "fastforward";
        amarokMap["dequeue_track"]        = "rewind";
    }

    if( iconMap.contains( name ) )
    {
        if( AmarokConfig::useCustomIconTheme() )
        {
            if( amarokMap.contains( name ) )
                return QString( "amarok_" ) + amarokMap[name];
            return QString( "amarok_" ) + name;
        }
        else
            return iconMap[name];
    }

    return name;
}


