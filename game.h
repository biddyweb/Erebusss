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
class Currency;
class CharacterTemplate;

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

    static AnimationSet *create(QPixmap image, AnimationType animation_type, int x_offset, size_t n_frames);
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
    void setAnimationSet(string name);
    void setDirection(Direction c_direction);
};

class MainGraphicsView;

class TextEffect : public QGraphicsTextItem {
    int time_expire;
    MainGraphicsView *view;

    virtual void advance(int phase);
public:
    TextEffect(MainGraphicsView *view, const QString &text, int duration_ms);
    virtual ~TextEffect() {
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
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    int mouse_down_x, mouse_down_y;
    //QGraphicsProxyWidget *gui_overlay_item;
    GUIOverlay *gui_overlay;
    float c_scale;
    set<TextEffect *> text_effects;

    const static float min_zoom_c;
    const static float max_zoom_c;

    void zoom(bool in);

    virtual bool event(QEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

public:
    MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent);
    virtual ~MainGraphicsView() {
    }

    void setGUIOverlay(/*QGraphicsProxyWidget *gui_overlay_item, */GUIOverlay *gui_overlay) {
        //this->gui_overlay_item = gui_overlay_item;
        this->gui_overlay = gui_overlay;
    }
    void setScale(float c_scale);
    float getScale() const {
        return this->c_scale;
    }
    void addTextEffect(TextEffect *text_effect);
    void removeTextEffect(TextEffect *text_effect);

public slots:
    void zoomOut() {
        this->zoom(false);
    }
    void zoomIn() {
        this->zoom(true);
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

    void drawBar(QPainter &painter, float fx, float fy, float fwidth, float fheight, float fraction, QColor color);
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

class StatsWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;

private slots:

public:
    StatsWindow(PlayingGamestate *playing_gamestate);
    virtual ~StatsWindow() {
    }
};

class ScrollingListWidget : public QListWidget {
    Q_OBJECT

    int saved_y;

    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
public:
    ScrollingListWidget();
    virtual ~ScrollingListWidget() {
    }
};

class ItemsWindow : public QWidget {
//class ItemsWindow : public QMainWindow {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    QListWidget *list;
    vector<Item *> list_items;

    QLabel *weightLabel;

    QPushButton *dropButton;
    QPushButton *armButton;
    QPushButton *wearButton;
    QPushButton *useButton;

    enum ViewType {
        VIEWTYPE_ALL = 0,
        VIEWTYPE_WEAPONS = 1,
        VIEWTYPE_SHIELDS = 2,
        VIEWTYPE_ARMOUR = 3,
        VIEWTYPE_AMMO = 4,
        VIEWTYPE_MAGIC = 5
    };
    ViewType view_type;

    void refreshList();
    void changeView(ViewType view_type);
    QString getItemString(const Item *item) const;
    //void setWeightLabel(int weight);
    void setWeightLabel();
    void itemIsDeleted(size_t index);

private slots:
    void changedSelectedItem(int currentRow);

    void clickedViewAll();
    void clickedViewWeapons();
    void clickedViewShields();
    void clickedViewArmour();
    void clickedViewAmmo();
    void clickedViewMagic();

    void clickedDropItem();
    void clickedArmWeapon();
    void clickedWearArmour();
    void clickedUseItem();

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

    //QWidget *mainwindow;
    QWidget *subwindow;
    QStackedWidget *main_stacked_widget;
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
    map<string, QPixmap> scenery_images;
    map<string, QPixmap> scenery_opened_images;
    map<string, CharacterTemplate *> character_templates;

private slots:
    void clickedStats();
    void clickedItems();
    void clickedOptions();
    void clickedRest();
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

    virtual void locationAddScenery(const Location *location, Scenery *scenery);
    virtual void locationRemoveScenery(const Location *location, Scenery *scenery);
    virtual void locationUpdateScenery(Scenery *scenery);

    void clickedMainView(float scene_x, float scene_y);
    void addWidget(QWidget *widget);
    void addTextEffect(string text, Vector2D pos, int duration_ms);

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
    Item *cloneStandardItem(string name) const;
    Currency *cloneGoldItem(int value) const;
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

    QFont font_std;
    QFont font_big;

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
    QFont getFontStd() const {
        return this->font_std;
    }
    QFont getFontBig() const {
        return this->font_big;
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
