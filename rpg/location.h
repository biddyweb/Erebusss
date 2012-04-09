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
    Vector2D pos; // pos in Location
    void *user_data_gfx;

    bool is_blocking;
    float width, height;

    bool opened;
    set<Item *> items;

public:
    Scenery(string name, string image_name);
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

    void setBlocking(bool is_blocking) {
        this->is_blocking = is_blocking;
    }
    bool isBlocking() const {
        return this->is_blocking;
    }
    float getWidth() const {
        return this->width;
    }
    float getHeight() const {
        return this->height;
    }
    void setSize(float width, float height) {
        // n.b., aspect-ratio should match that of the corresponding image for this scenery!
        this->width = width;
        this->height = height;
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
    void setOpened(bool opened);
    bool isOpened() const {
        return this->opened;
    }
};

class FloorRegion : public Polygon2D {
protected:
    FloorRegion() : Polygon2D() {
    }

public:

    static FloorRegion *createRectangle(float x, float y, float w, float h);
};

class Location {
    /*float width;
    float height;*/
    LocationListener *listener;
    void *listener_data;

    Graph *distance_graph;

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries; // first boundary is always the outside one

    set<Character *> characters;
    set<Item *> items;
    set<Scenery *> scenerys;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax);
    void intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax);

    vector<Vector2D> calculatePathWayPoints() const;
public:
    Location();
    ~Location();

    /*float getWidth() const {
        return this->width;
    }
    float getHeight() const {
        return this->height;
    }*/
    void addFloorRegion(FloorRegion *floorRegion) {
        this->floor_regions.push_back(floorRegion);
    }
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
    //void removeScenery(Scenery *scenery);
    void updateScenery(Scenery *scenery);
    void createBoundariesForScenery();
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

    bool collideWithTransient(const Character *character, Vector2D pos) const;
    bool intersectSweptSquareWithBoundaries(Vector2D *hit_pos, Vector2D start, Vector2D end, float width);
    bool intersectSweptSquareWithBoundariesAndNPCs(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width);

    void calculateDistanceGraph();
    const Graph *getDistanceGraph() const {
        return this->distance_graph;
    }

    //void precalculate();
};
