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
class Trap;
class Location;
class PlayingGamestate;
class Quest;

// source types for boundaries
const int SOURCETYPE_FLOOR = 0;
const int SOURCETYPE_SCENERY = 1;

class LocationListener {
public:
    virtual void locationAddItem(const Location *location, Item *item, bool visible)=0;
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
    string big_image_name; // used for description
    bool is_animation; // not saved
    Vector2D pos; // pos in Location (for centre)
    void *user_data_gfx; // not saved

    bool is_blocking;
    bool blocks_visibility;
    bool is_door, is_exit;
    string exit_location;
    Vector2D exit_location_pos;
    bool is_locked; // relevant only for some types, e.g., containers, doors
    bool locked_silent; // whether sample is played when trying to unlock
    string locked_text;
    bool locked_used_up;
    bool key_always_needed;
    string unlock_item_name;
    string unlock_text;
    int unlock_xp;
    string confirm_text; // relevant only for some types, e.g., doors
    DrawType draw_type;
    float opacity;
    bool has_smoke;
    float width, height;
    float visual_height; // not saved

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
    Trap *trap;

    string popup_text; // not saved at the moment (only set internally by the engine)
    string description;

public:
    Scenery(const string &name, const string &image_name, bool is_animation, float width, float height, float visual_height);
    virtual ~Scenery();

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
    bool isAnimation() const {
        return this->is_animation;
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
    void setDescription(const string &description) {
        this->description = description;
    }
    string getDescription() const {
        return this->description;
    }
    void setBigImageName(const string &big_image_name) {
        this->big_image_name = big_image_name;
    }
    string getBigImageName() const {
        return this->big_image_name;
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
    void setLockedText(const string &locked_text) {
        this->locked_text = locked_text;
    }
    string getLockedText() const {
        return this->locked_text;
    }
    void setLockedUsedUp(bool locked_used_up) {
        this->locked_used_up = locked_used_up;
    }
    bool isLockedUsedUp() const {
        return this->locked_used_up;
    }
    void setKeyAlwaysNeeded(bool key_always_needed) {
        this->key_always_needed = key_always_needed;
    }
    bool isKeyAlwaysNeeded() const {
        return this->key_always_needed;
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
    void setUnlockXP(int unlock_xp) {
        this->unlock_xp = unlock_xp;
    }
    int getUnlockXP() const {
        return this->unlock_xp;
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
    void setHasSmoke(bool has_smoke) {
        this->has_smoke = has_smoke;
    }
    bool hasSmoke() const {
        return this->has_smoke;
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
    float getVisualHeight() const {
        return this->visual_height;
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
    vector<string> getInteractionText(string *dialog_text) const;
    void interact(PlayingGamestate *playing_gamestate, int option);
    void setTrap(Trap *trap);
    Trap *getTrap() const {
        return this->trap;
    }
};

class Trap {
protected:
    //Location *location;
    string type;
    bool has_position;
    Vector2D pos; // pos in Location (for top-left)
    float width, height;
    int rating;
    int difficulty;

public:
    Trap(const string &type);
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
    void setRating(int rating) {
        this->rating = rating;
    }
    int getRating() const {
        return this->rating;
    }
    void setDifficulty(int diffculty) {
        this->difficulty = difficulty;
    }
    int getDifficulty() const {
        return this->difficulty;
    }

    bool isSetOff(const Character *character) const;
    void setOff(PlayingGamestate *playing_gamestate, Character *character) const;
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

    set<Scenery *> scenerys; // stored here as well as the Location, for visibility testing (note that scenerys may be in more than one FloorRegion)
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
    static FloorRegion *createRectangle(const Rect2D &rect);
};

class Location {
public:
    enum IntersectType {
        INTERSECTTYPE_MOVE = 0, // include scenery that is blocking
        INTERSECTTYPE_VISIBILITY = 1 // include scenery that blocks visibility
    };
    enum Type {
        TYPE_INDOORS = 0,
        TYPE_OUTDOORS = 1
    };

protected:
    string name;
    Type type;
    LocationListener *listener;
    void *listener_data;

    Graph *distance_graph;

    string wall_image_name;
    float wall_x_scale;
    string drop_wall_image_name;
    string floor_image_name;
    string background_image_name;

    unsigned char lighting_min;

    string wandering_monster_template;
    int wandering_monster_time_ms; // Poisson distribution - if non-zero, mean of one monster produced every wandering_monster_time_ms time interval
    int wandering_monster_rest_chance; // percentage change of having a wandering monster disturb the player when resting

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries;

    set<Character *> characters;
    set<Item *> items;
    set<Scenery *> scenerys;
    set<Trap *> traps;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const;
    void intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const Scenery *ignore_one_scenery, bool flying) const;

    struct PathWayPoint {
        Vector2D origin_point;
        Vector2D point;
        void *source;
        bool active;

        PathWayPoint(Vector2D origin_point, Vector2D point, void *source) : origin_point(origin_point), point(point), source(source), active(false) {
        }
    };
    vector<PathWayPoint> path_way_points;
    void calculatePathWayPoints();
    void testActivatePathWayPoint(PathWayPoint *path_way_point) const;

    bool testVisibility(Vector2D pos, const FloorRegion *floor_region, size_t j) const;
    bool testGraphVerticesHit(float *dist, GraphVertex *v_A, GraphVertex *v_B) const;
public:
    Location(const string &name);
    ~Location();

    string getName() const {
        return this->name;
    }
    void setType(Type type) {
        this->type = type;
    }
    Type getType() const {
        return this->type;
    }
    void setWallImageName(const string &wall_image_name) {
        this->wall_image_name = wall_image_name;
    }
    string getWallImageName() const {
        return this->wall_image_name;
    }
    void setWallXScale(float wall_x_scale) {
        this->wall_x_scale = wall_x_scale;
    }
    float getWallXScale() const {
        return this->wall_x_scale;
    }
    void setDropWallImageName(const string &drop_wall_image_name) {
        this->drop_wall_image_name = drop_wall_image_name;
    }
    string getDropWallImageName() const {
        return this->drop_wall_image_name;
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
    void setLightingMin(unsigned char lighting_min) {
        this->lighting_min = lighting_min;
    }
    unsigned char getLightingMin() const {
        return this->lighting_min;
    }
    void setWanderingMonster(const string &wandering_monster_template, int wandering_monster_time_ms, int wandering_monster_rest_chance) {
        this->wandering_monster_template = wandering_monster_template;
        this->wandering_monster_time_ms = wandering_monster_time_ms;
        this->wandering_monster_rest_chance = wandering_monster_rest_chance;
    }
    string getWanderingMonsterTemplate() const {
        return this->wandering_monster_template;
    }
    int getWanderingMonsterTimeMS() const {
        return this->wandering_monster_time_ms;
    }
    int getWanderingMonsterRestChance() const {
        return this->wandering_monster_rest_chance;
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
    vector<FloorRegion *> findFloorRegionsAt(Vector2D pos);
    vector<FloorRegion *> findFloorRegionsAt(Vector2D pos, float width, float height);
    vector<FloorRegion *> findFloorRegionsAt(const Scenery *scenery);

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
    void addSceneryToFloorRegions();

    bool intersectSweptSquareWithBoundaries(Vector2D *hit_pos, bool find_earliest, Vector2D start, Vector2D end, float width, IntersectType intersect_type, const Scenery *ignore_one_scenery, bool flying) const;
    Vector2D nudgeToFreeSpace(Vector2D src, Vector2D pos, float width) const;
    bool findFreeWayPoint(Vector2D *result, Vector2D from, bool visible, bool can_fly) const;
    bool collideWithTransient(const Character *character, Vector2D pos) const;
    bool visibilityTest(Vector2D src, Vector2D dest) const;

    void calculateDistanceGraph();
    const Graph *getDistanceGraph() const {
        return this->distance_graph;
    }
    vector<Vector2D> calculatePathTo(Vector2D src, Vector2D dest, const Scenery *ignore_scenery, bool can_fly) const;

#if 0
    void initVisibility(Vector2D pos);
#endif
    vector<FloorRegion *> updateVisibility(Vector2D pos);
};

class QuestObjective {
    string type;
    string arg1;
    int gold;
public:
    QuestObjective(const string &type, const string &arg1, int gold);

    bool testIfComplete(const PlayingGamestate *playing_gamestate, const Quest *quest) const;
    void completeQuest(PlayingGamestate *playing_gamestate) const;
    const string &getType() const {
        return type;
    }
    const string &getArg1() const {
        return arg1;
    }
    int getGold() const {
        return gold;
    }
};

class Quest {
    QuestObjective *quest_objective;
    vector<Location *> locations;
    string name;
    string info;
    string completed_text;
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
    void setName(const string &name) {
        this->name = name;
    }
    string getName() const {
        return this->name;
    }
    void setInfo(const string &info) {
        this->info = info;
    }
    string getInfo() const {
        return this->info;
    }
    void setCompletedText(const string &completed_text) {
        this->completed_text = completed_text;
    }
    string getCompletedText() const {
        return this->completed_text;
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
    int getNLocations() const {
        return this->locations.size();
    }

    bool isCompleted() const {
        return this->is_completed;
    }
    bool testIfComplete(const PlayingGamestate *playing_gamestate);
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

enum Direction4 {
    DIRECTION4_NORTH = 0,
    DIRECTION4_EAST = 1,
    DIRECTION4_SOUTH = 2,
    DIRECTION4_WEST = 3
};

class Seed {
public:
    Vector2D pos;
    Direction4 dir;

    Seed(Vector2D pos, Direction4 dir) : pos(pos), dir(dir) {
    }
};

class LocationGenerator {
    static bool collidesWithFloorRegions(vector<Rect2D> *floor_regions_rects, Rect2D rect, float gap);
    static void exploreFromSeed(Location *location, Seed seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first);

public:
    static Location *generateLocation(Vector2D *player_start);
};
