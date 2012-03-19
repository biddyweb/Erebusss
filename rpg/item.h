#pragma once

#include <string>
using std::string;

#include "utils.h"

enum ItemType {
    ITEMTYPE_GENERAL = 0,
    ITEMTYPE_WEAPON = 1,
    ITEMTYPE_SHIELD = 2,
    ITEMTYPE_ARMOUR = 3,
    ITEMTYPE_CURRENCY = 4
};

class Item {
    string name;
    string image_name;
    Vector2D pos; // when stored in a Location
    void *user_data_gfx;
    int weight; // in multiples of 100g

public:
    Item(string name, string image_name, int weight);
    virtual ~Item();

    virtual ItemType getType() const {
        return ITEMTYPE_GENERAL;
    }
    virtual Item *clone() const; // virtual copy constructor

    /*const Image *getImage() const {
        return this->image;
    }*/
    void setPos(float xpos, float ypos) {
        this->pos.set(xpos, ypos);
        /*if( this->listener != NULL ) {
            this->listener->characterMoved(this, this->listener_data);
        }*/
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
    void setWeight(int weight) {
        this->weight = weight;
    }
    int getWeight() const {
        return this->weight;
    }
};

class Weapon : public Item {
    string animation_name;
public:
    Weapon(string name, string image_name, int weight, string animation_name);
    virtual ~Weapon() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_WEAPON;
    }
    virtual Weapon *clone() const; // virtual copy constructor

    string getAnimationName() const {
        return this->animation_name;
    }
};

class Shield : public Item {
    string animation_name;
public:
    Shield(string name, string image_name, int weight, string animation_name);
    virtual ~Shield() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_SHIELD;
    }
    virtual Shield *clone() const; // virtual copy constructor

    string getAnimationName() const {
        return this->animation_name;
    }
};

class Armour : public Item {
    int rating;
public:
    Armour(string name, string image_name, int weight, int rating);
    virtual ~Armour() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_ARMOUR;
    }
    virtual Armour *clone() const; // virtual copy constructor

    int getRating() const {
        return this->rating;
    }
};

class Currency : public Item {
    int value;
public:
    Currency(string name, string image_name);
    virtual ~Currency() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_CURRENCY;
    }
    virtual Currency *clone() const; // virtual copy constructor

    int getValue() const {
        return this->value;
    }
    void setValue(int value) {
        this->value = value;
    }
};
