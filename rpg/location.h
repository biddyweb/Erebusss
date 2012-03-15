#pragma once

#include <vector>
using std::vector;
#include <set>
using std::set;

#include "utils.h"

class Character;
class Item;
class Location;

class LocationListener {
public:
    virtual void locationAddItem(const Location *location, Item *item)=0;
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

    vector<FloorRegion *> floor_regions;
    vector<Polygon2D> boundaries; // first boundary is always the outside one

    set<Character *> characters;
    set<Item *> items;

    void intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax);
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

    void setListener(LocationListener *listener) {
        this->listener = listener;
    }
    void addCharacter(Character *character, float xpos, float ypos);
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
    void charactersErase(set<Character *>::iterator iter) {
        this->characters.erase(iter);
    }
    void addItem(Item *item, float xpos, float ypos);
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

    bool intersectSweptSquareWithBoundaries(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width);
    //bool intersectSweptCircleWithBoundaries(Vector2D *hit_pos, Vector2D start, Vector2D end, float radius);

    vector<Vector2D> calculatePathWayPoints() const;

    void precalculate();
};
