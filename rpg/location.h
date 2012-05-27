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
};

class Scenery {
protected:
    Location *location;
    string name;
    string image_name;
    Vector2D pos; // pos in Location (for centre)
    void *user_data_gfx;

    bool is_blocking;
    bool blocks_visibility;
    bool is_door, is_exit;
    bool is_locked; // relevant only for some types, e.g., containers, doors
    string unlock_item_name;
    float width, height;

    bool opened;
    set<Item *> items;

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
    }
    bool isExit() const {
        return this->is_exit;
    }
    void setLocked(bool is_locked) {
        this->is_locked = is_locked;
    }
    bool isLocked() const {
        return this->is_locked;
    }
    void setUnlockItemName(const string &unlock_item_name) {
        this->unlock_item_name = unlock_item_name;
    }
    string getUnlockItemName() const {
        return this->unlock_item_name;
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
    void setOpened(bool opened);
    bool isOpened() const {
        return this->opened;
    }
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

    bool isSetOff(const Character *character);
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

    FloorRegion() : Polygon2D(), is_visible(false) {
    }

public:
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
    LocationListener *listener;
    void *listener_data;

    Graph *distance_graph;

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries;

    set<Character *> characters;
    set<Item *> items;
    set<Scenery *> scenerys;
    set<Trap *> traps;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const;
    void intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const Scenery *ignore_one_scenery) const;

    vector<Vector2D> calculatePathWayPoints() const;
public:
    Location();
    ~Location();

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
    void setQuestObjective(QuestObjective *quest_objective) {
        this->quest_objective = quest_objective;
    }
    void setInfo(const string &info) {
        this->info = info;
    }
    string getInfo() const {
        return this->info;
    }
    const vector<Location *>::const_iterator locationsBegin() const {
        return this->locations.begin();
    }
    const vector<Location *>::const_iterator locationsEnd() const {
        return this->locations.end();
    }

    bool isCompleted() const {
        return this->is_completed;
    }
    bool testIfComplete();
};

class QuestInfo {
    string filename;
public:
    QuestInfo(const string &filename);

    string getFilename() const {
        return filename;
    }
};
