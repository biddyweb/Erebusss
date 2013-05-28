#pragma once

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <set>
using std::set;

#include <map>
using std::map;

#include "utils.h"

class Character;
class CharacterTemplate;
class PlayingGamestate;
class Item;
class Weapon;
class Shield;
class Armour;
class Ring;
class Ammo;
class Location;
class Scenery;

/*class Value {
    enum Type {
        TYPE_INT = 0,
        TYPE_FLOAT = 1
    };
    Type type;
    int value_int;
    float value_float;
public:
    Value() : type(TYPE_INT), value_int(0), value_float(0.0f) {
    }

    void setInt(int value) {
        this->type = TYPE_INT;
        this->value_int = value;
    }
    void setFloat(float value) {
        this->type = TYPE_FLOAT;
        this->value_float = value;
    }
};*/

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
const float talk_range_c = hit_range_c;

const string profile_key_FP_c = "FP";
const string profile_key_BS_c = "BS";
const string profile_key_S_c = "S";
const string profile_key_A_c = "A";
const string profile_key_M_c = "M";
const string profile_key_D_c = "D";
const string profile_key_B_c = "B";
const string profile_key_Sp_c = "Sp";

string getLongString(const string &key);

class Profile {
    map<string, int> int_properties;
    map<string, float> float_properties;
public:

    Profile() {
        // create dummy profile - should call set() before reading any of the properties
    }
    Profile(const CharacterTemplate &character_template);
    Profile(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->set(FP, BS, S, A, M, D, B, Sp);
    }

    void setIntProperty(const string &key, int value) {
        this->int_properties[key] = value;
    }
    void setFloatProperty(const string &key, float value) {
        this->float_properties[key] = value;
    }
    int getIntProperty(const string &key) const;
    float getFloatProperty(const string &key) const;
    bool hasIntProperty(const string &key) const;
    bool hasFloatProperty(const string &key) const;
    map<string, int>::const_iterator intPropertiesBegin() const {
        return this->int_properties.begin();
    }
    map<string, int>::const_iterator intPropertiesEnd() const {
        return this->int_properties.end();
    }
    map<string, float>::const_iterator floatPropertiesBegin() const {
        return this->float_properties.begin();
    }
    map<string, float>::const_iterator floatPropertiesEnd() const {
        return this->float_properties.end();
    }

    // convenient wrapper to set all properties
    void set(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->setIntProperty(profile_key_FP_c, FP);
        this->setIntProperty(profile_key_BS_c, BS);
        this->setIntProperty(profile_key_S_c, S);
        this->setIntProperty(profile_key_A_c, A);
        this->setIntProperty(profile_key_M_c, M);
        this->setIntProperty(profile_key_D_c, D);
        this->setIntProperty(profile_key_B_c, B);
        this->setFloatProperty(profile_key_Sp_c, Sp);
    }
};

class Spell {
    string name;
    string type;
    string effect; // use to define more specific types of effects (distinct from the type, which is a more general grouping, to help the AI decide what to cast)
    int rollX, rollY, rollZ;
    bool damage_armour, damage_shield;
    bool mind_test; // if true, receipient can avoid bad effects if passes a mind test
public:
    Spell(const string &name, const string &type, const string &effect) : name(name), type(type), effect(effect), /*rating(1),*/ rollX(0), rollY(0), rollZ(0), damage_armour(false), damage_shield(false), mind_test(false) {
    }

    string getName() const {
        return this->name;
    }
    string getType() const {
        return this->type;
    }
    void setRoll(int rollX, int rollY, int rollZ) {
        this->rollX = rollX;
        this->rollY = rollY;
        this->rollZ = rollZ;
    }
    void setDamage(bool damage_armour, bool damage_shield) {
        this->damage_armour = damage_armour;
        this->damage_shield = damage_shield;
    }
    void setMindTest(bool mind_test) {
        this->mind_test = mind_test;
    }
    void castOn(PlayingGamestate *playing_gamestate, Character *source, Character *target) const;
};

class TalkItem {
public:
    string question;
    string answer;
    string action;
    string journal;
    bool while_not_done;
    bool objective;

    TalkItem(const string &question, const string &answer, const string &action, const string &journal, bool while_not_done, bool objective) :
        question(question), answer(answer), action(action), journal(journal), while_not_done(while_not_done), objective(objective)
    {
    }
};

class CharacterListener {
public:
    virtual void characterUpdateGraphics(const Character *character, void *user_data)=0;
    virtual void characterTurn(const Character *character, void *user_data)=0;
    virtual void characterMoved(Character *character, void *user_data)=0;
    virtual void characterSetAnimation(const Character *character, void *user_data, const string &name, bool force_restart)=0;
    virtual void characterDeath(Character *character, void *user_data)=0;
};

class CharacterTemplate {
    Profile profile;
    int health_min, health_max;
    bool has_natural_damage;
    int natural_damageX, natural_damageY, natural_damageZ;
    bool can_fly;
    int gold_min, gold_max;
    int xp_worth;
    bool causes_terror;
    int terror_effect;
    int causes_disease;
    int causes_paralysis;
    bool requires_magical; // requires magical weapon to hit?
    bool unholy;
    string animation_name;
    bool static_image;
    bool bounce;
    string weapon_resist_class; // resistance to this weapon class
    int weapon_resist_percentage; // damage to that weapon class is scaled by this amount (so lower means less damage; set to greater than 100 for more damage)

public:
    CharacterTemplate(const string &animation_name, int FP, int BS, int S, int A, int M, int D, int B, float Sp, int health_min, int health_max, int gold_min, int gold_max, int xp_worth, bool causes_terror, int terror_effect, int causes_disease, int causes_paralysis);

    const Profile *getProfile() const {
        return &this->profile;
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
    void setCanFly(bool can_fly) {
        this->can_fly = can_fly;
    }
    bool canFly() const {
        return this->can_fly;
    }
    int getGold() const;
    int getXPWorth() const {
        return xp_worth;
    }
    bool getCausesTerror() const {
        return this->causes_terror;
    }
    int getTerrorEffect() const {
        return this->terror_effect;
    }
    int getCausesDisease() const {
        return this->causes_disease;
    }
    int getCausesParalysis() const {
        return this->causes_paralysis;
    }
    void setRequiresMagical(bool requires_magical) {
        this->requires_magical = requires_magical;
    }
    bool requiresMagical() const {
        return requires_magical;
    }
    void setUnholy(bool unholy) {
        this->unholy = unholy;
    }
    bool isUnholy() const {
        return this->unholy;
    }
    string getAnimationName() const {
        return this->animation_name;
    }
    void setStaticImage(bool static_image) {
        this->static_image = static_image;
    }
    bool isStaticImage() const {
        return this->static_image;
    }
    void setBounce(bool bounce) {
        this->bounce = bounce;
    }
    bool isBounce() const {
        return this->bounce;
    }
    void setWeaponResist(const string &weapon_resist_class, int weapon_resist_percentage) {
        this->weapon_resist_class = weapon_resist_class;
        this->weapon_resist_percentage = weapon_resist_percentage;
    }
    string getWeaponResistClass() const {
        return this->weapon_resist_class;
    }
    int getWeaponResistPercentage() const {
        return this->weapon_resist_percentage;
    }
};

class ProfileEffect {
    Profile profile;
    int expires_ms;
public:
    ProfileEffect(const Profile &profile, int duration_ms);

    const Profile *getProfile() const {
        return &this->profile;
    }
    int getExpiresMS() const {
        return this->expires_ms;
    }
};

class Character {
    // basic info
    string name;
    string biography;
    bool is_ai; // not saved
    bool is_hostile;
    bool is_fixed; // whether character can move or not (for now used for non-hostile NPCs)
    string portrait;
    string animation_name; // for NPCs (player is handled separately)
    bool static_image; // for NPCs
    bool bounce;
    string weapon_resist_class; // resistance to this weapon class
    int weapon_resist_percentage; // damage to that weapon class is scaled by this amount (so lower means less damage; set to greater than 100 for more damage)

    // game data
    Location *location; // not saved
    CharacterListener *listener; // not saved
    void *listener_data; // not saved
    bool is_dead;
    int time_of_death_ms; // not saved
    Vector2D pos;
    Vector2D direction; // not saved
    bool is_visible; // not saved // for NPCs: whether player and NPC can see each other
    bool has_path; // not saved
    vector<Vector2D> path; // not saved
    Character *target_npc; // not saved
    int time_last_action_ms; // not saved
    //bool is_hitting; // not saved
    enum Action {
        ACTION_NONE = 0,
        ACTION_HITTING = 1,
        ACTION_FIRING = 2,
        ACTION_CASTING = 3
    };
    Action action;
    const Spell *casting_spell;
    Character *casting_spell_target;
    int time_last_complex_update_ms; // not saved
    // default positions only relevant for NPCs that don't change locations
    bool has_default_position;
    Vector2D default_position;
    bool has_last_known_player_position;
    Vector2D last_known_player_position;

    // rpg data
    //int FP, BS, S, A, M, D, B;
    //float Sp;
    Profile profile;
    int health;
    int max_health;
    int natural_damageX, natural_damageY, natural_damageZ;
    bool can_fly;
    bool is_paralysed;
    int paralysed_until;
    bool is_diseased;

    vector<ProfileEffect> profile_effects;

    // used for levelling:
    Profile initial_profile;
    int initial_level;

    set<Item *> items;
    Weapon *current_weapon;
    Ammo *current_ammo;
    Shield *current_shield;
    Armour *current_armour;
    Ring *current_ring;
    int gold;

    map<string, int> spells;

    int level;
    int xp;
    int xp_worth;
    bool causes_terror;
    int terror_effect;
    bool done_terror;
    bool is_fleeing; // for NPCs
    int causes_disease;
    int causes_paralysis;
    bool requires_magical; // requires magical weapon to hit?
    bool unholy;

    // npc talk information
    bool can_talk;
    bool has_talked;
    string interaction_type;
    string interaction_data;
    int interaction_xp;
    string interaction_reward_item;
    string interaction_journal;
    string interaction_set_flag;
    bool interaction_completed;
    string talk_opening_initial;
    string talk_opening_later;
    string talk_opening_interaction_complete;
    //map<string, string> talk_items;
    vector<TalkItem> talk_items;
    // shopkeepers
    string shop;

    string objective_id;

    bool useAmmo(Ammo *ammo);
    int getNaturalDamage() const;

    // rule of three
    /*Character& operator=(const Character &character) {
        throw string("Character assignment operator disallowed");
    }*/

public:
    Character(const string &name, string animation_name, bool is_ai);
    Character(const string &name, bool is_ai, const CharacterTemplate &character_template);
    explicit Character(const Character &character);
    ~Character();

    void setPortrait(const string &portrait) {
        this->portrait = portrait;
    }
    string getPortrait() const {
        return this->portrait;
    }
    void setBiography(const string &biography) {
        this->biography = biography;
    }
    string getBiography() const {
        return this->biography;
    }
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
    void setDirection(Vector2D dir);
    Vector2D getDirection() const {
        return this->direction;
    }
    string getName() const {
        return this->name;
    }
    string getAnimationName() const {
        return this->animation_name;
    }
    void setStaticImage(bool static_image) {
        this->static_image = static_image;
    }
    bool isStaticImage() const {
        return this->static_image;
    }
    void setBounce(bool bounce) {
        this->bounce = bounce;
    }
    bool isBounce() const {
        return this->bounce;
    }
    void setWeaponResist(const string &weapon_resist_class, int weapon_resist_percentage) {
        this->weapon_resist_class = weapon_resist_class;
        this->weapon_resist_percentage = weapon_resist_percentage;
    }
    string getWeaponResistClass() const {
        return this->weapon_resist_class;
    }
    int getWeaponResistPercentage() const {
        return this->weapon_resist_percentage;
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
        if( !is_hostile && this->is_ai ) {
            this->is_fixed = true;
        }
        else {
            this->is_fixed = false;
        }
    }
    bool isFixed() const {
        return this->is_fixed;
    }
    bool isDead() const {
        return this->is_dead;
    }
    void setDead(bool is_dead) {
        // Should only be used to directly set flag, e.g., when loading games. To kill a character, use kill().
        this->is_dead = is_dead;
    }
    void kill(PlayingGamestate *playing_gamestate);

    void setVisible(bool is_visible) {
        this->is_visible = is_visible;
    }
    bool isVisible() const {
        return this->is_visible;
    }
    void paralyse(int time_ms);
    bool isParalysed() const {
        return this->is_paralysed;
    }
    int getParalysedUntil() const {
        return this->paralysed_until;
    }
    void setDiseased(bool is_diseased) {
        this->is_diseased = is_diseased;
    }
    bool isDiseased() const {
        return this->is_diseased;
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
    void setDestination(float xdest, float ydest, const void *ignore);
    bool hasPath() const {
        return this->has_path;
    }
    Vector2D getDestination() const;
    bool update(PlayingGamestate *playing_gamestate);
    void setTargetNPC(Character *target_npc);
    Character *getTargetNPC() const {
        return this->target_npc;
    }
    void notifyDead(const Character *character);

    const Profile *getBaseProfile() const {
        return &this->profile;
    }
    const Profile *getInitialBaseProfile() const {
        return &this->initial_profile;
    }
    void changeBaseProfileIntProperty(const string &key, int change) {
        int value = this->profile.getIntProperty(key);
        value += change;
        this->profile.setIntProperty(key, value);
    }
    void changeBaseProfileFloatProperty(const string &key, float change) {
        float value = this->profile.getFloatProperty(key);
        value += change;
        this->profile.setFloatProperty(key, value);
    }
    int getBaseProfileIntProperty(const string &key) const {
        return this->profile.getIntProperty(key);
    }
    float getBaseProfileFloatProperty(const string &key) const {
        return this->profile.getFloatProperty(key);
    }
    int getProfileIntProperty(const string &key) const;
    float getProfileFloatProperty(const string &key) const;
    bool hasBaseProfileIntProperty(const string &key) const {
        return this->profile.hasIntProperty(key);
    }
    bool hasBaseProfileFloatProperty(const string &key) const {
        return this->profile.hasFloatProperty(key);
    }
    void setProfile(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->profile.set(FP, BS, S, A, M, D, B, Sp);
        this->initial_level = this->level;
        this->initial_profile = this->profile;
    }
    void setInitialProfile(int initial_level, int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->initial_level = initial_level;
        this->initial_profile.set(FP, BS, S, A, M, D, B, Sp);
    }
    void addProfileEffect(const ProfileEffect &profile_effect) {
        this->profile_effects.push_back(profile_effect);
    }
    vector<ProfileEffect>::const_iterator profileEffectsBegin() const {
        return this->profile_effects.begin();
    }
    vector<ProfileEffect>::const_iterator profileEffectsEnd() const {
        return this->profile_effects.end();
    }
    void expireProfileEffects() {
        this->profile_effects.clear();
    }
    int modifyStatForDifficulty(PlayingGamestate *playing_gamestate, int value) const;
    void initialiseHealth(int max_health) {
        if( max_health < 0 ) {
            // allow 0 for friendly NPCs
            throw string("max health must not be negative");
        }
        this->health = max_health;
        this->max_health = max_health;
    }
    void setHealth(int health) {
        if( health > max_health ) {
            throw string("health set to greater than max_health");
        }
        else if( health < 0 ) {
            // allow 0 for friendly NPCs
            throw string("health must not be negative");
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
    bool decreaseHealth(PlayingGamestate *playing_gamestate, int decrease, bool armour, bool shield);
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
    void setCanFly(bool can_fly) {
        this->can_fly = can_fly;
    }
    bool canFly() const {
        return this->can_fly;
    }
    int getArmourRating(bool armour, bool shield) const;

    const Weapon *getCurrentWeapon() const {
        return this->current_weapon;
    }
    Weapon *getCurrentWeapon() {
        return this->current_weapon;
    }
    void armWeapon(Weapon *item);
    const Ammo *getCurrentAmmo() const {
        return this->current_ammo;
    }
    Ammo *getCurrentAmmo() {
        return this->current_ammo;
    }
    void selectAmmo(Ammo *item);
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
    const Ring *getCurrentRing() const {
        return this->current_ring;
    }
    Ring *getCurrentRing() {
        return this->current_ring;
    }
    void wearRing(Ring *item);
    void addItem(Item *item) {
        addItem(item, true);
    }
    void addItem(Item *item, bool auto_arm);
    void pickupItem(Item *item);
    void takeItem(Item *item);
    void dropItem(Item *item);
    void deleteItem(const string &key);
    set<Item *>::iterator itemsBegin();
    set<Item *>::const_iterator itemsBegin() const;
    set<Item *>::iterator itemsEnd();
    set<Item *>::const_iterator itemsEnd() const;
    Item *findItem(const string &key);
    const Item *findItem(const string &key) const;
    Ammo *findAmmo(const string &key);
    int getGold() const {
        return this->gold;
    }
    void setGold(int gold);
    void addGold(int change);
    int calculateItemsWeight() const;
    int getCanCarryWeight() const;
    bool carryingTooMuch() const;
    string getWeightString() const;
    bool tooWeakForArmour() const;
    bool tooWeakForWeapon() const;
    bool canMove() const;
    void addSpell(const string &spell, int count) {
        this->spells[spell] = this->spells[spell] + count;
    }
    int getSpellCount(const string &spell) const {
        map<string, int>::const_iterator iter = this->spells.find(spell);
        if( iter != this->spells.end() ) {
            return iter->second;
        }
        return 0;
    }
    void useSpell(const string &spell);
    void addPainTextEffect(PlayingGamestate *playing_gamestate) const;

    int getLevel() const {
        return level;
    }
    void setLevel(int level) {
        this->level = level;
    }
    int getInitialLevel() const {
        return this->initial_level;
    }
    int getXP() const {
        return xp;
    }
    void setXP(int xp) {
        // should only be used in initialisation/loading
        this->xp = xp;
    }
    void addXP(PlayingGamestate *playing_gamestate, int change);
    void advanceLevel(PlayingGamestate *playing_gamestate);
    int getXPForNextLevel() const;
    int getXPWorth() const {
        return xp_worth;
    }
    void setXPWorth(int xp_worth) {
        this->xp_worth = xp_worth;
    }
    bool getCausesTerror() const {
        return this->causes_terror;
    }
    int getTerrorEffect() const {
        return this->terror_effect;
    }
    void setCausesTerror(int terror_effect) {
        this->causes_terror = true;
        this->terror_effect = terror_effect;
    }
    bool hasDoneTerror() const {
        return this->done_terror;
    }
    void setDoneTerror(bool done_terror) {
        this->done_terror = done_terror;
    }
    bool isFleeing() const {
        return this->is_fleeing;
    }
    void setFleeing(bool is_fleeing) {
        this->is_fleeing = is_fleeing;
    }
    int getCausesDisease() const {
        return this->causes_disease;
    }
    void setCausesDisease(int causes_disease) {
        this->causes_disease = causes_disease;
    }
    int getCausesParalysis() const {
        return this->causes_paralysis;
    }
    void setCausesParalysis(int causes_paralysis) {
        this->causes_paralysis = causes_paralysis;
    }
    bool requiresMagical() const {
        return requires_magical;
    }
    void setRequiresMagical(bool requires_magical) {
        this->requires_magical = requires_magical;
    }
    void setUnholy(bool unholy) {
        this->unholy = unholy;
    }
    bool isUnholy() const {
        return this->unholy;
    }

    bool canTalk() const {
        return this->can_talk;
    }
    void setCanTalk(bool can_talk) {
        this->can_talk = can_talk;
    }
    bool hasTalked() const {
        return this->has_talked;
    }
    void setHasTalked(bool has_talked) {
        this->has_talked = has_talked;
    }
    string getInteractionType() const {
        return this->interaction_type;
    }
    void setInteractionType(const string &interaction_type) {
        this->interaction_type = interaction_type;
    }
    string getInteractionData() const {
        return this->interaction_data;
    }
    void setInteractionData(const string &interaction_data) {
        this->interaction_data = interaction_data;
    }
    int getInteractionXP() const {
        return this->interaction_xp;
    }
    void setInteractionXP(int interaction_xp) {
        this->interaction_xp = interaction_xp;
    }
    string getInteractionRewardItem() const {
        return this->interaction_reward_item;
    }
    void setInteractionRewardItem(const string &interaction_reward_item) {
        this->interaction_reward_item = interaction_reward_item;
    }
    string getInteractionJournal() const {
        return this->interaction_journal;
    }
    void setInteractionJournal(const string &interaction_journal) {
        this->interaction_journal = interaction_journal;
    }
    string getInteractionSetFlag() const {
        return this->interaction_set_flag;
    }
    void setInteractionSetFlag(const string &interaction_set_flag) {
        this->interaction_set_flag = interaction_set_flag;
    }
    bool isInteractionCompleted() const {
        return this->interaction_completed;
    }
    void setInteractionCompleted(bool interaction_completed) {
        this->interaction_completed = interaction_completed;
    }
    bool canCompleteInteraction(PlayingGamestate *playing_gamestate) const;
    void completeInteraction(PlayingGamestate *playing_gamestate);
    string getTalkOpeningInitial() const {
        return this->talk_opening_initial;
    }
    void setTalkOpeningInitial(const string &talk_opening_initial) {
        this->talk_opening_initial = talk_opening_initial;
    }
    string getTalkOpeningLater() const {
        return this->talk_opening_later;
    }
    void setTalkOpeningLater(const string &talk_opening_later) {
        this->talk_opening_later = talk_opening_later;
    }
    string getTalkOpeningInteractionComplete() const {
        return this->talk_opening_interaction_complete;
    }
    void setTalkOpeningInteractionComplete(const string &talk_opening_interaction_complete) {
        this->talk_opening_interaction_complete = talk_opening_interaction_complete;
    }
    /*void addTalkItem(const string &question, const string &answer) {
        this->talk_items[question] = answer;
    }
    string getTalkItem(const string &question) const;
    map<string, string>::const_iterator talkItemsBegin() const {
        return this->talk_items.begin();
    }
    map<string, string>::const_iterator talkItemsEnd() const {
        return this->talk_items.end();
    }*/
    void addTalkItem(const string &question, const string &answer, const string &action, const string &journal, bool while_not_done, bool objective) {
        TalkItem talk_item(question, answer, action, journal, while_not_done, objective);
        this->talk_items.push_back(talk_item);
    }
    vector<TalkItem>::iterator talkItemsBegin() {
        return this->talk_items.begin();
    }
    vector<TalkItem>::iterator talkItemsEnd() {
        return this->talk_items.end();
    }
    vector<TalkItem>::const_iterator talkItemsBegin() const {
        return this->talk_items.begin();
    }
    vector<TalkItem>::const_iterator talkItemsEnd() const {
        return this->talk_items.end();
    }
    map<string, int>::const_iterator spellsBegin() const {
        return this->spells.begin();
    }
    map<string, int>::const_iterator spellsEnd() const {
        return this->spells.end();
    }
    void setShop(const string &shop) {
        this->shop = shop;
    }
    string getShop() const {
        return this->shop;
    }
    string getObjectiveId() const {
        return this->objective_id;
    }
    void setObjectiveId(const string &objective_id) {
        this->objective_id = objective_id;
    }

    static void hitEnemy(PlayingGamestate *playing_gamestate, Character *source, Character *target, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage);
};
