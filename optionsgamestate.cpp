#ifdef _DEBUG
#include <cassert>
#endif

#include <QtWebKit/QWebView>

#include "optionsgamestate.h"
#include "game.h"
#include "qt_screen.h"
#include "mainwindow.h"
#include "logiface.h"

OptionsGamestate *OptionsGamestate::optionsGamestate = NULL;

OptionsGamestate::OptionsGamestate() :
    main_stacked_widget(NULL), difficultyComboBox(NULL), /*difficultyButtonGroup(NULL),*/ characterComboBox(NULL), load_list(NULL), soundCheck(NULL), lightingCheck(NULL)
{
    LOG("OptionsGamestate::OptionsGamestate()\n");
    optionsGamestate = this;

//#ifndef Q_OS_ANDROID
    //this->music = NULL;
    if( !mobile_c ) {
        /*music = new Phonon::MediaObject(qApp);
        connect(music, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
        music->setCurrentSource(Phonon::MediaSource("music/no_more_magic.ogg"));
        Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
        Phonon::Path audioPath = Phonon::createPath(music, audioOutput);*/
        /*music = game_g->loadSound("music/no_more_magic.ogg");
        if( game_g->isSoundEnabled() ) {
            music->play();
        }*/
        game_g->loadSound("music_intro", "music/no_more_magic.ogg");
        game_g->playSound("music_intro");
    }
//#endif

    MainWindow *window = game_g->getScreen()->getMainWindow();
    QFont font = game_g->getFontBig();
    window->setFont(font);

    this->main_stacked_widget = new QStackedWidget();
    main_stacked_widget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(main_stacked_widget);

    QWidget *centralWidget = new QWidget(window);
    //centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    //window->setCentralWidget(centralWidget);
    main_stacked_widget->addWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    centralWidget->setLayout(layout);

    QPushButton *startButton = new QPushButton("Start game");
    game_g->initButton(startButton);
    startButton->setShortcut(QKeySequence(Qt::Key_S));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
    startButton->setToolTip("Start playing a new game (S)");
#endif
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStart()));

    QPushButton *loadButton = new QPushButton("Load game");
    game_g->initButton(loadButton);
    loadButton->setShortcut(QKeySequence(Qt::Key_L));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
    loadButton->setToolTip("Load a previously saved game (L)");
#endif
    loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoad()));

    QPushButton *optionsButton = new QPushButton("Options");
    game_g->initButton(optionsButton);
    optionsButton->setShortcut(QKeySequence(Qt::Key_O));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
    optionsButton->setToolTip("Configure various game options (O)");
#endif
    optionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(optionsButton);
    connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));

#ifndef Q_OS_ANDROID
    // applications don't quit on Android.
    QPushButton *quitButton = new QPushButton("Quit game");
    game_g->initButton(quitButton);
    quitButton->setShortcut(QKeySequence(Qt::Key_Escape));
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
#endif

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *titleLabel = new QLabel("erebus v" + QString::number(versionMajor) + "." + QString::number(versionMinor));
        //titleLabel->setAlignment(Qt::AlignCenter);
        //titleLabel->setStyleSheet("QLabel { color : red; }");
        titleLabel->setFont(game_g->getFontStd());
        h_layout->addWidget(titleLabel);

        QPushButton *offlineHelpButton = new QPushButton("Offline help");
        game_g->initButton(offlineHelpButton);
        offlineHelpButton->setShortcut(QKeySequence(Qt::Key_H));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        offlineHelpButton->setToolTip("View the game instructions (H)");
#endif
        //offlineHelpButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        offlineHelpButton->setFont(game_g->getFontStd());
        h_layout->addWidget(offlineHelpButton);
        connect(offlineHelpButton, SIGNAL(clicked()), this, SLOT(clickedOfflineHelp()));

        QPushButton *onlineHelpButton = new QPushButton("Online help");
        game_g->initButton(onlineHelpButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        onlineHelpButton->setToolTip("Visit the game's website");
#endif
        //onlineHelpButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        onlineHelpButton->setFont(game_g->getFontStd());
        h_layout->addWidget(onlineHelpButton);
        connect(onlineHelpButton, SIGNAL(clicked()), this, SLOT(clickedOnlineHelp()));
    }

    window->setEnabled(true); // ensure is enabled - may have been disabled if fail to load game, and return to OptionsGamestate
}

OptionsGamestate::~OptionsGamestate() {
    LOG("OptionsGamestate::~OptionsGamestate()\n");
    /*VI_flush(0); // delete all the gamestate objects, but leave the game level objects (which should be set at persistence level -1)
    VI_GraphicsEnvironment *genv = game_g->getGraphicsEnvironment();
    game_g->getGraphicsEnvironment()->setPanel(NULL); // as the main panel is now destroyed
    */
/*#ifndef Q_OS_ANDROID
    if( music != NULL ) {
        delete music;
    }
#endif*/
    game_g->freeSound("music_intro");

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    optionsGamestate = NULL;
    LOG("deleted OptionsGamestate\n");
}

/*#ifndef Q_OS_ANDROID
void OptionsGamestate::mediaStateChanged(Phonon::State newstate, Phonon::State oldstate) {
    LOG("OptionsGamestate::mediaStateChanged(%d, %d)\n", newstate, oldstate);
    if( newstate == Phonon::ErrorState ) {
        LOG("phonon reports error!: %d\n", music->errorType());
        LOG("error string: %s\n", music->errorString().toStdString().c_str());
    }
}
#endif*/

void OptionsGamestate::quitGame() {
    LOG("OptionsGamestate::quitGame()\n");
    qApp->quit();
}

void OptionsGamestate::clickedStart() {
    LOG("OptionsGamestate::clickedStart()\n");
    /*game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING);
    game_g->pushMessage(game_message);*/

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel("Character: ");
        label->setAlignment(Qt::AlignCenter);
        h_layout->addWidget(label);

        characterComboBox = new QComboBox();
        characterComboBox->setStyleSheet("color: black;"); // workaround for Android color bug
        characterComboBox->setFont(game_g->getFontBig());
        for(size_t i=0;i<game_g->getNPlayerTypes();i++) {
            characterComboBox->addItem(game_g->getPlayerType(i).c_str());
        }
        characterComboBox->setCurrentIndex(4); // select Warrior
        characterComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(characterComboBox);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel("Difficulty: ");
        label->setAlignment(Qt::AlignCenter);
        h_layout->addWidget(label);

        difficultyComboBox = new QComboBox();
        difficultyComboBox->setStyleSheet("color: black;"); // workaround for Android color bug
        difficultyComboBox->setFont(game_g->getFontBig());
        for(int i=0;i<N_DIFFICULTIES;i++) {
            Difficulty test_difficulty = (Difficulty)i;
            difficultyComboBox->addItem(game_g->getDifficultyString(test_difficulty).c_str());
        }
        difficultyComboBox->setCurrentIndex(1);
        difficultyComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(difficultyComboBox);

        /*QVBoxLayout *v_layout = new QVBoxLayout();
        h_layout->addLayout(v_layout);

        difficultyButtonGroup = new QButtonGroup(this);
        for(int i=0;i<N_DIFFICULTIES;i++) {
            Difficulty test_difficulty = (Difficulty)i;
            QRadioButton *radio = new QRadioButton( game_g->getDifficultyString(test_difficulty).c_str() );
            v_layout->addWidget(radio);
            difficultyButtonGroup->addButton(radio, i);
            if( test_difficulty == DIFFICULTY_MEDIUM ) {
                radio->setChecked(true);
            }
        }*/
    }

    QPushButton *startButton = new QPushButton("Start");
    game_g->initButton(startButton);
    startButton->setShortcut(QKeySequence(Qt::Key_Return));
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStartGame()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedStartGame() {
    LOG("OptionsGamestate::clickedStartGame()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    //int difficulty_id = this->difficultyButtonGroup->checkedId();
    int difficulty_id = this->difficultyComboBox->currentIndex();
    LOG("difficulty_id: %d\n", difficulty_id);
    ASSERT_LOGGER(difficulty_id > 0);
    ASSERT_LOGGER(difficulty_id < (int)N_DIFFICULTIES);
    Difficulty difficulty = (Difficulty)difficulty_id;
    ASSERT_LOGGER(this->characterComboBox->currentIndex() >= 0);
    ASSERT_LOGGER(this->characterComboBox->currentIndex() < game_g->getNPlayerTypes());

    StartGameMessage *game_message = new StartGameMessage(difficulty, this->characterComboBox->currentIndex());
    game_g->pushMessage(game_message);
}

void OptionsGamestate::clickedLoad() {
    LOG("OptionsGamestate::clickedLoad()\n");
    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    if( files.size() > 0 ) {
        load_list = new ScrollingListWidget();
        if( !mobile_c ) {
            QFont list_font = load_list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            load_list->setFont(list_font);
        }
        layout->addWidget(load_list);
        load_list->setSelectionMode(QAbstractItemView::SingleSelection);

        for(int i=0;i<files.size();i++) {
            const QString &file = files.at(i);
            load_list->addItem(file);
        }

        load_list->setCurrentRow(0);

        QPushButton *loadButton = new QPushButton("Load");
        game_g->initButton(loadButton);
        loadButton->setShortcut(QKeySequence(Qt::Key_Return));
        //loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(loadButton);
        connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoadGame()));

    }
    else {
        QLabel *label = new QLabel("No save game files\navailable");
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
    }

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedLoadGame() {
    LOG("OptionsGamestate::clickedLoadGame()\n");
    if( load_list == NULL ) {
        return;
    }
    int index = load_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < load_list->count() );
    QString filename = this->load_list->item(index)->text();
    LoadGameMessage *game_message = new LoadGameMessage(filename.toStdString());
    game_g->pushMessage(game_message);
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
}

void OptionsGamestate::clickedOptions() {
    LOG("OptionsGamestate::clickedOptions()\n");

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    soundCheck = new QCheckBox("Sound");
    soundCheck->setChecked(game_g->isSoundEnabled());
    layout->addWidget(soundCheck);

    lightingCheck = new QCheckBox("Lighting Effects");
    lightingCheck->setChecked(game_g->isLightingEnabled());
    layout->addWidget(lightingCheck);

    QPushButton *okayButton = new QPushButton("Okay");
    game_g->initButton(okayButton);
    okayButton->setShortcut(QKeySequence(Qt::Key_Return));
    okayButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(okayButton);
    connect(okayButton, SIGNAL(clicked()), this, SLOT(clickedOptionsOkay()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedOptionsOkay() {
    LOG("OptionsGamestate::clickedOptionsOkay");

    bool new_sound_enabled = soundCheck->isChecked();
    if( new_sound_enabled != game_g->isSoundEnabled() ) {
        game_g->setSoundEnabled(new_sound_enabled);
        /*if( music != NULL ) {
            if( new_sound_enabled ) {
                this->music->play();
            }
            else {
                this->music->pause();
            }
        }*/
        if( new_sound_enabled ) {
            game_g->playSound("music_intro");
        }
        else {
            game_g->pauseSound("music_intro");
        }
    }

    bool new_lighting_enabled = lightingCheck->isChecked();
    if( new_lighting_enabled != game_g->isLightingEnabled() ) {
        game_g->setLightingEnabled(new_lighting_enabled);
    }

    this->closeAllSubWindows();
}

void OptionsGamestate::clickedOnlineHelp() {
    LOG("OptionsGamestate::clickedOnlineHelp\n");
    QDesktopServices::openUrl(QUrl("http://homepage.ntlworld.com/mark.harman/erebus.html"));
}

void OptionsGamestate::clickedOfflineHelp() {
    LOG("OptionsGamestate::clickedOfflineHelp\n");

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    QWebView *help = new QWebView();
    game_g->setWebView(help);
    help->setUrl(QUrl(QString(DEPLOYMENT_PATH) + "docs/erebus.html"));
    help->show();
    layout->addWidget(help);

    QPushButton *closeButton = new QPushButton("Close");
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //closeButton->setFont(game_g->getFontSmall());
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::closeAllSubWindows() {
    LOG("OptionsGamestate::closeAllSubWindows\n");
    while( this->main_stacked_widget->count() > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(this->main_stacked_widget->count()-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
    }
}

void OptionsGamestate::clickedQuit() {
    LOG("OptionsGamestate::clickedQuit()\n");
    this->quitGame();
}
