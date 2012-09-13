#pragma once

#include <vector>
using std::vector;
#include <set>
using std::set;
#include <string>
using std::string;

#include "utils.h"

class Character;
class Item;
class Scenery;
class Location;
class PlayingGamestate;
class Quest;

// source types for boundaries
const int SOURCETYPE_SCENERY = 1;

class LocationListener {
public:
    virtual void locationAddItem(const Location *location, Item *item)=0;
    virtual void locationRemoveItem(const Location *location, Item *item)=0;

    virtual void locationAddScenery(const Location *location, Scenery *scenery)=0;
    virtual void locationRemoveScenery(const Location *location, Scenery *scenery)=0;
    virtual void locationUpdateScenery(Scenery *scenery)=0;

    virtual void locationAddCharacter(const Location *location, Character *character)=0;
};

class Scenery {
public:
    enum DrawType {
        DRAWTYPE_NORMAL = 0,
        DRAWTYPE_FLOATING = 1,
        DRAWTYPE_BACKGROUND = 2
    };

protected:
    Location *location; // not saved
    string name;
    string image_name;
    Vector2D pos; // pos in Location (for centre)
    void *user_data_gfx; // not saved

    bool is_blocking;
    bool blocks_visibility;
    bool is_door, is_exit;
    string exit_location;
    Vector2D exit_location_pos;
    bool is_locked; // relevant only for some types, e.g., containers, doors
    bool locked_silent;
    string unlock_item_name;
    string unlock_text;
    string confirm_text; // relevant onl for some types, e.g., doors
    DrawType draw_type;
    float opacity;
    float width, height;

    // actions are events which happen periodically
    int action_last_time;
    int action_delay;
    string action_type;
    int action_value;

    string interact_type;
    int interact_state;

    bool can_be_opened;
    bool opened;
    set<Item *> items;

    string popup_text; // not saved at the moment

public:
    Scenery(const string &name, const string &image_name, float width, float height);
    virtual ~Scenery() {
    }

    void setLocation(Location *location) {
        this->location = location;
    }
    void setPos(float xpos, float ypos) {
        this->pos.set(xpos, ypos);
    }
    float getX() const {
        return this->pos.x;
    }
    float getY() const {
        return this->pos.y;
    }
    Vector2D getPos() const {
        return this->pos;
    }
    string getName() const {
        return this->name;
    }
    string getImageName() const {
        return this->image_name;
    }
    void setUserGfxData(void *user_data_gfx) {
        this->user_data_gfx = user_data_gfx;
    }
    void *getUserGfxData() {
        return this->user_data_gfx;
    }
    string getPopupText() const {
        return this->popup_text;
    }
    void setBlocking(bool is_blocking, bool blocks_visibility);
    bool isBlocking() const {
        return this->is_blocking;
    }
    bool blocksVisibility() const {
        return this->blocks_visibility;
    }
    void setDoor(bool is_door) {
        this->is_door = is_door;
    }
    bool isDoor() const {
        return this->is_door;
    }
    void setExit(bool is_exit) {
        this->is_exit = is_exit;
        if( is_exit ) {
            this->popup_text = "Click on the door to exit this dungeon";
        }
    }
    bool isExit() const {
        return this->is_exit;
    }
    void setExitLocation(const string &exit_location, Vector2D exit_location_pos) {
        this->exit_location = exit_location;
        this->exit_location_pos = exit_location_pos;
    }
    string getExitLocation() const {
        return this->exit_location;
    }
    Vector2D getExitLocationPos() const {
        return this->exit_location_pos;
    }

    void setLocked(bool is_locked) {
        this->is_locked = is_locked;
    }
    bool isLocked() const {
        return this->is_locked;
    }
    void setLockedSilent(bool locked_silent) {
        this->locked_silent = locked_silent;
    }
    bool isLockedSilent() const {
        return this->locked_silent;
    }
    void setUnlockItemName(const string &unlock_item_name) {
        this->unlock_item_name = unlock_item_name;
    }
    string getUnlockItemName() const {
        return this->unlock_item_name;
    }
    void setUnlockText(const string &unlock_text) {
        this->unlock_text = unlock_text;
    }
    string getUnlockText() const {
        return this->unlock_text;
    }
    void setConfirmText(const string &confirm_text) {
        this->confirm_text = confirm_text;
    }
    string getConfirmText() const {
        return this->confirm_text;
    }
    void setDrawType(DrawType draw_type) {
        this->draw_type = draw_type;
    }
    DrawType getDrawType() const {
        return this->draw_type;
    }
    void setOpacity(float opacity) {
        this->opacity = opacity;
    }
    float getOpacity() const {
        return this->opacity;
    }
    void setActionLastTime(int action_last_time) {
        this->action_last_time = action_last_time;
    }
    int getActionLastTime() const {
        return this->action_last_time;
    }
    void setActionDelay(int action_delay) {
        this->action_delay = action_delay;
    }
    int getActionDelay() const {
        return this->action_delay;
    }
    void setActionType(const string &action_type) {
        this->action_type = action_type;
    }
    string getActionType() const {
        return this->action_type;
    }
    void setActionValue(int action_value) {
        this->action_value = action_value;
    }
    int getActionValue() const {
        return this->action_value;
    }
    string getInteractType() const {
        return this->interact_type;
    }
    void setInteractType(const string &interact_type) {
        this->interact_type = interact_type;
    }
    int getInteractState() const {
        return this->interact_state;
    }
    void setInteractState(int interact_state) {
        this->interact_state = interact_state;
    }

    float getWidth() const {
        return this->width;
    }
    float getHeight() const {
        return this->height;
    }

    void addItem(Item *item);
    void removeItem(Item *item);
    void eraseAllItems() {
        // n.b., don't delete
        this->items.clear();
    }
    set<Item *>::iterator itemsBegin() {
        return this->items.begin();
    }
    set<Item *>::const_iterator itemsBegin() const {
        return this->items.begin();
    }
    set<Item *>::iterator itemsEnd() {
        return this->items.end();
    }
    set<Item *>::const_iterator itemsEnd() const {
        return this->items.end();
    }
    int getNItems() const {
        return this->items.size();
    }
    void setCanBeOpened(bool can_be_opened) {
        this->can_be_opened = can_be_opened;
    }
    bool canBeOpened() const {
        return this->can_be_opened;
    }
    void setOpened(bool opened);
    bool isOpened() const {
        return this->opened;
    }
    bool isOn(const Character *character) const;
    void getInteractionText(string *dialog_text) const;
    void interact(PlayingGamestate *playing_gamestate);
};

class Trap {
protected:
    //Location *location;
    string type;
    Vector2D pos; // pos in Location (for top-left)
    float width, height;

public:
    Trap(const string &type, float width, float height);
    virtual ~Trap() {
    }

    /*void setLocation(Location *location) {
        this->location = location;
    }*/
    const string &getType() const {
        return this->type;
    }
    void setPos(float xpos, float ypos) {
        this->pos.set(xpos, ypos);
    }
    float getX() const {
        return this->pos.x;
    }
    float getY() const {
        return this->pos.y;
    }
    Vector2D getPos() const {
        return this->pos;
    }
    float getWidth() const {
        return this->width;
    }
    float getHeight() const {
        return this->height;
    }

    bool isSetOff(const Character *character) const;
    bool setOff(PlayingGamestate *playing_gamestate, Character *character) const;
};

class FloorRegion : public Polygon2D {
public:
    enum EdgeType {
        EDGETYPE_INTERNAL = 0,
        EDGETYPE_EXTERNAL = 1
    };
protected:
    bool is_visible;
    vector<EdgeType> edge_types; // should match points array of Polygon2D; gives the type of the edge following the i-th point
    vector<int> temp_marks; // should match points array
    void *user_data_gfx;

    set<Scenery *> scenerys; // stored here as well as the Location, for visibility testing
    set<Item *> items; // stored here as well as the Location, for visibility testing

public:
    FloorRegion() : Polygon2D(), is_visible(false) {
    }

    virtual void addPoint(Vector2D point) {
        Polygon2D::addPoint(point);
        this->edge_types.push_back(EDGETYPE_EXTERNAL);
        this->temp_marks.push_back(0);
    }
    virtual void insertPoint(size_t indx, Vector2D point);
    void setEdgeType(size_t indx, EdgeType edge_type) {
        this->edge_types.at(indx) = edge_type;
    }
    EdgeType getEdgeType(size_t indx) const {
        return this->edge_types.at(indx);
    }
    void setTempMark(size_t indx, int value) {
        this->temp_marks.at(indx) = value;
    }
    int getTempMark(size_t indx) const {
        return this->temp_marks.at(indx);
    }
    void setVisible(bool is_visible) {
        this->is_visible = is_visible;
    }
    bool isVisible() const {
        return this->is_visible;
    }
    void setUserGfxData(void *user_data_gfx) {
        this->user_data_gfx = user_data_gfx;
    }
    void *getUserGfxData() {
        return this->user_data_gfx;
    }

    set<Scenery *>::iterator scenerysBegin() {
        return this->scenerys.begin();
    }
    set<Scenery *>::const_iterator scenerysBegin() const {
        return this->scenerys.begin();
    }
    set<Scenery *>::iterator scenerysEnd() {
        return this->scenerys.end();
    }
    set<Scenery *>::const_iterator scenerysEnd() const {
        return this->scenerys.end();
    }
    void addScenery(Scenery *scenery) {
        this->scenerys.insert(scenery);
    }
    void removeScenery(Scenery *scenery) {
        this->scenerys.erase(scenery);
    }

    set<Item *>::iterator itemsBegin() {
        return this->items.begin();
    }
    set<Item *>::const_iterator itemsBegin() const {
        return this->items.begin();
    }
    set<Item *>::iterator itemsEnd() {
        return this->items.end();
    }
    set<Item *>::const_iterator itemsEnd() const {
        return this->items.end();
    }
    void addItem(Item *item) {
        this->items.insert(item);
    }
    void removeItem(Item *item) {
        this->items.erase(item);
    }

    static FloorRegion *createRectangle(float x, float y, float w, float h);
};

class Location {
public:
    enum IntersectType {
        INTERSECTTYPE_MOVE = 0, // include scenery that is blocking
        INTERSECTTYPE_VISIBILITY = 1 // include scenery that blocks visibility
    };

protected:
    string name;
    LocationListener *listener;
    void *listener_data;

    Graph *distance_graph;

    string wall_image_name;
    string floor_image_name;
    string background_image_name;

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries;

    set<Character *> characters;
    set<Item *> items;
    set<Scenery *> scenerys;
    set<Trap *> traps;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const;
    void intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const Scenery *ignore_one_scenery) const;

    vector<Vector2D> calculatePathWayPoints() const;

    bool testVisibility(Vector2D pos, const FloorRegion *floor_region, size_t j) const;
public:
    Location(const string &name);
    ~Location();

    string getName() const {
        return this->name;
    }
    void setWallImageName(const string &wall_image_name) {
        this->wall_image_name = wall_image_name;
    }
    string getWallImageName() const {
        return this->wall_image_name;
    }
    void setFloorImageName(const string &floor_image_name) {
        this->floor_image_name = floor_image_name;
    }
    string getFloorImageName() const {
        return this->floor_image_name;
    }
    void setBackgroundImageName(const string &background_image_name) {
        this->background_image_name = background_image_name;
    }
    string getBackgroundImageName() const {
        return this->background_image_name;
    }
    void addFloorRegion(FloorRegion *floorRegion);
    const FloorRegion *getFloorRegion(size_t i) const {
        return this->floor_regions.at(i);
    }
    FloorRegion *getFloorRegion(size_t i) {
        return this->floor_regions.at(i);
    }
    size_t getNFloorRegions() const {
        return this->floor_regions.size();
    }
    void calculateSize(float *w, float *h) const;
    FloorRegion *findFloorRegionAt(Vector2D pos);

    void addBoundary(Polygon2D boundary) {
        this->boundaries.push_back(boundary);
    }
    const Polygon2D *getBoundary(size_t i) const {
        return &this->boundaries.at(i);
    }
    size_t getNBoundaries() const {
        return this->boundaries.size();
    }

    void setListener(LocationListener *listener, void *listener_data) {
        this->listener = listener;
        this->listener_data = listener_data;
    }

    void addCharacter(Character *character, float xpos, float ypos);
    void removeCharacter(Character *character);
    set<Character *>::iterator charactersBegin() {
        return this->characters.begin();
    }
    set<Character *>::const_iterator charactersBegin() const {
        return this->characters.begin();
    }
    set<Character *>::iterator charactersEnd() {
        return this->characters.end();
    }
    set<Character *>::const_iterator charactersEnd() const {
        return this->characters.end();
    }
    bool hasEnemies(const PlayingGamestate *playing_gamstate) const;

    void addItem(Item *item, float xpos, float ypos);
    void removeItem(Item *item);
    set<Item *>::iterator itemsBegin() {
        return this->items.begin();
    }
    set<Item *>::const_iterator itemsBegin() const {
        return this->items.begin();
    }
    set<Item *>::iterator itemsEnd() {
        return this->items.end();
    }
    set<Item *>::const_iterator itemsEnd() const {
        return this->items.end();
    }

    void addScenery(Scenery *scenery, float xpos, float ypos);
    void removeScenery(Scenery *scenery);
    void updateScenery(Scenery *scenery);
    set<Scenery *>::iterator scenerysBegin() {
        return this->scenerys.begin();
    }
    set<Scenery *>::const_iterator scenerysBegin() const {
        return this->scenerys.begin();
    }
    set<Scenery *>::iterator scenerysEnd() {
        return this->scenerys.end();
    }
    set<Scenery *>::const_iterator scenerysEnd() const {
        return this->scenerys.end();
    }

    void addTrap(Trap *trap, float xpos, float ypos);
    void removeTrap(Trap *trap);
    set<Trap *>::iterator trapsBegin() {
        return this->traps.begin();
    }
    set<Trap *>::const_iterator trapsBegin() const {
        return this->traps.begin();
    }
    set<Trap *>::iterator trapsEnd() {
        return this->traps.end();
    }
    set<Trap *>::const_iterator trapsEnd() const {
        return this->traps.end();
    }

    void createBoundariesForRegions();
    void createBoundariesForScenery();

    bool collideWithTransient(const Character *character, Vector2D pos) const;
    bool intersectSweptSquareWithBoundaries(Vector2D *hit_pos, bool find_earliest, Vector2D start, Vector2D end, float width, IntersectType intersect_type, const Scenery *ignore_one_scenery) const;
    //bool intersectSweptSquareWithBoundariesAndNPCs(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width) const;
    Vector2D nudgeToFreeSpace(Vector2D pos, float width) const;

    void calculateDistanceGraph();
    const Graph *getDistanceGraph() const {
        return this->distance_graph;
    }

    void initVisibility(Vector2D pos);
    vector<FloorRegion *> updateVisibility(Vector2D pos);
};

class QuestObjective {
    string type;
    string arg1;
public:
    QuestObjective(const string &type, const string &arg1);

    bool testIfComplete(const Quest *quest) const;
    const string &getType() const {
        return type;
    }
    const string &getArg1() const {
        return arg1;
    }
};

class Quest {
    QuestObjective *quest_objective;
    vector<Location *> locations;
    string info;
    bool is_completed;
public:
    Quest();
    ~Quest();

    void addLocation(Location *location);
    Location *findLocation(const string &name);
    void setQuestObjective(QuestObjective *quest_objective) {
        this->quest_objective = quest_objective;
    }
    const QuestObjective *getQuestObjective() const {
        return this->quest_objective;
    }
    void setInfo(const string &info) {
        this->info = info;
    }
    string getInfo() const {
        return this->info;
    }
    vector<Location *>::iterator locationsBegin() {
        return this->locations.begin();
    }
    const vector<Location *>::const_iterator locationsBegin() const {
        return this->locations.begin();
    }
    const vector<Location *>::iterator locationsEnd() {
        return this->locations.end();
    }
    const vector<Location *>::const_iterator locationsEnd() const {
        return this->locations.end();
    }

    bool isCompleted() const {
        return this->is_completed;
    }
    bool testIfComplete();
    void setCompleted(bool is_completed) {
        this->is_completed = is_completed;
    }
};

class QuestInfo {
    string filename;
public:
    QuestInfo(const string &filename);

    string getFilename() const {
        return filename;
    }
};
