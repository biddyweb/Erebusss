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

Game::Game() : is_testing(false), test_n_info_dialog(0), test_expected_n_info_dialog(0), settings(NULL), style(NULL), webViewEventFilter(NULL), gamestate(NULL), screen(NULL), /*sound_enabled(default_sound_enabled_c),*/
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

void Game::setGameTimeMultiplier(int multiplier) {
    this->screen->setGameTimeMultiplier(multiplier);
}

void Game::setTextEdit(QTextEdit *textEdit) {
    // need to set colours for Android at least
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

    screen->runMainLoop();

    delete gamestate;
    gamestate = NULL;
}

enum TestID {
    TEST_PATHFINDING_0 = 0,
    TEST_PATHFINDING_1 = 1,
    TEST_PATHFINDING_2 = 2,
    TEST_PATHFINDING_3 = 3,
    TEST_PATHFINDING_4 = 4,
    TEST_PATHFINDING_5 = 5,
    TEST_PATHFINDING_6 = 6,
    TEST_POINTINPOLYGON_0 = 7,
    TEST_POINTINPOLYGON_1 = 8,
    TEST_POINTINPOLYGON_2 = 9,
    TEST_POINTINPOLYGON_3 = 10,
    TEST_POINTINPOLYGON_4 = 11,
    TEST_POINTINPOLYGON_5 = 12,
    TEST_POINTINPOLYGON_6 = 13,
    TEST_POINTINPOLYGON_7 = 14,
    TEST_POINTINPOLYGON_8 = 15,
    TEST_POINTINPOLYGON_9 = 16,
    TEST_POINTINPOLYGON_10 = 17,
    TEST_POINTINPOLYGON_11 = 18,
    TEST_PERF_POINTINPOLYGON_0 = 19,
    TEST_PERF_POINTINPOLYGON_1 = 20,
    TEST_PERF_POINTINPOLYGON_2 = 21,
    TEST_PERF_DISTANCEGRAPH_0 = 22,
    TEST_PERF_PATHFINDING_0 = 23,
    TEST_PERF_REMOVE_SCENERY_0 = 24,
    TEST_PERF_REMOVE_SCENERY_1 = 25,
    TEST_PERF_REMOVE_SCENERY_2 = 26,
    TEST_PERF_UPDATE_VISIBILITY_0 = 27,
    TEST_PERF_NUDGE_0 = 28,
    TEST_PERF_NUDGE_1 = 29,
    TEST_PERF_NUDGE_2 = 30,
    TEST_PERF_NUDGE_3 = 31,
    TEST_PERF_NUDGE_4 = 32,
    TEST_PERF_NUDGE_5 = 33,
    TEST_PERF_NUDGE_6 = 34,
    TEST_PERF_NUDGE_7 = 35,
    TEST_PERF_NUDGE_8 = 36,
    TEST_PERF_NUDGE_9 = 37,
    TEST_PERF_NUDGE_10 = 38,
    TEST_PERF_NUDGE_11 = 39,
    TEST_PERF_NUDGE_12 = 40,
    TEST_PERF_NUDGE_13 = 41,
    TEST_PERF_NUDGE_14 = 42,
    TEST_LOADSAVEQUEST_0 = 43,
    TEST_LOADSAVEQUEST_1 = 44,
    TEST_LOADSAVEQUEST_2 = 45,
    TEST_LOADSAVEQUEST_3 = 46,
    TEST_LOADSAVERANDOMQUEST_0 = 47,
    TEST_LOADSAVE_ACTION_LAST_TIME_BUG = 48,
    TEST_LOADSAVEWRITEQUEST_0_COMPLETE = 49,
    TEST_LOADSAVEWRITEQUEST_0_UNARMED = 50,
    TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN = 51,
    TEST_LOADSAVEWRITEQUEST_1_COMPLETE = 52,
    TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT = 53,
    TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST = 54,
    TEST_LOADSAVEWRITEQUEST_1_ELF = 55,
    TEST_LOADSAVEWRITEQUEST_2_COMPLETE = 56,
    TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH = 57,
    TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR = 58,
    N_TESTS = 59
};

/**
  TEST_PATHFINDING_0 - find a path
  TEST_PATHFINDING_1 - find a path
  TEST_PATHFINDING_2 - can't find a path as no route available
  TEST_PATHFINDING_3 - can't find a path, as start isn't in valid floor region
  TEST_PATHFINDING_4 - can't find a path, as destination isn't in valid floor region
  TEST_PATHFINDING_5 - find a path, with scenery being ignored
  TEST_PATHFINDING_6 - can't find a path - scenery is ignored but still blocking, as it doesn't block the last segment
  TEST_POINTINPOLYGON_0 - tests that a point is inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_1 - tests that a point is on a convex polygon (triangle)
  TEST_POINTINPOLYGON_2 - tests that a point is not inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_3 - tests that a point is inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_4 - tests that a point is on a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_5 - tests that a point is not inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_6 - tests that a point is inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_7 - tests that a point is on a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_8 - tests that a point is not inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_9 - tests that a point is inside a concave polygon (L-shape)
  TEST_POINTINPOLYGON_10 - tests that a point is on a concave polygon (L-shape)
  TEST_POINTINPOLYGON_11 - tests that a point is not inside a concave polygon (L-shape)
  TEST_PERF_POINTINPOLYGON_0 - performance test version of TEST_POINTINPOLYGON_9
  TEST_PERF_POINTINPOLYGON_1 - performance test version of TEST_POINTINPOLYGON_11
  TEST_PERF_POINTINPOLYGON_2 - as TEST_PERF_POINTINPOLYGON_1, but point is now outside of AABB
  TEST_PERF_DISTANCEGRAPH_0 - performance test for calculating a distance graph
  TEST_PERF_PATHFINDING_0 - performance test for calculating a shortest path
  TEST_PERF_REMOVE_SCENERY_0 - performance test for removing scenery (including recalculating distance graph)
  TEST_PERF_REMOVE_SCENERY_1 - performance test for removing scenery that was blocking important waypoint (including recalculating distance graph)
  TETS_PERF_REMOVE_SCENERY_2 - performance test for removing scenery, also tests the bug (fixed in 0.7) where we weren't updating the nodes correctly
  TEST_PERF_UPDATE_VISIBILITY_0 - performance test for updating visibility
  TEST_PERF_NUDGE_0 - performance test for nudging: clicking far side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_1 - performance test for nudging: clicking near side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_2 - performance test for nudging: clicking far side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_3 - performance test for nudging: clicking near side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_4 - performance test for nudging: clicking far side outside a west/east door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_5 - performance test for nudging: clicking near side outside a west/east door
  TEST_PERF_NUDGE_6 - performance test for nudging: clicking far side outside a north/south door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_7 - performance test for nudging: clicking near side outside a north/south door
  TEST_PERF_NUDGE_8 - performance test for nudging: clicking on west side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_9 - performance test for nudging: clicking on west side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_10 - performance test for nudging: clicking on east side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_11 - performance test for nudging: clicking on east side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_12 - performance test for nudging: clicking on east side of scenery, src is on east side
  TEST_PERF_NUDGE_13 - performance test for nudging: clicking on west side of scenery, src is on east side
  TEST_PERF_NUDGE_14 - performance test for nudging: clicking near 90 degree corner
  TEST_LOADSAVEQUEST_n - tests that we can load the nth quest, then test saving, then test loading the save game
  TEST_LOADSAVERANDOMQUEST_0 - tests that we can create a random quest, then test saving, then test loading the save game
  TEST_LOADSAVE_ACTION_LAST_TIME_BUG - tests load/save/load cycle for _test_savegames/action_last_time_bug.xml (this protects against a bug where we were writing out invalid html for the action_last_time attribute for Scenery; in this case, the save game file is valid
  TEST_LOADSAVEWRITEQUEST_0_COMPLETE - test for 1st quest: kill all goblins, check quest then complete
  TEST_LOADSAVEWRITEQUEST_0_UNARMED - test for 1st quest: check FP of player and goblin is as expected, then check again when they are unarmed; also test that Warrior gets FP bonus when armed with shield
  TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN - as TEST_LOADSAVEWRITEQUEST_0_UNARMED, but checks that Barbarian doesn't have FP penalty for being unarmed, and doesn't get FP bonus when armed with shield
  TEST_LOADSAVEWRITEQUEST_1_COMPLETE - test for 2nd quest: pick up item, check quest then complete
  TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT - test for 2nd quest: interact with Calbert
  TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST - test for 2nd quest: interact with Ghost
  TEST_LOADSAVEWRITEQUEST_1_ELF - test for 2nd quest: player has sprint bonus iff outdoors
  TEST_LOADSAVEWRITEQUEST_2_COMPLETE - test for 3rd quest: go through the exit, check quest then complete
  TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH - test for 3rd quest: interact with Anmareth
  TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR - test for 3rd quest: interact with Glenthor
  */

Item *Game::checkFindSingleItem(Scenery **scenery_owner, Character **character_owner, PlayingGamestate *playing_gamestate, Location *location, const string &item_name, bool owned_by_scenery, bool owned_by_npc) const {
    LOG("checkFindSingleItem for %s\n", item_name.c_str());
    vector<Scenery *> scenery_owners;
    vector<Character *> character_owners;
    vector<Item *> items = location->getItems(item_name, true, true, &scenery_owners, &character_owners);
    if( items.size() != scenery_owners.size() || items.size() != character_owners.size() ) {
        throw string("mismatched array lengths");
    }
    else if( items.size() != 1 ) {
        throw string("unexpected number of items");
    }
    else if( owned_by_scenery ) {
        if( scenery_owners[0] == NULL || character_owners[0] != NULL ) {
            throw string("expected item to be owned by scenery");
        }
    }
    else if( owned_by_npc ) {
        if( scenery_owners[0] != NULL || character_owners[0] == NULL ) {
            throw string("expected item to be owned by character");
        }
        else if( character_owners[0] == playing_gamestate->getPlayer() ) {
            throw string("didn't expect item to be owned by player, should be an NPC");
        }
    }
    else {
        if( scenery_owners[0] != NULL || character_owners[0] != NULL ) {
            throw string("expected item to be owned by nothing");
        }
    }
    if( scenery_owner != NULL )
        *scenery_owner = scenery_owners[0];
    if( character_owner != NULL )
        *character_owner = character_owners[0];
    return items[0];
}

void Game::checkLockedDoors(PlayingGamestate *playing_gamestate, const string &location_key_name, const string &location_doors_name, const string &key_name, int n_doors, bool key_owned_by_scenery, bool key_owned_by_npc) const {
    if( key_owned_by_scenery && key_owned_by_npc ) {
        throw string("test error: key can't be owned by scenery and npc");
    }
    Location *location_key = playing_gamestate->getQuest()->findLocation(location_key_name);
    if( location_key == NULL ) {
        throw string("can't find location for key");
    }
    Location *location_doors = playing_gamestate->getQuest()->findLocation(location_doors_name);
    if( location_doors == NULL ) {
        throw string("can't find location for doors");
    }
    set<Scenery *> scenerys = location_doors->getSceneryUnlockedBy(key_name);
    if( scenerys.size() != n_doors ) {
        throw string("unexpected number of locked scenerys");
    }
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( !scenery->isLocked() ) {
            throw string("didn't expect door to be unlocked");
        }
    }
    checkFindSingleItem(NULL, NULL, playing_gamestate, location_key, key_name, key_owned_by_scenery, key_owned_by_npc);
}

void Game::checkCanCompleteNPC(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, int expected_xp, int expected_gold, const string &expected_item, bool can_complete, bool quest_was_item) {
    Location *location = playing_gamestate->getQuest()->findLocation(location_npc_name);
    if( location == NULL ) {
        throw string("can't find location with npc");
    }
    if( location != playing_gamestate->getCLocation() ) {
        playing_gamestate->moveToLocation(location, location_npc_pos);
    }

    Character *npc = location->findCharacter(npc_name);
    if( npc == NULL ) {
        throw string("can't find ") + npc_name;
    }
    if( npc->canCompleteInteraction(playing_gamestate) != can_complete ) {
        if( can_complete )
            throw string("didn't expect to have completed sub-quest for ") + npc_name;
        else
            throw string("expected to have completed quest for ") + npc_name;
    }
    if( can_complete ) {
        const Character *player = playing_gamestate->getPlayer();
        int current_xp = player->getXP();
        int current_gold = player->getGold();
        int current_item = expected_item.length() == 0 ? 0 : player->findItemCount(expected_item);
        int current_total_items = player->getItemCount();
        /*for(set<Item *>::const_iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            const Item *item = *iter;
            LOG("item: %s\n", item->getName().c_str());
        }*/

        npc->completeInteraction(playing_gamestate);

        if( player->getXP() != current_xp + expected_xp ) {
            throw string("unexpected xp reward");
        }
        if( player->getGold() != current_gold + expected_gold ) {
            throw string("unexpected gold reward");
        }
        if( expected_item.length() > 0 && player->findItemCount(expected_item) != current_item+1 ) {
            throw string("didn't get item reward");
        }
        if( expected_item.length() > 0 && player->getItemCount() != (quest_was_item ? current_total_items : current_total_items+1) ) {
            // check we didn't get any other items
            // if quest_was_item: if we got a reward, then number of items should be the same, due to giving up the quest item
            throw string("unexpected additional item reward");
        }
        /*LOG("expected_item: %s", expected_item.c_str());
        LOG("count was %d now %d\n", current_total_items, player->getItemCount());
        for(set<Item *>::const_iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            const Item *item = *iter;
            LOG("item: %s\n", item->getName().c_str());
        }*/
        if( expected_item.length() == 0 && player->getItemCount() != (quest_was_item ? current_total_items-1 : current_total_items)) {
            // if quest_was_item: if no item reward, we should lose one, due to giving up the quest item
            throw string("unexpected item reward");
        }
    }
}

void Game::interactNPCItem(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &location_item_name, const Vector2D &location_item_pos, const string &item_name, bool owned_by_scenery, bool owned_by_npc, int expected_xp, int expected_gold, const string &expected_item) {
    // first check that we can't yet complete the interaction with NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, false, true);

    // now pick up item
    Location *location_item = playing_gamestate->getQuest()->findLocation(location_item_name);
    if( location_item == NULL ) {
        throw string("can't find location with item");
    }
    if( location_item != playing_gamestate->getCLocation() ) {
        playing_gamestate->moveToLocation(location_item, location_item_pos);
    }

    Scenery *scenery = NULL;
    Item *item = checkFindSingleItem(&scenery, NULL, playing_gamestate, location_item, item_name, owned_by_scenery, owned_by_npc);
    bool move = false;
    void *ignore = NULL;
    if( owned_by_scenery )
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
    playing_gamestate->getPlayer()->pickupItem(item);

    const Character *player = playing_gamestate->getPlayer();
    int current_quest_item = player->findItemCount(item_name);
    if( current_quest_item == 0 ) {
        throw string("player didn't pick up quest item");
    }

    // now return to NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, true, true);

    if( player->findItemCount(item_name) != current_quest_item-1 ) {
        throw string("player didn't give up quest item");
    }
}

void Game::interactNPCKill(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &objective_id, const string &check_kill_location, const string &check_kill_name, int expected_xp, int expected_gold, const string &expected_item) {
    // first check that we can't yet complete the interaction with NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, false, false);

    const Quest *quest = playing_gamestate->getQuest();
    for(vector<Location *>::const_iterator iter = quest->locationsBegin(); iter != quest->locationsEnd(); ++iter) {
        Location *location = *iter;
        while( location->getNCharacters() > 0 ) {
            Character *npc = NULL;
            for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd() && npc==NULL; ++iter) {
                Character *character = *iter;
                if( character->getObjectiveId() == objective_id ) {
                    if( character == playing_gamestate->getPlayer() ) {
                        throw string("player has objective_id!");
                    }
                    if( !character->isHostile() ) {
                        throw string("npc with objective_id isn't hostile");
                    }
                    if( check_kill_location.length() > 0 && check_kill_location != location->getName() ) {
                        throw string("enemies with objective_id in unexpected location");
                    }
                    if( check_kill_name.length() > 0 && check_kill_name != character->getName() ) {
                        throw string("enemy with objective_id has unexpected name");
                    }
                    npc = character;
                }
            }
            if( npc == NULL )
                break;
            location->removeCharacter(npc);
            delete npc;
        }
    }

    // now return to NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, true, false);
}



/** Optional read-only checks on a loaded game.
  */
void Game::checkSaveGame(PlayingGamestate *playing_gamestate, int test_id) {
    LOG("checkSaveGame\n");
    if( test_id == TEST_LOADSAVEQUEST_0 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "", "", "Goblin's Key", 1, false, true);
    }
    else if( test_id == TEST_LOADSAVEQUEST_1 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "entrance" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "level_1", "level_1", "Vespar's Cell Key", 5, false, true);
        checkLockedDoors(playing_gamestate, "level_3", "level_3", "Hand Mirror", 1, false, false);
        checkLockedDoors(playing_gamestate, "cave", "level_past", "Missing Teleporter Piece", 1, true, false);
        checkLockedDoors(playing_gamestate, "cave", "cave", "Orc Warlord's Key", 2, false, true);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Maze Key", 1, true, false);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Bull Statuette", 1, true, false);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Minotaur's Key", 1, false, true);
    }
    else if( test_id == TEST_LOADSAVEQUEST_2 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "entrance" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "level_1", "level_2", "Dwarven Key", 1, true, false);
        checkLockedDoors(playing_gamestate, "level_2", "level_3", "Derrin's Ring", 1, false, true);
        checkLockedDoors(playing_gamestate, "level_2", "level_4", "Derrin's Ring", 1, false, true);
    }
    else if( test_id == TEST_LOADSAVEQUEST_3 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        /*Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "Axbury" ) {
            throw string("unexpected start location");
        }*/
        checkLockedDoors(playing_gamestate, "Dungeons near Axbury", "Dungeons near Axbury", "Axbury Dungeon Key", 2, false, true);
        checkLockedDoors(playing_gamestate, "Upper Level, Wentbridge Fort", "Ground Level, Wentbridge Fort", "Wentbridge Dungeon Key", 1, true, false);
        checkLockedDoors(playing_gamestate, "Dungeons, Wentbridge Fort", "Dungeons, Wentbridge Fort", "Wentbridge Cell Key", 1, true, false);
        checkLockedDoors(playing_gamestate, "Dungeons, Wentbridge Fort", "Dungeons Lower Level, Wentbridge Fort", "Wentbridge Cell Key", 1, true, false);
    }
    else if( test_id == TEST_LOADSAVE_ACTION_LAST_TIME_BUG ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
    }
    else {
        throw string("unknown test_id");
    }
}

void Game::checkSaveGameWrite(PlayingGamestate *playing_gamestate, int test_id) {
    LOG("checkSaveGameWrite");
    if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE ) {
        // check quest not completed until all goblins completed
        Location *location = playing_gamestate->getCLocation();
        while( location->getNCharacters() > 0 ) {
            Character *npc = NULL;
            for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd() && npc==NULL; ++iter) {
                Character *character = *iter;
                if( character != playing_gamestate->getPlayer() ) {
                    if( !character->isHostile() ) {
                        throw string("npc isn't hostile");
                    }
                    npc = character;
                }
            }
            if( npc == NULL )
                break;
            if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
                throw string("didn't expect quest to already be completed");
            }
            location->removeCharacter(npc);
            delete npc;
        }
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED || test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN ) {
        Character *player = playing_gamestate->getPlayer();
        // check skill is as expected
        if( test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED && !player->hasSkill(skill_shield_combat_c) ) {
            throw string("player doesn't have unarmed combat skill");
        }
        if( test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN && !player->hasSkill(skill_unarmed_combat_c) ) {
            throw string("player doesn't have unarmed combat skill");
        }
        int base_fp = player->getBaseProfileIntProperty(profile_key_FP_c);
        int fp = player->getProfileIntProperty(profile_key_FP_c);
        if( fp != base_fp ) {
            throw string("player fp " + numberToString(fp) + " is different to base fp " + numberToString(base_fp));
        }
        // now disarm, and check we get a penalty (except for barbarian)
        player->armWeapon(NULL);
        fp = player->getProfileIntProperty(profile_key_FP_c);
        int exp_fp = (test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED) ? base_fp-2 : base_fp;
        if( fp != exp_fp ) {
            throw string("player unarmed fp " + numberToString(fp) + " is not " + numberToString(exp_fp) + " as expected, base fp was " + numberToString(base_fp));
        }
        // now rearm, and add a shield, and check we get a bonus (except for barbarian)
        player->addItem(playing_gamestate->cloneStandardItem("Long Sword"), true);
        player->addItem(playing_gamestate->cloneStandardItem("Shield"), true);
        if( player->getCurrentWeapon() == NULL ) {
            throw string("failed to arm player with weapon");
        }
        if( player->getCurrentShield() == NULL ) {
            throw string("failed to arm player with shield");
        }
        fp = player->getProfileIntProperty(profile_key_FP_c);
        exp_fp = (test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED) ? base_fp+1 : base_fp;
        if( fp != exp_fp ) {
            throw string("player with shield fp " + numberToString(fp) + " is not " + numberToString(exp_fp) + " as expected, base fp was " + numberToString(base_fp));
        }

        Location *location = playing_gamestate->getCLocation();
        Character *goblin = location->findCharacter("Goblin");
        if( goblin == NULL ) {
            throw string("can't find goblin");
        }
        base_fp = goblin->getBaseProfileIntProperty(profile_key_FP_c);
        fp = goblin->getProfileIntProperty(profile_key_FP_c);
        if( fp != base_fp ) {
            throw string("goblin fp " + numberToString(fp) + " is different to base fp " + numberToString(base_fp));
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE ) {
        // check quest completed iff item picked up
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getQuest()->findLocation("level_6");
        if( location == NULL ) {
            throw string("can't find location");
        }
        vector<Scenery *> scenery_owners;
        vector<Item *> items = location->getItems("Vespar's Skull", true, false, &scenery_owners, NULL);
        if( items.size() != scenery_owners.size() ) {
            throw string("mismatched array lengths");
        }
        else if( items.size() != 1 ) {
            throw string("unexpected number of items");
        }
        Item *item = items[0];
        Scenery *scenery = scenery_owners[0];
        bool move = false;
        void *ignore = NULL;
        playing_gamestate->moveToLocation(location, Vector2D(0.0f, 0.0f));
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
        playing_gamestate->getPlayer()->pickupItem(item);
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT ) {
        // interact with Calbert
        this->interactNPCItem(playing_gamestate, "level_3", Vector2D(7.5f, 14.0f), "Calbert", "level_3", Vector2D(7.5f, 14.0f), "Necromancy for Beginners", true, false, 50, 0, "");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST ) {
        // interact with Ghost
        this->interactNPCItem(playing_gamestate, "level_6", Vector2D(5.9f, 28.0f), "Ghost", "level_6", Vector2D(5.9f, 28.0f), "Ghost's Bones", false, false, 30, 0, "");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_ELF ) {
        Character *player = playing_gamestate->getPlayer();
        // check skill is as expected
        if( !player->hasSkill(skill_sprint_c) ) {
            throw string("player doesn't have sprint skill");
        }
        float base_sp = player->getBaseProfileFloatProperty(profile_key_Sp_c);
        float sp = player->getProfileFloatProperty(profile_key_Sp_c);
        if( fabs(base_sp - sp) > E_TOL_LINEAR ) {
            throw string("player sp " + numberToString(sp) + " is different to base sp " + numberToString(base_sp));
        }
        Location *location = playing_gamestate->getQuest()->findLocation("level_past");
        playing_gamestate->moveToLocation(location, Vector2D(25.0f, 24.0));
        sp = player->getProfileFloatProperty(profile_key_Sp_c);
        if( fabs(base_sp + 0.2f - sp) > E_TOL_LINEAR ) {
            throw string("player sp " + numberToString(sp) + " does not have bonus over base sp " + numberToString(base_sp));
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE ) {
        // check quest completed iff go through exit picked up
        test_expected_n_info_dialog++;
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getQuest()->findLocation("level_5");
        if( location == NULL ) {
            throw string("can't find location");
        }
        Scenery *scenery = location->findScenery("North Exit");
        if( scenery == NULL ) {
            throw string("can't find north exit");
        }
        bool move = false;
        void *ignore = NULL;
        playing_gamestate->moveToLocation(location, Vector2D(0.0f, 0.0f));
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH ) {
        // interact with Anmareth
        this->interactNPCItem(playing_gamestate, "entrance", Vector2D(3.5f, 6.0f), "Anmareth", "level_1", Vector2D(21.0f, 39.0f), "Dire Leaf", true, false, 30, 0, "Potion of Healing");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR ) {
        this->interactNPCKill(playing_gamestate, "level_1", Vector2D(39.0f, 29.0f), "Glenthor", "Glenthor", "level_1", "Wyvern", 75, 0, "");
    }
    else {
        throw string("unknown test_id");
    }
}

void Game::runTest(const string &filename, int test_id) {
    LOG(">>> Run Test: %d\n", test_id);
    ASSERT_LOGGER(test_id >= 0);
    ASSERT_LOGGER(test_id < N_TESTS);

    this->is_testing = true;
    this->test_n_info_dialog = 0;
    this->test_expected_n_info_dialog = 0;

    FILE *testfile = fopen(filename.c_str(), "at+");
    if( testfile == NULL ) {
        LOG("### FAILED to open/create %s\n", filename.c_str());
        return;
    }
    fprintf(testfile, "%d,", test_id);

    bool ok = true;
    bool has_score = false;
    double score = 0;

    try {
        if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_1 || test_id == TEST_PATHFINDING_2 || test_id == TEST_PATHFINDING_3 || test_id == TEST_PATHFINDING_4 || test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 10.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id != TEST_PATHFINDING_2 ) {
                    FloorRegion *floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                    location.addFloorRegion(floor_region);
                }
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }

            void *ignore = NULL;
            Scenery *scenery = NULL;
            if( test_id == TEST_PATHFINDING_5 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.0f);
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 8.0f, 8.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 1.5f, 5.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();

            Vector2D src, dest;
            int expected_points = 0;
            if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_2 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = test_id == TEST_PATHFINDING_0 ? 3 : 0;
            }
            else if( test_id == TEST_PATHFINDING_1 ) {
                src = Vector2D(4.0f, 4.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_3 ) {
                src = Vector2D(1.0f, 5.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 0;
            }
            else if( test_id == TEST_PATHFINDING_4 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(15.0f, 2.0f);
                expected_points = 0;
            }
            else if( test_id == TEST_PATHFINDING_5 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(2.5f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(1.5f, 1.0f);
                expected_points = 3;
            }
            else {
                throw string("missing case for this test_id");
            }
            vector<Vector2D> path = location.calculatePathTo(src, dest, ignore, false);

            LOG("path has %d points\n", path.size());
            for(size_t i=0;i<path.size();i++) {
                Vector2D point = path.at(i);
                LOG("    %d : %f, %f\n", i, point.x, point.y);
            }

            if( path.size() != expected_points ) {
                throw string("Unexpected number of path points");
            }
            else if( ignore == NULL && path.size() > 0 && path.at( path.size()-1 ) != dest ) {
                throw string("Unexpected end of path");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_0 || test_id == TEST_POINTINPOLYGON_1 || test_id == TEST_POINTINPOLYGON_2 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(-0.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_0 ) {
                test_pt.set(0.0f, -1.05f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_1 ) {
                test_pt.set(0.5f, -1.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.0f, -1.1f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_3 || test_id == TEST_POINTINPOLYGON_4 || test_id == TEST_POINTINPOLYGON_5 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(0.5f, 0.8f));
            poly.addPoint(Vector2D(-1.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_3 ) {
                test_pt.set(0.49f, 0.75f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_4 ) {
                test_pt.set(0.5f, 0.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.51f, 0.75f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_6 || test_id == TEST_POINTINPOLYGON_7 || test_id == TEST_POINTINPOLYGON_8 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, 1.0f));
            poly.addPoint(Vector2D(0.9f, -0.9f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_6 ) {
                test_pt.set(0.91f, -0.91f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_7 ) {
                test_pt.set(0.95f, 0.05f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.8f, -0.89f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ||
                 test_id == TEST_PERF_POINTINPOLYGON_0 || test_id == TEST_PERF_POINTINPOLYGON_1 || test_id == TEST_PERF_POINTINPOLYGON_2
                 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(0.0f, 0.0f));
            poly.addPoint(Vector2D(0.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 0.0f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_PERF_POINTINPOLYGON_0 ) {
                test_pt.set(0.5f, 2.5f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_10 ) {
                test_pt.set(0.5f, 0.0f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_11 || test_id == TEST_PERF_POINTINPOLYGON_1 ) {
                test_pt.set(1.1f, 0.1f);
                exp_inside = false;
            }
            else {
                test_pt.set(1.1f, -0.1f);
                exp_inside = false;
            }

            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ) {
                bool inside = poly.pointInside(test_pt);
                if( inside != exp_inside ) {
                    throw string("failed point inside polygon test");
                }
            }
            else {
                QElapsedTimer timer;
                timer.start();
                //int time_s = clock();
                int n_times = 1000000;
                for(int i=0;i<n_times;i++) {
                    bool inside = poly.pointInside(test_pt);
                    if( inside != exp_inside ) {
                        throw string("failed point inside polygon test");
                    }
                }
                has_score = true;
                //score = ((double)(clock() - time_s)) / (double)n_times;
                score = ((double)timer.elapsed()) / ((double)n_times);
                score /= 1000.0;
            }
        }
        else if( test_id == TEST_PERF_DISTANCEGRAPH_0 || test_id == TEST_PERF_PATHFINDING_0 || test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 || test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(14.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 5.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 10.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 15.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 25.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(0.0f, 30.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(5.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(15.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(20.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 5.5f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.25f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 3.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                n_times = 1; // test can only be run once!
            }
            for(int i=0;i<n_times;i++) {
                if( test_id == TEST_PERF_DISTANCEGRAPH_0 ) {
                    location.calculateDistanceGraph();
                }
                else if( test_id == TEST_PERF_PATHFINDING_0 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                    Vector2D src(0.5f, 0.5f);
                    Vector2D dest(0.5f, 3.5f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
                    Vector2D pos(1.0f, 1.0f);
                    location.clearVisibility();
                    vector<FloorRegion *> floor_regions = location.updateVisibility(pos);
                    if( floor_regions.size() == 0 ) {
                        throw string("didn't find any floor regions");
                    }
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;

            //vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);

        }
        else if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 || test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 || test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 ) {
                    floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(0.0f, 4.0f, 5.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                floor_region = FloorRegion::createRectangle(2.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 5.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 ) {
                    floor_region = FloorRegion::createRectangle(3.0f, 0.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(4.0f, 0.0f, 1.0f, 5.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 4.0f, 0.01f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.01f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 2.5f);
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                scenery = new Scenery("", "", 1.0f, 0.1f, 0.9f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.95f);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();
            for(size_t i=0;i<location.getNFloorRegions();i++) {
                FloorRegion *floor_region = location.getFloorRegion(i);
                floor_region->setVisible(true);
            }

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            for(int i=0;i<n_times;i++) {
                Vector2D src, pos, expected_nudge;
                if( test_id == TEST_PERF_NUDGE_0 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.91f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_1 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.99f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_2 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.91f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_3 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.99f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_4 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(4.85f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_5 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(5.05f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_6 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 4.85f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_7 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 5.05f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_8 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.5f, 3.0f);
                    expected_nudge = Vector2D(4.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_9 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.9f, 2.0f);
                    expected_nudge = Vector2D(5.0f - npc_radius_c - E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_10 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.5f, 3.0f);
                    expected_nudge = Vector2D(5.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_11 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.11f, 2.0f);
                    expected_nudge = Vector2D(5.01f + npc_radius_c + E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_12 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(3.1f, 2.5f);
                    expected_nudge = Vector2D(3.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_13 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(1.9f, 2.6f);
                    expected_nudge = Vector2D(2.0f - npc_radius_c - E_TOL_LINEAR, 2.6f);
                }
                else if( test_id == TEST_PERF_NUDGE_14 ) {
                    src = Vector2D(2.5f, 2.5f);
                    pos = Vector2D(0.1f, 4.95f);
                    expected_nudge = Vector2D(npc_radius_c + E_TOL_LINEAR, 5.0f - npc_radius_c - E_TOL_LINEAR);
                }
                Vector2D nudge = location.nudgeToFreeSpace(src, pos, npc_radius_c);
                if( (nudge - expected_nudge).magnitude() > E_TOL_LINEAR ) {
                    LOG("src: %f, %f\n", src.x, src.y);
                    LOG("pos: %f, %f\n", pos.x, pos.y);
                    LOG("nudge: %f, %f\n", nudge.x, nudge.y);
                    LOG("expected_nudge: %f, %f\n", expected_nudge.x, expected_nudge.y);
                    throw string("unexpected nudge");
                }
                vector<Vector2D> path = location.calculatePathTo(src, nudge, NULL, false);
                if( path.size() == 0 ) {
                    throw string("failed to find path");
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVEQUEST_0 || test_id == TEST_LOADSAVEQUEST_1 || test_id == TEST_LOADSAVEQUEST_2 || test_id == TEST_LOADSAVEQUEST_3 ) {
            // load, check, save, load, check
            QElapsedTimer timer;
            timer.start();
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, "Warrior", "name", false, false, 0);
            gamestate = playing_gamestate;

            QString qt_filename;
            if( test_id == TEST_LOADSAVEQUEST_0 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_1 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_2 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_3 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_necromancer.xml");
            }

            // load
            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            // save
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            // load
            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVERANDOMQUEST_0 ) {
            // load, save, load
            QElapsedTimer timer;
            timer.start();

            // load
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_RANDOM, "Warrior", "name", false, false, 0);
            gamestate = playing_gamestate;

            playing_gamestate->createRandomQuest();
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            // save
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            // load
            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            // n.b., use GAMETYPE_CAMPAIGN as we're loading a game
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVE_ACTION_LAST_TIME_BUG ) {
            // load, check, save, load, check
            QElapsedTimer timer;
            timer.start();

            // load
            QString load_filename = "../erebus/_test_savegames/action_last_time_bug.xml"; // hack to get local directory rather than deployed directory
            LOG("try loading %s\n", load_filename.toStdString().c_str());
            PlayingGamestate *playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(load_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            // save
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            // load
            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_ELF ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR ) {
            // load, check, load, save, load, check
            QElapsedTimer timer;
            timer.start();

            // load
            LOG("1 load\n");
            string player = "Warrior";
            if( test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN )
                player = "Barbarian";
            else if( test_id == TEST_LOADSAVEWRITEQUEST_1_ELF )
                player = "Elf";
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, player, "name", false, false, 0);
            gamestate = playing_gamestate;

            QString qt_filename;
            if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED || test_id == TEST_LOADSAVEWRITEQUEST_0_UNARMED_BARBARIAN ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT || test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST || test_id == TEST_LOADSAVEWRITEQUEST_1_ELF ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH || test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }

            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            LOG("2 check\n");
            checkSaveGameWrite(playing_gamestate, test_id);

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            // load
            LOG("3 load\n");
            playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, player, "name", false, false, 0);
            gamestate = playing_gamestate;

            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // save
            LOG("4 save\n");
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            // load
            LOG("5 load\n");
            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            LOG("6 check\n");
            checkSaveGameWrite(playing_gamestate, test_id);

            delete gamestate;
            gamestate = NULL;
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else {
            throw string("unknown test");
        }

        // final checks
        if( this->test_n_info_dialog != this->test_expected_n_info_dialog ) {
            throw string("unexpected test_n_info_dialog; expected: ") + numberToString(test_expected_n_info_dialog) + string(" actual: ") + numberToString(test_n_info_dialog);
        }

        fprintf(testfile, "PASSED,");
        if( has_score ) {
            if( score < 0.01 ) {
                fprintf(testfile, "%E", score);
            }
            else {
                fprintf(testfile, "%f", score);
            }
        }
        fprintf(testfile, "\n");
    }
    catch(const string &str) {
        LOG("ERROR: %s\n", str.c_str());
        fprintf(testfile, "FAILED,%s\n", str.c_str());
        ok = false;
    }

    if( ok ) {
        LOG("<<< TEST %d PASSED\n", test_id);
    }
    else {
        LOG("<<< TEST %d FAILED\n", test_id);
    }

    fclose(testfile);
}

void Game::runTests() {
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

    this->init(true); // some tests need a Screen etc
    for(int i=0;i<N_TESTS;i++) {
        //runTest(filename, i);
    }
    runTest(filename, ::TEST_LOADSAVEWRITEQUEST_1_ELF);
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
                    playing_gamestate->loadQuest(qt_filename, false);
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

        for(size_t i=0;i<files.size();i++) {
            QFileInfo file_info = files.at(i);
            filenames->push_back(file_info.fileName());
            QString file_str = file_info.fileName();
            file_str += " [" + file_info.lastModified().toString("d-MMM-yyyy hh:mm") + "]";
            (*list)->addItem(file_str);
        }
    }

}
