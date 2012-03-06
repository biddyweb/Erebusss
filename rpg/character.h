#pragma once

#include <string>
using std::string;

#include "utils.h"

class Character;
class PlayingGamestate;

class CharacterListener {
public:
    virtual void characterTurn(const Character *character, void *user_data, Vector2D dir)=0;
    virtual void characterMoved(const Character *character, void *user_data)=0;
    virtual void characterSetAnimation(const Character *character, void *user_data, string name)=0;
    virtual void characterDeath(Character *character, void *user_data)=0;
};

class Character {
    // basic info
    string name;
    bool is_ai;

    // game data
    bool is_dead;
    int time_of_death_ms;
    Vector2D pos;
    CharacterListener *listener;
    void *listener_data;
    bool has_destination;
    Vector2D dest;
    Character *target_npc;
    int time_last_action_ms;
    bool is_hitting;

    // rpg data
    int health;
    int max_health;
public:
    Character(string name, bool is_ai);
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
    void setListener(CharacterListener *listener, void *listener_data) {
        this->listener = listener;
        this->listener_data = listener_data;
    }
    void setDestination(float xdest, float ydest) {
        this->has_destination = true;
        this->dest.set(xdest, ydest);
        this->is_hitting = false;
        if( this->listener != NULL ) {
            this->listener->characterSetAnimation(this, this->listener_data, "run");
        }
    }
    bool update(PlayingGamestate *playing_gamestate, int time_ms);
    void setTargetNPC(Character *target_npc) {
        if( this->target_npc != target_npc ) {
            this->target_npc = target_npc;
            this->is_hitting = false;
            if( this->listener != NULL ) {
                this->listener->characterSetAnimation(this, this->listener_data, "");
            }
        }
    }
    Character *getTargetNPC() const {
        return this->target_npc;
    }
    float getRadius() const {
        return 0.25f;
    }

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
    int getHealthPercent() const {
        return (int)((100.0f * health)/(float)max_health);
    }
    int changeHealth(int change) {
        this->health += change;
        if( health > max_health )
            health = max_health;
        return this->health;
    }
};
