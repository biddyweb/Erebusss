#pragma once

#include <string>
using std::string;

#include "utils.h"

class Image;

enum ItemType {
    ITEMTYPE_GENERAL = 0,
    ITEMTYPE_WEAPON = 1,
    ITEMTYPE_SHIELD = 2,
    ITEMTYPE_ARMOUR = 3,
    ITEMTYPE_CURRENCY = 4
};

class Item {
    string name;
    Image *image;
    Vector2D pos; // when stored in a Location
    void *user_data_gfx;

public:
    Item(string name, Image *image);
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
    void setUserGfxData(void *user_data_gfx) {
        this->user_data_gfx = user_data_gfx;
    }
    void *getUserGfxData() {
        return this->user_data_gfx;
    }
};

class Weapon : public Item {
    string animation_filename;
public:
    Weapon(string name, Image *image, string animation_filename);
    virtual ~Weapon() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_WEAPON;
    }
    virtual Weapon *clone() const; // virtual copy constructor

    string getAnimationFilename() const {
        return this->animation_filename;
    }
};

class Armour : public Item {
    int rating;
public:
    Armour(string name, Image *image, int rating);
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
    Currency(string name, Image *image);
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
