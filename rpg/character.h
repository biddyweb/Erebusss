#pragma once

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <set>
using std::set;

#include "utils.h"

class Character;
class PlayingGamestate;
class Item;
class Weapon;
class Shield;
class Armour;
class Ammo;
class Location;
class Scenery;

/** The visibility range - how far the player and NPCs can see. This should be
  * used for every possible action, such as ranged attacks, spells. For NPCs
  * further away than this, we don't need to do any AI.
  * If ever we make it so that different characters can have different ranges,
  * then the AI should be based on whatever is the maximum (if an NPC can see
  * the player, then clearly they should be able to do things such as fire
  * weapons; if the player can see the NPC, then the NPC should still be able
  * to react to being attacked from a distance, also if we have things like
  * scriptable movement, we want to be able to see the NPC moving).
  */
const float npc_visibility_c = 10.0f;
const float npc_radius_c = 0.25f;
const float hit_range_c = sqrt(2.0f) * ( npc_radius_c + npc_radius_c );

class CharacterListener {
public:
    virtual void characterUpdateGraphics(const Character *character, void *user_data)=0;
    virtual void characterTurn(const Character *character, void *user_data, Vector2D dir)=0;
    virtual void characterMoved(Character *character, void *user_data)=0;
    virtual void characterSetAnimation(const Character *character, void *user_data, const string &name)=0;
    virtual void characterDeath(Character *character, void *user_data)=0;
};

class CharacterTemplate {
    int FP, BS, S, A, M, D, B;
    float Sp;
    int health_min, health_max;
    bool has_natural_damage;
    int natural_damageX, natural_damageY, natural_damageZ;
    int gold_min, gold_max;
    int xp_worth;
    string animation_name;
public:
    CharacterTemplate(const string &animation_name, int FP, int BS, int S, int A, int M, int D, int B, float Sp, int health_min, int health_max, int gold_min, int gold_max, int xp_worth);

    int getFP() const {
        return this->FP;
    }
    int getBS() const {
        return this->BS;
    }
    int getStrength() const {
        return this->S;
    }
    int getAttacks() const {
        return this->A;
    }
    int getMind() const {
        return this->M;
    }
    int getDexterity() const {
        return this->D;
    }
    int getBravery() const {
        return this->B;
    }
    float getSpeed() const {
        return this->Sp;
    }
    int getHealth() const;
    void setNaturalDamage(int natural_damageX, int natural_damageY, int natural_damageZ) {
        this->has_natural_damage = true;
        this->natural_damageX = natural_damageX;
        this->natural_damageY = natural_damageY;
        this->natural_damageZ = natural_damageZ;
    }
    bool hasNaturalDamage() const {
        return this->has_natural_damage;
    }
    void getNaturalDamage(int *natural_damageX, int *natural_damageY, int *natural_damageZ) const {
        *natural_damageX = this->natural_damageX;
        *natural_damageY = this->natural_damageY;
        *natural_damageZ = this->natural_damageZ;
    }
    int getGold() const;
    int getXPWorth() const {
        return xp_worth;
    }
    string getAnimationName() const {
        return this->animation_name;
    }
};

class Character {
    // basic info
    string name;
    bool is_ai; // not saved
    bool is_hostile;
    string animation_name; // for NPCs (player is handled separately)

    // game data
    Location *location; // not saved
    CharacterListener *listener; // not saved
    void *listener_data; // not saved
    bool is_dead;
    int time_of_death_ms; // not saved
    Vector2D pos;
    bool is_visible; // not saved // for NPCs: whether player and NPC can see each other
    bool has_path; // not saved
    vector<Vector2D> path; // not saved
    Character *target_npc; // not saved
    int time_last_action_ms; // not saved
    bool is_hitting; // not saved

    // default positions only relevant for NPCs that don't change locations
    bool has_default_position; // saved
    Vector2D default_position; // saved

    // rpg data
    int FP, BS, S, A, M, D, B;
    float Sp;
    int health;
    int max_health;
    int natural_damageX, natural_damageY, natural_damageZ;

    set<Item *> items;
    Weapon *current_weapon;
    Shield *current_shield;
    Armour *current_armour;
    int gold;

    int xp;
    int xp_worth;

    Item *findItem(const string &key);
    bool useAmmo(Ammo *ammo);
    int getNaturalDamage() const;

public:
    Character(const string &name, string animation_name, bool is_ai);
    Character(const string &name, bool is_ai, const CharacterTemplate &character_template);
    ~Character();

    void setStateIdle();
    void setDefaultPosition(float xpos, float ypos) {
        this->has_default_position = true;
        this->default_position.set(xpos, ypos);
    }
    bool hasDefaultPosition() const {
        return this->has_default_position;
    }
    float getDefaultX() const {
        return this->default_position.x;
    }
    float getDefaultY() const {
        return this->default_position.y;
    }
    void setPos(float xpos, float ypos) {
        this->pos.set(xpos, ypos);
        if( this->listener != NULL ) {
            this->listener->characterMoved(this, this->listener_data);
        }
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
    string getAnimationName() const {
        return this->animation_name;
    }
    void setLocation(Location *location) {
        this->location = location;
    }
    Location *getLocation() const {
        return this->location;
    }
    void setListener(CharacterListener *listener, void *listener_data) {
        this->listener = listener;
        this->listener_data = listener_data;
    }
    void *getListenerData() {
        return this->listener_data;
    }
    bool isAI() const {
        return this->is_ai;
    }
    bool isHostile() const {
        return this->is_hostile;
    }
    void setHostile(bool is_hostile) {
        this->is_hostile = is_hostile;
    }
    bool isDead() const {
        return this->is_dead;
    }
    void setDead(bool is_dead) {
        // Should only be used to directly set flag, e.g., when loading games. To kill a character, use kill().
        this->is_dead = is_dead;
    }
    void kill(const PlayingGamestate *playing_gamestate);

    void setVisible(bool is_visible) {
        this->is_visible = is_visible;
    }
    bool isVisible() const {
        return this->is_visible;
    }
    /*void setDestination(float xdest, float ydest) {
        bool old_has_destination = this->has_destination;
        this->has_destination = true;
        this->dest.set(xdest, ydest);
        this->is_hitting = false;
        if( this->listener != NULL && !old_has_destination ) {
            this->listener->characterSetAnimation(this, this->listener_data, "run");
        }
    }*/
    void setPath(vector<Vector2D> &path);
    void setDestination(float xdest, float ydest, const Scenery *ignore_scenery);
    bool update(PlayingGamestate *playing_gamestate);
    void setTargetNPC(Character *target_npc) {
        if( this->target_npc != target_npc ) {
            this->target_npc = target_npc;
            if( this->is_hitting ) {
                this->is_hitting = false;
                if( this->listener != NULL ) {
                    this->listener->characterSetAnimation(this, this->listener_data, "");
                }
            }
        }
    }
    Character *getTargetNPC() const {
        return this->target_npc;
    }
    /*float getRadius() const {
        return 0.25f;
    }*/

    void setProfile(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->FP = FP;
        this->BS = BS;
        this->S = S;
        this->A = A;
        this->M = M;
        this->D = D;
        this->B = B;
        this->Sp = Sp;
    }
    void setFP(int FP) {
        this->FP = FP;
    }
    int getFP() const {
        return this->FP;
    }
    void setBS(int BS) {
        this->BS = BS;
    }
    int getBS() const {
        return this->BS;
    }
    void setStrength(int S) {
        this->S = S;
    }
    int getStrength() const {
        return this->S;
    }
    int getAttacks() const {
        return this->A;
    }
    void setMind(int M) {
        this->M = M;
    }
    int getMind() const {
        return this->M;
    }
    void setDexterity(int D) {
        this->D = D;
    }
    int getDexterity() const {
        return this->D;
    }
    void setBravery(int B) {
        this->B = B;
    }
    int getBravery() const {
        return this->B;
    }
    void setSpeed(int Sp) {
        this->Sp = Sp;
    }
    float getSpeed() const {
        return this->Sp;
    }
    int modifyStatForDifficulty(PlayingGamestate *playing_gamestate, int value) const;
    void initialiseHealth(int max_health) {
        if( max_health <= 0 ) {
            throw string("max health must be greater than 0");
        }
        this->health = max_health;
        this->max_health = max_health;
    }
    void setHealth(int health) {
        if( health > max_health ) {
            throw string("health set to greater than max_health");
        }
        else if( health <= 0 ) {
            throw string("health must be greater than 0");
        }
        this->health = health;
    }
    int getHealth() const {
        return this->health;
    }
    int getMaxHealth() const {
        return this->max_health;
    }
    int getHealthPercent() const {
        return (int)((100.0f * health)/(float)max_health);
    }
    void increaseHealth(int increase);
    bool decreaseHealth(const PlayingGamestate *playing_gamestate, int decrease, bool armour, bool shield);
    void restoreHealth() {
        this->health = this->max_health;
    }
    void setNaturalDamage(int natural_damageX, int natural_damageY, int natural_damageZ) {
        this->natural_damageX = natural_damageX;
        this->natural_damageY = natural_damageY;
        this->natural_damageZ = natural_damageZ;
    }
    void getNaturalDamage(int *natural_damageX, int *natural_damageY, int *natural_damageZ) const {
        *natural_damageX = this->natural_damageX;
        *natural_damageY = this->natural_damageY;
        *natural_damageZ = this->natural_damageZ;
    }

    const Weapon *getCurrentWeapon() const {
        return this->current_weapon;
    }
    Weapon *getCurrentWeapon() {
        return this->current_weapon;
    }
    void armWeapon(Weapon *item);
    const Shield *getCurrentShield() const {
        return this->current_shield;
    }
    Shield *getCurrentShield() {
        return this->current_shield;
    }
    void armShield(Shield *item);
    const Armour *getCurrentArmour() const {
        return this->current_armour;
    }
    Armour *getCurrentArmour() {
        return this->current_armour;
    }
    void wearArmour(Armour *item);
    void addItem(Item *item) {
        addItem(item, true);
    }
    void addItem(Item *item, bool auto_arm);
    void pickupItem(Item *item);
    void takeItem(Item *item);
    void dropItem(Item *item);
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
    int getGold() const {
        return this->gold;
    }
    void setGold(int gold);
    void addGold(int change);
    int calculateItemsWeight() const;
    int getCanCarryWeight() const;
    bool canMove() const;
    int getXP() const {
        return xp;
    }
    void setXP(int xp) {
        // should only be used in initialisation/loading
        this->xp = xp;
    }
    void addXP(PlayingGamestate *playing_gamestate, int change);
    int getXPWorth() const {
        return xp_worth;
    }
    void setXPWorth(int xp_worth) {
        this->xp_worth = xp_worth;
    }
};
