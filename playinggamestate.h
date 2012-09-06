#pragma once

#include <QtGui>

#include <sstream>
using std::stringstream;

#include <map>
using std::map;

// Phonon not supported on Qt Android
#ifdef Q_OS_ANDROID
//#include <SLES/OpenSLES.h>
#else
#include <phonon/MediaObject>
#include <phonon/AudioOutput>
#endif

#include "common.h"
#include "gamestate.h"
#include "rpg/character.h"
#include "rpg/location.h"

class MainGraphicsView;
class Sound;
class AnimationLayer;
class Shop;
class Currency;

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

    PlayingGamestate *playing_gamestate;
    int mouse_down_x, mouse_down_y;
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

    const static float min_zoom_c;
    const static float max_zoom_c;

    void zoom(QPointF zoom_centre, bool in);

    virtual bool event(QEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
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
    //void setWeightLabel(int weight);
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

    QListWidget *list;
    const vector<const Item *> &items;
    const vector<int> &costs;

    QListWidget *player_list;
    vector<Item *> player_items;
    vector<int> player_costs;

    void addPlayerItem(Item *item, int buy_cost);
    void updateGoldLabel();

private slots:
    void clickedBuy();
    void clickedSell();
    void clickedInfo();

public:
    TradeWindow(PlayingGamestate *playing_gamestate, const vector<const Item *> &items, const vector<int> &costs);
    virtual ~TradeWindow() {
    }
};

class CampaignWindow : public QWidget {
    Q_OBJECT

    PlayingGamestate *playing_gamestate;

private slots:
    void clickedClose();
    void clickedShop();
    void clickedTraining();

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
    map<string, QPixmap> scenery_opened_images;
    map<string, QPixmap> builtin_images;
    map<string, QPixmap> npc_static_images;
    map<string, CharacterTemplate *> character_templates;
    vector<Shop *> shops;
#ifndef Q_OS_ANDROID
    map<string, Sound *> sound_effects;
#endif

    Item *parseXMLItem(QXmlStreamReader &reader);
    void updateVisibilityForFloorRegion(FloorRegion *floor_region);
    void updateVisibility(Vector2D pos);
    void moveToLocation(Location *location, Vector2D pos);
    void setupView();

    void saveItem(FILE *file, const Item *item) const;
    void saveItem(FILE *file, const Item *item, const Character *character) const;

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
    PlayingGamestate(bool is_savegame);
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
        if( !active ) {
            this->saveGame("autosave.xml");
        }
    }
    //virtual void mouseClick(int m_x, int m_y);

    virtual void characterUpdateGraphics(const Character *character, void *user_data);
    virtual void characterTurn(const Character *character, void *user_data, Vector2D dir);
    virtual void characterMoved(Character *character, void *user_data);
    virtual void characterSetAnimation(const Character *character, void *user_data, const string &name, bool force_restart);
    virtual void characterDeath(Character *character, void *user_data);

    virtual void locationAddItem(const Location *location, Item *item);
    virtual void locationRemoveItem(const Location *location, Item *item);

    virtual void locationAddScenery(const Location *location, Scenery *scenery);
    virtual void locationRemoveScenery(const Location *location, Scenery *scenery);
    virtual void locationUpdateScenery(Scenery *scenery);

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

    void addStandardItem(Item *item);
    Item *cloneStandardItem(const string &name) const;
    Currency *cloneGoldItem(int value) const;

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
