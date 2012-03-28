#pragma once

#include <string>
using std::string;

#include "utils.h"

class Character;
class PlayingGamestate;

enum ItemType {
    ITEMTYPE_GENERAL = 0,
    ITEMTYPE_WEAPON = 1,
    ITEMTYPE_SHIELD = 2,
    ITEMTYPE_ARMOUR = 3,
    ITEMTYPE_AMMO = 4,
    ITEMTYPE_CURRENCY = 5
};

enum ItemUse {
    ITEMUSE_NONE = 0,
    ITEMUSE_POTION_HEALING = 1
};

class Item {
protected:
    string name;
    string image_name;
    Vector2D pos; // when stored in a Location
    void *user_data_gfx;
    int weight; // in multiples of 100g

    ItemUse item_use;
    int rating;
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
    string getKey() const {
        // the name is used as an ID key
        return this->name;
    }
    virtual string getName() const {
        // may be overloaded to give more descriptive names
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
    bool canUse() const {
        return item_use != ITEMUSE_NONE;
    }
    string getUseVerb() const;
    bool use(PlayingGamestate *playing_gamestate, Character *character);
    void setUse(ItemUse item_use) {
        this->item_use = item_use;
    }
    void setRating(int rating) {
        this->rating = rating;
    }
};

class Weapon : public Item {
    string animation_name;
    bool is_two_handed;
    bool is_ranged;
    bool requires_ammo;
    string ammo_key;
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

    void setTwoHanded(bool is_two_handed) {
        this->is_two_handed = is_two_handed;
    }
    bool isTwoHanded() const {
        return this->is_two_handed;
    }
    void setRanged(bool is_ranged) {
        this->is_ranged = is_ranged;
    }
    bool isRanged() const {
        return this->is_ranged;
    }
    void setRequiresAmmo(bool requires_ammo, string ammo_key) {
        this->requires_ammo = requires_ammo;
        this->ammo_key = ammo_key;
    }
    bool getRequiresAmmo() const {
        return this->requires_ammo;
    }
    string getAmmoKey() const {
        return this->requires_ammo ? this->ammo_key : "";
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

class Ammo : public Item {
    string projectile_image_name;
    int amount;
public:
    Ammo(string name, string image_name, string projectile_image_name, int amount);
    virtual ~Ammo() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_AMMO;
    }
    virtual Ammo *clone() const; // virtual copy constructor

    virtual string getName() const;

    string getProjectileImageName() const {
        return this->projectile_image_name;
    }
    int getAmount() const {
        return this->amount;
    }
    void setAmount(int amount) {
        this->amount = amount;
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

    virtual string getName() const;
    int getValue() const {
        return this->value;
    }
    void setValue(int value) {
        this->value = value;
    }
};
