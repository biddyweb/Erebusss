#pragma once

#include "common.h"

#include <QtGui>

#include <sstream>
using std::stringstream;

#include <map>
using std::map;

#include "common.h"
#include "gamestate.h"
#include "rpg/character.h"
#include "rpg/location.h"

class MainGraphicsView;
class AnimationLayer;
class Shop;
class Currency;
class Spell;

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

class TextEffect : public QGraphicsTextItem {
    int time_expire;
    MainGraphicsView *view;

    virtual void advance(int phase);
public:
    TextEffect(MainGraphicsView *view, const QString &text, int duration_ms, const QColor &color);
    virtual ~TextEffect() {
    }
};

class GUIOverlay;

class MainGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    enum KeyCode {
        KEY_L = 0,
        KEY_R = 1,
        KEY_U = 2,
        KEY_D = 3,
        KEY_LU = 4,
        KEY_RU = 5,
        KEY_LD = 6,
        KEY_RD = 7,
        N_KEYS = 8
    };

private:
    PlayingGamestate *playing_gamestate;
    int mouse_down_x, mouse_down_y;
    bool single_left_mouse_down; // if left mouse button is down, and not a multitouch operation
    bool has_last_mouse;
    int last_mouse_x, last_mouse_y;
    //float kinetic_scroll_x, kinetic_scroll_y;
    bool has_kinetic_scroll;
    Vector2D kinetic_scroll_dir;
    float kinetic_scroll_speed;
    //QGraphicsProxyWidget *gui_overlay_item;
    GUIOverlay *gui_overlay;
    float c_scale;
    set<TextEffect *> text_effects;
    bool calculated_lighting_pixmap;
    QPixmap lighting_pixmap;
    bool calculated_lighting_pixmap_scaled;
    int lasttime_calculated_lighting_pixmap_scaled_ms;
    QPixmap lighting_pixmap_scaled;
    unsigned char darkness_alpha;

    bool key_down[N_KEYS];

    bool handleKey(const QKeyEvent *event, bool down);

    const static float min_zoom_c;
    const static float max_zoom_c;

    void zoom(QPointF zoom_centre, bool in);

    virtual bool viewportEvent(QEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void paintEvent(QPaintEvent *event);

public:
    MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent);
    virtual ~MainGraphicsView() {
    }

    void setGUIOverlay(/*QGraphicsProxyWidget *gui_overlay_item, */GUIOverlay *gui_overlay) {
        //this->gui_overlay_item = gui_overlay_item;
        this->gui_overlay = gui_overlay;
    }
    void createLightingMap(unsigned char lighting_min);
    void update();
    void setScale(float c_scale);
    void setScale(QPointF centre, float c_scale);
    float getScale() const {
        return this->c_scale;
    }
    void addTextEffect(TextEffect *text_effect);
    void removeTextEffect(TextEffect *text_effect);
    void clear() {
        this->scene()->clear();
        this->text_effects.clear();
    }
    bool keyDown(KeyCode code) {
        return this->key_down[(int)code];
    }

public slots:
    void zoomOut();
    void zoomIn();
    void centreOnPlayer();
};

class GUIOverlay : public QWidget {
    PlayingGamestate *playing_gamestate;

    virtual void paintEvent(QPaintEvent *event);
    /*virtual QSize sizeHint() const {
        return QSize(640, 360);
    }*/

    bool display_progress;
    int  progress_percent;

    float fps;

    void drawBar(QPainter &painter, float fx, float fy, float fwidth, float fheight, float fraction, QColor color);
public:
    GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view);
    virtual ~GUIOverlay() {
    }

    void setProgress(int progress_percent) {
        this->display_progress = true;
        this->progress_percent = progress_percent;
        this->repaint();
    }
    void unsetProgress() {
        this->display_progress = false;
    }
    void setFPS(float fps) {
        this->fps = fps;
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

    QString writeStat(const string &visual_name, const string &stat_key, bool is_float) const;
private slots:

public:
    StatsWindow(PlayingGamestate *playing_gamestate);
    virtual ~StatsWindow() {
    }
};

class ItemsWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    QListWidget *list;
    vector<Item *> list_items;

    QLabel *weightLabel;

    QPushButton *armButton;
    QPushButton *wearButton;
    QPushButton *useButton;
    QPushButton *dropButton;
    QPushButton *infoButton;

    enum ViewType {
        VIEWTYPE_ALL = 0,
        VIEWTYPE_WEAPONS = 1,
        VIEWTYPE_AMMO = 2,
        VIEWTYPE_SHIELDS = 3,
        VIEWTYPE_ARMOUR = 4,
        VIEWTYPE_MAGIC = 5,
        VIEWTYPE_MISC = 6
    };
    ViewType view_type;

    void refreshList();
    void changeView(ViewType view_type);
    QString getItemString(const Item *item) const;
    void setWeightLabel();
    void itemIsDeleted(size_t index);

private slots:
    void changedSelectedItem(int currentRow);

    void clickedViewAll();
    void clickedViewWeapons();
    void clickedViewAmmo();
    void clickedViewShields();
    void clickedViewArmour();
    void clickedViewMagic();
    void clickedViewMisc();

    void clickedArmWeapon();
    void clickedWearArmour();
    void clickedUseItem();
    void clickedDropItem();
    void clickedInfo();

public:
    ItemsWindow(PlayingGamestate *playing_gamestate);
    virtual ~ItemsWindow() {
    }
};

class TradeWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;

    QLabel *goldLabel;
    QLabel *weightLabel;

    QListWidget *list;
    const vector<const Item *> &items;
    const vector<int> &costs;

    QListWidget *player_list;
    vector<Item *> player_items;
    vector<int> player_costs;

    void addPlayerItem(Item *item, int buy_cost);
    void updateGoldLabel();
    void setWeightLabel();

private slots:
    void clickedBuy();
    void clickedSell();
    void clickedInfo();

public:
    TradeWindow(PlayingGamestate *playing_gamestate, const vector<const Item *> &items, const vector<int> &costs);
    virtual ~TradeWindow() {
    }
};

class ItemsPickerWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;

    QListWidget *list;
    vector<Item *> items;

    QListWidget *player_list;
    vector<Item *> player_items;

    QLabel *weightLabel;

    void addGroundItem(const Item *item);
    void addPlayerItem(Item *item);
    void refreshPlayerItems();
    void setWeightLabel();

private slots:
    void clickedPickUp();
    void clickedDrop();
    void clickedInfo();

public:
    ItemsPickerWindow(PlayingGamestate *playing_gamestate, vector<Item *> items);
    virtual ~ItemsPickerWindow() {
    }
};

class CampaignWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;

private slots:
    void clickedClose();
    void clickedShop();
    //void clickedTraining();

public:
    CampaignWindow(PlayingGamestate *playing_gamestate);
    virtual ~CampaignWindow() {
    }
};

class SaveGameWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    QListWidget *list;
    QLineEdit *edit;

private slots:
    void clickedSave();
    void clickedDelete();
    void clickedSaveNew();

public:
    SaveGameWindow(PlayingGamestate *playing_gamestate);
    virtual ~SaveGameWindow() {
    }
};

class PlayingGamestate : public Gamestate, CharacterListener, LocationListener {
    Q_OBJECT

    static PlayingGamestate *playingGamestate; // singleton pointer, needed for static member functions

    QGraphicsScene *scene;
    MainGraphicsView *view;
    GUIOverlay *gui_overlay;

    QStackedWidget *main_stacked_widget;

    Difficulty difficulty;

    Character *player;
    stringstream journal_ss;

    vector<QuestInfo> quest_list;
    size_t c_quest_indx;
    Location *c_location;
    Quest *quest;

    // character items in the view
    set<QGraphicsItem *> graphicsitems_characters;

    // data
    map<string, AnimationLayer *> animation_layers;
    map<string, Item *> standard_items;
    map<string, QPixmap> item_images;
    map<string, QPixmap> scenery_images;
    map<string, AnimationLayer *> scenery_animation_layers;
    map<string, QPixmap> builtin_images;
    map<string, QPixmap> npc_static_images;
    map<string, CharacterTemplate *> character_templates;
    map<string, Spell *> spells;
    vector<Shop *> shops;

    Item *parseXMLItem(QXmlStreamReader &reader);
    void updateVisibilityForFloorRegion(FloorRegion *floor_region);
    void updateVisibility(Vector2D pos);
    void moveToLocation(Location *location, Vector2D pos);
    void setupView();
    void autoSave() {
        this->saveGame("autosave.xml");
    }
    void requestPlayerMove(Vector2D dest, const Scenery *ignore_scenery);
    void clickedOnNPC(Character *character);
    bool handleClickForItems(Vector2D dest);
    bool clickedOnScenerys(bool *move, Scenery **ignore_scenery, const vector<Scenery *> &clicked_scenerys);
    bool handleClickForScenerys(bool *move, Scenery **ignore_scenery, Vector2D dest);

    void saveItem(FILE *file, const Item *item) const;
    void saveItem(FILE *file, const Item *item, const Character *character) const;
    void saveTrap(FILE *file, const Trap *trap) const;

private slots:
    void clickedStats();
    void clickedItems();
    void clickedJournal();
    void clickedOptions();
    void clickedRest();
    void clickedSave();
    void clickedQuit();
    void playBackgroundMusic();

public:
    PlayingGamestate(bool is_savegame, size_t player_type);
    virtual ~PlayingGamestate();

    float getDifficultyModifier() const;
    Difficulty getDifficulty() const {
        return this->difficulty;
    }
    void setDifficulty(Difficulty difficulty);
    void loadQuest(string filename, bool is_savegame);
    bool saveGame(const string &filename) const;

    virtual void quitGame();
    virtual void update();
    virtual void activate(bool active) {
        // n.b., don't autosave for now - if we ever allow this, we need to make sure that it doesn't autosave if enemies are nearby (as with normal save game rules!)
        /*if( !active ) {
            this->autoSave();
        }*/
    }
    //virtual void mouseClick(int m_x, int m_y);
    void checkQuestComplete();

    virtual void characterUpdateGraphics(const Character *character, void *user_data);
    virtual void characterTurn(const Character *character, void *user_data);
    virtual void characterMoved(Character *character, void *user_data);
    virtual void characterSetAnimation(const Character *character, void *user_data, const string &name, bool force_restart);
    virtual void characterDeath(Character *character, void *user_data);

    virtual void locationAddItem(const Location *location, Item *item, bool visible);
    virtual void locationRemoveItem(const Location *location, Item *item);

    virtual void locationAddScenery(const Location *location, Scenery *scenery);
    virtual void locationRemoveScenery(const Location *location, Scenery *scenery);
    virtual void locationUpdateScenery(Scenery *scenery);

    virtual void locationAddCharacter(const Location *location, Character *character);

    void actionCommand();
    void clickedMainView(float scene_x, float scene_y);
    void addWidget(QWidget *widget);
    void addTextEffect(const string &text, Vector2D pos, int duration_ms);
    void addTextEffect(const string &text, Vector2D pos, int duration_ms, int r, int g, int b);
    void playSound(const string &sound_effect);
    void showInfoWindow(const string &html);

    Character *getPlayer() {
        return this->player;
    }
    const Character *getPlayer() const {
        return this->player;
    }
    /*Location *getLocation() {
        return this->location;
    }
    const Location *getLocation() const {
        return this->location;
    }*/
    const Quest *getQuest() const {
        return this->quest;
    }
    const QuestInfo &getCQuestInfo() const {
        return this->quest_list.at(this->c_quest_indx);
    }
    bool isLastQuest() const {
        return this->c_quest_indx == this->quest_list.size()-1;
    }
    void advanceQuest();
    Location *getCLocation() const {
        return this->c_location;
    }

    void addStandardItem(Item *item);
    Item *cloneStandardItem(const string &name) const;
    Currency *cloneGoldItem(int value) const;
    const Spell *findSpell(const string &name) const;
    Character *createCharacter(const string &template_name) const;

    vector<Shop *>::const_iterator shopsBegin() const {
        return shops.begin();
    }
    vector<Shop *>::const_iterator shopsEnd() const {
        return shops.end();
    }

    QPixmap &getItemImage(const string &name);

    void showInfoDialog(const string &message);
    void showInfoDialog(const string &message, const string &picture);
    bool askQuestionDialog(const string &message);

public slots:
    void closeSubWindow();
    void closeAllSubWindows();
};
