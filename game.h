#pragma once

#include "common.h"

#include <QtGui>
#include <QtWebKit/QWebView>

// Phonon not supported on Qt Android
#ifdef Q_OS_ANDROID

#include "androidaudio/androidaudio.h"

#else
#include <phonon/MediaObject>
#include <phonon/AudioOutput>
#endif

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

class Screen;
class Gamestate;
class Quest;
class Location;
class Item;
class Currency;
class Shop;
class CharacterTemplate;
class Character;

const string savegame_root = "savegame_";
const string savegame_ext = ".xml";
const string savegame_folder = "savegames/";

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
#else
typedef AndroidSoundEffect Sound;
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

class AnimationSet {
public:
    enum AnimationType {
        ANIMATIONTYPE_LOOP   = 0,
        ANIMATIONTYPE_BOUNCE = 1,
        ANIMATIONTYPE_SINGLE = 2
    };

protected:
    AnimationType animation_type;
    unsigned int n_dimensions;
    size_t n_frames;
    vector<QPixmap> pixmaps; // vector of length n_dimensions * n_frames
    /*QRectF bounding_rect;

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);*/
public:
    AnimationSet(AnimationType animation_type, unsigned int n_dimensions, size_t n_frames, vector<QPixmap> pixmaps); // pixmaps array of length n_dimensions * n_frames
    virtual ~AnimationSet();

    unsigned int getNDimensions() const {
        return this->n_dimensions;
    }
    size_t getNFrames() const {
        return this->n_frames;
    }
    const QPixmap &getFrame(unsigned int c_dimension, size_t c_frame) const;

    static AnimationSet *create(const QPixmap &image, AnimationType animation_type, int stride_x, int stride_y, int x_offset, unsigned int n_dimensions, size_t n_frames, int icon_off_x, int icon_off_y, int icon_width, int icon_height);
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
    //int off_x, off_y, width, height;
    //float off_x, off_y, width, height;
public:
    //AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type, int off_x, int off_y, int width, int height) :
    //AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type, float off_x, float off_y, float width, float height) :
    AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type) :
        //name(name), position(position), n_frames(n_frames), animation_type(animation_type), off_x(off_x), off_y(off_y), width(width), height(height) {
        name(name), position(position), n_frames(n_frames), animation_type(animation_type) {
    }
};

class AnimationLayer {
    map<string, const AnimationSet *> animation_sets;
    int width, height; // size of each frame image in pixels
public:
    AnimationLayer(int width, int height) : width(width), height(height) {
    }
    ~AnimationLayer();

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
    int getWidth() const {
        return this->width;
    }
    int getHeight() const {
        return this->height;
    }

    static AnimationLayer *create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
    static AnimationLayer *create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
};

class AnimatedObject : public QGraphicsItem {
    bool is_static_image;
    QPixmap static_image;
    vector<AnimationLayer *> animation_layers;
    bool set_c_animation_name;
    string c_animation_name;
    vector<const AnimationSet *> c_animation_sets;
    unsigned int c_dimension;
    size_t c_frame;
    int animation_time_start_ms;
    bool bounce; // whether to bounce up and down

    virtual void advance(int phase);
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    //void setFrame(int c_frame);
public:
    AnimatedObject();
    virtual ~AnimatedObject();

    void addAnimationLayer(AnimationLayer *animation_layer);
    void clearAnimationLayers();
    void setAnimationSet(const string &name, bool force_restart);
    void setDimension(unsigned int c_dimension);
    int getWidth() const;
    int getHeight() const;
    void setStaticImage(const QPixmap &static_image) {
        this->is_static_image = true;
        this->static_image = static_image;
    }
    void clearStaticImage() {
        this->is_static_image = false;
        this->static_image = NULL;
    }
    void setBounce(bool bounce) {
        this->bounce = bounce;
    }
};

class Particle {
    float xpos, ypos; // floats to allow for movement
    int birth_time;
public:
    Particle();

    float getX() const {
            return this->xpos;
    }
    float getY() const {
            return this->ypos;
    }
    void setPos(float xpos, float ypos) {
            this->xpos = xpos;
            this->ypos = ypos;
    }
    int getBirthTime() const {
            return this->birth_time;
    }
};

class ParticleSystem : public QGraphicsItem {
protected:
    vector<Particle> particles;
    QPixmap pixmap;

public:
    ParticleSystem(const QPixmap &pixmap) : pixmap(pixmap) {
    }

    virtual void advance(int phase);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    virtual QRectF boundingRect() const;
    virtual void update()=0;
};

class SmokeParticleSystem : public ParticleSystem {
    float birth_rate;
    int life_exp;
    int last_emit_time;
public:
    SmokeParticleSystem(const QPixmap &pixmap);
    void setBirthRate(float birth_rate);

    virtual void update();
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
    size_t player_type;
public:
    StartGameMessage(Difficulty difficulty, size_t player_type) : GameMessage(GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING), difficulty(difficulty), player_type(player_type) {
    }

    Difficulty getDifficulty() const {
        return this->difficulty;
    }
    size_t getPlayerType() const {
        return this->player_type;
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
    map<string, Sound *> sound_effects;

#ifdef Q_OS_ANDROID
    AndroidAudio androidAudio;
#endif

    QSettings *settings;

    string application_path;
    string logfilename;
    string oldlogfilename;

    QStyle *style;
    QFont font_scene;
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
    bool lighting_enabled;

    vector<string> player_types;

    void createPlayerNames();
    void runTest(const string &filename, int test_id);

public:
    Game();
    ~Game();

    Screen *getScreen() {
        return this->screen;
    }
    const Screen *getScreen() const {
        return this->screen;
    }
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

    void initButton(QPushButton *button) const;

    void pushMessage(GameMessage *message) {
        message_queue.push(message);
    }

    void setWebView(QWebView *webView) {
        this->webViewEventFilter->setWebView(webView);
    }

    void run();
    void update();
    void updateInput();
    void render();
    void runTests();

    void activate(bool active);
    Gamestate *getGamestate() const {
        return this->gamestate;
    }
    //void mouseClick(int m_x, int m_y);
    string getApplicationFilename(const string &name);
    //void log(const char *text, ...);
    void log(const char *text);
    QPixmap loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height, int expected_width) const;
    QPixmap loadImage(const string &filename) const {
        return loadImage(filename, false, 0, 0, 0, 0, 0);
    }
    void loadSound(const string &id, const string &filename);
    void playSound(const string &sound_effect);
    void pauseSound(const string &sound_effect);
    void freeSound(const string &sound_effect);
    bool isSoundEnabled() const {
        return this->sound_enabled;
    }
    void setSoundEnabled(bool sound_enabled);
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
    Character *createPlayer(size_t i) const;

    void showErrorDialog(const string &message);
    /*void showInfoDialog(const string &title, const string &message);
    bool askQuestionDialog(const string &title, const string &message);*/
};

extern Game *game_g;

/*const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else game_g->log
#endif*/
