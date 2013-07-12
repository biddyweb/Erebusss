#pragma once

#include "common.h"

#include <QtGui>
#include <QtWebKit/QWebView>

// Phonon not supported on Qt Android
#ifdef Q_OS_ANDROID

#include "androidaudio/androidaudio.h"

// hack for Android, as moc doesn't seem to recognise precompiler, so can't be made to ignore stateChanged().

class Phonon {
 public:
    enum State {
    };
};

#else

#ifdef USING_PHONON
#include <phonon/MediaObject>
#include <phonon/AudioOutput>
#else

#include <SFML/Audio.hpp>

#endif

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

//const QString savegame_root = "savegame_";
const QString savegame_ext = ".xml";
const QString savegame_folder = "savegames/";

class Sound : public QObject {
    Q_OBJECT

#ifndef Q_OS_ANDROID

    float volume;
    bool stream;
#ifdef USING_PHONON
    Phonon::MediaObject *mediaObject;
    Phonon::AudioOutput *audioOutput;
#else
    // if not streaming:
    sf::SoundBuffer buffer;
    sf::Sound sound;
    // if streaming:
    sf::Music music;
#endif

public:
    Sound(const string &filename, bool stream);
    ~Sound() {
#ifdef USING_PHONON
        delete mediaObject;
        delete audioOutput;
#endif
    }

    bool isStream() const {
        return this->stream;
    }
    void updateVolume(); // updates the volume to take the global game volume into account
    void setVolume(float volume);
    void play(bool loop) {
        // if already playing, this function has no effect (doesn't restart)
        //qDebug("play");
#ifdef USING_PHONON
        if( this->mediaObject != NULL ) {
            if( this->mediaObject->state() == Phonon::PlayingState ) {
                //qDebug("    already playing, pos %d", this->mediaObject->currentTime());
            }
            else {
                this->updateVolume();
                this->mediaObject->seek(0);
                this->mediaObject->play();
            }
        }
#else
        if( stream ) {
            if( music.getStatus() == sf::SoundSource::Playing ) {
                //qDebug("    already playing");
            }
            else {
                this->updateVolume();
                music.setLoop(loop);
                music.play();
            }
        }
        else {
            if( sound.getStatus() == sf::SoundSource::Playing ) {
                //qDebug("    already playing");
            }
            else {
                this->updateVolume();
                sound.setLoop(loop);
                sound.play();
            }
        }
#endif
    }
    void pause() {
#ifdef USING_PHONON
        if( this->mediaObject != NULL ) {
            this->mediaObject->pause();
        }
#else
        if( stream ) {
            music.pause();
        }
        else {
            sound.pause();
        }
#endif
    }
    void stop() {
#ifdef USING_PHONON
        if( this->mediaObject != NULL ) {
            this->mediaObject->stop();
        }
#else
        if( stream ) {
            music.stop();
        }
        else {
            sound.stop();
        }
#endif
    }

#else
    AndroidSoundEffect *android_sound;

private slots:
    void aboutToFinish() {
    }
public:
    Sound(AndroidSoundEffect *android_sound) : android_sound(android_sound) {
    }
    ~Sound() {
        delete android_sound;
    }

    AndroidSoundEffect *getAndroidSound() const {
        return android_sound;
    }
#endif

};

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
public:
    AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type) :
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

    static AnimationLayer *create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
    static AnimationLayer *create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
};

class AnimatedObject : public QGraphicsItem {
    int ms_per_frame;
    vector<AnimationLayer *> animation_layers;
    bool set_c_animation_name;
    string c_animation_name;
    vector<const AnimationSet *> c_animation_sets;
    unsigned int c_dimension;
    size_t c_frame;
    int animation_time_start_ms;
    bool bounce; // whether to bounce up and down
    bool clip; // whether to only draw a portion
    float clip_sx, clip_sy, clip_sw, clip_sh;

    virtual void advance(int phase);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public:
    AnimatedObject(int ms_per_frame);
    virtual ~AnimatedObject();

    virtual QRectF boundingRect() const;

    void addAnimationLayer(AnimationLayer *animation_layer);
    void clearAnimationLayers();
    void setAnimationSet(const string &name, bool force_restart);
    void setDimension(unsigned int c_dimension);
    int getWidth() const;
    int getHeight() const;
    void setBounce(bool bounce) {
        this->bounce = bounce;
    }
    void setClip(float sx, float sy, float sw, float sh) {
        this->clip = true;
        this->clip_sx = sx;
        this->clip_sy = sy;
        this->clip_sw = sw;
        this->clip_sh = sh;
    }
};

class LazyAnimationLayer {
    AnimationLayer *animation_layer;
    // if animation_layer == NULL, then we also store the information to load it when required:
    string filename;
    vector<AnimationLayerDefinition> animation_layer_definitions;
    bool clip; // must be true for animations
    int off_x;
    int off_y;
    int width;
    int height;
    int stride_x;
    int stride_y;
    int expected_total_width;
    unsigned int n_dimensions;
public:
    LazyAnimationLayer(AnimationLayer *animation_layer) :
        animation_layer(animation_layer), clip(false), off_x(0), off_y(0), width(0), height(0), stride_x(0), stride_y(0), expected_total_width(0), n_dimensions(0)
    {
    }
    LazyAnimationLayer(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) :
        animation_layer(NULL), filename(filename), animation_layer_definitions(animation_layer_definitions), clip(clip), off_x(off_x), off_y(off_y), width(width), height(height), stride_x(stride_x), stride_y(stride_y), expected_total_width(expected_total_width), n_dimensions(n_dimensions)
    {
    }
    LazyAnimationLayer(const QPixmap &pixmap, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);

    ~LazyAnimationLayer() {
        if( this->animation_layer != NULL ) {
            delete animation_layer;
        }
    }

    AnimationLayer *getAnimationLayer();
};

class Particle {
    float xpos, ypos; // floats to allow for movement
    int birth_time;
    bool flag;
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
    void setFlag(bool flag) {
        this->flag = flag;
    }
    bool isFlag() const {
        return this->flag;
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
    virtual void updatePS()=0;
};

class SmokeParticleSystem : public ParticleSystem {
    float birth_rate;
    int life_exp;
    int last_emit_time;
public:
    SmokeParticleSystem(const QPixmap &pixmap);
    void setBirthRate(float birth_rate);

    virtual void updatePS();
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
    map<string, Sound *> sound_effects;
    string current_stream_sound_effect;

#ifdef Q_OS_ANDROID
    AndroidAudio androidAudio;
#endif

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

    QPalette gui_palette;
    QBrush gui_brush_buttons;

    Gamestate *gamestate;
    Screen *screen;
    queue<GameMessage *> message_queue;

#ifndef Q_OS_ANDROID
    int sound_volume;
#endif
    bool lighting_enabled;

    vector<string> player_types;

    void init(bool fullscreen);
    void createPlayerNames();
    void runTest(const string &filename, int test_id);

private slots:
#ifdef USING_PHONON
    void stateChanged(Phonon::State newstate, Phonon::State oldstate) const;
#endif

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

    void initButton(QWidget *button) const;

    void pushMessage(GameMessage *message) {
        message_queue.push(message);
    }

    void setWebView(QWebView *webView) {
        this->webViewEventFilter->setWebView(webView);
    }

    void run(bool fullscreen);
    void handleMessages();
    void update();
    void updateInput();
    void render();
    void runTests();

    void activate(bool active);
    void keyPress(QKeyEvent *key_event);
    Gamestate *getGamestate() const {
        return this->gamestate;
    }
    //void mouseClick(int m_x, int m_y);

#ifdef Q_OS_ANDROID
    bool isSDCardOk() const {
        return this->sdcard_ok;
    }
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
    void fillSaveGameFiles(ScrollingListWidget **list, vector<QString> *filenames) const;

    void showErrorDialog(const string &message);
    void showInfoDialog(const string &title, const string &message);
    //bool askQuestionDialog(const string &title, const string &message);
};

extern Game *game_g;

/*const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else game_g->log
#endif*/
