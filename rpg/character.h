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
    virtual void characterSetAnimation(const Character *character, void *user_data, string name)=0;
    virtual void characterDeath(Character *character, void *user_data)=0;
};

class CharacterTemplate {
    int health_min, health_max;
    int gold_min, gold_max;
    string animation_name;
public:
    CharacterTemplate(string animation_name, int health_min, int health_max, int gold_min, int gold_max);

    int getHealth() const;
    int getGold() const;
    string getAnimationName() const {
        return this->animation_name;
    }
};

class Character {
    // basic info
    string name;
    bool is_ai;
    bool is_hostile;
    string animation_name; // for NPCs (player is handled separately)

    // game data
    Location *location;
    CharacterListener *listener;
    void *listener_data;
    bool is_dead;
    int time_of_death_ms;
    Vector2D pos;
    bool is_visible; // for NPCs: whether player and NPC can see each other
    /*bool has_destination;
    Vector2D dest;*/
    bool has_path;
    vector<Vector2D> path;
    Character *target_npc;
    int time_last_action_ms;
    bool is_hitting;

    // rpg data
    int fighting_prowess;
    int strength;
    int health;
    int max_health;

    set<Item *> items;
    Weapon *current_weapon;
    Shield *current_shield;
    Armour *current_armour;
    int gold;

    void setStateIdle();

    Item *findItem(string key);
    bool useAmmo(Ammo *ammo);

public:
    Character(string name, string animation_name, bool is_ai);
    Character(string name, bool is_ai, const CharacterTemplate &character_template);
    ~Character();

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

    void initialiseHealth(int max_health) {
        if( max_health <= 0 ) {
            throw string("max health must be greater than 0");
        }
        this->health = max_health;
        this->max_health = max_health;
    }
    void setHealth(int heath) {
        if( heath > max_health ) {
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
    //int changeHealth(const PlayingGamestate *playing_gamestate, int change);
    int increaseHealth(int increase);
    int decreaseHealth(const PlayingGamestate *playing_gamestate, int decrease);
    void restoreHealth() {
        this->health = this->max_health;
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
    void addItem(Item *item);
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
    void addGold(int change);
    int calculateItemsWeight() const;
    int getCanCarryWeight() const;
    bool canMove() const;
};
