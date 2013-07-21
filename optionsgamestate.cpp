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
    main_stacked_widget(NULL), options_page_index(0), gametypeComboBox(NULL), characterComboBox(NULL), difficultyComboBox(NULL), /*difficultyButtonGroup(NULL),*/ permadeathCheckBox(NULL),
    nameLineEdit(NULL),
    load_list(NULL), /*soundCheck(NULL),*/ soundSlider(NULL), lightingCheck(NULL),
    cheat_mode(false), cheat_start_level(0)
{
    try {
        //cheat_mode = true;
        //cheat_start_level = 1;
        LOG("OptionsGamestate::OptionsGamestate()\n");
        optionsGamestate = this;

    //#ifndef Q_OS_ANDROID
        //this->music = NULL;
        if( !lightdistribution_c ) {
            game_g->loadSound("music_intro", string(DEPLOYMENT_PATH) + "music/no_more_magic.ogg", true);
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

        QPushButton *startButton = new QPushButton(tr("Start game"));
        game_g->initButton(startButton);
        startButton->setShortcut(QKeySequence(Qt::Key_Return));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        startButton->setToolTip(tr("Start playing a new game (Return)"));
#endif
        startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(startButton);
        connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStart()));

        QPushButton *loadButton = new QPushButton(tr("Load game"));
        game_g->initButton(loadButton);
        loadButton->setShortcut(QKeySequence(Qt::Key_L));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        loadButton->setToolTip(tr("Load a previously saved game (L)"));
#endif
        loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(loadButton);
        connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoad()));

        QPushButton *optionsButton = new QPushButton(tr("Options"));
        game_g->initButton(optionsButton);
        optionsButton->setShortcut(QKeySequence(Qt::Key_O));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        optionsButton->setToolTip(tr("Configure various game options (O)"));
#endif
        optionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(optionsButton);
        connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));

#ifndef Q_OS_ANDROID
        // applications don't quit on Android.
        QPushButton *quitButton = new QPushButton(tr("Quit game"));
        game_g->initButton(quitButton);
        quitButton->setShortcut(QKeySequence(Qt::Key_Escape));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            quitButton->setToolTip(tr("Exit the game (Escape)"));
#endif
        quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(quitButton);
        connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
#endif

        //throw string("error"); // test
        //throw "unexpected error"; // test
        {
            QHBoxLayout *h_layout = new QHBoxLayout();
            layout->addLayout(h_layout);

            QLabel *titleLabel = new QLabel("erebus v" + QString::number(versionMajor) + "." + QString::number(versionMinor));
            //titleLabel->setStyleSheet("QLabel { color : red; }");
            titleLabel->setFont(game_g->getFontStd());
            h_layout->addWidget(titleLabel);

            QPushButton *offlineHelpButton = new QPushButton(tr("Offline help"));
            game_g->initButton(offlineHelpButton);
            offlineHelpButton->setShortcut(QKeySequence(Qt::Key_H));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            offlineHelpButton->setToolTip(tr("View the game instructions (H)"));
#endif
            //offlineHelpButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            offlineHelpButton->setFont(game_g->getFontStd());
            h_layout->addWidget(offlineHelpButton);
            connect(offlineHelpButton, SIGNAL(clicked()), this, SLOT(clickedOfflineHelp()));

            QPushButton *onlineHelpButton = new QPushButton(tr("Online help"));
            game_g->initButton(onlineHelpButton);
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            onlineHelpButton->setToolTip(tr("Visit the game's website"));
#endif
            //onlineHelpButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            onlineHelpButton->setFont(game_g->getFontStd());
            h_layout->addWidget(onlineHelpButton);
            connect(onlineHelpButton, SIGNAL(clicked()), this, SLOT(clickedOnlineHelp()));
        }

        window->setEnabled(true); // ensure is enabled - may have been disabled if fail to load game, and return to OptionsGamestate
    }
    catch(...) {
        // clean up stuff created in the constructor
        LOG("OptionsGamestate constructor throwing an exception");
        this->cleanup();
        throw;
    }
}

OptionsGamestate::~OptionsGamestate() {
    LOG("OptionsGamestate::~OptionsGamestate()\n");
    this->cleanup();
}

void OptionsGamestate::cleanup() {
    LOG("OptionsGamestate::cleanup()\n");
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
    if( this->main_stacked_widget->count() > 1 ) {
        this->closeAllSubWindows();
    }
    else {
        qApp->quit();
    }
}

void OptionsGamestate::keyPress(QKeyEvent *key_event) {
    if( key_event->key() == Qt::Key_F12 ) {
        qApp->beep();
        if( !cheat_mode ) {
            cheat_mode = true;
            LOG("enabled cheat mode\n");
        }
        cheat_start_level++;
        LOG("set cheat start level to %d\n", cheat_start_level);
    }
}

const int n_options_pages = 3;

void OptionsGamestate::clickedStart() {
    LOG("OptionsGamestate::clickedStart()\n");
    LOG("options_page_index = %d\n", options_page_index);
    /*game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING);
    game_g->pushMessage(game_message);*/
    ASSERT_LOGGER(options_page_index >= 0 && options_page_index < n_options_pages);

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    if( options_page_index == 0 ) {
        {
            {
                int n_row = 0;
                QGridLayout *g_layout = new QGridLayout();
                layout->addLayout(g_layout);

                QLabel *label = new QLabel(tr("Select a game type: "));
                g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
                gametypeComboBox = new QComboBox();
#ifdef Q_OS_ANDROID
                gametypeComboBox->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
                gametypeComboBox->setFont(game_g->getFontBig());
                gametypeComboBox->addItem(tr("Begin Campaign"));
                gametypeComboBox->addItem(tr("Random Dungeon"));
                gametypeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                g_layout->addWidget(gametypeComboBox, n_row, 1);
                n_row++;
            }
        }
    }
    else if( options_page_index == 1 ) {
        if( !smallscreen_c )
        {
            QLabel *label = new QLabel(tr("Choose game settings: "));
            label->setAlignment(Qt::AlignCenter);
            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(label);
        }

        {
            int n_row = 0;
            QGridLayout *g_layout = new QGridLayout();
            layout->addLayout(g_layout);

            QLabel *label = new QLabel(tr("Character: "));
            g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
            characterComboBox = new QComboBox();
#ifdef Q_OS_ANDROID
            characterComboBox->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
            characterComboBox->setFont(game_g->getFontBig());
            for(size_t i=0;i<game_g->getNPlayerTypes();i++) {
                characterComboBox->addItem(game_g->getPlayerType(i).c_str());
            }
            characterComboBox->setCurrentIndex(4); // select Warrior
            characterComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            g_layout->addWidget(characterComboBox, n_row, 1);
            n_row++;

            label = new QLabel(tr("Difficulty: "));
            g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
            difficultyComboBox = new QComboBox();
#ifdef Q_OS_ANDROID
            difficultyComboBox->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
            difficultyComboBox->setFont(game_g->getFontBig());
            for(int i=0;i<N_DIFFICULTIES;i++) {
                Difficulty test_difficulty = (Difficulty)i;
                difficultyComboBox->addItem(game_g->getDifficultyString(test_difficulty).c_str());
            }
            difficultyComboBox->setCurrentIndex(1);
            difficultyComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            g_layout->addWidget(difficultyComboBox, n_row, 1);
            n_row++;

            label = new QLabel(tr("Permadeath: "));
            g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
#ifdef Q_OS_ANDROID
            permadeathCheckBox = new QCheckBox("        "); // needed for workaround due to Android bug
            permadeathCheckBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed otherwise appears too small
#else
            permadeathCheckBox = new QCheckBox("");
#endif
            permadeathCheckBox->setToolTip(tr("If checked, then once your player dies,\nyou won't be able to restore from a save game!"));
            g_layout->addWidget(permadeathCheckBox, n_row, 1);
            n_row++;
        }

    }
    else if( options_page_index == 2 ) {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel(tr("Your name:"));
        h_layout->addWidget(label);

        int character_id = this->characterComboBox->currentIndex();
        nameLineEdit = new QLineEdit( game_g->getPlayerType(character_id).c_str() );
        h_layout->addWidget(nameLineEdit);
        nameLineEdit->setFocus();
        nameLineEdit->setInputMethodHints(Qt::ImhNoPredictiveText); // needed on Android at least due to buggy behaviour (both with default keyboard, and makes Swype crash); probably useful on other platforms
        nameLineEdit->selectAll();
        if( options_page_index == n_options_pages-1 ) {
            connect(nameLineEdit, SIGNAL(returnPressed()), this, SLOT(clickedStartGame()));
        }
        else {
            connect(nameLineEdit, SIGNAL(returnPressed()), this, SLOT(clickedNext()));
        }
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *startButton = new QPushButton(tr("Next"));
        game_g->initButton(startButton);
        startButton->setShortcut(QKeySequence(Qt::Key_Return));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        startButton->setToolTip(tr("Accept the options and proceed to next step (Return)"));
#endif
        startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(startButton);
        if( options_page_index == n_options_pages-1 ) {
            connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStartGame()));
        }
        else {
            connect(startButton, SIGNAL(clicked()), this, SLOT(clickedNext()));
        }

        QPushButton *closeButton = new QPushButton(tr("Cancel"));
        game_g->initButton(closeButton);
        closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        closeButton->setToolTip(tr("Return to main menu (Escape)"));
#endif
        closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(closeButton);
        //connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
        connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedCancel()));
    }
}

void OptionsGamestate::clickedNext() {
    this->options_page_index++;
    this->clickedStart();
}

void OptionsGamestate::clickedCancel() {
    this->options_page_index = 0;
    this->closeAllSubWindows();
}

void OptionsGamestate::clickedStartGame() {
    LOG("OptionsGamestate::clickedStartGame()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    int gametype_id = this->gametypeComboBox->currentIndex();
    LOG("gametype_id: %d\n", gametype_id);
    ASSERT_LOGGER(gametype_id >= 0 && gametype_id <= 1);
    GameType gametype = (GameType)gametype_id;

    //int difficulty_id = this->difficultyButtonGroup->checkedId();
    int difficulty_id = this->difficultyComboBox->currentIndex();
    LOG("difficulty_id: %d\n", difficulty_id);
    ASSERT_LOGGER(difficulty_id >= 0);
    ASSERT_LOGGER(difficulty_id < (int)N_DIFFICULTIES);
    Difficulty difficulty = (Difficulty)difficulty_id;

    bool permadeath = this->permadeathCheckBox->isChecked();

    ASSERT_LOGGER(this->characterComboBox->currentIndex() >= 0);
    ASSERT_LOGGER(this->characterComboBox->currentIndex() < game_g->getNPlayerTypes());

    StartGameMessage *game_message = new StartGameMessage(gametype, difficulty, this->characterComboBox->currentText().toStdString(), permadeath, this->nameLineEdit->text().toStdString(), cheat_mode, cheat_start_level);
    game_g->pushMessage(game_message);
}

void OptionsGamestate::clickedLoad() {
    LOG("OptionsGamestate::clickedLoad()\n");
    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    this->load_list = NULL;
    this->load_filenames.clear();
    game_g->fillSaveGameFiles(&load_list, &load_filenames);

    // load_list only created if it has items
    if( load_list != NULL ) {
        ASSERT_LOGGER(load_list->count() > 0 );
        ASSERT_LOGGER(load_list->count() == load_filenames.size());
        layout->addWidget(load_list);
        load_list->setCurrentRow(0);

        {
            QHBoxLayout *h_layout = new QHBoxLayout();
            layout->addLayout(h_layout);

            QPushButton *loadButton = new QPushButton(tr("Load"));
            game_g->initButton(loadButton);
            loadButton->setShortcut(QKeySequence(Qt::Key_Return));
            //loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            h_layout->addWidget(loadButton);
            connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoadGame()));

#ifdef Q_OS_ANDROID
            if( game_g->isSDCardOk() ) {
                QPushButton *exportButton = new QPushButton(tr("Export to SD Card"));
                game_g->initButton(exportButton);
                h_layout->addWidget(exportButton);
                connect(exportButton, SIGNAL(clicked()), this, SLOT(clickedExportButton()));
            }
#endif
        }
    }
    else {
        QLabel *label = new QLabel(tr("No save game files\navailable"));
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
    }

    QPushButton *closeButton = new QPushButton(tr("Cancel"));
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
    ASSERT_LOGGER(load_list->count() == load_filenames.size());
    ASSERT_LOGGER(index >= 0 && index < load_list->count());
    ASSERT_LOGGER(index >= 0 && index < load_filenames.size());
    QString filename = this->load_filenames.at(index);
    LoadGameMessage *game_message = new LoadGameMessage(filename);
    game_g->pushMessage(game_message);
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
}

void OptionsGamestate::clickedExportButton() {
#ifdef Q_OS_ANDROID
    LOG("OptionsGamestate::clickedExportButton()\n");
    if( load_list == NULL ) {
        qDebug("no load list");
        return;
    }
    if( !game_g->isSDCardOk() ) {
        qDebug("no SD Card access");
        return;
    }
    if( load_filenames.size() == 0 ) {
        qDebug("no filenames");
        return;
    }
    for(size_t i=0;i<load_filenames.size();i++) {
        QString filename = load_filenames.at(i);
        QString full_filename = game_g->getApplicationFilename(savegame_folder + filename);
        game_g->exportFilenameToSDCard(full_filename, filename);
    }
    game_g->showInfoDialog("", "Successfully saved files to SD card");
#endif
}

void OptionsGamestate::clickedOptions() {
    LOG("OptionsGamestate::clickedOptions()\n");

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    {
        int n_row = 0;
        QLabel *label = NULL;
        QGridLayout *g_layout = new QGridLayout();
        layout->addLayout(g_layout);

#ifndef Q_OS_ANDROID
        // on Android, volume is handled by the volume control keys
        label = new QLabel(tr("Volume: "));
        g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
        soundSlider = new QSlider(Qt::Horizontal);
        soundSlider->setMinimum(0);
        soundSlider->setMaximum(100);
        soundSlider->setValue(game_g->getGlobalSoundVolume());
        g_layout->addWidget(soundSlider, n_row, 1);
        n_row++;
#endif

        label = new QLabel(tr("Lighting (uncheck if too slow): "));
        g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
#ifdef Q_OS_ANDROID
        lightingCheck = new QCheckBox("        "); // needed for workaround due to Android bug
        lightingCheck->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed otherwise appears too small
#else
        lightingCheck = new QCheckBox("");
#endif
        lightingCheck->setChecked(game_g->isLightingEnabled());
        g_layout->addWidget(lightingCheck, n_row, 1);
        n_row++;
    }

    QPushButton *okayButton = new QPushButton(tr("Okay"));
    game_g->initButton(okayButton);
    okayButton->setShortcut(QKeySequence(Qt::Key_Return));
    okayButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(okayButton);
    connect(okayButton, SIGNAL(clicked()), this, SLOT(clickedOptionsOkay()));

    QPushButton *closeButton = new QPushButton(tr("Cancel"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedOptionsOkay() {
    LOG("OptionsGamestate::clickedOptionsOkay");

    /*bool new_sound_enabled = soundCheck->isChecked();
    if( new_sound_enabled != game_g->isSoundEnabled() ) {
        game_g->setSoundEnabled(new_sound_enabled);
        if( new_sound_enabled ) {
            game_g->playSound("music_intro");
        }
        else {
            game_g->pauseSound("music_intro");
        }
    }*/
#ifndef Q_OS_ANDROID
    game_g->setGlobalSoundVolume( soundSlider->value() );
    game_g->updateSoundVolume("music_intro");
#endif

    bool new_lighting_enabled = lightingCheck->isChecked();
    if( new_lighting_enabled != game_g->isLightingEnabled() ) {
        game_g->setLightingEnabled(new_lighting_enabled);
    }

    this->closeAllSubWindows();
}

void OptionsGamestate::clickedOnlineHelp() {
    LOG("OptionsGamestate::clickedOnlineHelp\n");
    QDesktopServices::openUrl(QUrl("http://erebusrpg.sourceforge.net/erebus.html"));
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

    QPushButton *closeButton = new QPushButton(tr("Close"));
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
