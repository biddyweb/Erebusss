#pragma once

#include <string>
using std::string;

#include "../common.h"

#include "utils.h"
#include "profile.h"

class Character;
class PlayingGamestate;

const float default_icon_width_c = 0.5f;

enum ItemType {
    ITEMTYPE_GENERAL = 0,
    ITEMTYPE_WEAPON = 1,
    ITEMTYPE_SHIELD = 2,
    ITEMTYPE_ARMOUR = 3,
    ITEMTYPE_RING = 4,
    ITEMTYPE_AMMO = 5,
    ITEMTYPE_CURRENCY = 6
};

/*enum ItemUse {
    ITEMUSE_NONE = 0,
    ITEMUSE_POTION_HEALING = 1
};*/

class Item {
protected:
    string name;
    string image_name;
    string description;
    string base_template; // if non-empty, stores the standard item this is a variation of (used for Shops)
    Vector2D pos; // when stored in a Location
    void *user_data_gfx; // not saved
    float icon_width; // width of icon when drawn on screen in metres
    int weight; // in multiples of 100g

    string use;
    string use_verb;
    int arg1, arg2; // used for "use"
    string arg1_s; // used for "use"
    int rating;
    bool is_magical;
    int worth_bonus;
    Profile profile_bonus;

    string getProfileBonusDescriptionInt(const string &key) const;
    string getProfileBonusDescriptionFloat(const string &key) const;

public:
    Item(const string &name, const string &image_name, int weight);

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
    void setName(const string &name) {
        this->name = name;
    }
    virtual string getName() const {
        // may be overloaded to give more descriptive names
        return this->name;
    }
    void setBaseTemplate(const string &base_template) {
        this->base_template = base_template;
    }
    string getBaseTemplate() const {
        return this->base_template;
    }
    void setDescription(const string &description) {
        this->description = description;
    }
    string getDescription() const {
        return this->description;
    }
    string getDetailedDescription(const Character *player) const;
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
    void setArg1(int arg1) {
        this->arg1 = arg1;
    }
    int getArg1() const {
        return this->arg1;
    }
    void setArg2(int arg2) {
        this->arg2 = arg2;
    }
    int getArg2() const {
        return this->arg2;
    }
    void setArg1s(const string &arg1_s) {
        this->arg1_s = arg1_s;
    }
    string getArg1s() const {
        return this->arg1_s;
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
    void setWorthBonus(int worth_bonus) {
        this->worth_bonus = worth_bonus;
    }
    int getWorthBonus() const {
        return this->worth_bonus;
    }
    void setProfileBonusIntProperty(const string &key, int value) {
        this->profile_bonus.setIntProperty(key, value);
    }
    void setProfileBonusFloatProperty(const string &key, float value) {
        this->profile_bonus.setFloatProperty(key, value);
    }
    int getRawProfileBonusIntProperty(const string &key) const;
    float getRawProfileBonusFloatProperty(const string &key) const;
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const;
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const;
};

class ItemCompare {
public:
    bool operator()(const Item *lhs, const Item *rhs) const;
};

class Weapon : public Item {
public:
    enum WeaponType {
        WEAPONTYPE_HAND = 0,
        WEAPONTYPE_RANGED = 1,
        WEAPONTYPE_THROWN = 2
    };
private:
    string animation_name;
    bool is_two_handed;
    WeaponType weapon_type;
    bool requires_ammo;
    string ammo_key;
    int damageX, damageY, damageZ;
    int min_strength;
    bool unholy_only;
    string weapon_class;
public:
    Weapon(const string &name, const string &image_name, int weight, const string &animation_name, int damageX, int damageY, int damageZ);
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
    void setWeaponType(WeaponType weapon_type) {
        this->weapon_type = weapon_type;
    }
    WeaponType getWeaponType() const {
        return this->weapon_type;
    }
    bool isRangedOrThrown() const {
        return this->weapon_type == WEAPONTYPE_RANGED || this->weapon_type == WEAPONTYPE_THROWN;
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
    void setDamage(int damageX, int damageY, int damageZ) {
        this->damageX = damageX;
        this->damageY = damageY;
        this->damageZ = damageZ;
    }
    void getDamage(int *damageX, int *damageY, int *damageZ) const {
        *damageX = this->damageX;
        *damageY = this->damageY;
        *damageZ = this->damageZ;
    }
    int getDamage() const;
    void setMinStrength(int min_strength) {
        this->min_strength = min_strength;
    }
    int getMinStrength() const {
        return this->min_strength;
    }
    void setUnholyOnly(bool unholy_only) {
        this->unholy_only = unholy_only;
    }
    bool isUnholyOnly() const {
        return this->unholy_only;
    }
    void setWeaponClass(const string weapon_class) {
        this->weapon_class = weapon_class;
    }
    string getWeaponClass() const {
        return this->weapon_class;
    }
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const;
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const;
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
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const;
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const;
};

class Armour : public Item {
    //int rating;
    int min_strength;
public:
    Armour(const string &name, const string &image_name, int weight, int rating);
    virtual ~Armour() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_ARMOUR;
    }
    virtual Armour *clone() const; // virtual copy constructor

    /*int getRating() const {
        return this->rating;
    }*/
    void setMinStrength(int min_strength) {
        this->min_strength = min_strength;
    }
    int getMinStrength() const {
        return this->min_strength;
    }
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const;
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const;
};

class Ring : public Item {
public:
    Ring(const string &name, const string &image_name, int weight);
    virtual ~Ring() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_RING;
    }
    virtual Ring *clone() const; // virtual copy constructor

    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const;
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const;
};

class Ammo : public Item {
    string ammo_type;
    string projectile_image_name;
    int amount;
public:
    Ammo(const string &name, const string &image_name, const string &ammo_type, const string &projectile_image_name, int weight, int amount);
    virtual ~Ammo() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_AMMO;
    }
    virtual Ammo *clone() const; // virtual copy constructor

    virtual string getName() const;

    string getAmmoType() const {
        return this->ammo_type;
    }
    string getProjectileImageName() const {
        return this->projectile_image_name;
    }
    int getAmount() const {
        return this->amount;
    }
    void setAmount(int amount) {
        this->amount = amount;
    }
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const {
        return 0;
    }
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const {
        return 0;
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
    virtual int getProfileBonusIntProperty(const Character *character, const string &key) const {
        return 0;
    }
    virtual float getProfileBonusFloatProperty(const Character *character, const string &key) const {
        return 0;
    }
};

class Shop {
    string name;
    vector<const Item *> items;
    vector<int> costs;
    bool allow_random_npc; // whether to allow as an NPC seller in random dungeons
    bool campaign; // whether to appear on campaign screens
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
    void setAllowRandomNPC(bool allow_random_npc) {
        this->allow_random_npc = allow_random_npc;
    }
    bool isAllowRandomNPC() const{
        return this->allow_random_npc;
    }
    void setCampaign(bool campaign) {
        this->campaign = campaign;
    }
    bool isCampaign() const {
        return this->campaign;
    }
};
