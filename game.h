#pragma once

#include "rpg/character.h"
#include "rpg/location.h"

#include <QtGui>
#include <QtWebKit/QWebView>

// Phonon not supported on Qt Android
#ifdef Q_OS_ANDROID
//#include <SLES/OpenSLES.h>
#else
#include <phonon/MediaObject>
#include <phonon/AudioOutput>
#endif

#include <queue>
using std::queue;

#include <set>
using std::set;

#include <map>
using std::map;
using std::pair;

#include <string>
using std::string;

class Screen;
class Gamestate;
class Quest;
class Location;
class Item;
class Currency;
class Shop;
class CharacterTemplate;

#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const bool mobile_c = true;
#else
const bool mobile_c = false;
#endif

const string savegame_root = "savegame_";
const string savegame_ext = ".xml";
const string savegame_folder = "savegames/";

const int versionMajor = 0;
const int versionMinor = 2;

enum Difficulty {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_MEDIUM = 1,
    DIFFICULTY_HARD = 2,
    N_DIFFICULTIES = 3
};

enum Direction {
    DIRECTION_W = 0,
    DIRECTION_NW = 1,
    DIRECTION_N = 2,
    DIRECTION_NE = 3,
    DIRECTION_E = 4,
    DIRECTION_SE = 5,
    DIRECTION_S = 6,
    DIRECTION_SW = 7,
    N_DIRECTIONS = 8
};

#ifndef Q_OS_ANDROID
class Sound {
    Phonon::MediaObject *mediaObject;
    Phonon::AudioOutput *audioOutput;
public:
    Sound(Phonon::MediaObject *mediaObject, Phonon::AudioOutput *audioOutput) : mediaObject(mediaObject), audioOutput(audioOutput) {
    }
    ~Sound() {
        delete mediaObject;
        delete audioOutput;
    }

    void play() {
        this->mediaObject->play();
    }
    void pause() {
        this->mediaObject->pause();
    }
    void stop() {
        this->mediaObject->stop();
    }
    void seek(qint64 time) {
        this->mediaObject->seek(time);
    }
    Phonon::State state() const {
        return this->mediaObject->state();
    }
};
#endif

class WebViewEventFilter : public QObject {
    Q_OBJECT

    QWebView *webView;
    bool filterMouseMove;
    int orig_mouse_x, orig_mouse_y;
    int saved_mouse_x, saved_mouse_y;
protected:

    bool eventFilter(QObject *obj, QEvent *event);

public:
    WebViewEventFilter(QObject *parent) : QObject(parent), webView(NULL), filterMouseMove(false), orig_mouse_x(0), orig_mouse_y(0), saved_mouse_x(0), saved_mouse_y(0) {
    }

    void setWebView(QWebView *webView);
};

//class AnimationSet : public QGraphicsItem {
class AnimationSet {
public:
    enum AnimationType {
        ANIMATIONTYPE_LOOP   = 0,
        ANIMATIONTYPE_BOUNCE = 1,
        ANIMATIONTYPE_SINGLE = 2
    };

protected:
    AnimationType animation_type;
    size_t n_frames;
    //QPixmap *pixmaps; // array of length N_DIRECTIONS * n_frames
    vector<QPixmap> pixmaps; // vector of length N_DIRECTIONS * n_frames
    /*QRectF bounding_rect;

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);*/
public:
    AnimationSet(AnimationType animation_type, size_t n_frames, vector<QPixmap> pixmaps); // pixmaps array of length N_DIRECTIONS * n_frames
    virtual ~AnimationSet();

    size_t getNFrames() const {
        return this->n_frames;
    }
    //QPixmap *getFrames(Direction c_direction);
    const QPixmap &getFrame(Direction c_direction, size_t c_frame) const;

    static AnimationSet *create(const QPixmap &image, AnimationType animation_type, int size, int x_offset, size_t n_frames);
};

/* Helper class used to define animation image formats, when loading in the
 * animation frames.
 */
class AnimationLayerDefinition {
    friend class AnimationLayer;
    string name;
    int position;
    size_t n_frames;
    AnimationSet::AnimationType animation_type;
public:
    AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type) :
        name(name), position(position), n_frames(n_frames), animation_type(animation_type) {
    }
};

class AnimationLayer {
    map<string, const AnimationSet *> animation_sets;
    int size; // size of each frame image in pixels
public:
    AnimationLayer(int size) : size(size) {
    }
    ~AnimationLayer() {
    }

    void addAnimationSet(const string &name, const AnimationSet *animation_set) {
        //this->animation_sets[name] = animation_set;
        //this->animation_sets.insert(pair<string, const AnimationSet *>(name, animation_set));
        this->animation_sets[name] = animation_set;
    }
    const AnimationSet *getAnimationSet(const string &name) const {
        map<string, const AnimationSet *>::const_iterator iter = this->animation_sets.find(name);
        if( iter == this->animation_sets.end() )
            return NULL;
        return iter->second;
    }
    int getSize() const {
        return this->size;
    }

    static AnimationLayer *create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions);
    static AnimationLayer *create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions);
};

//class AnimatedObject : public QGraphicsPixmapItem {
class AnimatedObject : public QGraphicsItem {
    //AnimationLayer *animation_layer;
    vector<AnimationLayer *> animation_layers;
    //const AnimationSet *c_animation_set;
    bool set_c_animation_name;
    string c_animation_name;
    vector<const AnimationSet *> c_animation_sets;
    Direction c_direction;
    size_t c_frame;
    int animation_time_start_ms;

    virtual void advance(int phase);
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    //void setFrame(int c_frame);
public:
    AnimatedObject();
    virtual ~AnimatedObject();

    //void setAnimationSet(AnimationSet *animation_set);
    //void setAnimationLayer(AnimationLayer *animation_layer);
    void addAnimationLayer(AnimationLayer *animation_layer);
    void clearAnimationLayers();
    void setAnimationSet(const string &name);
    void setDirection(Direction c_direction);
    int getSize() const;
};

class ScrollingListWidget : public QListWidget {
    Q_OBJECT

    int saved_x;
    int saved_y;

    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
public:
    ScrollingListWidget();
    virtual ~ScrollingListWidget() {
    }
};

// used for passing messages
class GameMessage {
public:
    enum GameMessageType {
        GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING = 0,
        GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD = 1,
        GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS = 2
    };

protected:
    GameMessageType game_message_type;

public:
    GameMessage(GameMessageType game_message_type) : game_message_type(game_message_type) {
    }

    GameMessageType getGameMessageType() const {
        return game_message_type;
    }
};

class StartGameMessage : public GameMessage {
    Difficulty difficulty;
public:
    StartGameMessage(Difficulty difficulty) : GameMessage(GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING), difficulty(difficulty) {
    }

    Difficulty getDifficulty() const {
        return this->difficulty;
    }
};

class LoadGameMessage : public GameMessage {
    string filename;
public:
    LoadGameMessage(string filename) : GameMessage(GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD), filename(filename) {
    }

    const string &getFilename() const {
        return this->filename;
    }
};

class Game : public QObject {
    Q_OBJECT

protected:
    QSettings *settings;

    string application_path;
    string logfilename;
    string oldlogfilename;

    QStyle *style;
    QFont font_small;
    QFont font_std;
    QFont font_big;

    WebViewEventFilter *webViewEventFilter;

    QPalette gui_palette;
    QBrush gui_brush_buttons;

    Gamestate *gamestate;
    Screen *screen;
    queue<GameMessage *> message_queue;

    bool sound_enabled;

private slots:
/*#ifndef Q_OS_ANDROID
    void mediaStateChanged(Phonon::State newstate, Phonon::State oldstate) const;
#endif*/

public:
    Game();
    ~Game();

    Screen *getScreen() {
        return this->screen;
    }
    const Screen *getScreen() const {
        return this->screen;
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

    void initButton(QPushButton *button) const;

    void pushMessage(GameMessage *message) {
        message_queue.push(message);
    }

    void setWebView(QWebView *webView) {
        this->webViewEventFilter->setWebView(webView);
    }

    void run();
    void update();
    //void mouseClick(int m_x, int m_y);
    string getApplicationFilename(const string &name);
    //void log(const char *text, ...);
    void log(const char *text);
    QPixmap loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height) const;
    QPixmap loadImage(const string &filename) const {
        return loadImage(filename, false, 0, 0, 0, 0);
    }
#ifndef Q_OS_ANDROID
    Sound *loadSound(string filename) const;
#endif
    bool isSoundEnabled() const {
        return this->sound_enabled;
    }
    void setSoundEnabled(bool sound_enabled);
    static string getDifficultyString(Difficulty difficulty);

    void showErrorDialog(const string &message);
    /*void showInfoDialog(const string &title, const string &message);
    bool askQuestionDialog(const string &title, const string &message);*/
};

extern Game *game_g;

/*const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else game_g->log
#endif*/
