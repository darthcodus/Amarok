/***************************************************************************
                         playerwidget.h  -  description
                            -------------------
   begin                : Mit Nov 20 2002
   copyright            : (C) 2002 by Mark Kretschmann
   email                :
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLAYERWIDGET_H
#define PLAYERWIDGET_H

#include <qlabel.h>
#include <qwidget.h>
#include <qpixmap.h>
#include <qslider.h>

#include <ksystemtray.h>

#include "amarokdcopiface.h"

class QBitmap;
class QBoxLayout;
class QFrame;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QPopupMenu;
class QPushButton;
class QString;
class QTimer;
class QTimerEvent;
class QToolButton;

class KActionCollection;
class KSystemTray;

class AmarokButton;
class ArtsConfigWidget;
class VisWidget;

class PlayerApp;
extern PlayerApp *pApp;


/**
 *@author mark
 */

// CLASS AmarokSlider ------------------------------------------------------------

class AmarokSlider : public QSlider
{
        Q_OBJECT

    public:
        AmarokSlider( QWidget *parent );
        ~AmarokSlider();


        // ATTRIBUTES ------

    public slots:

    signals:

    private:
        void mousePressEvent( QMouseEvent *e );

        // ATTRIBUTES ------
};


// CLASS AmarokSystray ------------------------------------------------------------

class PlayerWidget;

class AmarokSystray : public KSystemTray
{
   public:
      AmarokSystray( PlayerWidget *, KActionCollection * );

   private:
      void mousePressEvent( QMouseEvent * );
      void wheelEvent( QWheelEvent * );
      void showEvent( QShowEvent * ) {} //Don't add me a Quit button automagically
};


// CLASS PlayerWidget ------------------------------------------------------------

class PlayerWidget : public QWidget, virtual public AmarokIface
{
        Q_OBJECT

    public:
        PlayerWidget( QWidget *parent = 0, const char *name = 0 );
        ~PlayerWidget();

        void setScroll( QString text, QString bitrate, QString samplerate );
        void drawScroll();
        void timeDisplay( bool remaining, int hours, int minutes, int seconds );
        void createVis();

        // ATTRIBUTES ------
        KActionCollection *m_pActionCollection;

        QPopupMenu *m_pPopupMenu;
        VisWidget *m_pVis;
        QFrame *m_pFrame;
        QFrame *m_pFrameButtons;
        AmarokSlider *m_pSlider;
        AmarokSlider *m_pSliderVol;
        QLabel *m_pTimeDisplayLabel;

        AmarokButton *m_pButtonPl;
        AmarokButton *m_pButtonEq;
        AmarokButton *m_pButtonLogo;

        QPushButton *m_pButtonPrev;
        QPushButton *m_pButtonPlay;
        QPushButton *m_pButtonPause;
        QPushButton *m_pButtonStop;
        QPushButton *m_pButtonNext;

        int m_IdRepeatTrack;
        int m_IdRepeatPlaylist;
        int m_IdConfPlayObject;
        int m_IdRandomMode;
        ArtsConfigWidget *m_pPlayObjConfigWidget;

        //originally AmarokSystray::wheelEvent was a friend of this class,
        //but it's seem silly when it's safe to just make this function public
        void wheelEvent( QWheelEvent *e );

    public slots:
        void slotConfigShortcuts();
        void slotConfigGlobalShortcuts();
        void slotCopyClipboard();
        void slotConfigPlayObject();
        void slotConfigWidgetDestroyed();
        void slotUpdateTrayIcon( bool visible );

        virtual void show();
        virtual void hide();

    public /* DCOP */ slots:
       /* FIXME: move dcop iface to a separate impl class */
       void play();
       void stop();
       void next();
       void prev();
       void pause();
       QString nowPlaying();
       bool isPlaying();

    signals:
        void sigMinimized();
        void sigRestored();

    private slots:
        void visClicked();

    private:
        void initScroll();
        void initTimeDisplay();
        void polish();

        void paintEvent( QPaintEvent *e );
        void mouseReleaseEvent( QMouseEvent *e );
        void mousePressEvent( QMouseEvent *e );
        void queryClose();
        void closeEvent( QCloseEvent *e );
        void moveEvent( QMoveEvent *e );

        // ATTRIBUTES ------
        QString m_bitrate, m_samplerate;
        QTimer *scrollTimer;
        QBoxLayout *m_pLay6;

        QPixmap m_oldBgPixmap;
        QPixmap *m_pScrollPixmap;
        QPixmap *m_pBgPixmap;
        QPixmap *m_pComposePixmap;
        QBitmap *m_pScrollMask;
        QPixmap *m_pTimePixmap;
        QPixmap *m_pTimeBgPixmap;
        QPixmap *m_pTimeComposePixmap;

        int m_timeDisplayX;
        int m_timeDisplayY;
        int m_timeDisplayW;

        int m_pixmapWidth;
        int m_pixmapHeight;
        int m_scrollWidth;
        int m_sx;
        int m_sy;
        int m_sxAdd;
        AmarokSystray *m_pTray;

        QString m_nowPlaying; /* state for DCOP iface nowPlaying() */
};
#endif
