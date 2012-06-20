#ifdef _DEBUG
#include <cassert>
#endif

#include "optionsgamestate.h"
#include "game.h"
#include "qt_screen.h"
#include "mainwindow.h"

OptionsGamestate *OptionsGamestate::optionsGamestate = NULL;

OptionsGamestate::OptionsGamestate() :
    main_stacked_widget(NULL), difficultyComboBox(NULL), load_list(NULL), soundCheck(NULL)
{
    LOG("OptionsGamestate::OptionsGamestate()\n");
    optionsGamestate = this;

#ifndef Q_OS_ANDROID
    this->music = NULL;
    if( !mobile_c ) {
        /*music = new Phonon::MediaObject(qApp);
        connect(music, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
        music->setCurrentSource(Phonon::MediaSource("music/no_more_magic.ogg"));
        Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
        Phonon::Path audioPath = Phonon::createPath(music, audioOutput);*/
        music = game_g->loadSound("music/no_more_magic.ogg");
        if( game_g->isSoundEnabled() ) {
            music->play();
        }
    }
#endif

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
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStart()));

    QPushButton *loadButton = new QPushButton("Load game");
    game_g->initButton(loadButton);
    loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoad()));

    QPushButton *optionsButton = new QPushButton("Options");
    game_g->initButton(optionsButton);
    optionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(optionsButton);
    connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));

#ifndef Q_OS_ANDROID
    // applications don't quit on Android.
    QPushButton *quitButton = new QPushButton("Quit game");
    game_g->initButton(quitButton);
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
#endif

    QLabel *titleLabel = new QLabel("erebus v" + QString::number(versionMajor) + "." + QString::number(versionMinor));
    //titleLabel->setAlignment(Qt::AlignCenter);
    //titleLabel->setStyleSheet("QLabel { color : red; }");
    titleLabel->setFont(game_g->getFontSmall());
    layout->addWidget(titleLabel);

    window->setEnabled(true); // ensure is enabled - may have been disabled if fail to load game, and return to OptionsGamestate
}

OptionsGamestate::~OptionsGamestate() {
    LOG("OptionsGamestate::~OptionsGamestate()\n");
    /*VI_flush(0); // delete all the gamestate objects, but leave the game level objects (which should be set at persistence level -1)
    VI_GraphicsEnvironment *genv = game_g->getGraphicsEnvironment();
    game_g->getGraphicsEnvironment()->setPanel(NULL); // as the main panel is now destroyed
    */
#ifndef Q_OS_ANDROID
    if( music != NULL ) {
        delete music;
    }
#endif

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

        QLabel *label = new QLabel("Difficulty: ");
        label->setAlignment(Qt::AlignRight);
        h_layout->addWidget(label);

        difficultyComboBox = new QComboBox();
        for(int i=0;i<N_DIFFICULTIES;i++) {
            Difficulty test_difficulty = (Difficulty)i;
            difficultyComboBox->addItem(game_g->getDifficultyString(test_difficulty).c_str());
        }
        difficultyComboBox->setCurrentIndex(1);
        h_layout->addWidget(difficultyComboBox);
    }

    QPushButton *startButton = new QPushButton("Start");
    game_g->initButton(startButton);
    //startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStartGame()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedStartGame() {
    LOG("OptionsGamestate::clickedStartGame()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    //GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING);
    ASSERT_LOGGER(this->difficultyComboBox->currentIndex() < (int)N_DIFFICULTIES);
    Difficulty difficulty = (Difficulty)this->difficultyComboBox->currentIndex();
    StartGameMessage *game_message = new StartGameMessage(difficulty);
    game_g->pushMessage(game_message);
}

void OptionsGamestate::clickedLoad() {
    LOG("OptionsGamestate::clickedLoad()\n");
    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    load_list = new ScrollingListWidget();
    if( !mobile_c ) {
        QFont list_font = load_list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        load_list->setFont(list_font);
    }
    layout->addWidget(load_list);
    load_list->setSelectionMode(QAbstractItemView::SingleSelection);

    QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    for(int i=0;i<files.size();i++) {
        const QString &file = files.at(i);
        load_list->addItem(file);
    }

    load_list->setCurrentRow(0);

    QPushButton *loadButton = new QPushButton("Load");
    game_g->initButton(loadButton);
    //loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoadGame()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedLoadGame() {
    LOG("OptionsGamestate::clickedLoadGame()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    //GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD);
    int index = load_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < load_list->count() );
    QString filename = this->load_list->item(index)->text();
    LoadGameMessage *game_message = new LoadGameMessage(filename.toStdString());
    game_g->pushMessage(game_message);
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

    QPushButton *okayButton = new QPushButton("Okay");
    game_g->initButton(okayButton);
    okayButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(okayButton);
    connect(okayButton, SIGNAL(clicked()), this, SLOT(clickedOptionsOkay()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedOptionsOkay() {
    LOG("OptionsGamestate::clickedOptionsOkay");

    bool new_sound_enabled = soundCheck->isChecked();
    if( new_sound_enabled != game_g->isSoundEnabled() ) {
        game_g->setSoundEnabled(new_sound_enabled);
#ifndef Q_OS_ANDROID
        if( new_sound_enabled ) {
            this->music->play();
        }
        else {
            this->music->pause();
        }
#endif
    }

    this->closeAllSubWindows();
}

void OptionsGamestate::closeAllSubWindows() {
    LOG("OptionsGamestate::closeAllSubWindows");
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
