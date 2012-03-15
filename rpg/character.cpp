#include "character.h"
#include "location.h"
#include "item.h"
#include "../game.h"
#include "../qt_screen.h"

#include <cmath>

Character::Character(string name, string animation_name, bool is_ai) :
    name(name), animation_name(animation_name), is_ai(is_ai),
    is_dead(false), time_of_death_ms(0),
    listener(NULL), listener_data(NULL),
    has_destination(false), target_npc(NULL), time_last_action_ms(0), is_hitting(false),
    health(0), max_health(0),
    current_weapon(NULL)
{

}

Character::~Character() {
    LOG("Character::~Character(): %s\n", this->name.c_str());
    qDebug("Character::~Character(): %s", this->name.c_str());
    if( this->listener != NULL ) {
        this->listener->characterDeath(this, this->listener_data);
    }
    for(set<Item *>::iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        Item *item = *iter;
        delete item;
    }
}

bool Character::update(PlayingGamestate *playing_gamestate, int time_ms) {
    /*if( is_ai )
        return false;*/
    int elapsed_ms = game_g->getScreen()->getElapsedMS();

    if( is_dead ) {
        if( elapsed_ms > time_of_death_ms + 400 ) {
            //qDebug("kill this character");
            return true; // now remove from location/scene and delete character
        }
        return false;
    }

    if( is_hitting && target_npc == NULL ) {
        throw string("is_hitting is true, but no target_npc");
    }
    /*if( target_npc != NULL && !is_hitting ) {
        qDebug("### %d vs %d : %d", elapsed_ms, time_last_action_ms, time_last_action_ms + 1000);
    }*/
    /*if( target_npc != NULL ) {
        qDebug("is_hitting? %d", is_hitting);
    }*/
    if( elapsed_ms > time_last_action_ms + 400 && target_npc != NULL && is_hitting ) {
        is_hitting = false;
        LOG("character %s hit %s\n", this->getName().c_str(), target_npc->getName().c_str());
        qDebug("character %s hit %s", this->getName().c_str(), target_npc->getName().c_str());
        if( this->listener != NULL ) {
            this->listener->characterSetAnimation(this, this->listener_data, "");
        }
        if( !target_npc->is_dead ) {
            int new_health = target_npc->changeHealth(-1);
            qDebug("    health now %d", new_health);
            if( new_health <= 0 ) {
                target_npc->is_dead = true;
                target_npc->time_of_death_ms = elapsed_ms;
                /*if( target_npc->listener != NULL ) {
                    target_npc->listener->characterDeath(target_npc, target_npc->listener_data);
                }*/
                if( target_npc->listener != NULL ) {
                    target_npc->listener->characterSetAnimation(target_npc, target_npc->listener_data, "death");
                }
            }
        }
    }
    else if( elapsed_ms > time_last_action_ms + 1000 && !is_hitting && target_npc != NULL ) {
    //else if( !is_hitting && target_npc != NULL ) {
        float dist = ( target_npc->getPos() - this->getPos() ).magnitude();
        if( dist <= sqrt(2.0f) * ( this->getRadius() + target_npc->getRadius() ) ) {
            // take a swing!
            is_hitting = true;
            has_destination = false;
            time_last_action_ms = elapsed_ms;
            if( this->listener != NULL ) {
                this->listener->characterSetAnimation(this, this->listener_data, "attack");
                Vector2D dir = target_npc->getPos() - this->getPos();
                if( dist > 0.0f ) {
                    dir.normalise();
                    this->listener->characterTurn(this, this->listener_data, dir);
                }
            }
        }
    }

    if( is_ai && !is_hitting ) {
        //qDebug("ping");
        this->setTargetNPC( playing_gamestate->getPlayer() );
        if( playing_gamestate->getPlayer() != NULL ) {
            Vector2D dest = playing_gamestate->getPlayer()->getPos();
            Vector2D hit_pos;
            bool hit = playing_gamestate->getLocation()->intersectSweptSquareWithBoundaries(this, &hit_pos, this->getPos(), dest, this->getRadius());
            if( hit ) {
                //qDebug("hit at: %f, %f", hit_pos.x, hit_pos.y);
                dest = hit_pos;
            }
            this->setDestination(dest.x, dest.y);
        }
        else {
            this->has_destination = false;
        }
    }

    if( this->has_destination && !is_hitting ) {
        //qDebug("move character");
        /*float diff_x = this->dest.x - this->pos.x;
        float diff_y = this->dest.y - this->pos.y;
        float step = 0.1f;
        float dist = sqrt(diff_x*diff_x + diff_y*diff_y);*/
        Vector2D diff = this->dest - this->pos;
        float step = 0.002f * time_ms;
        float dist = diff.magnitude();
        Vector2D new_pos = pos;
        if( step >= dist ) {
            new_pos = this->dest;
            this->has_destination = false;
            if( this->listener != NULL ) {
                this->listener->characterSetAnimation(this, this->listener_data, "");
            }
        }
        else {
            /*diff_x /= dist;
            diff_y /= dist;
            new_pos.x += step * diff_x;
            new_pos.y += step * diff_y;*/
            diff /= dist;
            new_pos += diff * step;
        }
        this->setPos(new_pos.x, new_pos.y);
    }

    return false;
}

void Character::addItem(Item *item) {
    this->items.insert(item);
    bool graphics_changed = false;
    if( this->current_weapon == NULL && item->getType() == ITEMTYPE_WEAPON ) {
        // automatically arm weapon
        this->current_weapon = static_cast<Weapon *>(item);
        graphics_changed = true;
    }
    if( this->listener != NULL && graphics_changed ) {
        this->listener->characterUpdateGraphics(this, this->listener_data);
    }
}

void Character::dropItem(Location *location, Item *item) {
    this->items.erase(item);
    bool graphics_changed = false;
    if( this->current_weapon == item ) {
        this->current_weapon = NULL;
        graphics_changed = true;
    }
    if( location != NULL ) {
        location->addItem(item, this->pos.x, this->pos.y);
    }
    if( this->listener != NULL && graphics_changed ) {
        this->listener->characterUpdateGraphics(this, this->listener_data);
    }
}
