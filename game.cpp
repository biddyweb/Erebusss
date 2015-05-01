#include <ctime>

#ifdef _DEBUG
#include <cassert>
#endif

#include <QApplication>
#include <QDesktopWidget>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QImageReader>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QStyleFactory>
#include <QDir>
#include <QDateTime>
#include <QTextEdit>

#if QT_VERSION < 0x050000
#include <QWindowsStyle>
#endif

#include "game.h"
#include "optionsgamestate.h"
#include "playinggamestate.h"
#include "qt_screen.h"
#include "qt_utils.h"
#include "sound.h"
#include "logiface.h"
#include "test.h"

Game *game_g = NULL;

/*const QString sound_enabled_key_c = "sound_enabled";
const int default_sound_enabled_c = true;*/
#ifndef Q_OS_ANDROID
const QString sound_volume_key_c = "sound_volume";
const int default_sound_volume_c = 100;
const QString sound_volume_music_key_c = "sound_volume_music";
const int default_sound_volume_music_c = 100;
const QString sound_volume_effects_key_c = "sound_volume_effects";
const int default_sound_volume_effects_c = 100;
#endif

const QString lighting_enabled_key_c = "lighting_enabled";
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const int default_lighting_enabled_c = false; // lighting effects can be a bit too slow on mobile platforms
#else
const int default_lighting_enabled_c = true;
#endif

QuestInfo::QuestInfo(const string &filename, const string &name) : filename(filename), name(name) {
}

Game::Game() : is_testing(false), test_n_info_dialog(0), settings(NULL), style(NULL), webViewEventFilter(NULL), gamestate(NULL), screen(NULL), /*sound_enabled(default_sound_enabled_c),*/
#ifdef Q_OS_ANDROID
    sdcard_ok(false),
#else
    sound_volume(default_sound_volume_c),
    sound_volume_music(default_sound_volume_music_c),
    sound_volume_effects(default_sound_volume_effects_c),
#endif
    lighting_enabled(default_lighting_enabled_c) {
    game_g = this;

    QCoreApplication::setApplicationName("erebus");
    settings = new QSettings("Mark Harman", "erebus");

#if QT_VERSION >= 0x050000
    style = QStyleFactory::create("windows");
#else
    style = new QWindowsStyle(); // needed to get the textured buttons (which doesn't work with Windows XP, Symbian or Android styles)
#endif

    webViewEventFilter = new WebViewEventFilter();

    // initialise paths
    // n.b., not safe to use logging until after copied/removed old log files!
#if QT_VERSION >= 0x050000
    QString pathQt = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    QString pathQt = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
    qDebug("user data location path: %s", pathQt.toStdString().c_str());
    if( !QDir(pathQt).exists() ) {
        QDir().mkpath(pathQt);
    }
    QString nativePath(QDir::toNativeSeparators(pathQt));
    application_path = nativePath;

#if defined(Q_OS_ANDROID)
    // on Android, try for the sdcard, so we can find somewhere more accessible to the user (and that I can read on my Galaxy Nexus!!!)
    this->sdcard_ok = true;
    sdcard_path = QString("/sdcard/net.sourceforge.erebusrpg");
    qDebug("try sd card: %s", sdcard_path.toStdString().c_str());
    if( !QDir(sdcard_path).exists() ) {
        qDebug("try creating application folder in sdcard/");
        // folder doesn't seem to exist - try creating it
        QDir().mkdir(sdcard_path);
        if( !QDir(sdcard_path).exists() ) {
            qDebug("failed to create application folder in sdcard/");
            sdcard_ok = false;
        }
    }
    if( sdcard_ok ) {
        logfilename = getFilename(sdcard_path, "log.txt");
        oldlogfilename = getFilename(sdcard_path, "log_old.txt");
    }
    else {
        logfilename = getApplicationFilename("log.txt");
        oldlogfilename = getApplicationFilename("log_old.txt");
    }
#else
    logfilename = getApplicationFilename("log.txt");
    oldlogfilename = getApplicationFilename("log_old.txt");
#endif
    qDebug("application_path: %s", application_path.toUtf8().data());
    qDebug("logfilename: %s", logfilename.toUtf8().data());
    qDebug("oldlogfilename: %s", oldlogfilename.toUtf8().data());

    {
        QFile logfile(logfilename);
        if( !logfile.open(QIODevice::Append | QIODevice::Text) ) {
            qDebug("Failed to open logfile! Try in program folder");
            logfilename = "log.txt";
            oldlogfilename = "log_old.txt";
            qDebug("logfilename: %s", logfilename.toUtf8().data());
            qDebug("oldlogfilename: %s", oldlogfilename.toUtf8().data());
            QFile logfile2(logfilename);
            if( !logfile2.open(QIODevice::Append | QIODevice::Text) ) {
                qDebug("Still failed to open logfile!");
            }
        }
    }

    QFile::remove(oldlogfilename);
    QFile::rename(logfilename, oldlogfilename);
    QFile::remove(logfilename); // just in case we failed to rename, make sure the old log file is removed

    LOG("Initialising Log File...\n");
    LOG("erebus startup\n");
    LOG("Version %d.%d\n", versionMajor, versionMinor);

#ifdef _DEBUG
    LOG("Running in Debug mode\n");
#else
    LOG("Running in Release mode\n");
#endif

#if defined(Q_WS_SIMULATOR)
    LOG("Platform: Qt Smartphone Simulator\n");
#elif defined(_WIN32)
    LOG("Platform: Windows\n");
#elif defined(Q_WS_MAEMO_5)
    // must be before __linux, as Maemo/Meego also defines __linux
    LOG("Platform: Maemo/Meego\n");
#elif defined(Q_OS_ANDROID)
    // must be before __linux, as Android also defines __linux
    LOG("Platform: Android\n");
#elif __linux
    LOG("Platform: Linux\n");
#elif defined(__APPLE__) && defined(__MACH__)
    LOG("Platform: MacOS X\n");
#elif defined(Q_OS_SYMBIAN)
    LOG("Platform: Symbian\n");
#else
    LOG("Platform: UNKNOWN\n");
#endif

    LOG("Data folder: %s\n", DEPLOYMENT_PATH);

    bool ok = true;

#ifndef Q_OS_ANDROID
    int sound_volume_i = settings->value(sound_volume_key_c, default_sound_volume_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume not ok, set to default");
        this->sound_volume = default_sound_volume_c;
    }
    else {
        this->sound_volume = sound_volume_i;
    }

    int sound_volume_music_i = settings->value(sound_volume_music_key_c, default_sound_volume_music_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume_music not ok, set to default");
        this->sound_volume_music = default_sound_volume_music_c;
    }
    else {
        this->sound_volume_music = sound_volume_music_i;
    }

    int sound_volume_effects_i = settings->value(sound_volume_effects_key_c, default_sound_volume_effects_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume_effects not ok, set to default");
        this->sound_volume_effects = default_sound_volume_effects_c;
    }
    else {
        this->sound_volume_effects = sound_volume_effects_i;
    }
#endif

    int lighting_enabled_i = settings->value(lighting_enabled_key_c, default_lighting_enabled_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings lighting_enabled not ok, set to default");
        this->lighting_enabled = default_lighting_enabled_c;
    }
    else {
        this->lighting_enabled = lighting_enabled_i != 0;
    }

    QString savegame_path = getApplicationFilename(savegame_folder);
    if( !QDir(savegame_path).exists() ) {
        LOG("create savegame_path: %s\n", savegame_path.toStdString().c_str());
        QDir().mkpath(savegame_path);
        if( !QDir(savegame_path).exists() ) {
            LOG("failed to create savegame_path!\n");
        }
    }

    this->createPlayerNames();
}

Game::~Game() {
    LOG("Game::~Game()\n");
    if( screen != NULL ) {
        LOG("delete screen\n");
        // hack to prevent the MyApplication::event() from calling screen functions when deactivating
        Screen *local_screen = screen;
        screen = NULL;
        delete local_screen;
    }

    LOG("delete sounds\n");
    for(map<string, Sound *>::iterator iter = this->sound_effects.begin(); iter != this->sound_effects.end(); ++iter) {
        Sound *sound = iter->second;
        delete sound;
    }

    if( webViewEventFilter != NULL ) {
        LOG("delete webViewEventFilter");
        delete webViewEventFilter;
        webViewEventFilter = NULL;
    }

    if( style != NULL ) {
        LOG("delete style\n");
        delete style;
    }

    if( settings != NULL ) {
        LOG("delete settings");
        delete settings;
    }

    LOG("Game::~Game() done\n");
    game_g = NULL;
}

void Game::init(bool fullscreen) {
    LOG("fullscreen? %d\n", fullscreen);
    screen = new Screen(fullscreen);

    LOG("smallscreen? %d\n", smallscreen_c);
    LOG("touchscreen? %d\n", touchscreen_c);
    LOG("lightdistribution? %d\n", lightdistribution_c);

    // setup fonts
    const MainWindow *window = this->screen->getMainWindow();
    // palettes disabled for now - problems with differences between platforms, plus setting it for this window doesn't apply to the subwindows that we open up
    /*{
        QPalette palette = window->palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::gray);
        // we should also set sensible defaults (note that buttons usually have Button and ButtonText set via gui_palette, but we should still set a sensible default
        palette.setColor(QPalette::Button, Qt::lightGray);
        palette.setColor(QPalette::ButtonText, Qt::black);
        // n.b., on Android, Comboboxes uses "Text" for the text, but uses "Window" (instead of "Base") for the background)
        // so for consistency between platforms we set Window and Base to be the same
        palette.setColor(QPalette::Base, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
        window->setPalette(palette);
    }*/
    int screen_w = QApplication::desktop()->availableGeometry().width();
    int screen_h = QApplication::desktop()->availableGeometry().height();
    LOG("resolution %d x %d\n", screen_w, screen_h);
#if defined(Q_OS_ANDROID)
    {
        qDebug("setting up fonts for Android");
        QFont new_font = window->font();
#if QT_VERSION < 0x050000
        // n.b., need to set font size directly, as new_font.pointSize() returns -1 on Android!
        if( screen_w <= 480 ) {
            // optimise for smaller screens
            this->font_scene = new_font;
            this->font_small = QFont(new_font);
            this->font_small.setPointSize(5);
            this->font_std = QFont(new_font);
            this->font_std.setPointSize(7);
            this->font_big = QFont(new_font);
            this->font_big.setPointSize(8);
        }
        else {
            this->font_scene = new_font;
            this->font_small = QFont(new_font);
            this->font_small.setPointSize(7);
            this->font_std = QFont(new_font);
            this->font_std.setPointSize(9);
            this->font_big = QFont(new_font);
            this->font_big.setPointSize(13);
        }
#else
        // don't have Android look-and-feel when not using necessitas with Qt 5
        this->font_scene = new_font;
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() - 2);
        this->font_std = new_font;
        this->font_big = new_font;
#endif
    }
#else
    if( smallscreen_c ) {
        QFont new_font = window->font();
        qDebug("setting up fonts for non-Android smallscreen");
        if( screen_w == 640 && screen_h == 480 ) {
            // hackery for Nokia E6 - almost all Symbian^1 phones or later are 640x360, but things don't look right with the E6, which is 640x480
            LOG("Nokia E6 font hackery\n");
            this->font_scene = QFont(new_font);
            this->font_scene.setPointSize(font_scene.pointSize() - 2);
        }
        else {
            this->font_scene = new_font;
        }
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() - 2);
        this->font_std = new_font;
        this->font_big = new_font;

        // leave web font as default
    }
    else {
        qDebug("setting up fonts for non-Android non-mobile");
        LOG("default font size: %d\n", window->font().pointSize());
        QString font_family = window->font().family();
        LOG("font family: %s\n", font_family.toStdString().c_str());
        this->font_scene = QFont(font_family, 16, QFont::Normal);
        this->font_small = QFont(font_family, 16, QFont::Normal);
        this->font_std = QFont(font_family, 18, QFont::Normal);
        this->font_big = QFont(font_family, 24, QFont::Bold);

    }
#endif

/*#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // see Game::initButton()
#else*/
    /*unsigned char filter_max[] = {220, 220, 220};
    unsigned char filter_min[] = {120, 120, 120};*/
    unsigned char filter_max[] = {228, 217, 206};
    unsigned char filter_min[] = {160, 153, 143};
    QPixmap gui_pixmap_buttons = createNoise(256, 256, 16.0f, 16.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
    this->gui_brush_buttons.setTexture(gui_pixmap_buttons);
    this->gui_palette.setBrush(QPalette::Button, gui_brush_buttons);
    this->gui_palette.setColor(QPalette::ButtonText, QColor(76, 0, 0));
//#endif

    loadPortraits();
}

bool Game::isFullscreen() const {
    return this->screen->isFullscreen();
}

MainWindow *Game::getMainWindow() {
    return this->screen->getMainWindow();
}

const MainWindow *Game::getMainWindow() const {
    return this->screen->getMainWindow();
}

bool Game::isPaused() const {
    return this->screen->isPaused();
}

void Game::setPaused(bool paused, bool also_input) {
    this->screen->setPaused(paused, also_input);
}

void Game::togglePaused() {
    this->screen->togglePaused();
}

void Game::restartElapsedTimer() {
    this->screen->restartElapsedTimer();
}

int Game::getGameTimeFrameMS() const {
    return this->screen->getGameTimeFrameMS();
}

int Game::getGameTimeTotalMS() const {
    return this->screen->getGameTimeTotalMS();
}

int Game::getInputTimeFrameMS() const {
    return this->screen->getInputTimeFrameMS();
}

int Game::getElapsedMS() const {
    return this->screen->getElapsedMS();
}

int Game::getElapsedFrameMS() const {
    return this->screen->getElapsedFrameMS();
}

void Game::setGameTimeMultiplier(int multiplier) {
    this->screen->setGameTimeMultiplier(multiplier);
}

void Game::setTextEdit(QTextEdit *textEdit) {
    // need to set colours for Android at least
    // also allows setting of things such as background
    QPalette p = textEdit->palette();
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::Text, Qt::black);
    textEdit->setPalette(p);
    textEdit->setReadOnly(true);
    textEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    //textEdit->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    //textEdit->setFont(this->getFontSmall());
    QFont font = this->getFontSmall();
    if( !smallscreen_c ) {
        // font seems slightly too large, at least for Windows
        font.setPointSizeF(font.pointSizeF() - 4.5f);
    }
    textEdit->setFont(font);
#ifdef Q_OS_ANDROID
    // scrollbars need to be explicitly disabled, as otherwise we get a space being left, even though scrollbars don't show
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
    QString background_filename = DEPLOYMENT_PATH + QString("gfx/textures/paper.png");
    QString stylesheet_string = "background-image: url(\"" + background_filename + "\");";
    LOG("stylesheet string: %s\n", stylesheet_string.toStdString().c_str());
    textEdit->setStyleSheet(stylesheet_string);
    this->webViewEventFilter->setTextEdit(textEdit);
}

void Game::resizeTopLevelWidget(QWidget *widget) const {
    int width = QApplication::desktop()->availableGeometry().width();
    int height = QApplication::desktop()->availableGeometry().height();
    widget->setMinimumWidth(width/2);
    widget->setMinimumHeight(height/2);
}

void Game::run(bool fullscreen) {

    this->init(fullscreen);

    try {
        gamestate = new OptionsGamestate();
    }
    catch(string &error) {
        LOG("exception creating initial gamestate: %s\n", error.c_str());
        this->screen->getMainWindow()->unsetCursor();

        stringstream str;
        str << "Failed to start game:\n" << error;
        game_g->showErrorDialog(str.str());
        // can't use qApp->quit() here, as not yet in main event loop
        return;
    }
    catch(...) {
        LOG("unexpected exception creating initial gamestate\n");
        this->screen->getMainWindow()->unsetCursor();

        stringstream str;
        str << "Unexpected error starting game!\n";
        game_g->showErrorDialog(str.str());
        // can't use qApp->quit() here, as not yet in main event loop
        return;
    }

    screen->initMainLoop();
    screen->runMainLoop();

    delete gamestate;
    gamestate = NULL;
}

void Game::runTests(bool fullscreen) {
    string filename = "test_results.csv";
    {
        FILE *testfile = fopen(filename.c_str(), "wt+");
        if( testfile == NULL ) {
            LOG("### FAILED to open/create %s\n", filename.c_str());
            return;
        }
        time_t time_val;
        time(&time_val);
        fprintf(testfile, "%s\n", ctime(&time_val));
        fprintf(testfile, "TEST,RESULT,SCORE\n");
        fclose(testfile);
    }

    this->init(fullscreen); // some tests need a Screen etc
    screen->initMainLoop(); // needed to avoid failures of tests TEST_USE_AMMO etc on Linux due to Screen::elapsed_timer not being initialised; also needed for TEST_LEVEL_UP
    for(int i=0;i<N_TESTS;i++) {
        Test::runTest(filename, i);
    }
    //Test::runTest(filename, ::TEST_LOADSAVEWRITEQUEST_2_ITEMS);
    //Test::runTest(filename, ::TEST_POINTINPOLYGON_11);
    //Test::runTest(filename, ::TEST_POINTINPOLYGON_12);
    //Test::runTest(filename, ::TEST_POINTINPOLYGON_13);
    //Test::runTest(filename, ::TEST_POINTINPOLYGON_14);
    //Test::runTest(filename, ::TEST_FLOORREGIONS_0);
    /*Test::runTest(filename, ::TEST_LOADSAVEQUEST_3);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_1);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_2);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_3);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_4);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_5);
    Test::runTest(filename, ::TEST_LOADSAVERANDOMQUEST_6);*/
}

void Game::initButton(QWidget *button) const {
    MainWindow *window = this->screen->getMainWindow();
    button->setFont(window->font()); // needed for Android at least
    button->setStyle(style);
    button->setAutoFillBackground(true);
    button->setPalette(this->gui_palette);
}

void Game::handleMessages() {
    //qDebug("Game::handleMessages()");

    while( !message_queue.empty() ) {
        GameMessage *message = message_queue.front();
        message_queue.pop();
        try {
            switch( message->getGameMessageType() ) {
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING:
            {
                StartGameMessage *start_message = static_cast<StartGameMessage *>(message);
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(false, start_message->getGametype(), start_message->getPlayerType(), start_message->getName(), start_message->getPermadeath(), start_message->getCheatMode(), start_message->getCheatStartLevel());
                gamestate = playing_gamestate;
                playing_gamestate->setDifficulty(start_message->getDifficulty());

                if( start_message->getGametype() == GAMETYPE_CAMPAIGN ) {
                    const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
                    QString qt_filename = DEPLOYMENT_PATH + QString(c_quest_info.getFilename().c_str());
                    playing_gamestate->loadQuest(qt_filename, false, start_message->getCheatMode());
                }
                else if( start_message->getGametype() == GAMETYPE_RANDOM ) {
                    playing_gamestate->createRandomQuest();
                }
                else {
                    ASSERT_LOGGER(false);
                }
                this->screen->getMainWindow()->unsetCursor();
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD:
            {
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
                gamestate = playing_gamestate;
                LoadGameMessage *load_message = static_cast<LoadGameMessage *>(message);
                QString full_filename = this->getApplicationFilename(savegame_folder + load_message->getFilename());
                try {
                    playing_gamestate->loadQuest(full_filename, true);
                    this->screen->getMainWindow()->unsetCursor();
                }
                catch(string &error) {
                    LOG("exception creating new gamestate when loading: %s\n", error.c_str());
                    this->screen->getMainWindow()->unsetCursor();
                    stringstream str;
                    str << "Failed to load save game file:\n" << error;
#ifdef Q_OS_ANDROID
                    // also copy failed save game to sdcard on Android, so user can potentially access it to send it to me
                    this->exportFilenameToSDCard(full_filename, load_message->getFilename());
#endif
                    game_g->showErrorDialog(str.str());
                    delete gamestate;
                    gamestate = NULL;
                    gamestate = new OptionsGamestate();
                }
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS:
                delete gamestate;
                gamestate = NULL;
                gamestate = new OptionsGamestate();
                break;
            case GameMessage::GAMEMESSAGETYPE_QUIT:
                if( gamestate != NULL )
                    gamestate->quitGame();
                // else ignore
                break;
            default:
                ASSERT_LOGGER(false);
            }
        }
        catch(string &error) {
            LOG("exception creating new gamestate: %s\n", error.c_str());
            this->screen->getMainWindow()->unsetCursor();
            if( gamestate != NULL ) {
                LOG("deleting gamestate");
                delete gamestate;
                gamestate = NULL;
            }

            stringstream str;
            str << "Failed to load game data:\n" << error;
            game_g->showErrorDialog(str.str());
            qApp->quit();
        }
        catch(...) {
            LOG("unexpected exception creating new gamestate\n");
            this->screen->getMainWindow()->unsetCursor();
            if( gamestate != NULL ) {
                LOG("deleting gamestate");
                delete gamestate;
                gamestate = NULL;
            }

            stringstream str;
            str << "Unexpected error loading game data!\n";
            game_g->showErrorDialog(str.str());
            qApp->quit();
        }
        delete message;
    }
}

void Game::update() {
    //qDebug("Game::update");
    this->handleMessages(); // needed to process any messages from earlier update call
    if( this->current_stream_sound_effect.length() > 0 ) {
#ifndef Q_OS_ANDROID
        Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
        ASSERT_LOGGER(current_sound != NULL);
        if( current_sound != NULL ) {
            current_sound->update();
        }
#endif
    }
    if( gamestate != NULL ) {
        gamestate->update();
    }
}

void Game::updateInput() {
    this->handleMessages(); // needed to process any messages from earlier updateInput call
    if( gamestate != NULL ) {
        gamestate->updateInput();
    }
}

void Game::render() {
    if( gamestate != NULL ) {
        gamestate->render();
    }
}

/*void Game::mouseClick(int m_x, int m_y) {
    gamestate->mouseClick(m_x, m_y);
}*/

QString Game::getFilename(const QString &path, const QString &name) const {
    // not safe to use LOG here, as logfile may not have been initialised!
    QString pathQt = path + QString("/") + name;
    QString nativePath(QDir::toNativeSeparators(pathQt));
    qDebug("getFilename returns: %s", nativePath.toUtf8().data());
    return nativePath;
}

QString Game::getApplicationFilename(const QString &name) const {
    // not safe to use LOG here, as logfile may not have been initialised!
    return getFilename(application_path, name);
}

#ifdef Q_OS_ANDROID
int Game::importFilesToSDCard() const {
    int count = 0;
    if( this->sdcard_ok ) {
        QDir dir( sdcard_path );
        QStringList filter;
        filter << "*" + savegame_ext;
        QFileInfoList files = dir.entryInfoList(filter, QDir::Files, QDir::Time);
        for(size_t i=0;i<files.size();i++) {
            QFileInfo file_info = files.at(i);
            QString full_filename = this->getApplicationFilename(savegame_folder + file_info.fileName());
            QFile file(full_filename);
            file.remove(); // in case already present
            qDebug("copy from %s to %s", file_info.filePath().toStdString().c_str(), full_filename.toStdString().c_str());
            QFile::copy(file_info.filePath(), full_filename);
            count++;
        }
    }
    return count;
}

void Game::exportFilenameToSDCard(const QString &src_fullfilename, const QString &filename) const {
    if( this->sdcard_ok ) {
        qDebug("Game::exportFilenameToSDCard(%s, %s)", src_fullfilename.toStdString().c_str(), filename.toStdString().c_str());
        QFile dir_file(this->getFilename(sdcard_path, filename));
        dir_file.remove(); // in case already present
        QFile::copy(src_fullfilename, dir_file.fileName());
    }
}
#endif

void Game::log(const char *text, va_list vlist) {
    char buffer[65536] = "";
    vsprintf(buffer,text,vlist);

    if( logfilename.length() > 0 ) {
        QFile logfile(logfilename);
        if( logfile.open(QIODevice::Append | QIODevice::Text) ) {
            QTextStream stream(&logfile);
            stream << buffer;
        }
    }
    qDebug("%s", buffer);
}

QPixmap Game::loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height, int expected_width) const {
    // need to use QImageReader - QPixmap::load doesn't work on large images on Symbian!
    QImageReader reader(filename.c_str());
    //qDebug("Game::loadImage(): %s", filename.c_str());
    if( clip ) {
        //qDebug("clipping");
        int actual_width = reader.size().width();
        if( actual_width != expected_width ) {
            float ratio = ((float)actual_width)/(float)expected_width;
            xpos *= ratio;
            ypos *= ratio;
            width *= ratio;
            height *= ratio;
        }
        if( xpos > 0 || ypos > 0 || width < actual_width || height < reader.size().height() ) {
            reader.setClipRect(QRect(xpos, ypos, width, height));
        }
    }
    /*QImage image = reader.read();
    if( image.isNull() ) {
        LOG("failed to read image: %s\n", filename.c_str());
        LOG("error: %d\n", reader.error());
        LOG("error string: %s\n", reader.errorString().toStdString().c_str());
        stringstream error;
        error << "Failed to load image: " << filename;
        throw error.str();
    }
    QPixmap pixmap = QPixmap::fromImage(image);*/
    QPixmap pixmap = QPixmap::fromImageReader(&reader);
    if( pixmap.isNull() ) {
        LOG("failed to convert image to pixmap: %s\n", filename.c_str());
        LOG("image reader error: %d\n", reader.error());
        LOG("image reader error string: %s\n", reader.errorString().toStdString().c_str());
        //throw string("Failed to convert image to pixmap");
        string error;
        if( reader.error() == QImageReader::InvalidDataError ) {
            // Normally this code returns the string "Unable to read image data", but (on Symbian at least) can occur when we run
            // out of memory. Better to return a more helpful code (and make it clear it's not a bug, but a problem with lack of resources!)
            error = "Out of memory";
        }
        else {
            error = reader.errorString().toStdString();
        }
        throw error;
    }
    //qDebug("    %d  %d\n", pixmap.width(), pixmap.height());
    return pixmap;
}

void Game::loadSound(const string &id, const string &filename, bool stream) {
    LOG("load sound: %s : %s , %d\n", id.c_str(), filename.c_str(), stream);
    try {
        this->sound_effects[id] = new Sound(filename, stream);
    }
    catch(const string &str) {
        LOG("Error when loading %s\n", filename.c_str());
        LOG("%s\n", str.c_str());
    }
}

void Game::playSound(const string &sound_effect, bool loop) {
    //qDebug("play sound: %s\n", sound_effect.c_str());
    //if( game_g->isSoundEnabled() )
    {
        Sound *sound = this->sound_effects[sound_effect];
        if( sound != NULL ) {
#ifndef Q_OS_ANDROID
            if( sound->isStream() ) {
                this->cancelCurrentStream();
            }
            sound->play(loop);
            if( sound->isStream() ) {
                this->current_stream_sound_effect = sound_effect;
            }
#else
            sound->play(loop);
#endif
        }
    }
}

void Game::pauseSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("pause sound: %s\n", sound_effect.c_str());
    // n.b., need to allow pausing even if sound not enabled (so we can disable sound, then pause music)
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->pause();
    }
#endif
    // not yet supported on Android
}

void Game::stopSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("stop sound: %s\n", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->stop();
    }
#endif
    // not yet supported on Android
    if( current_stream_sound_effect == sound_effect ) {
        this->current_stream_sound_effect = "";
    }
}

void Game::fadeSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("fade sound: %s\n", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->fadeOut(2000);
    }
#endif
    // not yet supported on Android
}

void Game::cancelCurrentStream() {
#ifndef Q_OS_ANDROID
    if( this->current_stream_sound_effect.length() > 0 ) {
        // cancel current stream
        qDebug("cancel stream: %s", this->current_stream_sound_effect.c_str());
        Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
        ASSERT_LOGGER(current_sound != NULL);
        if( current_sound != NULL ) {
            current_sound->stop();
        }
        this->current_stream_sound_effect = "";
    }
#endif
    // not yet supported on Android
}

void Game::freeSound(const string &sound_effect) {
    qDebug("Game::freeSound(%s)", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        this->sound_effects.erase( this->sound_effects.find(sound_effect) );
        delete sound;
    }
    if( current_stream_sound_effect == sound_effect ) {
        this->current_stream_sound_effect = "";
    }
}

void Game::updateSoundVolume(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->updateVolume();
    }
#endif
    // not yet supported on Android
}

void Game::setSoundVolume(const string &sound_effect, float volume) {
#ifndef Q_OS_ANDROID
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->setVolume(volume);
    }
#endif
    // not yet supported on Android
}

/*void Game::setSoundEnabled(bool sound_enabled) {
    this->sound_enabled = sound_enabled;
    this->settings->setValue(sound_enabled_key_c, sound_enabled ? 1 : 0);
}*/

#ifndef Q_OS_ANDROID
void Game::setGlobalSoundVolume(int sound_volume) {
    this->sound_volume = sound_volume;
    this->settings->setValue(sound_volume_key_c, sound_volume);
}

void Game::setGlobalSoundVolumeMusic(int sound_volume_music) {
    this->sound_volume_music = sound_volume_music;
    this->settings->setValue(sound_volume_music_key_c, sound_volume_music);
}

void Game::setGlobalSoundVolumeEffects(int sound_volume_effects) {
    this->sound_volume_effects = sound_volume_effects;
    this->settings->setValue(sound_volume_effects_key_c, sound_volume_effects);
}
#endif

void Game::setLightingEnabled(bool lighting_enabled) {
    this->lighting_enabled = lighting_enabled;
    this->settings->setValue(lighting_enabled_key_c, lighting_enabled ? 1 : 0);
}

string Game::getDifficultyString(Difficulty difficulty) {
    if( difficulty == DIFFICULTY_EASY ) {
        return "Easy";
    }
    else if( difficulty == DIFFICULTY_MEDIUM ) {
        return "Medium";
    }
    else if( difficulty == DIFFICULTY_HARD ) {
        return "Hard";
    }
    ASSERT_LOGGER(false);
    return "";
}

void Game::createPlayerNames() {
    this->player_types.push_back("Barbarian");
    this->player_types.push_back("Elf");
    this->player_types.push_back("Halfling");
    this->player_types.push_back("Ranger");
    this->player_types.push_back("Warrior");
}

void Game::loadPortraits() {
    LOG("Game::loadPortraits()\n");
    this->portrait_images["portrait_barbarian"] = loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/barbarian_m0.png");
    this->portrait_images["portrait_elf"] = loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/elf_f0.png");
    this->portrait_images["portrait_halfling"] = loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/halfling_f0.png");
    this->portrait_images["portrait_ranger"] = loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/ranger_m0.png");
    this->portrait_images["portrait_warrior"] = loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/warrior_m0.png");
}

QPixmap &Game::getPortraitImage(const string &name) {
    map<string, QPixmap>::iterator image_iter = this->portrait_images.find(name);
    if( image_iter == this->portrait_images.end() ) {
        LOG("failed to find image for portrait_images: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find portrait_images's image");
    }
    return image_iter->second;
}

Character *Game::createPlayer(const string &player_type, const string &player_name) const {
    //ASSERT_LOGGER(i < this->getNPlayerTypes() );
    //Character *character = new Character(player_types.at(i), "", false);
    Character *character = new Character(player_name, "", false);
    if( player_type == "Barbarian" ) {
        character->initialiseProfile(1, 8, 6, 8, 1, 5, 5, 8, 1.8f);
        character->initialiseHealth(75);
        character->addSkill(skill_unarmed_combat_c);
        character->addSkill(skill_charge_c);
        character->setBiography(PlayingGamestate::tr("You grew up in distant lands to the west, and have lived much of your life in the outdoors. You were trained from an early age in the arts of combat. You have travelled east in search of noble quests.").toStdString());
        character->setPortrait("portrait_barbarian");
        character->setAnimationFolder("isometric_hero");
        character->addGold( rollDice(2, 6, 10) );
    }
    else if( player_type == "Elf" ) {
        character->initialiseProfile(1, 7, 8, 6, 1, 8, 7, 8, 2.25f);
        character->initialiseHealth(60);
        character->addSkill(skill_sprint_c);
        character->addSkill(skill_hatred_orcs_c);
        character->setBiography(PlayingGamestate::tr("You come from the White Willow Forest, where you lived in a great Elven city built in the treetops. Many Elves prefer to never meddle with humans, but you have ventured out to explore the wider world.").toStdString());
        character->setPortrait("portrait_elf");
        character->setAnimationFolder("isometric_heroine");
        character->addGold( rollDice(2, 6, 10) );
    }
    else if( player_type == "Halfling" ) {
        character->initialiseProfile(1, 6, 7, 5, 1, 7, 10, 7, 1.8f);
        character->initialiseHealth(75);
        character->addSkill(skill_disease_resistance_c);
        character->addSkill(skill_luck_c);
        character->addSkill(skill_slingshot_c);
        character->setBiography(PlayingGamestate::tr("Halflings do not make great warriors and make unlikely adventurers, but they are suprisingly hardy, and their special skills can help them succeed where others might fail.").toStdString());
        character->setPortrait("portrait_halfling");
        character->setAnimationFolder("isometric_heroine");
        character->addGold( rollDice(2, 6, 20) );
    }
    else if( player_type == "Ranger" ) {
        character->initialiseProfile(1, 7, 8, 7, 1, 7, 8, 6, 2.2f);
        character->initialiseHealth(60);
        character->addSkill(skill_hideaway_c);
        character->addSkill(skill_fast_shooter_c);
        character->setBiography(PlayingGamestate::tr("You prefer the country life to cities. You are used to living and surviving independently, and you have had much time to hone your skills such as your proficiency with the bow.").toStdString());
        character->setPortrait("portrait_ranger");
        character->setAnimationFolder("isometric_hero");
        character->addGold( rollDice(2, 6, 10) );
    }
    else if( player_type == "Warrior" ) {
        character->initialiseProfile(1, 8, 7, 7, 1, 6, 7, 7, 2.0f);
        character->initialiseHealth(70);
        character->addSkill(skill_shield_combat_c);
        character->setBiography(PlayingGamestate::tr("You come from the great city of Eastport. At a young age, you joined the army where you were trained how to fight, and saw combat in wars with Orcs to the north. After completing your service of seven years, you now work independently, hoping to find riches in return for your services.").toStdString());
        character->setPortrait("portrait_warrior");
        character->setAnimationFolder("isometric_hero");
        character->addGold( rollDice(2, 6, 10) );
    }
    else {
        ASSERT_LOGGER(false);
    }

    //character->initialiseHealth(600); // CHEAT
    //character->addGold( 333 ); // CHEAT, simulate start of quest 2
    //character->setXP(68); // CHEAT, simulate start of quest 2
    //character->addGold( 1000 ); // CHEAT

    return character;
}

vector<QuestInfo> Game::loadQuests() const {
    vector<QuestInfo> quest_list;
    QFile file(QString(DEPLOYMENT_PATH) + "data/quests.xml");
    if( !file.open(QFile::ReadOnly | QFile::Text) ) {
        throw string("Failed to open quests xml file");
    }
    QXmlStreamReader reader(&file);
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        if( reader.isStartElement() )
        {
            if( reader.name() == "quest" ) {
                QStringRef filename_s = reader.attributes().value("filename");
                QStringRef name_s = reader.attributes().value("name");
                qDebug("found quest: %s name: %s", filename_s.toString().toStdString().c_str(), name_s.toString().toStdString().c_str());
                if( filename_s.length() == 0 ) {
                    LOG("error at line %d\n", reader.lineNumber());
                    throw string("quest doesn't have filename info");
                }
                QuestInfo quest_info(filename_s.toString().toStdString(), name_s.toString().toStdString());
                quest_list.push_back(quest_info);
            }
        }
    }
    if( reader.hasError() ) {
        LOG("error at line %d\n", reader.lineNumber());
        LOG("error reading quests.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
        throw string("error reading quests xml file");
    }
    if( quest_list.size() == 0 ) {
        throw string("failed to find any quests");
    }
    return quest_list;
}

void Game::showErrorDialog(const string &message) {
    LOG("Game::showErrorDialog: %s\n", message.c_str());
    this->screen->enableUpdateTimer(false);
    QMessageBox::critical(this->screen->getMainWindow(), "Error", message.c_str());
    this->screen->enableUpdateTimer(true);
}

void Game::activate(bool active) {
    this->screen->enableUpdateTimer(active);
    bool newly_paused = false;
    if( !active ) {
        newly_paused = !this->screen->isPaused();
        this->screen->setPaused(true, false); // automatically pause when application goes inactive
#ifndef Q_OS_ANDROID
        if( this->current_stream_sound_effect.length() > 0 ) {
            // pause current stream
            qDebug("pause stream: %s", this->current_stream_sound_effect.c_str());
            Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
            ASSERT_LOGGER(current_sound != NULL);
            if( current_sound != NULL ) {
                current_sound->pause();
            }
        }
#endif
    }
    else {
#ifndef Q_OS_ANDROID
        if( this->current_stream_sound_effect.length() > 0 ) {
            // unpause current stream
            qDebug("unpause stream: %s", this->current_stream_sound_effect.c_str());
            Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
            ASSERT_LOGGER(current_sound != NULL);
            if( current_sound != NULL ) {
                current_sound->unpause();
            }
        }
#endif
    }
    if( this->gamestate != NULL ) {
        this->gamestate->activate(active, newly_paused);
    }
}

void Game::keyPress(QKeyEvent *key_event) {
    if( this->gamestate != NULL ) {
        this->gamestate->keyPress(key_event);
    }
}

void Game::fillSaveGameFiles(ScrollingListWidget **list, vector<QString> *filenames) const {
    QDir dir( game_g->getApplicationFilename(savegame_folder) );
    QStringList filter;
    filter << "*" + savegame_ext;
    QFileInfoList files = dir.entryInfoList(filter, QDir::Files, QDir::Time);
    if( files.size() > 0 ) {
        if( *list == NULL ) {
            *list = new ScrollingListWidget();
            (*list)->grabKeyboard();
            if( !smallscreen_c ) {
                QFont list_font = (*list)->font();
                list_font.setPointSize( list_font.pointSize() + 8 );
                (*list)->setFont(list_font);
            }
            (*list)->setSelectionMode(QAbstractItemView::SingleSelection);
        }

        for(int i=0;i<files.size();i++) {
            QFileInfo file_info = files.at(i);
            filenames->push_back(file_info.fileName());
            QString file_str = file_info.fileName();
            file_str += " [" + file_info.lastModified().toString("d-MMM-yyyy hh:mm") + "]";
            (*list)->addItem(file_str);
        }
    }

}
