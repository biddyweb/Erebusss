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

/*enum ItemUse {
    ITEMUSE_NONE = 0,
    ITEMUSE_POTION_HEALING = 1
};*/

class Item {
protected:
    string name;
    string image_name;
    Vector2D pos; // when stored in a Location
    void *user_data_gfx; // not saved
    float icon_width; // width of icon when drawn on screen in metres
    int weight; // in multiples of 100g

    string use;
    string use_verb;
    int rating;
    bool is_magical;
public:
    Item(const string &name, const string &image_name, int weight);
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
    void setIconWidth(float icon_width) {
        this->icon_width = icon_width;
    }
    float getIconWidth() const {
        return this->icon_width;
    }
    void setWeight(int weight) {
        this->weight = weight;
    }
    int getWeight() const {
        return this->weight;
    }
    bool canUse() const;
    string getUse() const {
        return this->use;
    }
    string getUseVerb() const;
    bool useItem(PlayingGamestate *playing_gamestate, Character *character);
    /*void setUse(ItemUse item_use) {
        this->item_use = item_use;
    }*/
    void setUse(const string &use, const string &use_verb) {
        this->use = use;
        this->use_verb = use_verb;
    }
    void setRating(int rating) {
        this->rating = rating;
    }
    int getRating() const {
        return this->rating;
    }
    void setMagical(bool is_magical) {
        this->is_magical = is_magical;
    }
    bool isMagical() const {
        return this->is_magical;
    }
};

class Weapon : public Item {
    string animation_name;
    bool is_two_handed;
    bool is_ranged;
    bool requires_ammo;
    string ammo_key;
public:
    Weapon(const string &name, const string &image_name, int weight, const string &animation_name);
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
    void setRequiresAmmo(bool requires_ammo, const string &ammo_key) {
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
    Shield(const string &name, const string &image_name, int weight, const string &animation_name);
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
    //int rating;
public:
    Armour(const string &name, const string &image_name, int weight, int rating);
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
    Ammo(const string &name, const string &image_name, const string &projectile_image_name, int amount);
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
    Currency(const string &name, const string &image_name);
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

class Shop {
    string name;
    vector<const Item *> items;
    vector<int> costs;
public:
    Shop(const string &name);
    ~Shop();

    virtual string getName() const {
        return this->name;
    }
    void addItem(const Item *item, int cost);
    /*size_t getNItems() const;
    const Item *getItem(int *cost, size_t i) const;*/
    const vector<const Item *> &getItems() const {
        return items;
    }
    const vector<int> &getCosts() const {
        return costs;
    }
};
