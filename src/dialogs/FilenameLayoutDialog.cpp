/****************************************************************************************
 * Copyright (c) 2008 Téo Mrnjavac <teo@kde.org>                                        *
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Daniel Dewald <Daniel.Dewald@time.shift.de>                       *
 * Copyright (c) 2012 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "FilenameLayoutDialog.h"
#include "TagGuesser.h"
#include "../widgets/TokenDropTarget.h"
#include "../widgets/TokenPool.h"

#include "amarokconfig.h"

#include "MetaValues.h"
#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "widgets/TokenDropTarget.h"

#include <KConfig>
#include <KColorScheme>
#include <KInputDialog>

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QStackedWidget>

static const QStringList typeElements = ( QStringList()
<< QString()
<< QLatin1String("%ignore%")
<< QLatin1String("%track%")
<< QLatin1String("%title%")
<< QLatin1String("%artist%")
<< QLatin1String("%composer%")
<< QLatin1String("%year%")
<< QLatin1String("%album%")
<< QLatin1String("%albumartist%")
<< QLatin1String("%comment%")
<< QLatin1String("%genre%")
<< QLatin1String("%filetype%")
<< QLatin1String("%folder%")
<< QLatin1String("%initial%")
<< QLatin1String("%discnumber%")
<< QLatin1String(" ")
<< QLatin1String("/")
<< QLatin1String(".")
<< QLatin1String("-")
<< QLatin1String("_") );

// -------------- TagGuessOptionWidget ------------
TagGuessOptionWidget::TagGuessOptionWidget( QWidget *parent )
    : QWidget( parent )
{
    setupUi( this );

    m_caseEditRadioButtons << rbAllUpper
        << rbAllLower
        << rbFirstLetter
        << rbTitleCase;

    int caseOptions = Amarok::config( "TagGuesser" ).readEntry( "Case options", 4 );
    if( !caseOptions )
        cbCase->setChecked( false );
    else
    {
        cbCase->setChecked( true );
        switch( caseOptions )
        {
        case 4:
            rbAllLower->setChecked( true );
            break;
        case 3:
            rbAllUpper->setChecked( true );
            break;
        case 2:
            rbFirstLetter->setChecked( true );
            break;
        case 1:
            rbTitleCase->setChecked( true );
            break;
        default:
            debug() << "OUCH";
        }
    }

    cbEliminateSpaces->setChecked(    Amarok::config( "TagGuesser" ).readEntry( "Eliminate trailing spaces", false ) );
    cbReplaceUnderscores->setChecked( Amarok::config( "TagGuesser" ).readEntry( "Replace underscores", false ) );

    connect( cbCase, SIGNAL( toggled( bool ) ),
             this, SLOT( editStateEnable( bool ) ) );
    connect( cbCase, SIGNAL( toggled( bool ) ),
             this, SIGNAL( optionsChanged() ) );
    connect( rbTitleCase, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
    connect( rbFirstLetter, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
    connect( rbAllLower, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
    connect( rbAllUpper, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
    connect( cbEliminateSpaces, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
    connect( cbReplaceUnderscores, SIGNAL( toggled(bool) ),
             this, SIGNAL( optionsChanged() ) );
}

void
TagGuessOptionWidget::editStateEnable( bool checked )      //SLOT
{
    foreach( QRadioButton *rb, m_caseEditRadioButtons )
        rb->setEnabled( checked );
}

//Returns a code for the configuration.
int
TagGuessOptionWidget::getCaseOptions()
{
    //Amarok::config( "TagGuesser" ).readEntry( "Filename schemes", QStringList() );
    if( !cbCase->isChecked() )
        return 0;
    else
    {
        if( rbAllLower->isChecked() )
            return 4;
        else if( rbAllUpper->isChecked() )
            return 3;
        else if( rbFirstLetter->isChecked() )
            return 2;
        else if( rbTitleCase->isChecked() )
            return 1;
        else
        {
            debug() << "OUCH!";
            return 0;
        }
    }
}

//As above
bool
TagGuessOptionWidget::getWhitespaceOptions()
{
    return cbEliminateSpaces->isChecked();
}

//As above
bool
TagGuessOptionWidget::getUnderscoreOptions()
{
    return cbReplaceUnderscores->isChecked();
}


// -------------- FilenameLayoutOptionWidget ------------
FilenameLayoutOptionWidget::FilenameLayoutOptionWidget( QWidget *parent )
    : QWidget( parent )
{
    setupUi( this );

    connect( spaceCheck, SIGNAL(toggled(bool)), SIGNAL(optionsChanged()) );
    connect( ignoreTheCheck, SIGNAL(toggled(bool)), SIGNAL(optionsChanged()) );
    connect( vfatCheck, SIGNAL(toggled(bool)), SIGNAL(optionsChanged()) );
    connect( asciiCheck, SIGNAL(toggled(bool)), SIGNAL(optionsChanged()) );
    connect( regexpEdit, SIGNAL(editingFinished()), SIGNAL(optionsChanged()) );
    connect( replaceEdit, SIGNAL(editingFinished()), SIGNAL(optionsChanged()) );
}


// ------------------------- OrganizeCollectionWidget -------------------

OrganizeCollectionWidget::OrganizeCollectionWidget( QWidget *parent )
    : FilenameLayoutWidget( parent )
{
    m_configCategory = "OrganizeCollectionDialog";

    m_tokenPool->addToken( createToken( Title ) );
    m_tokenPool->addToken( createToken( Artist ) );
    m_tokenPool->addToken( createToken( Composer ) );
    m_tokenPool->addToken( createToken( Track ) );
    m_tokenPool->addToken( createToken( Year ) );
    m_tokenPool->addToken( createToken( Album ) );
    m_tokenPool->addToken( createToken( AlbumArtist ) );
    m_tokenPool->addToken( createToken( Comment ) );
    m_tokenPool->addToken( createToken( Genre ) );

    m_tokenPool->addToken( createToken( Initial ) );
    m_tokenPool->addToken( createToken( FileType ) );
    m_tokenPool->addToken( createToken( DiscNumber ) );

    m_tokenPool->addToken( createToken( Slash ) );
    m_tokenPool->addToken( createToken( Underscore ) );
    m_tokenPool->addToken( createToken( Dash ) );
    m_tokenPool->addToken( createToken( Dot ) );
    m_tokenPool->addToken( createToken( Space ) );

    m_optionsWidget = new FilenameLayoutOptionWidget();
    m_mainLayout->addWidget( m_optionsWidget );

    // show some non-editable tags before and after
    m_schemaLineLayout->insertWidget( 0,
                                      createStaticToken( CollectionRoot ), 0 );
    m_schemaLineLayout->insertWidget( 1,
                                      createStaticToken( Slash ), 0 );

    m_schemaLineLayout->insertWidget( m_schemaLineLayout->count(),
                                      createStaticToken( Dot ) );
    m_schemaLineLayout->insertWidget( m_schemaLineLayout->count(),
                                      createStaticToken( FileType ) );


    m_syntaxLabel->setText( i18nc("Please do not translate the %foo% words as they define a syntax used internally by a parser to describe a filename.",
                          // xgettext: no-c-format
                          "The following tokens can be used to define a filename scheme: \
                          <br>%track%, %title%, %artist%, %composer%, %year%, %album%, %albumartist%, %comment%, %genre%, %initial%, %folder%, %filetype%, %discnumber%." ) );

    populateConfiguration();

    connect( m_optionsWidget, SIGNAL(optionsChanged()), SIGNAL(schemeChanged()));
}


// ------------------------- TagGuesserWidget -------------------

TagGuesserWidget::TagGuesserWidget( QWidget *parent )
    : FilenameLayoutWidget( parent )
{
    m_configCategory = "FilenameLayoutWidget";

    m_filenamePreview = new QLabel();
    m_filenamePreview->setAlignment( Qt::AlignHCenter );
    m_mainLayout->addWidget( m_filenamePreview );

    m_optionsWidget =  new TagGuessOptionWidget();
    m_mainLayout->addWidget( m_optionsWidget );

    m_tokenPool->addToken( createToken( Title ) );
    m_tokenPool->addToken( createToken( Artist ) );
    m_tokenPool->addToken( createToken( Composer ) );
    m_tokenPool->addToken( createToken( Track ) );
    m_tokenPool->addToken( createToken( Year ) );
    m_tokenPool->addToken( createToken( Album ) );
    m_tokenPool->addToken( createToken( AlbumArtist ) );
    m_tokenPool->addToken( createToken( Comment ) );
    m_tokenPool->addToken( createToken( Genre ) );
    m_tokenPool->addToken( createToken( Ignore ) );
    m_tokenPool->addToken( createToken( Slash ) );
    m_tokenPool->addToken( createToken( Underscore ) );
    m_tokenPool->addToken( createToken( Dash ) );
    m_tokenPool->addToken( createToken( Dot ) );
    m_tokenPool->addToken( createToken( Space ) );

    m_syntaxLabel->setText( i18nc("Please do not translate the %foo% words as they define a syntax used internally by a parser to describe a filename.",
                          // xgettext: no-c-format
                          "The following tokens can be used to define a filename scheme:<br> \
                          <font color=\"%1\">%track%</font>, <font color=\"%2\">%title%</font>, \
                          <font color=\"%3\">%artist%</font>, <font color=\"%4\">%composer%</font>, \
                          <font color=\"%5\">%year%</font>, <font color=\"%6\">%album%</font>, \
                          <font color=\"%7\">%albumartist%</font>, <font color=\"%8\">%comment%</font>, \
                          <font color=\"%9\">%genre%</font>, %ignore%."
                          , QColor( track_color ).name(), QColor( title_color ).name(), QColor( artist_color ).name(), \
                          QColor( composer_color ).name(), QColor( year_color ).name(), QColor( album_color ).name(), QColor( albumartist_color ).name(), \
                          QColor( comment_color ).name(), QColor( genre_color ).name() ) );

    populateConfiguration();

    connect( m_optionsWidget, SIGNAL( optionsChanged() ),
             this, SLOT( updatePreview() ) );

    connect( m_dropTarget, SIGNAL( changed() ),
             this, SLOT( updatePreview() ) );
    connect( m_filenameLayoutEdit, SIGNAL( textChanged( const QString & ) ),
             this, SLOT( updatePreview() ) );
}

//Sets Filename for Preview
void
TagGuesserWidget::setFileName( const QString& fileName )
{
    m_filename = fileName;
    updatePreview();
}

QString
TagGuesserWidget::getParsableFileName()
{
    return parsableFileName( QFileInfo( m_filename ) );
}

//Stores the configuration when the dialog is accepted.
void
TagGuesserWidget::onAccept()    //SLOT
{
    FilenameLayoutWidget::onAccept();

    Amarok::config( "TagGuesser" ).writeEntry( "Case options", m_optionsWidget->getCaseOptions() );
    Amarok::config( "TagGuesser" ).writeEntry( "Eliminate trailing spaces", m_optionsWidget->getWhitespaceOptions() );
    Amarok::config( "TagGuesser" ).writeEntry( "Replace underscores", m_optionsWidget->getUnderscoreOptions() );
    // Amarok::config( "TagGuesser" ).writeEntry( "Use full file path", cbUseFullPath->isChecked() );
}

QMap<qint64,QString>
TagGuesserWidget::guessedTags()
{
    QString scheme = getParsableScheme();
    QString fileName = getParsableFileName();

    if( scheme.isEmpty() )
        return QMap<qint64,QString>();

    TagGuesser guesser;
    guesser.setFilename( fileName );
    guesser.setCaseType( m_optionsWidget->getCaseOptions() );
    guesser.setConvertUnderscores( m_optionsWidget->getUnderscoreOptions() );
    guesser.setCutTrailingSpaces( m_optionsWidget->getWhitespaceOptions() );
    guesser.setSchema( scheme );

    if( !guesser.guess() )
    {
        m_filenamePreview->setText( getParsableFileName() );
        return QMap<qint64,QString>();
    }

    m_filenamePreview->setText(guesser.coloredFileName());
    return guesser.tags();
}

//Updates the Filename Preview
void
TagGuesserWidget::updatePreview()                 //SLOT
{
    DEBUG_BLOCK;

    QMap<qint64,QString> tags = guessedTags();

    QString emptyTagText = i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" );

    quint64 fields[] = {
        Meta::valAlbum,
        Meta::valAlbumArtist,
        Meta::valTitle,
        Meta::valAlbum,
        Meta::valArtist,
        Meta::valComposer,
        Meta::valGenre,
        Meta::valComment,
        Meta::valTrackNr,
        Meta::valYear,
        0};

    QLabel *labels[] = {
        m_optionsWidget->Album_result,
        m_optionsWidget->AlbumArtist_result,
        m_optionsWidget->Title_result,
        m_optionsWidget->Album_result,
        m_optionsWidget->Artist_result,
        m_optionsWidget->Composer_result,
        m_optionsWidget->Genre_result,
        m_optionsWidget->Comment_result,
        m_optionsWidget->Track_result,
        m_optionsWidget->Year_result,
        0};

    for( int i = 0; fields[i]; i++ )
    {
        if( tags.contains( fields[i] ) )
            labels[i]->setText( "<font color='" + TagGuesser::fieldColor( fields[i] ) + "'>" + tags[ fields[i] ] + "</font>" );
        else
            labels[i]->setText( emptyTagText );
    }
}

Token*
TagGuesserWidget::createToken(qint64 value) const
{
    Token* token = FilenameLayoutWidget::createToken( value );

    // return colored tokens.
    QColor color = Qt::transparent;
    switch( value )
    {
    case Track: color = QColor( track_color ); break;
    case Title: color = QColor( title_color ); break;
    case Artist: color = QColor( artist_color ); break;
    case Composer: color = QColor( composer_color ); break;
    case Year: color = QColor( year_color ); break;
    case Album: color = QColor( album_color ); break;
    case AlbumArtist: color = QColor( albumartist_color ); break;
    case Comment: color = QColor( comment_color ); break;
    case Genre: color = QColor( genre_color );
    }
    if (color != Qt::transparent)
        token->setTextColor( color );

    return token;
}

QString
TagGuesserWidget::parsableFileName( const QFileInfo &fileInfo ) const
{
    QString path = fileInfo.absoluteFilePath();

    int schemaLevels = getParsableScheme().count( '/' );
    int pathLevels   = path.count( '/' );

    // -- cut paths
    int pos;
    for( pos = 0; pathLevels > schemaLevels && pos < path.length(); pos++ )
        if( path[pos] == '/' )
            pathLevels--;

    // -- cut extension
    int dotPos = path.lastIndexOf( '.' );
    if( dotPos >= 0 )
        dotPos -= pos;

    return path.mid( pos, dotPos );
}



// ------------------------- FilenameLayoutWidget -------------------

FilenameLayoutWidget::FilenameLayoutWidget( QWidget *parent )
    : QWidget( parent )
    , m_advancedMode( false )
{
    m_mainLayout = new QVBoxLayout( this );

    // --- presets
    QHBoxLayout* presetLayout1 = new QHBoxLayout();

    QLabel* presetLabel = new QLabel( i18n("Preset:"), this );
    presetLayout1->addWidget( presetLabel, 0 );

    m_presetCombo = new QComboBox( this );
    m_presetCombo->setWhatsThis( i18n("A list of selectable filename scheme/format presets." ) );
    presetLayout1->addWidget( m_presetCombo, 1 );

    m_mainLayout->addLayout( presetLayout1 );

    // - the preset buttons
    QHBoxLayout* presetLayout2 = new QHBoxLayout();

    m_addPresetButton = new QPushButton( i18n("Add preset"), this );
    m_addPresetButton->setToolTip( i18n("Saves the current scheme/format above as a preset.", 0));
    presetLayout2->addWidget( m_addPresetButton );

    m_updatePresetButton = new QPushButton( i18n("Update preset"), this );
    presetLayout2->addWidget( m_updatePresetButton );

    m_removePresetButton = new QPushButton( i18n("Remove preset"), this );
    m_removePresetButton->setToolTip( i18n("Removes the currently selected format preset") );
    presetLayout2->addWidget( m_removePresetButton );
    presetLayout2->addStretch( 1 );

    m_mainLayout->addLayout( presetLayout2 );

    // -- stacked widget
    QGroupBox* schemeGroup = new QGroupBox( i18n("Scheme"), this );
    QVBoxLayout* schemeGroupLayout = new QVBoxLayout( schemeGroup );

    m_advancedButton = new QPushButton( i18n("Advanced"), this );
    schemeGroupLayout->addWidget( m_advancedButton );

    m_schemeStack = new QStackedWidget( this );

    // - simple schema
    QWidget* simpleLayoutWidget = new QWidget( this );
    QVBoxLayout *simpleLayout = new QVBoxLayout( simpleLayoutWidget );

    // a token pool
    m_tokenPool = new TokenPool( this );
    simpleLayout->addWidget( m_tokenPool );

    // token drop target inside a frame
    QFrame* dropTargetFrame = new QFrame( this );
    dropTargetFrame->setFrameShape(QFrame::StyledPanel);
    dropTargetFrame->setFrameShadow(QFrame::Sunken);
    m_dropTarget = new TokenDropTarget( "application/x-amarok-tag-token", this );
    m_dropTarget->setRowLimit( 1 );

    m_schemaLineLayout = new QHBoxLayout();
    m_schemaLineLayout->addWidget( m_dropTarget );
    dropTargetFrame->setLayout( m_schemaLineLayout );
    simpleLayout->addWidget( dropTargetFrame );

    m_schemeStack->addWidget( simpleLayoutWidget );

    // - advanced schema
    QWidget* advancedLayoutWidget = new QWidget( this );
    QVBoxLayout *advancedLayout = new QVBoxLayout( advancedLayoutWidget );

    m_syntaxLabel = new QLabel( this ); // placeholder for format description
    advancedLayout->addWidget( m_syntaxLabel );

    m_filenameLayoutEdit = new KLineEdit( this );
    advancedLayout->addWidget( m_filenameLayoutEdit );

    m_schemeStack->addWidget( advancedLayoutWidget );

    schemeGroupLayout->addWidget( m_schemeStack );
    // --

    m_mainLayout->addWidget( schemeGroup );

    connect( m_tokenPool, SIGNAL( onDoubleClick( Token * ) ),
             m_dropTarget, SLOT( insertToken( Token* ) ) );
    connect( m_advancedButton, SIGNAL( clicked() ),
             this, SLOT( toggleAdvancedMode() ) );
    connect( m_dropTarget, SIGNAL( changed() ),
             this, SIGNAL( schemeChanged() ) );
    connect( m_dropTarget, SIGNAL( changed() ),
             this, SLOT( slotUpdatePresetButton() ) );
    connect( m_addPresetButton, SIGNAL( clicked( bool ) ),
             this, SLOT( slotAddFormat() ) );
    connect( m_removePresetButton, SIGNAL( clicked( bool ) ),
             this, SLOT( slotRemoveFormat() ) );
    connect( m_updatePresetButton, SIGNAL( clicked( bool ) ),
             this, SLOT( slotUpdateFormat() ) );

    connect( m_filenameLayoutEdit, SIGNAL( textChanged( const QString & ) ),
             this, SIGNAL( schemeChanged() ) );
debug() << "st3.1";
}

Token*
FilenameLayoutWidget::createToken(qint64 value) const
{
    struct TokenDefinition
    {
        QString name;
        const char* iconName;
        qint64 value;
    };

    static const TokenDefinition tokenDefinitions[] = {
        { i18n( "Track number" ),"filename-track-amarok", Track },
        { i18n( "Title" ), "filename-title-amarok", Title },
        { i18n( "Artist" ), "filename-artist-amarok", Artist },
        { i18n( "Composer" ), "filename-composer-amarok", Composer },
        { i18n( "Year" ), "filename-year-amarok", Year },
        { i18n( "Album" ), "filename-album-amarok", Album },
        { i18n( "Album Artist" ), "filename-artist-amarok", AlbumArtist },
        { i18n( "Comment" ), "filename-comment-amarok", Comment },
        { i18n( "Genre" ), "filename-genre-amarok", Genre },

        { i18n( "File type" ), "filename-filetype-amarok", FileType },
        { i18n( "Ignore" ), "filename-ignore-amarok", Ignore },
        { i18n( "Folder" ), "filename-folder-amarok", Folder },
        { i18nc( "Artist's Initial", "Initial" ), "filename-initial-amarok", Initial },
        { i18n( "Disc number" ), "filename-discnumber-amarok", DiscNumber },

        { "/", "filename-slash-amarok", Slash },
        { "_", "filename-underscore-amarok", Underscore },
        { "-", "filename-dash-amarok", Dash },
        { ".", "filename-dot-amarok", Dot },
        { " ", "filename-space-amarok", Space },
        { i18n( "Collection root" ), "collection-amarok", CollectionRoot },
        { QString(), 0, Space }
    };

    for( int i = 0; !tokenDefinitions[i].name.isNull(); ++i )
    {
        if( value == tokenDefinitions[i].value )
        {
            return new Token( tokenDefinitions[i].name,
                              tokenDefinitions[i].iconName,
                              tokenDefinitions[i].value );
        }
    }

    return 0;
}

Token*
FilenameLayoutWidget::createStaticToken(qint64 value) const
{
    Token* token = createToken( value );
    token->setActive( false );
    token->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Preferred );

    return token;
}

//Stores the configuration when the dialog is accepted.
void
FilenameLayoutWidget::onAccept()    //SLOT
{
    slotSaveFormatList();
}

QString
FilenameLayoutWidget::getParsableScheme() const
{
    QString scheme = m_advancedMode ? m_filenameLayoutEdit->text() : dropTargetScheme();

    Amarok::config( m_configCategory ).writeEntry( "Custom Scheme", scheme );
    return scheme;
}

// attempts to set the scheme
void FilenameLayoutWidget::setScheme(const QString& scheme)
{
    m_filenameLayoutEdit->setText( scheme );
    inferScheme( scheme );

    slotUpdatePresetButton();
    emit schemeChanged();
}



//Handles the modifications to the dialog to toggle between advanced and basic editing mode.
void
FilenameLayoutWidget::toggleAdvancedMode()
{
    setAdvancedMode( !m_advancedMode );
}

//handles switching between basic and advanced mode
void
FilenameLayoutWidget::setAdvancedMode( bool isAdvanced )
{
    setScheme( getParsableScheme() ); // setScheme set's both the edit and the drop target
    m_advancedMode = isAdvanced;

    if( isAdvanced )
    {
        m_advancedButton->setText( i18n( "&Basic..." ) );
        m_schemeStack->setCurrentIndex( 1 );
    }
    else // set Basic mode
    {
        m_advancedButton->setText( i18n( "&Advanced..." ) );
        m_schemeStack->setCurrentIndex( 0 );
    }

    QString entryValue  = m_advancedMode ? "Advanced" : "Basic";

    Amarok::config( m_configCategory ).writeEntry( "Mode", entryValue );
}


QString
FilenameLayoutWidget::dropTargetScheme() const
{
    QString parsableScheme = "";

    QList< Token *> list = m_dropTarget->drags( 0 );

    foreach( Token *token, list )
    {
        parsableScheme += typeElements[token->value()];
    }

    return parsableScheme;
}

void
FilenameLayoutWidget::inferScheme( const QString &s ) //SLOT
{
    DEBUG_BLOCK

    debug() << "infering scheme: " << s;

    m_dropTarget->clear();
    for( int i = 0; i < s.size(); )
    {
        if( s.at(i) == '%')
        {
            if( s.midRef( i, 7 ) == "%title%" )
            {
                m_dropTarget->insertToken( createToken( Title ) );
                i += 7;
            }
            else if( s.midRef( i, 7 ) == "%track%" )
            {
                m_dropTarget->insertToken( createToken( Track ) );
                i += 7;
            }
            else if( s.midRef( i, 8 ) == "%artist%" )
            {
                m_dropTarget->insertToken( createToken( Artist ) );
                i += 8;
            }
            else if( s.midRef( i, 10 ) == "%composer%" )
            {
                m_dropTarget->insertToken( createToken( Composer ) );
                i += 10;
            }
            else if( s.midRef( i, 6 ) == "%year%" )
            {
                m_dropTarget->insertToken( createToken( Year ) );
                i += 6;
            }
            else if( s.midRef( i, 13 ) == "%albumartist%" )
            {
                m_dropTarget->insertToken( createToken( AlbumArtist ) );
                i += 13;
            }
            else if( s.midRef( i, 7 ) == "%album%" )
            {
                m_dropTarget->insertToken( createToken( Album ) );
                i += 7;
            }
            else if( s.midRef( i, 9 ) == "%comment%" )
            {
                m_dropTarget->insertToken( createToken( Comment ) );
                i += 9;
            }
            else if( s.midRef( i, 7 ) == "%genre%" )
            {
                m_dropTarget->insertToken( createToken( Genre ) );
                i += 7;
            }
            else if( s.midRef( i, 10 ) == "%filetype%" )
            {
                m_dropTarget->insertToken( createToken( FileType ) );
                i += 10;
            }
            else if( s.midRef( i, 8 ) == "%ignore%" )
            {
                m_dropTarget->insertToken( createToken( Ignore ) );
                i += 8;
            }
            else if( s.midRef( i, 8 ) == "%folder%" )
            {
                m_dropTarget->insertToken( createToken( Folder ) );
                i += 8;
            }
            else if( s.midRef( i, 9 ) == "%initial%" )
            {
                m_dropTarget->insertToken( createToken( Initial ) );
                i += 9;
            }
            else if( s.midRef( i, 12 ) == "%discnumber%" )
            {
                m_dropTarget->insertToken( createToken( DiscNumber ) );
                i += 12;
            }
            else
                ++i; // skip junk
        }
        else
        {
            if( s.at(i) == '_' )
                m_dropTarget->insertToken( createToken( Underscore ) );
            else if( s.at(i) == '-' )
                m_dropTarget->insertToken( createToken( Dash ) );
            else if( s.at(i) == '.' )
                m_dropTarget->insertToken( createToken( Dot ) );
            else if( s.at(i) == ' ' )
                m_dropTarget->insertToken( createToken( Space ) );
            else if( s.at(i) == '/' )
                m_dropTarget->insertToken( createToken( Slash ) );
            else
                debug() << "'" << s.at(i) << "' can't be represented as TokenLayoutWidget Token";
            i++;
        }
    }
}

void
FilenameLayoutWidget::populateConfiguration()
{
    QString mode = Amarok::config( m_configCategory ).readEntry( "Mode" );
    setAdvancedMode( mode == QLatin1String( "Advanced" ) );

    setScheme( Amarok::config( m_configCategory ).readEntryUntranslated( "Custom Scheme" ) );

    populateFormatList();
}


void
FilenameLayoutWidget::populateFormatList()
{
    DEBUG_BLOCK

    // items are stored in the config list in the following format:
    // Label#DELIM#format string#DELIM#selected
    // the last item to have the third parameter is the default selected preset
    // the third param isnis optional
    QStringList presets_raw;
    int selected_index = -1;
    m_presetCombo->clear();
    presets_raw = AmarokConfig::formatPresets();
    // presets_raw = Amarok::config( m_configCategory ).readEntry( QString::fromLatin1( "Format Presets" ), QStringList() );

    debug() << "--- got preset for" << m_configCategory << presets_raw;

    foreach( QString str, presets_raw )
    {
        QStringList items;
        items = str.split( "#DELIM#", QString::SkipEmptyParts );
        if( items.size() < 2 )
            continue;
        m_presetCombo->addItem( items.at( 0 ), items.at( 1 ) ); // Label, format string
        if( items.size() == 3 )
            selected_index = m_presetCombo->findData( items.at( 1 ) );
    }

    if( selected_index > 0 )
        m_presetCombo->setCurrentIndex( selected_index );

    slotFormatPresetSelected( selected_index );
    connect( m_presetCombo, SIGNAL( currentIndexChanged( int ) ), this, SLOT( slotFormatPresetSelected( int ) ) );
}

void
FilenameLayoutWidget::slotUpdatePresetButton()
{
    QString comboScheme = m_presetCombo->itemData( m_presetCombo->currentIndex() ).  toString();
    m_updatePresetButton->setEnabled( comboScheme != getParsableScheme() );
}

void
FilenameLayoutWidget::slotSaveFormatList()
{
    if( !m_formatListModified )
        return;

    QStringList presets;
    int n = m_presetCombo->count();
    int current_idx = m_presetCombo->currentIndex();

    for( int i = 0; i < n; ++i )
    {
        QString item;
        if( i == current_idx )
            item = "%1#DELIM#%2#DELIM#selected";
        else
            item = "%1#DELIM#%2";

        QString scheme = m_presetCombo->itemData( i ).toString();
        QString label = m_presetCombo->itemText( i );
        item = item.arg( label, scheme );
        presets.append( item );
    }

   Amarok::config( m_configCategory ).writeEntry( QString::fromLatin1( "Format Presets" ), presets );
}

void
FilenameLayoutWidget::slotFormatPresetSelected( int index )
{
    QString scheme = m_presetCombo->itemData( index ).toString();
    setScheme( scheme );
}

void
FilenameLayoutWidget::slotAddFormat()
{
    bool ok = false;
    QString name = KInputDialog::getText( i18n( "New Format Preset" ), i18n( "Preset Name" ), i18n( "New Preset" ),  &ok, this );
    if( !ok )
        return; // user canceled.

    QString format = getParsableScheme();
    m_presetCombo->insertItem(0, name, format);
    m_presetCombo->setCurrentIndex( 0 );
    m_formatListModified = true;
}

void
FilenameLayoutWidget::slotRemoveFormat()
{
    int idx = m_presetCombo->currentIndex();
    m_presetCombo->removeItem( idx );
    m_formatListModified = true;
}

void
FilenameLayoutWidget::slotUpdateFormat()
{
    int idx = m_presetCombo->currentIndex();
    QString formatString = getParsableScheme();
    m_presetCombo->setItemData( idx, formatString );
    m_updatePresetButton->setEnabled( false );
    m_formatListModified = true;
}
