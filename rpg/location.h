#pragma once

#include <vector>
using std::vector;
#include <set>
using std::set;
#include <string>
using std::string;

#include "../common.h"

#include "utils.h"

class Character;
class Item;
class Scenery;
class Location;
class Quest;
class PlayingGamestate;

// source types for boundaries
const int SOURCETYPE_FLOOR = 0;
const int SOURCETYPE_SCENERY = 1;
const int SOURCETYPE_FIXED_NPC = 2;

class LocationListener {
public:
    virtual void locationAddItem(const Location *location, Item *item, bool visible)=0;
    virtual void locationRemoveItem(const Location *location, Item *item)=0;

    virtual void locationAddScenery(const Location *location, Scenery *scenery)=0;
    virtual void locationRemoveScenery(const Location *location, Scenery *scenery)=0;
    virtual void locationUpdateScenery(Scenery *scenery)=0;

    virtual void locationAddCharacter(const Location *location, Character *character)=0;
};

class Trap {
protected:
    string type;
    bool has_position;
    Vector2D pos; // pos in Location (for top-left)
    float width, height;
    int rating;
    int difficulty;

public:
    Trap(const string &type);
    Trap(const string &type, float width, float height);

    const string &getType() const {
        return this->type;
    }
    void setPos(float xpos, float ypos) {
        this->has_position = true;
        this->pos.set(xpos, ypos);
    }
    void setSize(float width, float height) {
        this->has_position = true;
        this->width = width;
        this->height = height;
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
    void setDifficulty(int difficulty) {
        this->difficulty = difficulty;
    }
    int getDifficulty() const {
        return this->difficulty;
    }

    bool isSetOff(const Character *character) const;
    void setOff(PlayingGamestate *playing_gamestate, Character *character) const;
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
    Vector2D pos; // pos in Location (for centre)
    vector<void *> user_data_gfx;

    string requires_flag; // used for various purposes - if door can be opened, if exit[_location] can be exited, if scenery can be interacted with
    bool is_blocking;
    bool blocks_visibility;
    bool is_door, is_exit;
    string exit_location;
    Vector2D exit_location_pos;
    int exit_travel_time;
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
    Vector2D smoke_pos;
    float width, height;
    float visual_height; // not saved
    bool boundary_iso;
    float boundary_iso_ratio;
    Polygon2D boundary_base;
    Polygon2D boundary_visual;

    // actions are events which happen periodically
    int action_last_time; // not saved
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

    // rule of three
    Scenery& operator=(const Scenery &) {
        throw string("Scenery assignment operator disallowed");
    }
    /*Scenery(const Scenery &) {
        throw string("Scenery copy constructor disallowed");
    }*/
public:
    Scenery(const string &name, const string &image_name, float width, float height, float visual_height, bool boundary_iso, float boundary_iso_ratio);
    virtual ~Scenery();

    virtual Scenery *clone() const; // virtual copy constructor

    void setLocation(Location *location) {
        this->location = location;
    }
    void setPos(float xpos, float ypos);
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
    void clearUserGfxData() {
        this->user_data_gfx.clear();
    }
    void addUserGfxData(void *user_data_gfx) {
        this->user_data_gfx.push_back(user_data_gfx);
    }
    size_t getNUserGfxData() const {
        return this->user_data_gfx.size();
    }
    void *getUserGfxData(size_t i) const {
        return this->user_data_gfx.at(i);
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

    void setRequiresFlag(const string &requires_flag) {
        this->requires_flag = requires_flag;
    }
    string getRequiresFlag() const {
        return this->requires_flag;
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
    void setExitLocation(const string &exit_location, Vector2D exit_location_pos, int exit_travel_time) {
        this->exit_location = exit_location;
        this->exit_location_pos = exit_location_pos;
        this->exit_travel_time = exit_travel_time;
    }
    string getExitLocation() const {
        return this->exit_location;
    }
    Vector2D getExitLocationPos() const {
        return this->exit_location_pos;
    }
    int getExitTravelTime() const {
        return this->exit_travel_time;
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
    void setHasSmoke(bool has_smoke, Vector2D smoke_pos) {
        this->has_smoke = has_smoke;
        this->smoke_pos = smoke_pos;
    }
    bool hasSmoke() const {
        return this->has_smoke;
    }
    Vector2D getSmokePos() const {
        return this->smoke_pos;
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
    Polygon2D getBoundary(bool include_visual) const {
        return include_visual ? this->boundary_visual : this->boundary_base;
    }
    //void getBox(Vector2D *box_centre, float *box_width, float *box_height, bool include_visual) const;
    float distFromPoint(Vector2D point, bool include_visual) const;

    /*void setBoundaryIso(float boundary_iso_ratio) {
        this->boundary_iso = true;
        this->boundary_iso_ratio = boundary_iso_ratio;
    }*/
    bool isBoundaryIso() const {
        return this->boundary_iso;
    }
    float getBoundaryIsoRatio() const {
        return this->boundary_iso_ratio;
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
    vector<string> getInteractionText(PlayingGamestate *playing_gamestate, string *dialog_text) const;
    void interact(PlayingGamestate *playing_gamestate, int option);
    void setTrap(Trap *trap);
    Trap *getTrap() const {
        return this->trap;
    }
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

    string floor_image_name; // if set, overrides the Location setting

public:
    FloorRegion() : Polygon2D(), is_visible(false) {
    }
    virtual ~FloorRegion() {
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
    void removeItem(Item *item);
    void setFloorImageName(const string &floor_image_name) {
        this->floor_image_name = floor_image_name;
    }
    string getFloorImageName() const {
        return this->floor_image_name;
    }

    static FloorRegion *createRectangle(float x, float y, float w, float h);
    static FloorRegion *createRectangle(const Rect2D &rect);
};

class Tilemap {
    Vector2D pos; // pos in Location (for top-left)
    //float width, height;
    string imagemap;
    int tile_width;
    int tile_height;
    vector<string> rows;
    int max_length;

public:
    Tilemap(float x, float y, const string &imagemap, int tile_width, int tile_height, vector<string> rows);

    const string &getImagemap() const {
        return this->imagemap;
    }
    int getTileWidth() const {
        return this->tile_width;
    }
    int getTileHeight() const {
        return this->tile_height;
    }
    float getX() const {
        return pos.x;
    }
    float getY() const {
        return pos.y;
    }
    int getWidthi() const {
        return max_length;
    }
    int getHeighti() const {
        return (int)rows.size();
    }
    char getTileAt(int x, int y) const;
};

class Location {
public:
    enum IntersectType {
        INTERSECTTYPE_MOVE = 0, // include scenery that is blocking
        INTERSECTTYPE_VISIBILITY = 1 // include scenery that blocks visibility
    };
    enum Type {
        // note, this represents how walls are drawn, so in practice non-dungeon outdoor locations often still have TYPE_INDOORS
        TYPE_INDOORS = 0,
        TYPE_OUTDOORS = 1
    };
    enum GeoType {
        GEOTYPE_DUNGEON = 0,
        GEOTYPE_OUTDOORS = 1
    };

protected:
    string name;
    bool display_name; // whether to display the name of the location?
    Type type;
    GeoType geo_type;
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

    string rest_summon_location; // summons all NPCs from this location when resting

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries;

    vector<Tilemap *> tilemaps;

    set<Character *> characters;
    set<Item *> items;
    set<Scenery *> scenerys;
    set<Trap *> traps;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const;
    void intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const void *ignore_one, bool flying) const;

    struct PathWayPoint {
        Vector2D origin_point;
        Vector2D point;
        void *source;
        bool active;
        bool used_for_pathfinding; // if false, we don't actually need the path way point for pathfinding, but still use it for things like flee-points and wandering monster spawn points

        PathWayPoint(Vector2D origin_point, Vector2D point, void *source) : origin_point(origin_point), point(point), source(source), active(false), used_for_pathfinding(true) {
        }
    };
    vector<PathWayPoint> path_way_points;
    void calculatePathWayPoints();
    void testActivatePathWayPoint(PathWayPoint *path_way_point) const;

    bool testVisibility(Vector2D pos, const FloorRegion *floor_region, size_t j) const;
    bool testGraphVerticesHit(float *dist, GraphVertex *v_A, GraphVertex *v_B, const void *ignore, bool can_fly) const;

    void createBoundaryForRect(Vector2D pos, float width, float height, bool boundary_iso, float boundary_iso_ratio, void *source, int source_type);

    // rule of three
    Location& operator=(const Location &) {
        throw string("Location assignment operator disallowed");
    }
    Location(const Location &) {
        throw string("Location copy constructor disallowed");
    }
public:
    Location(const string &name);
    ~Location();

    string getName() const {
        return this->name;
    }
    void setDisplayName(bool display_name) {
        this->display_name = display_name;
    }
    bool isDisplayName() const {
        return this->display_name;
    }
    void setType(Type type) {
        this->type = type;
    }
    Type getType() const {
        return this->type;
    }
    void setGeoType(GeoType geo_type) {
        this->geo_type = geo_type;
    }
    GeoType getGeoType() const {
        return this->geo_type;
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
    int getBaseWanderingMonsterRestChance() const {
        return this->wandering_monster_rest_chance;
    }
    int getWanderingMonsterRestChance(const Character *player) const;
    void setRestSummonLocation(const string &rest_summon_location) {
        this->rest_summon_location = rest_summon_location;
    }
    string getRestSummonLocation() const {
        return this->rest_summon_location;
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
    FloorRegion *findFloorRegionInside(Vector2D pos, float width, float height) const;
    FloorRegion *findFloorRegionAt(Vector2D pos) const;
    vector<FloorRegion *> findFloorRegionsAt(Vector2D pos) const;
    vector<FloorRegion *> findFloorRegionsAt(Vector2D pos, float width, float height) const;
    vector<FloorRegion *> findFloorRegionsAt(const Scenery *scenery) const;

    void addBoundary(Polygon2D boundary) {
        this->boundaries.push_back(boundary);
    }
    const Polygon2D *getBoundary(size_t i) const {
        return &this->boundaries.at(i);
    }
    size_t getNBoundaries() const {
        return this->boundaries.size();
    }

    void addTilemap(Tilemap *tilemap) {
        this->tilemaps.push_back(tilemap);
    }
    const Tilemap *getTilemap(size_t i) const {
        return this->tilemaps.at(i);
    }
    size_t getNTilemaps() const {
        return this->tilemaps.size();
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
    size_t getNCharacters() const {
        return this->characters.size();
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
    void createBoundariesForFixedNPCs();
    void addSceneryToFloorRegions();

    bool intersectSweptSquareWithBoundaries(Vector2D *hit_pos, bool find_earliest, Vector2D start, Vector2D end, float width, IntersectType intersect_type, const void *ignore_one, bool flying) const;
    Vector2D nudgeToFreeSpace(Vector2D src, Vector2D pos, float width) const;
    bool findFleePoint(Vector2D *result, Vector2D from, Vector2D fleeing_from, bool can_fly) const;
    bool findFreeWayPoint(Vector2D *result, Vector2D from, bool visible, bool can_fly) const;
    bool collideWithTransient(const Character *character, Vector2D pos) const;
    bool visibilityTest(Vector2D src, Vector2D dest) const;

    void calculateDistanceGraph();
    const Graph *getDistanceGraph() const {
        return this->distance_graph;
    }
    vector<Vector2D> calculatePathTo(Vector2D src, Vector2D dest, const void *ignore, bool can_fly) const;
    static float distanceOfPath(Vector2D src, const vector<Vector2D> &path, bool has_max_dist, float max_dist);

#if 0
    void initVisibility(Vector2D pos);
#endif
    void clearVisibility();
    void revealMap(PlayingGamestate *playing_gamestate);
    vector<FloorRegion *> updateVisibility(Vector2D pos);

    // for testing:
    set<Scenery *> getSceneryUnlockedBy(const string &unlocked_by_template);
    Scenery *findScenery(const string &scenery_name);
    Character *findCharacter(const string &character_name);
    vector<Item *> getItems(const string &item_name, bool include_scenery, bool include_characters, vector<Scenery *> *scenery_owners, vector<Character *> *character_owners);
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
    set<string> flags;

    // rule of three
    Quest& operator=(const Quest &) {
        throw string("Quest assignment operator disallowed");
    }
    Quest(const Quest &) {
        throw string("Quest copy constructor disallowed");
    }
public:
    Quest();
    ~Quest();

    void addLocation(Location *location);
    Location *findLocation(const string &name);
    void addFlag(const string &name) {
        this->flags.insert(name);
    }
    bool hasFlag(const string &name) const {
        return this->flags.find(name) != this->flags.end();
    }
    set<string>::const_iterator flagsBegin() const {
        return this->flags.begin();
    }
    set<string>::const_iterator flagsEnd() const {
        return this->flags.end();
    }
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
