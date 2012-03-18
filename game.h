#pragma once

#include "rpg/character.h"
#include "rpg/location.h"

#include <QtGui>

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
class PlayingGamestate;
class Location;
class Item;

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
    int n_frames;
    //QPixmap *pixmaps; // array of length N_DIRECTIONS * n_frames
    vector<QPixmap> pixmaps; // vector of length N_DIRECTIONS * n_frames
    /*QRectF bounding_rect;

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);*/
public:
    AnimationSet(AnimationType animation_type, int n_frames, vector<QPixmap> pixmaps); // pixmaps array of length N_DIRECTIONS * n_frames
    virtual ~AnimationSet();

    int getNFrames() const {
        return this->n_frames;
    }
    //QPixmap *getFrames(Direction c_direction);
    const QPixmap &getFrame(Direction c_direction, int c_frame) const;

    static AnimationSet *create(QPixmap image, AnimationType animation_type, int x_offset, int n_frames);
};

/* Helper class used to define animation image formats, when loading in the
 * animation frames.
 */
class AnimationLayerDefinition {
    friend class AnimationLayer;
    string name;
    int position;
    int n_frames;
    AnimationSet::AnimationType animation_type;
public:
    AnimationLayerDefinition(string name, int position, int n_frames, AnimationSet::AnimationType animation_type) :
        name(name), position(position), n_frames(n_frames), animation_type(animation_type) {
    }
};

class AnimationLayer {
    map<string, const AnimationSet *> animation_sets;
public:
    AnimationLayer() {
    }
    ~AnimationLayer() {
    }

    void addAnimationSet(string name, const AnimationSet *animation_set) {
        //this->animation_sets[name] = animation_set;
        //this->animation_sets.insert(pair<string, const AnimationSet *>(name, animation_set));
        this->animation_sets[name] = animation_set;
    }
    const AnimationSet *getAnimationSet(string name) const {
        map<string, const AnimationSet *>::const_iterator iter = this->animation_sets.find(name);
        if( iter == this->animation_sets.end() )
            return NULL;
        return iter->second;
    }

    static AnimationLayer *create(const char *filename, const vector<AnimationLayerDefinition> &animation_layer_definitions);
};

//class AnimatedObject : public QGraphicsPixmapItem {
class AnimatedObject : public QGraphicsItem {
    //AnimationLayer *animation_layer;
    vector<AnimationLayer *> animation_layers;
    //const AnimationSet *c_animation_set;
    vector<const AnimationSet *> c_animation_sets;
    Direction c_direction;
    int c_frame;
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
    void setAnimationSet(string name);
    void setDirection(Direction c_direction);
};

class Image {
    QPixmap pixmap;
public:
    Image(const QPixmap &pixmap) : pixmap(pixmap) {
    }
    virtual ~Image() {
    }

    const QPixmap &getPixmap() const {
        return this->pixmap;
    }
};

class Gamestate : public QObject {
public:
    //virtual VI_Panel *getPanel()=0;
    virtual void quitGame()=0;
    virtual void update()=0;
    /*virtual void mouseClick(int m_x, int m_y) {
    }*/
};

class OptionsGamestate : public Gamestate {
    Q_OBJECT

    static OptionsGamestate *optionsGamestate; // singleton pointer, needed for static member functions
    /*VI_Panel  *gamePanel;
    VI_Button *startgameButton;
    VI_Button *quitgameButton;*/

    //static void action(VI_Panel *source);

private slots:
    void clickedStart();
    void clickedLoad();
    void clickedQuit();

public:
    OptionsGamestate();
    virtual ~OptionsGamestate();

    /*virtual VI_Panel *getPanel() {
        return gamePanel;
    }*/

    virtual void quitGame();
    virtual void update() {
    }
};

class GUIOverlay;

class MainGraphicsView : public QGraphicsView {
    PlayingGamestate *playing_gamestate;
    int mouse_down_x, mouse_down_y;
    //QGraphicsProxyWidget *gui_overlay_item;
    GUIOverlay *gui_overlay;

    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

public:
    MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent) :
        QGraphicsView(scene, parent), playing_gamestate(playing_gamestate), mouse_down_x(0), mouse_down_y(0), /*gui_overlay_item(NULL),*/ gui_overlay(NULL)
    {
    }
    virtual ~MainGraphicsView() {
    }

    void setGUIOverlay(/*QGraphicsProxyWidget *gui_overlay_item, */GUIOverlay *gui_overlay) {
        //this->gui_overlay_item = gui_overlay_item;
        this->gui_overlay = gui_overlay;
    }
};

class GUIOverlay : public QWidget {
    PlayingGamestate *playing_gamestate;

    virtual void paintEvent(QPaintEvent *event);
    /*virtual QSize sizeHint() const {
        return QSize(640, 360);
    }*/

    bool display_progress;
    int  progress_percent;

    void drawBar(QPainter &painter, int x, int y, int width, int height, float fraction, QColor color);
public:
    GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view);
    virtual ~GUIOverlay() {
    }

    void setProgress(int progress_percent) {
        this->display_progress = true;
        this->progress_percent = progress_percent;
    }
    void unsetProgress() {
        this->display_progress = false;
    }
};

/*class GUIOverlayItem : public QGraphicsProxyWidget {
    MainGraphicsView *view;

    virtual void advance(int phase);

public:
    GUIOverlayItem(MainGraphicsView *view) : QGraphicsProxyWidget(), view(view) {
    }
    virtual ~GUIOverlayItem() {
    }
};*/

/*class StatusBar : public QWidget {
    int percent;
    QColor color;

    virtual void paintEvent(QPaintEvent *event);

public:
    StatusBar() : percent(100), color(Qt::white) {
    }
    virtual ~StatusBar() {
    }
};*/

class ItemsWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    QListWidget *list;
    vector<Item *> list_items;

    QPushButton *armButton;
    QPushButton *wearButton;

    QString getItemString(const Item *item) const;

private slots:
    void changedSelectedItem(int currentRow);

    void clickedDropItem();
    void clickedArmWeapon();
    void clickedWearArmour();

public:
    ItemsWindow(PlayingGamestate *playing_gamestate);
    virtual ~ItemsWindow() {
    }
};

class PlayingGamestate : public Gamestate, CharacterListener, LocationListener {
    Q_OBJECT

    static PlayingGamestate *playingGamestate; // singleton pointer, needed for static member functions

    QGraphicsScene *scene;
    MainGraphicsView *view;
    GUIOverlay *gui_overlay;

    QWidget *subwindow;
    //QListWidget *list;
    //vector<Item *> list_items;

    Character *player;

    Location *location;

    // character items in the view
    set<QGraphicsItem *> graphicsitems_characters;

    // data
    map<string, AnimationLayer *> animation_layers;
    map<string, Item *> standard_items;
    map<string, QPixmap> item_images;

private slots:
    void clickedItems();
    void clickedOptions();
    void clickedQuit();
    void clickedCloseSubwindow();

public:
    PlayingGamestate();
    virtual ~PlayingGamestate();

    virtual void quitGame();
    virtual void update();
    //virtual void mouseClick(int m_x, int m_y);

    virtual void characterUpdateGraphics(const Character *character, void *user_data);
    virtual void characterTurn(const Character *character, void *user_data, Vector2D dir);
    virtual void characterMoved(const Character *character, void *user_data);
    virtual void characterSetAnimation(const Character *character, void *user_data, string name);
    virtual void characterDeath(Character *character, void *user_data);

    virtual void locationAddItem(const Location *location, Item *item);
    virtual void locationRemoveItem(const Location *location, Item *item);

    void clickedMainView(float scene_x, float scene_y);

    Character *getPlayer() {
        return this->player;
    }
    const Character *getPlayer() const {
        return this->player;
    }
    Location *getLocation() {
        return this->location;
    }
    const Location *getLocation() const {
        return this->location;
    }

    void addStandardItem(Item *item);
    Item *cloneStandardItem(string name);
};

// used for passing messages
class GameMessage {
public:
    enum GameMessageType {
        GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING = 0,
        GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS = 1
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

class Game {
    string application_path;
    string logfilename;
    string oldlogfilename;

    Gamestate *gamestate;
    Screen *screen;
    queue<GameMessage *> message_queue;

public:
    Game();
    ~Game() {
    }

    Screen *getScreen() {
        return this->screen;
    }
    const Screen *getScreen() const {
        return this->screen;
    }

    void pushMessage(GameMessage *message) {
        message_queue.push(message);
    }

    void run();
    void update();
    //void mouseClick(int m_x, int m_y);
    string getApplicationFilename(const char *name);
    void log(const char *text, ...);
    QPixmap loadImage(const char *filename, bool clip, int xpos, int ypos, int width, int height) const;
    QPixmap loadImage(const char *filename) const {
        return loadImage(filename, false, 0, 0, 0, 0);
    }
    void showErrorWindow(const char *message);
    void showInfoWindow(const char *title, const char *message);
    bool askQuestionWindow(const char *title, const char *message);
};

extern Game *game_g;

const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else game_g->log
#endif
