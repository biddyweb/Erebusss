#include "character.h"
#include "location.h"
#include "item.h"
#include "../game.h"
#include "../qt_screen.h"

#include <cmath>

#ifdef _DEBUG
#include <cassert>
#endif

CharacterTemplate::CharacterTemplate(const string &animation_name, int health_min, int health_max, int gold_min, int gold_max) :
    animation_name(animation_name), health_min(health_min), health_max(health_max), gold_min(gold_min), gold_max(gold_max)
{
}

int CharacterTemplate::getHealth() const {
    if( health_min == health_max )
        return health_min;
    int r = rand() % (health_max - health_min + 1);
    return health_min + r;
}

int CharacterTemplate::getGold() const {
    if( gold_min == gold_max )
        return gold_min;
    int r = rand() % (gold_max - gold_min + 1);
    return gold_min + r;
}

Character::Character(const string &name, string animation_name, bool is_ai) :
    name(name), animation_name(animation_name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    location(NULL), is_dead(false), time_of_death_ms(0),
    listener(NULL), listener_data(NULL),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), is_hitting(false),
    fighting_prowess(0), strength(0), health(0), max_health(0),
    current_weapon(NULL), current_shield(NULL), current_armour(NULL), gold(0)
{

}

Character::Character(const string &name, bool is_ai, const CharacterTemplate &character_template) :
    name(name), //animation_name(animation_name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    location(NULL), is_dead(false), time_of_death_ms(0),
    listener(NULL), listener_data(NULL),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), is_hitting(false),
    fighting_prowess(0), strength(0), health(0), max_health(0),
    current_weapon(NULL), current_shield(NULL), current_armour(NULL), gold(0)
{
    this->animation_name = character_template.getAnimationName();
    this->initialiseHealth( character_template.getHealth() );
    this->gold = character_template.getGold();
}

Character::~Character() {
    LOG("Character::~Character(): %s\n", this->name.c_str());
    if( this->listener != NULL ) {
        this->listener->characterDeath(this, this->listener_data);
    }
    for(set<Item *>::iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        Item *item = *iter;
        delete item;
    }
}

Item *Character::findItem(const string &key) {
    //qDebug("Character::findItem(%s)", key.c_str());
    for(set<Item *>::iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        Item *item = *iter;
        //qDebug("    compare to: %s", item->getKey().c_str());
        if( item->getKey() == key )
            return item;
    }
    //qDebug("    not found");
    return NULL;
}

bool Character::useAmmo(Ammo *ammo) {
    // n.b., must be an item owned by Character!
    bool used_up = false;
    int amount = ammo->getAmount();

    ASSERT_LOGGER( amount > 0 );
    amount--;
    if( amount > 0 ) {
        ammo->setAmount(amount);
    }
    else {
        this->items.erase(ammo);
        delete ammo;
        used_up = true;
    }
    return used_up;
}

bool Character::update(PlayingGamestate *playing_gamestate) {
    if( this->location == NULL ) {
        return false;
    }
    /*if( is_ai )
        return false;*/
    //int elapsed_ms = game_g->getScreen()->getElapsedMS();
    int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();

    if( is_dead ) {
        if( elapsed_ms > time_of_death_ms + 400 ) {
            return true; // now remove from location/scene and delete character
        }
        return false;
    }

    //float hit_range_c = target_npc == NULL ? 0.0f : sqrt(2.0f) * ( npc_radius_c + npc_radius_c );
    bool ai_try_moving = true;

    /*if( is_hitting && target_npc == NULL ) {
        throw string("is_hitting is true, but no target_npc");
    }*/
    ASSERT_LOGGER( !( is_hitting && target_npc == NULL ) );
    if( target_npc != NULL ) {
        enum HitState {
            HITSTATE_HAS_HIT = 0,
            HITSTATE_IS_HITTING = 1,
            HITSTATE_IS_NOT_HITTING = 2
        };
        HitState hit_state = HITSTATE_IS_NOT_HITTING;
        if( elapsed_ms > time_last_action_ms + 400 && is_hitting ) {
            hit_state = HITSTATE_HAS_HIT;
        }
        else if( is_hitting ) {
            hit_state = HITSTATE_IS_HITTING;
        }

        if( hit_state == HITSTATE_HAS_HIT || hit_state == HITSTATE_IS_NOT_HITTING ) {
            bool is_ranged = this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->isRanged();
            float dist = ( target_npc->getPos() - this->getPos() ).magnitude();
            /* We could use the is_visible flag, but for future use we might want
               to cater for Enemy NPCs shooting friendly NPCs.
               This shouldn't be a performance issue, as this code is only
               executed when firing/hitting, and not every frame.
            */
            bool can_hit = false;
            if( is_ranged ) {
                if( dist <= npc_visibility_c ) {
                    // check line of sight
                    Vector2D hit_pos;
                    if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, this->pos, target_npc->getPos(), 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL) ) {
                        can_hit = true;
                    }
                    else {
                        //LOG("Character %s can't get line of sight to fire\n", this->getName().c_str());
                        //LOG("hit from: %f, %f\n", this->pos.x, this->pos.y);
                        //LOG("hit to: %f, %f\n", target_npc->getPos().x, target_npc->getPos().y);
                        //LOG("hit at: %f, %f\n", hit_pos.x, hit_pos.y);
                    }
                }
            }
            else {
                can_hit = dist <= hit_range_c;
            }
            if( hit_state == HITSTATE_HAS_HIT ) {
                this->setStateIdle();
                if( can_hit ) {
                    LOG("character %s hit %s\n", this->getName().c_str(), target_npc->getName().c_str());
                    ai_try_moving = false; // no point trying to move, just wait to hit again
                    if( !target_npc->is_dead ) {
                        //target_npc->changeHealth(playing_gamestate, -1);
                        target_npc->decreaseHealth(playing_gamestate, 1);
                        string text;
                        int r = rand() % 4;
                        if( r == 0 )
                            text = "Argh!";
                        else if( r == 1 )
                            text = "Ow!";
                        else if( r == 2 )
                            text = "Ouch!";
                        else
                            text = "Eek!";
                        playing_gamestate->addTextEffect(text, target_npc->getPos(), 500);
                    }
                }
            }
            else if( hit_state == HITSTATE_IS_NOT_HITTING ) {
                if( can_hit ) {
                    ai_try_moving = false; // even if we can't hit yet, we should just wait until we can
                    if( elapsed_ms > time_last_action_ms + 1000 ) {
                        // take a swing!
                        bool can_hit = true;
                        Ammo *ammo = NULL;
                        string ammo_key;
                        if( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->getRequiresAmmo() ) {
                            ammo_key = this->getCurrentWeapon()->getAmmoKey();
                            Item *item = this->findItem(ammo_key);
                            if( item == NULL && this == playing_gamestate->getPlayer() ) {
                                // this case occurs if the player arms a ranged weapon without having any ammo (as opposed to the check below, where we check for running out of ammo after firing)
                                LOG("Character %s has no ammo: %s\n", this->getName().c_str(), ammo_key.c_str());
                                playing_gamestate->addTextEffect("Run out of " + ammo_key + "!", this->getPos(), 1000);
                                can_hit = false;
                                this->armWeapon(NULL); // disarm it
                            }
                            /*else if( item->getType() != ITEMTYPE_AMMO ) {
                                LOG("required ammo type %s is not ammo\n", item->getName().c_str());
                                throw string("required ammo type is not ammo");
                            }*/
                            else {
                                if( item->getType() != ITEMTYPE_AMMO ) {
                                    LOG("required ammo type %s is not ammo\n", item->getName().c_str());
                                    ASSERT_LOGGER( item->getType() == ITEMTYPE_AMMO );
                                }
                                ammo = static_cast<Ammo *>(item);
                            }
                        }
                        if( can_hit ) {
                            if( ammo != NULL ) {
                                // ammo will be deleted if used up!
                                if( this->useAmmo(ammo) ) {
                                    ammo = NULL; // just to be safe, as pointer now deleted
                                    if( this->findItem(ammo_key) == NULL && this == playing_gamestate->getPlayer() ) {
                                        // really has used up all available ammo
                                        if( this == playing_gamestate->getPlayer() ) {
                                            LOG("Character %s has run out of ammo: %s\n", this->getName().c_str(), ammo_key.c_str());
                                            playing_gamestate->addTextEffect("Run out of " + ammo_key + "!", this->getPos(), 1000);
                                            this->armWeapon(NULL); // disarm it
                                        }
                                    }
                                }
                            }
                            is_hitting = true;
                            //has_destination = false;
                            has_path = false;
                            time_last_action_ms = elapsed_ms;
                            if( this->listener != NULL ) {
                                string anim = is_ranged ? "ranged" : "attack";
                                this->listener->characterSetAnimation(this, this->listener_data, anim);
                                Vector2D dir = target_npc->getPos() - this->getPos();
                                if( dist > 0.0f ) {
                                    dir.normalise();
                                    this->listener->characterTurn(this, this->listener_data, dir);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if( is_ai && !is_hitting && ai_try_moving ) {
        bool done_target = false;
        if( is_hostile && playing_gamestate->getPlayer() != NULL ) {
            if( this->is_visible ) {
                this->setTargetNPC( playing_gamestate->getPlayer() );
                Vector2D dest = playing_gamestate->getPlayer()->getPos();
                this->setDestination(dest.x, dest.y, NULL);
                done_target = true;
            }
        }

        if( !done_target ) {
            this->setTargetNPC(NULL);
            //this->has_destination = false;
            //this->has_path = false;
            if( this->has_path ) {
                this->setStateIdle();
            }
        }
    }

    //if( this->has_destination && !is_hitting ) {
    if( this->has_path && !is_hitting ) {
        //Vector2D diff = this->dest - this->pos;
        if( this->path.size() == 0 ) {
            LOG("Character %s has path, but empty path vector\n", this->getName().c_str());
            ASSERT_LOGGER( this->path.size() != 0 );
        }
        if( !this->canMove() ) {
            // can't move!
        }
        else {
            Vector2D dest = this->path.at(0);
            Vector2D diff = dest - this->pos;
            int time_ms = game_g->getScreen()->getGameTimeFrameMS();
            float step = 0.002f * time_ms;
            float dist = diff.magnitude();
            Vector2D new_pos = pos;
            if( step >= dist ) {
                /*new_pos = this->dest;
                this->setStateIdle();*/
                new_pos = dest;
                this->path.erase(this->path.begin());
                if( this->path.size() == 0 ) {
                    this->setStateIdle();
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
            if( location->collideWithTransient(this, new_pos) ) {
                // can't move - so stay where we are
                // though we don't modify the path, so we can continue if the obstacle moves
            }
            else {
                this->setPos(new_pos.x, new_pos.y);
            }
        }
    }

    return false;
}

void Character::setStateIdle() {
    //has_destination = false;
    has_path = false;
    is_hitting = false;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "");
    }
}

/*int Character::changeHealth(const PlayingGamestate *playing_gamestate, int change) {
    if( this->is_dead ) {
        LOG("tried to changeHealth of %s by %d - already dead!\n", this->getName().c_str(), change);
        throw string("can't change health of dead character");
    }
    this->health += change;
    if( health > max_health )
        health = max_health;
    if( health <= 0 ) {
        LOG("%s has died\n", this->getName().c_str());
        int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();
        this->is_dead = true;
        this->time_of_death_ms = elapsed_ms;
        if( this->listener != NULL ) {
            this->listener->characterSetAnimation(this, this->listener_data, "death");
        }
        if( this->location != NULL ) {
            while( this->items.size() > 0 ) {
                Item *item = *this->items.begin();
                this->dropItem(item);
            }
            if( this->gold > 0 ) {
                Currency *currency = playing_gamestate->cloneGoldItem(this->gold);
                location->addItem(currency, this->pos.x, this->pos.y);
                this->gold = 0;
            }
        }
    }
    return this->health;
}*/

int Character::increaseHealth(int increase) {
    if( this->is_dead ) {
        LOG("tried to increaseHealth of %s by %d - already dead!\n", this->getName().c_str(), increase);
        ASSERT_LOGGER( !this->is_dead );
    }
    else if( increase <= 0 ) {
        LOG("increaseHealth: received non-positive increase: %d\n", increase);
        ASSERT_LOGGER( increase > 0 );
    }
    this->health += increase;
    if( health > max_health )
        health = max_health;
    return this->health;
}

int Character::decreaseHealth(const PlayingGamestate *playing_gamestate, int decrease) {
    if( this->is_dead ) {
        LOG("tried to decreaseHealth of %s by %d - already dead!\n", this->getName().c_str(), decrease);
        ASSERT_LOGGER( !this->is_dead );
    }
    else if( decrease <= 0 ) {
        LOG("decreaseHealth: received non-positive decrease: %d\n", decrease);
        ASSERT_LOGGER( decrease > 0 );
    }
    this->health -= decrease;
    if( health <= 0 ) {
        LOG("%s has died\n", this->getName().c_str());
        int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();
        this->is_dead = true;
        this->time_of_death_ms = elapsed_ms;
        if( this->listener != NULL ) {
            this->listener->characterSetAnimation(this, this->listener_data, "death");
        }
        if( this->location != NULL ) {
            while( this->items.size() > 0 ) {
                Item *item = *this->items.begin();
                this->dropItem(item);
            }
            if( this->gold > 0 ) {
                Currency *currency = playing_gamestate->cloneGoldItem(this->gold);
                location->addItem(currency, this->pos.x, this->pos.y);
                this->gold = 0;
            }
        }
    }
    return this->health;
}

void Character::armWeapon(Weapon *item) {
    // n.b., must be an item owned by Character!
    // set NULL to disarm
    if( this->current_weapon != item ) {
        this->current_weapon = item;
        if( this->is_hitting ) {
            qDebug("cancel attack due to changing weapon");
            this->setStateIdle();
        }
        if( item != NULL && item->isTwoHanded() && this->current_shield != NULL ) {
            this->current_shield = NULL;
        }
        if( this->listener != NULL ) {
            this->listener->characterUpdateGraphics(this, this->listener_data);
        }
    }
}

void Character::armShield(Shield *item) {
    // n.b., must be an item owned by Character!
    // set NULL to disarm
    if( item != NULL && this->current_weapon != NULL && this->current_weapon->isTwoHanded() ) {
        // can't arm shield!
    }
    else if( this->current_shield != item ) {
        this->current_shield = item;
        if( this->listener != NULL ) {
            this->listener->characterUpdateGraphics(this, this->listener_data);
        }
    }
}

void Character::wearArmour(Armour *item) {
    // n.b., must be an item owned by Character!
    // set NULL to disarm
    if( this->current_armour != item ) {
        this->current_armour = item;
        /*if( this->listener != NULL ) {
            this->listener->characterUpdateGraphics(this, this->listener_data);
        }*/
    }
}

void Character::addItem(Item *item) {
    if( item->getType() == ITEMTYPE_CURRENCY ) {
        // special case
        Currency *currency = static_cast<Currency *>(item);
        this->gold += currency->getValue();
        LOG("delete gold item: %d\n", item);
        delete item;
        return;
    }

    this->items.insert(item);
    if( this->current_weapon == NULL && item->getType() == ITEMTYPE_WEAPON ) {
        // automatically arm weapon
        this->armWeapon( static_cast<Weapon *>(item) );
    }
    if( this->current_shield == NULL && item->getType() == ITEMTYPE_SHIELD ) {
        // automatically arm shield
        this->armShield( static_cast<Shield *>(item) );
    }
    if( this->current_armour == NULL && item->getType() == ITEMTYPE_ARMOUR ) {
        // automatically wear aromur
        this->wearArmour( static_cast<Armour *>(item) );
    }
}

void Character::pickupItem(Item *item) {
    if( location != NULL ) {
        location->removeItem(item);
    }
    this->addItem(item);
}

void Character::takeItem(Item *item) {
    this->items.erase(item);
    bool graphics_changed = false;
    if( this->current_weapon == item ) {
        this->current_weapon = NULL;
        graphics_changed = true;
    }
    if( this->current_shield == item ) {
        this->current_shield = NULL;
        graphics_changed = true;
    }
    if( this->current_armour == item ) {
        this->current_armour = NULL;
        //graphics_changed = true;
    }

    if( this->listener != NULL && graphics_changed ) {
        this->listener->characterUpdateGraphics(this, this->listener_data);
    }
}

void Character::dropItem(Item *item) {
    this->takeItem(item);
    if( location != NULL ) {
        location->addItem(item, this->pos.x, this->pos.y);
    }
    else {
        delete item;
    }
}

void Character::addGold(int change) {
    this->gold += change;
    if( this->gold < 0 ) {
        LOG("Character::addGold(), removed %d, leaves %d\n", -change, this->gold);
        ASSERT_LOGGER( gold >= 0 );
    }
}

int Character::calculateItemsWeight() const {
    int weight = 0;
    for(set<Item *>::const_iterator iter = this->items.begin();iter != this->items.end(); ++iter) {
        const Item *item = *iter;
        weight += item->getWeight();
    }
    return weight;
}

void Character::setPath(vector<Vector2D> &path) {
    //LOG("Character::setPath() for %s\n", this->getName().c_str());
    bool old_has_path = this->has_path;
    this->has_path = true;
    this->path = path;
    this->is_hitting = false;
    if( this->listener != NULL && !old_has_path ) {
        this->listener->characterSetAnimation(this, this->listener_data, "run");
    }
}

void Character::setDestination(float xdest, float ydest, const Scenery *ignore_scenery) {
    //LOG("Character::setDestination(%f, %f) for %s , currently at %f, %f\n", xdest, ydest, this->getName().c_str(), this->pos.x, this->pos.y);
    if( this->location == NULL ) {
        LOG("can't set destination for character with NULL location");
        ASSERT_LOGGER( location != NULL );
    }

    Vector2D dest(xdest, ydest);

    vector<Vector2D> new_path;

    Vector2D hit_pos;
    if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, this->pos, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, ignore_scenery) ) {
        // easy
        //LOG("easy\n");
        new_path.push_back(dest);
    }
    else {
        //LOG("    calculate path\n");
        const Graph *distance_graph = this->location->getDistanceGraph();
        Graph *graph = distance_graph->clone();

        size_t n_old_vertices = graph->getNVertices();
        GraphVertex start_vertex(this->pos, NULL);
        GraphVertex end_vertex(dest, NULL);
        size_t start_index = graph->addVertex(start_vertex);
        size_t end_index = graph->addVertex(end_vertex);

        // n.b., don't need to check for link between start_vertex and end_vertex, as this code path is only for where we can't walk directly between the start and end!
        for(size_t i=0;i<n_old_vertices;i++) {
            GraphVertex *v_A = graph->getVertex(i);
            Vector2D A = v_A->getPos();
            for(size_t j=n_old_vertices;j<graph->getNVertices();j++) {
                GraphVertex *v_B = graph->getVertex(j);
                Vector2D B = v_B->getPos();
                Vector2D hit_pos;
                float dist = (A - B).magnitude();
                bool hit = false;
                if( dist <= E_TOL_LINEAR ) {
                    // needed for coi way points?
                    hit = false;
                }
                else {
                    hit = location->intersectSweptSquareWithBoundaries(&hit_pos, false, A, B, npc_radius_c, Location::INTERSECTTYPE_MOVE, j==end_index ? ignore_scenery : NULL);
                }
                if( !hit ) {
                    v_A->addNeighbour(j, dist);
                    v_B->addNeighbour(i, dist);
                }
            }
        }

        vector<GraphVertex *> shortest_path = graph->shortestPath(start_index, end_index);
        if( shortest_path.size() == 0 ) {
            // can't reach destination (or already at it)
            //LOG("    can't reach destination (or already at it)\n");
        }
        else {
            for(vector<GraphVertex *>::const_iterator iter = shortest_path.begin(); iter != shortest_path.end(); ++iter) {
                const GraphVertex *vertex = *iter;
                new_path.push_back(vertex->getPos());
            }
        }

        delete graph;
    }

    if( new_path.size() > 0 ) {
        //LOG("set path\n");
        if( ignore_scenery != NULL ) {
            Vector2D p0 = this->pos;
            if( new_path.size() >= 2 ) {
                p0 = new_path.at( new_path.size() - 2 );
            }
            if( location->intersectSweptSquareWithBoundaries(&hit_pos, true, p0, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, NULL) ) {
                new_path[ new_path.size() - 1 ] = hit_pos;
            }
        }
        this->setPath(new_path);
    }

}

int Character::getCanCarryWeight() const {
    return 400;
    //return 10;
}

bool Character::canMove() const {
    bool can_move = true;
    if( can_move && this->calculateItemsWeight() > this->getCanCarryWeight() ) {
        can_move = false;
    }
    return can_move;
}
