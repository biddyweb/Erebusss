#pragma once

#include <vector>
using std::vector;

#include <queue>
using std::queue;

#include <set>
using std::set;

#include <map>
using std::map;
using std::pair;

#include <string>
using std::string;

#include <QSettings>
#include <QPushButton>
#include <QTextEdit>

#include "common.h"

#include "rpg/utils.h" // for Vector2D

#include "scrollinglistwidget.h"
#include "webvieweventfilter.h"

class Screen;
class MainWindow;
class Sound;
class Gamestate;
class PlayingGamestate;
class Quest;
class Location;
class Item;
class Currency;
class Shop;
class CharacterTemplate;
class Character;
class Scenery;

//const QString savegame_root = "savegame_";
const QString savegame_ext = ".xml";
const QString savegame_folder = "savegames/";

class QuestInfo {
    string filename;
    string name;
public:
    QuestInfo(const string &filename, const string &name);

    string getFilename() const {
        return filename;
    }
    string getName() const {
        return name;
    }
};

// used for passing messages
class GameMessage {
public:
    enum GameMessageType {
        GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING = 0,
        GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD = 1,
        GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS = 2,
        GAMEMESSAGETYPE_QUIT = 3
    };

protected:
    GameMessageType game_message_type;

public:
    GameMessage(GameMessageType game_message_type) : game_message_type(game_message_type) {
        qDebug("send game message: %d", game_message_type);
    }

    GameMessageType getGameMessageType() const {
        return game_message_type;
    }
};

class StartGameMessage : public GameMessage {
    GameType gametype;
    Difficulty difficulty;
    string player_type;
    bool permadeath;
    string name;
    bool cheat_mode;
    int cheat_start_level;
public:
    StartGameMessage(GameType gametype, Difficulty difficulty, const string &player_type, bool permadeath, const string &name, bool cheat_mode, int cheat_start_level) : GameMessage(GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING), gametype(gametype), difficulty(difficulty), player_type(player_type), permadeath(permadeath), name(name), cheat_mode(cheat_mode), cheat_start_level(cheat_start_level) {
    }

    GameType getGametype() const {
        return this->gametype;
    }
    Difficulty getDifficulty() const {
        return this->difficulty;
    }
    string getPlayerType() const {
        return this->player_type;
    }
    string getName() const {
        return name;
    }
    bool getPermadeath() const {
        return this->permadeath;
    }
    bool getCheatMode() const {
        return this->cheat_mode;
    }
    int getCheatStartLevel() const {
        return this->cheat_start_level;
    }
};

class LoadGameMessage : public GameMessage {
    QString filename;
public:
    LoadGameMessage(const QString &filename) : GameMessage(GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD), filename(filename) {
    }

    const QString &getFilename() const {
        return this->filename;
    }
};

class Game : public QObject {
    Q_OBJECT

protected:
    bool is_testing;
    int test_n_info_dialog;

    map<string, Sound *> sound_effects;
    string current_stream_sound_effect;

    QSettings *settings;

    QString application_path;
    QString logfilename;
    QString oldlogfilename;
#ifdef Q_OS_ANDROID
    bool sdcard_ok;
    QString sdcard_path;
#endif

    QStyle *style;
    QFont font_scene;
    QFont font_small;
    QFont font_std;
    QFont font_big;

    WebViewEventFilter *webViewEventFilter;

    QLineEdit *osk_lineEdit;

    QPalette gui_palette;
    QBrush gui_brush_buttons;

    Gamestate *gamestate;
    Screen *screen;
    queue<GameMessage *> message_queue;

#ifndef Q_OS_ANDROID
    int sound_volume;
    int sound_volume_music;
    int sound_volume_effects;
#endif
    bool lighting_enabled;

    vector<string> player_types;
    map<string, QPixmap> portrait_images;

    void init(bool fullscreen);
    void createPlayerNames();
    void loadPortraits();

private slots:
    void showOSK();

public:
    Game();
    ~Game();

    Screen *getScreen() {
        return this->screen;
    }
    const Screen *getScreen() const {
        return this->screen;
    }
    bool isFullscreen() const;
    MainWindow *getMainWindow();
    const MainWindow *getMainWindow() const;
    int getIconSize() const;
    int getButtonSize() const;
    bool isPaused() const;
    void setPaused(bool paused, bool also_input);
    void togglePaused();
    void restartElapsedTimer();
    int getGameTimeFrameMS() const;
    int getGameTimeTotalMS() const;
    int getInputTimeFrameMS() const;
    int getElapsedMS() const;
    int getElapsedFrameMS() const;
    void setGameTimeMultiplier(int multiplier);

    QFont getFontScene() const {
        return this->font_scene;
    }
    QFont getFontSmall() const {
        return this->font_small;
    }
    QFont getFontStd() const {
        return this->font_std;
    }
    QFont getFontBig() const {
        return this->font_big;
    }
    /*const QBrush &getGuiBrushButtons() const {
        return this->gui_brush_buttons;
    }*/
    QPixmap &getPortraitImage(const string &name);

    void initButton(QWidget *button) const;
#if defined(_WIN32)
    QPushButton *createOSKButton(QLineEdit *lineEdit);
    void clearOSKButton();
#endif

    void pushMessage(GameMessage *message) {
        message_queue.push(message);
    }
    bool hasMessages() const {
        return !message_queue.empty();
    }

    void setTextEdit(QTextEdit *textEdit);
    void resizeTopLevelWidget(QWidget *widget) const;

    void run(bool fullscreen);
    void handleMessages();
    void update();
    void updateInput();
    void render();
    void runTests(bool fullscreen);

    void activate(bool active);
    void keyPress(QKeyEvent *key_event);
    Gamestate *getGamestate() const {
        return this->gamestate;
    }
    void setGamestate(Gamestate *gamestate) {
        this->gamestate = gamestate;
    }
    //void mouseClick(int m_x, int m_y);

#ifdef Q_OS_ANDROID
    bool isSDCardOk() const {
        return this->sdcard_ok;
    }
    int importFilesToSDCard() const;
    void exportFilenameToSDCard(const QString &src_fullfilename, const QString &filename) const;
#endif
    QString getFilename(const QString &path, const QString &name) const;
    QString getApplicationFilename(const QString &name) const;
    //void log(const char *text, ...);
    void log(const char *text, va_list vlist);

    QPixmap loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height, int expected_width) const;
    QPixmap loadImage(const string &filename) const {
        return loadImage(filename, false, 0, 0, 0, 0, 0);
    }
    void loadSound(const string &id, const string &filename, bool stream);
    void playSound(const string &sound_effect) {
        this->playSound(sound_effect, false);
    }
    void playSound(const string &sound_effect, bool loop);
    void pauseSound(const string &sound_effect);
    void stopSound(const string &sound_effect);
    void fadeSound(const string &sound_effect);
    void cancelCurrentStream();
    void freeSound(const string &sound_effect);
    void updateSoundVolume(const string &sound_effect);
    void setSoundVolume(const string &sound_effect, float volume);
    /*bool isSoundEnabled() const {
        return this->sound_enabled;
    }
    void setSoundEnabled(bool sound_enabled);*/
#ifndef Q_OS_ANDROID
    int getGlobalSoundVolume() const {
        return this->sound_volume;
    }
    void setGlobalSoundVolume(int sound_volume);
    int getGlobalSoundVolumeMusic() const {
        return this->sound_volume_music;
    }
    void setGlobalSoundVolumeMusic(int sound_volume_music);
    int getGlobalSoundVolumeEffects() const {
        return this->sound_volume_effects;
    }
    void setGlobalSoundVolumeEffects(int sound_volume_effects);
#endif
    bool isLightingEnabled() const {
        return this->lighting_enabled;
    }
    void setLightingEnabled(bool lighting_enabled);
    static string getDifficultyString(Difficulty difficulty);

    size_t getNPlayerTypes() const {
        return player_types.size();
    }
    string getPlayerType(size_t i) const {
        return player_types.at(i);
    }
    Character *createPlayer(const string &player_type, const string &player_name) const;
    vector<QuestInfo> loadQuests() const;
    void fillSaveGameFiles(ScrollingListWidget **list, vector<QString> *filenames) const;

    void showErrorDialog(const string &message);

    void startTesting() {
        this->is_testing = true;
        this->test_n_info_dialog = 0;
    }
    void stopTesting() {
        this->is_testing = false;
    }
    bool isTesting() const {
        return this->is_testing;
    }
    void recordTestInfoDialog() {
        this->test_n_info_dialog++;
    }
    int getTestNInfoDialog() const {
        return this->test_n_info_dialog;
    }
};

extern Game *game_g;

/*const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else game_g->log
#endif*/
