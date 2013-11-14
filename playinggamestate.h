#pragma once

#include "common.h"

#include <QtGui>
#include <QtWebKit/QWebView>

#include <sstream>
using std::stringstream;

#include <map>
using std::map;

#include "common.h"
#include "gamestate.h"
#include "rpg/character.h"
#include "rpg/location.h"

class MainGraphicsView;
class LazyAnimationLayer;
class AnimationLayer;
class Shop;
class Currency;
class Spell;
class ScrollingListWidget;

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

Direction directionFromVecDir(Vector2D dir);

class CharacterAction {
    enum Type {
        CHARACTERACTION_RANGED_WEAPON = 0,
        CHARACTERACTION_SPELL = 1
    };
    Type type;

    Character *source;
    Vector2D source_pos;
    Character *target_npc;
    Vector2D dest_pos;
    int time_ms;
    int duration_ms;
    float offset_y;

    bool hits;
    bool weapon_no_effect_magical;
    bool weapon_no_effect_holy;
    int weapon_damage;

    const Spell *spell;

    QGraphicsItem *object;

    CharacterAction(Type type, Character *source, Character *target_npc, float offset_y);

public:
    ~CharacterAction();

    void implement(PlayingGamestate *playing_gamestate) const;
    void update();
    void notifyDead(const Character *character);
    bool isExpired() const;

    static CharacterAction *createSpellAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, const Spell *spell);
    static CharacterAction *createProjectileAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, bool hits, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage, const string &projectile_key, float icon_width);
};

class UncloseWidget : public QWidget {
protected:
    void closeEvent(QCloseEvent *event) {
        // prevent unexpected behaviour from Alt+F4, Escape, Android back button, etc - we handle closing via keypresses via key shortcuts
        event->ignore();
    }
public:
};

class CloseSubWindowWidget : public QWidget {
protected:
    PlayingGamestate *playing_gamestate;

    void closeEvent(QCloseEvent *event);
public:
    CloseSubWindowWidget(PlayingGamestate *playing_gamestate) : playing_gamestate(playing_gamestate) {
    }
};

class CloseAllSubWindowsWidget : public QWidget {
protected:
    PlayingGamestate *playing_gamestate;

    void closeEvent(QCloseEvent *event);
public:
    CloseAllSubWindowsWidget(PlayingGamestate *playing_gamestate) : playing_gamestate(playing_gamestate) {
    }
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
    int fps_frame_count;
    QElapsedTimer fps_timer;

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
    GUIOverlay *getGUIOverlay() {
        return this->gui_overlay;
    }
    void createLightingMap(unsigned char lighting_min);
    void updateInput();
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
    string progress_message;
    int  progress_percent;

    float fps;

    bool has_fade;
    bool fade_in;
    int fade_time_start_ms;

    void drawBar(QPainter &painter, float fx, float fy, float fwidth, float fheight, float fraction, QColor color);
public:
    GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view);
    virtual ~GUIOverlay() {
    }

    void setProgress(int progress_percent);
    void setProgress(int progress_percent, const string &progress_message) {
        this->display_progress = true;
        this->progress_percent = progress_percent;
        this->progress_message = progress_message;
        this->repaint();
    }
    void unsetProgress() {
        this->display_progress = false;
    }
    void setFPS(float fps) {
        this->fps = fps;
    }
    void setFadeIn();
    void setFadeOut();
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

class StatsWindow : public CloseSubWindowWidget {
    Q_OBJECT

    QString writeStat(const string &stat_key, bool is_float) const;

public:
    StatsWindow(PlayingGamestate *playing_gamestate);
    virtual ~StatsWindow() {
    }
};

class ItemsWindow : public CloseSubWindowWidget {
    Q_OBJECT

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
    void clickedWear();
    void clickedUseItem();
    void clickedDropItem();
    void clickedInfo();

public:
    ItemsWindow(PlayingGamestate *playing_gamestate);
    virtual ~ItemsWindow() {
    }
};

class TradeWindow : public CloseSubWindowWidget {
    Q_OBJECT

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

class ItemsPickerWindow : public CloseSubWindowWidget {
    Q_OBJECT

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

class LevelUpWindow : public UncloseWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;
    QPushButton *closeButton;
    map<string, QCheckBox *> check_boxes;
    vector<QCheckBox *> selected;

    QCheckBox *addProfileCheckBox(const string &key);

private slots:
    void clickedCheckBox(int state);
    void clickedLevelUp();

public:
    LevelUpWindow(PlayingGamestate *playing_gamestate);
    virtual ~LevelUpWindow() {
    }
};


class CampaignWindow : public UncloseWidget {
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

class SaveGameWindow : public CloseAllSubWindowsWidget {
    Q_OBJECT

    vector<QString> save_filenames;
    ScrollingListWidget *list;
    QLineEdit *edit;

private slots:
    void clickedSave();
    void clickedDelete();
    void clickedSaveNew();

public:
    SaveGameWindow(PlayingGamestate *playing_gamestate);
    virtual ~SaveGameWindow() {
    }

    void requestNewSaveGame();
};

class PlayingGamestate : public Gamestate, CharacterListener, LocationListener {
    Q_OBJECT

    friend class MainGraphicsView;

    static PlayingGamestate *playingGamestate; // singleton pointer, needed for static member functions

    QGraphicsScene *scene;
    MainGraphicsView *view;
    GUIOverlay *gui_overlay;

    bool view_transform_3d;
    bool view_walls_3d;

    //QStackedWidget *main_stacked_widget;
    vector<QWidget *> widget_stack;

    QPushButton *turboButton;
    QPushButton *quickSaveButton;
    QPushButton *zoomoutButton;
    QPushButton *zoominButton;
    QPushButton *centreButton;

    Difficulty difficulty;
    bool permadeath;
    bool permadeath_has_savefilename;
    QString permadeath_savefilename;

    Character *player;
    stringstream journal_ss;
    int time_hours;

    vector<QuestInfo> quest_list;
    size_t c_quest_indx;
    Location *c_location;
    Quest *quest;

    vector<CharacterAction *> character_actions;

    bool is_keyboard_moving;

    // character items in the view
    set<QGraphicsItem *> graphicsitems_characters;

    // data
    map<string, LazyAnimationLayer *> animation_layers;
    map<string, LazyAnimationLayer *> scenery_animation_layers;
    map<string, AnimationLayer *> projectile_animation_layers;
    map<string, Item *> standard_items;
    map<string, QPixmap> item_images;
    map<string, QPixmap> builtin_images;
    map<string, QPixmap> portrait_images;
    map<string, CharacterTemplate *> character_templates;
    map<string, Spell *> spells;
    vector<Shop *> shops;

    QPixmap smoke_pixmap;
    QPixmap fireball_pixmap;
    QPixmap target_pixmap;
    AnimationLayer *target_animation_layer;
    QGraphicsItem *target_item;

    int time_last_complex_update_ms; // see update() for details

    bool cheat_mode;

    bool need_visibility_update;

    void processLocations(int progress_lo, int progress_hi);
    void updateVisibilityForFloorRegion(FloorRegion *floor_region);
    void updateVisibility(Vector2D pos);
    void moveToLocation(Location *location, Vector2D pos);
    void setupView();
    void autoSave() {
        if( !this->permadeath ) {
            this->saveGame("autosave.xml", false);
        }
    }
    void displayPausedMessage();
    void requestPlayerMove(Vector2D dest, const void *ignore);
    void clickedOnNPC(Character *character);
    bool handleClickForItems(Vector2D dest);
    bool clickedOnScenerys(bool *move, void **ignore, const vector<Scenery *> &clicked_scenerys);
    bool handleClickForScenerys(bool *move, void **ignore, Vector2D dest, bool is_click);
    void testFogOfWar();
    bool canSaveHere();
    int getRestTime() const;

    void saveItemProfileBonusInt(QTextStream &stream, const Item *item, const string &key) const;
    void saveItemProfileBonusFloat(QTextStream &stream, const Item *item, const string &key) const;
    void saveItem(QTextStream &stream, const Item *item) const;
    void saveItem(QTextStream &stream, const Item *item, const Character *character) const;
    void saveTrap(QTextStream &stream, const Trap *trap) const;

    void parseXMLItemProfileAttributeInt(Item *item, const QXmlStreamReader &reader, const string &key) const;
    void parseXMLItemProfileAttributeFloat(Item *item, const QXmlStreamReader &reader, const string &key) const;
    Item *parseXMLItem(QXmlStreamReader &reader) const;
    Character *loadNPC(bool *is_player, Vector2D *pos, QXmlStreamReader &reader) const;
    Item *loadItem(Vector2D *pos, QXmlStreamReader &reader, Scenery *scenery, Character *npc) const;

    void cleanup();

#ifdef _DEBUG
    vector<QGraphicsItem *> debug_items;

    void refreshDebugItems();
#endif

private slots:
    void clickedStats();
    void clickedItems();
    void clickedJournal();
    void clickedPause();
    void clickedOptions();
    void clickedRest();
    void clickedSave();
    void clickedQuit();
    void playBackgroundMusic();
    void turboToggled(bool checked);
    void quickSave();

public:
    PlayingGamestate(bool is_savegame, const string &player_type, const string &player_name, bool permadeath, bool cheat_mode, int cheat_start_level);
    virtual ~PlayingGamestate();

    float getDifficultyModifier() const;
    Difficulty getDifficulty() const {
        return this->difficulty;
    }
    void setDifficulty(Difficulty difficulty);
    /*void setPermadeath(bool permadeath) {
        this->permadeath = permadeath;
    }*/
    bool isPermadeath() const {
        return this->permadeath;
    }
    bool hasPermadeathSavefilename() const {
        return this->permadeath_has_savefilename;
    }
    QString getPermadeathSavefilename() const {
        return this->permadeath_savefilename;
    }
    void querySceneryImage(float *ret_size_w, float *ret_size_h, float *ret_visual_h, const string &image_name, bool has_size, float size, float size_w, float size_h, bool has_visual_h, float visual_h);
    void loadQuest(const QString &filename, bool is_savegame);
    void createRandomQuest();
    bool saveGame(const QString &filename, bool already_fullpath);

    virtual void quitGame();
    virtual void update();
    virtual void updateInput();
    virtual void render();
    virtual void activate(bool active, bool newly_paused);
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

    void actionCommand(bool pickup_only);
    void cycleTargetNPC();
    void clickedMainView(float scene_x, float scene_y);
    void addWidget(QWidget *widget, bool fullscreen_hint);
    void addTextEffect(const string &text, int duration_ms);
    void addTextEffect(const string &text, Vector2D pos, int duration_ms);
    void addTextEffect(const string &text, Vector2D pos, int duration_ms, int r, int g, int b);
    void playSound(const string &sound_effect);
    QWebView *showInfoWindow(const string &html);

    bool isTransform3D() const {
        return this->view_transform_3d;
    }
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
    Quest *getQuest() {
        return this->quest;
    }
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
    MainGraphicsView *getView() {
        return this->view;
    }
    bool isKeyboardMoving() const {
        return this->is_keyboard_moving;
    }

    void addCharacterAction(CharacterAction *character_action) {
        this->character_actions.push_back(character_action);
    }
    void addGraphicsItem(QGraphicsItem *object, float width, bool undo_3d);
    QGraphicsItem *addPixmapGraphic(const QPixmap &pixmap, Vector2D pos, float width, bool undo_3d, bool on_ground);
    QGraphicsItem *addSpellGraphic(Vector2D pos);

    void addStandardItem(Item *item);
    Item *cloneStandardItem(const string &name) const;
    const Item *getStandardItem(const string &name) const;
    Currency *cloneGoldItem(int value) const;
    const Spell *findSpell(const string &name) const;
    Character *createCharacter(const string &name, const string &template_name) const;

    vector<Shop *>::const_iterator shopsBegin() const {
        return shops.begin();
    }
    vector<Shop *>::const_iterator shopsEnd() const {
        return shops.end();
    }
    const Shop *getRandomShop(bool is_random_npc) const;

    AnimationLayer *getProjectileAnimationLayer(const string &name);
    QPixmap &getPortraitImage(const string &name);
    QPixmap &getItemImage(const string &name);
    QString getItemString(const Item *item, bool want_weight) const;

    void showInfoDialog(const string &message);
    void showInfoDialog(const string &message, const string &picture);
    bool askQuestionDialog(const string &message);

    void writeJournal(const string &text) {
        journal_ss << text;
    }
    string getJournalDate() {
        int days = time_hours/24;
        days++; // start count from 1, not 0
        stringstream ss;
        ss << "<b>Day " << days << ": </b>";
        return ss.str();
    }
    void writeJournalDate() {
        journal_ss << getJournalDate();
    }

    void hitEnemy(Character *source, Character *target, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage);

public slots:
    void closeSubWindow();
    void closeAllSubWindows();
};
