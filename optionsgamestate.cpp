#include <sstream>
using std::stringstream;

#ifdef _DEBUG
#include <cassert>
#endif

#include <QApplication>
#include <QDesktopWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDesktopServices>
#include <QTextBrowser>
#include <QFile>
#include <QTextStream>

#include "rpg/character.h"

#include "optionsgamestate.h"
#include "game.h"
#include "mainwindow.h"
#include "infodialog.h"
#include "logiface.h"

Help::Help(OptionsGamestate *options_gamestate) : QTextEdit(), options_gamestate(options_gamestate) {
    game_g->setTextEdit(this);
}

GameTypeHelp::GameTypeHelp(OptionsGamestate *options_gamestate) : Help(options_gamestate) {
}

void GameTypeHelp::changedGameType(int index) {
    LOG("GameTypeHelp::changedGameType(%d)\n", index);
    if( index == GAMETYPE_CAMPAIGN ) {
        //this->setText("Play through the series of quests that have been written for Erebus.");
        this->setHtml("Play through the series of quests that have been written for Erebus.");
    }
    else if( index == GAMETYPE_RANDOM ) {
        //this->setText("Play a single one-off dungeon that is randomly generated. There is no objective to random dungeons, but you can see how much experience (XP) you can achieve, or perhaps how much gold you can find!");
        this->setHtml("Play a single one-off dungeon that is randomly generated. There is no objective to random dungeons, but you can see how much experience (XP) you can achieve, or perhaps how much gold you can find!");
    }
    else {
        //this->setText("");
        this->setHtml("");
    }
}

CharacterHelp::CharacterHelp(OptionsGamestate *options_gamestate) : Help(options_gamestate) {
}

void CharacterHelp::changedGameType(int index) {
    LOG("CharacterHelp::changedGameType(%d)\n", index);
    string player = options_gamestate->characterComboBox->currentText().toStdString();
    Character *character = game_g->createPlayer(player, "");
    QString html = "";
    html += "<table>";
    html += character->writeStat(profile_key_FP_c, false, true);
    html += character->writeStat(profile_key_BS_c, false, true);
    html += character->writeStat(profile_key_S_c, false, true);
    html += character->writeStat(profile_key_A_c, false, true);
    html += character->writeStat(profile_key_M_c, false, true);
    html += character->writeStat(profile_key_D_c, false, true);
    html += character->writeStat(profile_key_B_c, false, true);
    html += character->writeStat(profile_key_Sp_c, true, true);
    html += "<tr>";
    html += "<td>";
    html += "<b>Health:</b> ";
    html += "</td>";
    html += "<td>";
    html += QString::number(character->getMaxHealth());
    //html += "<br/><br/>";
    html += " ";

    html += "</td>";
    html += "</tr>";
    html += "</table>";

    html += "<hr/>";
    html += character->writeSkills();

    html += character->getBiography().c_str();
    html += "<br/>";

    this->setHtml(html);

    // also update portrait
    QPixmap pixmap = game_g->getPortraitImage(character->getPortrait());
    int height = QApplication::desktop()->availableGeometry().height();
    int max_pic_height = height/3;
    qDebug("pixmap height %d , height %d , max_pic_height %d", pixmap.height(), height, max_pic_height);
    if( pixmap.height() > max_pic_height ) {
        qDebug("    scale...");
        pixmap = pixmap.scaledToHeight(max_pic_height, Qt::SmoothTransformation);
    }
    options_gamestate->portraitLabel->setPixmap(pixmap);

    delete character;
}

OptionsGamestate *OptionsGamestate::optionsGamestate = NULL;

OptionsGamestate::OptionsGamestate() :
    main_stacked_widget(NULL), options_page_index(0), gametypeComboBox(NULL), characterComboBox(NULL), portraitLabel(NULL), difficultyComboBox(NULL), /*difficultyButtonGroup(NULL),*/ permadeathCheckBox(NULL),
    nameLineEdit(NULL),
    load_list(NULL),
#ifndef Q_OS_ANDROID
    soundSlider(NULL),
    soundSliderMusic(NULL),
    soundSliderEffects(NULL),
#endif
    lightingCheck(NULL),
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

        MainWindow *window = game_g->getMainWindow();
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

    MainWindow *window = game_g->getMainWindow();
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
        this->clickedCancel();
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
                g_layout->setColumnStretch(1, 1);
                layout->addLayout(g_layout);

                QLabel *label = new QLabel(tr("Select a game type: "));
                g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
                gametypeComboBox = new QComboBox();
#ifdef Q_OS_ANDROID
                gametypeComboBox->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
                gametypeComboBox->setFont(game_g->getFontBig());
                // should match order of GameType enum
                gametypeComboBox->addItem(tr("Begin Campaign"));
                gametypeComboBox->addItem(tr("Random Dungeon"));
                //gametypeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                gametypeComboBox->setFocus(); // sometimes don't get focus (meaning we have to click twice to make the combo list popup)?
                g_layout->addWidget(gametypeComboBox, n_row, 1);
                n_row++;

            }
            // be careful if we change this code - need to ensure layout still works okay, including on Android
            GameTypeHelp *helpLabel = new GameTypeHelp(this);
            //helpLabel->setWordWrap(true);
            //helpLabel->setFont(game_g->getFontSmall());
            connect(gametypeComboBox, SIGNAL(currentIndexChanged(int)), helpLabel, SLOT(changedGameType(int)));
            layout->addWidget(helpLabel);
            helpLabel->changedGameType( gametypeComboBox->currentIndex() ); // force initial update
        }
    }
    else if( options_page_index == 1 ) {
        if( !smallscreen_c )
        {
            QLabel *label = new QLabel(tr("Choose game settings: "));
            label->setAlignment(Qt::AlignCenter);
            //label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
        }

        {
            // be careful if we change this code - need to ensure layout still works okay, including on Android
            QHBoxLayout *h_layout = new QHBoxLayout();
            layout->addLayout(h_layout);

            this->portraitLabel = new QLabel();
            h_layout->addWidget(portraitLabel);

            CharacterHelp *helpLabel = new CharacterHelp(this);
            connect(characterComboBox, SIGNAL(currentIndexChanged(int)), helpLabel, SLOT(changedGameType(int)));
            h_layout->addWidget(helpLabel);
            helpLabel->changedGameType( characterComboBox->currentIndex() ); // force initial update
        }
    }
    else if( options_page_index == 2 ) {
        {
            int n_row = 0;
            QGridLayout *g_layout = new QGridLayout();
            layout->addLayout(g_layout);

            QLabel *label = new QLabel(tr("Your name:"));
            g_layout->addWidget(label, n_row, 0, Qt::AlignRight);

            int character_id = this->characterComboBox->currentIndex();
            nameLineEdit = new QLineEdit( game_g->getPlayerType(character_id).c_str() );
            g_layout->addWidget(nameLineEdit, n_row, 1);
            n_row++;
            nameLineEdit->setFocus();
            nameLineEdit->setInputMethodHints(Qt::ImhNoPredictiveText); // needed on Android at least due to buggy behaviour (both with default keyboard, and makes Swype crash); probably useful on other platforms
            nameLineEdit->selectAll();
            /*if( options_page_index == n_options_pages-1 ) {
                connect(nameLineEdit, SIGNAL(returnPressed()), this, SLOT(clickedStartGame()));
            }
            else {
                connect(nameLineEdit, SIGNAL(returnPressed()), this, SLOT(clickedNext()));
            }*/

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
        startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
        closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    game_g->getMainWindow()->setCursor(Qt::WaitCursor);
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

#ifdef Q_OS_ANDROID
            if( game_g->isSDCardOk() ) {
                QHBoxLayout *h_layout = new QHBoxLayout();
                layout->addLayout(h_layout);

                QPushButton *importButton = new QPushButton(tr("Import"));
                game_g->initButton(importButton);
                h_layout->addWidget(importButton);
                connect(importButton, SIGNAL(clicked()), this, SLOT(clickedImportButton()));

                if( load_list != NULL ) {
                    QPushButton *exportButton = new QPushButton(tr("Export"));
                    game_g->initButton(exportButton);
                    h_layout->addWidget(exportButton);
                    connect(exportButton, SIGNAL(clicked()), this, SLOT(clickedExportButton()));
                }
            }
#endif

    // load_list only created if it has items
    if( load_list != NULL ) {
        ASSERT_LOGGER(load_list->count() > 0 );
        ASSERT_LOGGER(load_list->count() == load_filenames.size());
        layout->addWidget(load_list);
        load_list->setCurrentRow(0);

        QPushButton *loadButton = new QPushButton(tr("Load"));
        game_g->initButton(loadButton);
        loadButton->setShortcut(QKeySequence(Qt::Key_Return));
        //loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(loadButton);
        connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoadGame()));
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
    game_g->getMainWindow()->setCursor(Qt::WaitCursor);
}

void OptionsGamestate::clickedImportButton() {
#ifdef Q_OS_ANDROID
    LOG("OptionsGamestate::clickedImportButton()\n");
    if( !game_g->isSDCardOk() ) {
        qDebug("no SD Card access");
        return;
    }

    InfoDialog *dialog = InfoDialog::createInfoDialogYesNo("Do you want to import save game files from external storage (files with same name will be overwritten)?");
    this->main_stacked_widget->addWidget(dialog);
    this->main_stacked_widget->setCurrentWidget(dialog);
    int result = dialog->exec();
    LOG("dialog returns %d\n", result);
    this->closeSubWindow();
    if( result == 0 ) {
        int count = game_g->importFilesToSDCard();
        stringstream str;
        str << "Successfully imported " << count << " save game files.";
        dialog = InfoDialog::createInfoDialogOkay(str.str());
        this->main_stacked_widget->addWidget(dialog);
        this->main_stacked_widget->setCurrentWidget(dialog);
        dialog->exec();
    }

    this->closeAllSubWindows();
    this->clickedLoad();
#endif
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

    InfoDialog *dialog = InfoDialog::createInfoDialogYesNo("Do you want to import save game files from external storage (files with same name will be overwritten)?");
    this->main_stacked_widget->addWidget(dialog);
    this->main_stacked_widget->setCurrentWidget(dialog);
    int result = dialog->exec();
    LOG("dialog returns %d\n", result);
    this->closeSubWindow();
    if( result == 0 ) {
        for(size_t i=0;i<load_filenames.size();i++) {
            QString filename = load_filenames.at(i);
            QString full_filename = game_g->getApplicationFilename(savegame_folder + filename);
            game_g->exportFilenameToSDCard(full_filename, filename);
        }

        stringstream str;
        str << "Successfully exported " << load_filenames.size() << " save game files.";
        dialog = InfoDialog::createInfoDialogOkay(str.str());
        this->main_stacked_widget->addWidget(dialog);
        this->main_stacked_widget->setCurrentWidget(dialog);
        dialog->exec();

        this->closeAllSubWindows();
        this->clickedLoad();
    }
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
        label = new QLabel(tr("Master Volume: "));
        g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
        soundSlider = new QSlider(Qt::Horizontal);
        soundSlider->setMinimum(0);
        soundSlider->setMaximum(100);
        soundSlider->setValue(game_g->getGlobalSoundVolume());
        g_layout->addWidget(soundSlider, n_row, 1);
        n_row++;

        label = new QLabel(tr("Music Volume: "));
        g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
        soundSliderMusic = new QSlider(Qt::Horizontal);
        soundSliderMusic->setMinimum(0);
        soundSliderMusic->setMaximum(100);
        soundSliderMusic->setValue(game_g->getGlobalSoundVolumeMusic());
        g_layout->addWidget(soundSliderMusic, n_row, 1);
        n_row++;

        label = new QLabel(tr("Effects Volume: "));
        g_layout->addWidget(label, n_row, 0, Qt::AlignRight);
        soundSliderEffects = new QSlider(Qt::Horizontal);
        soundSliderEffects->setMinimum(0);
        soundSliderEffects->setMaximum(100);
        soundSliderEffects->setValue(game_g->getGlobalSoundVolumeEffects());
        g_layout->addWidget(soundSliderEffects, n_row, 1);
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
    game_g->setGlobalSoundVolumeMusic( soundSliderMusic->value() );
    game_g->setGlobalSoundVolumeEffects( soundSliderEffects->value() );
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

    QTextBrowser *help = new QTextBrowser();
    game_g->setTextEdit(help);
    help->setOpenExternalLinks(true);
    //help->setSearchPaths(QStringList(QString(DEPLOYMENT_PATH) + "docs/")); // not needed, as we have to handle links manually anyway

    QFile file(QString(DEPLOYMENT_PATH) + "docs/erebus.html");
    if( file.open(QFile::ReadOnly | QFile::Text) ) {
        QTextStream in(&file);
        help->setHtml(in.readAll());
    }
    layout->addWidget(help);

    QPushButton *closeButton = new QPushButton(tr("Close"));
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //closeButton->setFont(game_g->getFontSmall());
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::closeSubWindow() {
    LOG("OptionsGamestate::closeSubWindow\n");
    int n_stacked_widgets = this->main_stacked_widget->count();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(n_stacked_widgets-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
    }
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
