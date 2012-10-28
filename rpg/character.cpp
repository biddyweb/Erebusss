#include "character.h"
#include "location.h"
#include "item.h"
#include "../game.h"
#include "../playinggamestate.h"
#include "../qt_screen.h"
#include "../logiface.h"

#include <cmath>

#ifdef _DEBUG
#include <cassert>
#endif

Profile::Profile(const CharacterTemplate &character_template) {
    *this = *character_template.getProfile();
}

int Profile::getIntProperty(const string &key) const {
    map<string, int>::const_iterator iter = this->int_properties.find(key);
    if( iter == this->int_properties.end() ) {
        return 0;
    }
    return iter->second;
}

float Profile::getFloatProperty(const string &key) const {
    map<string, float>::const_iterator iter = this->float_properties.find(key);
    if( iter == this->float_properties.end() ) {
        return 0;
    }
    return iter->second;
}

bool Profile::hasIntProperty(const string &key) const {
    map<string, int>::const_iterator iter = this->int_properties.find(key);
    if( iter == this->int_properties.end() ) {
        return false;
    }
    return true;
}

bool Profile::hasFloatProperty(const string &key) const {
    map<string, float>::const_iterator iter = this->float_properties.find(key);
    if( iter == this->float_properties.end() ) {
        return false;
    }
    return true;
}

void Spell::castOn(PlayingGamestate *playing_gamestate, Character *source, Character *target) const {
    if( this->type == "attack" ) {
        int damage = rollDice(rollX, rollY, rollZ);
        if( damage > 0 ) {
            if( target->decreaseHealth(playing_gamestate, damage, true, true) ) {
                target->addPainTextEffect(playing_gamestate);
                if( target->isDead() && source == playing_gamestate->getPlayer() ) {
                    source->addXP(playing_gamestate, target->getXPWorth());
                }
            }
        }
    }
    else if( this->type == "heal" ) {
    }
    else {
        LOG("unknown spell type: %s\n", this->type.c_str());
        ASSERT_LOGGER(false);
    }
    source->useSpell(this->getName());
}

CharacterTemplate::CharacterTemplate(const string &animation_name, int FP, int BS, int S, int A, int M, int D, int B, float Sp, int health_min, int health_max, int gold_min, int gold_max, int xp_worth) :
    //FP(FP), BS(BS), S(S), A(A), M(M), D(D), B(B), Sp(Sp), health_min(health_min), health_max(health_max), has_natural_damage(false), natural_damageX(0), natural_damageY(0), natural_damageZ(0), can_fly(false), gold_min(gold_min), gold_max(gold_max), xp_worth(xp_worth), requires_magical(false), animation_name(animation_name), static_image(false)
    profile(FP, BS, S, A, M, D, B, Sp), health_min(health_min), health_max(health_max), has_natural_damage(false), natural_damageX(0), natural_damageY(0), natural_damageZ(0), can_fly(false), gold_min(gold_min), gold_max(gold_max), xp_worth(xp_worth), requires_magical(false), animation_name(animation_name), static_image(false)
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

ProfileEffect::ProfileEffect(const Profile &profile, int duration_ms) : profile(profile), expires_ms(0) {
    this->expires_ms = game_g->getScreen()->getGameTimeTotalMS() + duration_ms;
}

const int default_natural_damageX = 1;
const int default_natural_damageY = 3;
const int default_natural_damageZ = -1;

Character::Character(const string &name, string animation_name, bool is_ai) :
    name(name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    animation_name(animation_name), static_image(false),
    location(NULL), listener(NULL), listener_data(NULL),
    is_dead(false), time_of_death_ms(0), is_visible(false),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), action(ACTION_NONE), casting_spell(NULL), has_default_position(false),
    //FP(0), BS(0), S(0), A(0), M(0), D(0), B(0), Sp(0.0f),
    health(0), max_health(0),
    natural_damageX(default_natural_damageX), natural_damageY(default_natural_damageY), natural_damageZ(default_natural_damageZ),
    can_fly(false),
    is_paralysed(false), paralysed_until(0),
    current_weapon(NULL), current_shield(NULL), current_armour(NULL), gold(0), xp(0), xp_worth(0), requires_magical(false),
    can_talk(false), has_talked(false), interaction_xp(0), interaction_completed(false)
{

}

Character::Character(const string &name, bool is_ai, const CharacterTemplate &character_template) :
    name(name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    static_image(character_template.isStaticImage()),
    location(NULL), listener(NULL), listener_data(NULL),
    is_dead(false), time_of_death_ms(0), is_visible(false),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), action(ACTION_NONE), casting_spell(NULL), has_default_position(false),
    //FP(character_template.getFP()), BS(character_template.getBS()), S(character_template.getStrength()), A(character_template.getAttacks()), M(character_template.getMind()), D(character_template.getDexterity()), B(character_template.getBravery()), Sp(character_template.getSpeed()),
    profile(character_template),
    health(0), max_health(0),
    natural_damageX(default_natural_damageX), natural_damageY(default_natural_damageY), natural_damageZ(default_natural_damageZ),
    can_fly(character_template.canFly()),
    is_paralysed(false), paralysed_until(0),
    current_weapon(NULL), current_shield(NULL), current_armour(NULL), gold(0), xp(0), xp_worth(0), requires_magical(false),
    can_talk(false), has_talked(false), interaction_xp(0), interaction_completed(false)
{
    this->animation_name = character_template.getAnimationName();
    this->initialiseHealth( character_template.getHealth() );
    if( character_template.hasNaturalDamage() ) {
        character_template.getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);
    }
    this->gold = character_template.getGold();
    this->xp_worth = character_template.getXPWorth();
    this->requires_magical = character_template.requiresMagical();
}

Character::~Character() {
    qDebug("Character::~Character(): %s", this->name.c_str());
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

const Item *Character::findItem(const string &key) const {
    //qDebug("Character::findItem(%s)", key.c_str());
    for(set<Item *>::const_iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        const Item *item = *iter;
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

int Character::getProfileIntProperty(const string &key) const {
    qDebug("key: %s", key.c_str());
    int value = this->getBaseProfileIntProperty(key);
    for(vector<ProfileEffect>::const_iterator iter = this->profile_effects.begin(); iter != this->profile_effects.end(); ++iter) {
        const ProfileEffect &profile_effect = *iter;
        int effect_bonus = profile_effect.getProfile()->getIntProperty(key);
        qDebug("    increase by %d\n", effect_bonus);
        value += effect_bonus;
    }
    return value;
}

float Character::getProfileFloatProperty(const string &key) const {
    float value = this->getBaseProfileFloatProperty(key);
    for(vector<ProfileEffect>::const_iterator iter = this->profile_effects.begin(); iter != this->profile_effects.end(); ++iter) {
        const ProfileEffect &profile_effect = *iter;
        float effect_bonus = profile_effect.getProfile()->getFloatProperty(key);
        value += effect_bonus;
    }
    return value;
}

int Character::modifyStatForDifficulty(PlayingGamestate *playing_gamestate, int value) const {
    if( this == playing_gamestate->getPlayer() ) {
        value = (int)(value * playing_gamestate->getDifficultyModifier() + 0.5f);
    }
    else {
        value = (int)(value / playing_gamestate->getDifficultyModifier() + 0.5f);
    }
    return value;
}

bool Character::update(PlayingGamestate *playing_gamestate) {
    //qDebug("Character::update() for: %s", this->name.c_str());
    if( this->location == NULL ) {
        return false;
    }

    const int time_to_die_c = 400;
    const int time_to_hit_c = 400;
    const int time_to_fire_c = 400;
    const int time_to_cast_c = 2000;

    int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();

    // expire profile effects
    // count backwards, so we can delete
    // careful of unsigned size_t! see http://stackoverflow.com/questions/665745/whats-the-best-way-to-do-a-reverse-for-loop-with-an-unsigned-index
    for(size_t i=profile_effects.size();i-->0;) {
        const ProfileEffect &profile_effect = profile_effects[i];
        if( elapsed_ms >= profile_effect.getExpiresMS() ) {
            LOG("%s: effect %d expires: %d, %d\n", this->name.c_str(), i, profile_effect.getExpiresMS(), elapsed_ms);
            profile_effects.erase(profile_effects.begin() + i);
        }
    }

    if( is_dead ) {
        if( elapsed_ms > time_of_death_ms + time_to_die_c ) {
            return true; // now remove from location/scene and delete character
        }
        return false;
    }

    if( this->is_paralysed ) {
        if( elapsed_ms >= this->paralysed_until ) {
            this->is_paralysed = false;
        }
        else {
            return false;
        }
    }

    bool ai_try_moving = true;

    bool are_enemies = false;
    if( target_npc != NULL ) {
        if( this == playing_gamestate->getPlayer() )
            are_enemies = target_npc->isHostile();
        else
            are_enemies = this->isHostile();
    }

    //ASSERT_LOGGER( !( is_hitting && target_npc == NULL ) );
    ASSERT_LOGGER( !( (action == ACTION_HITTING || action == ACTION_FIRING || action == ACTION_CASTING) && target_npc == NULL ) );
    if( target_npc != NULL && are_enemies ) {
        enum HitState {
            HITSTATE_HAS_HIT = 0,
            HITSTATE_IS_HITTING = 1,
            HITSTATE_IS_NOT_HITTING = 2
        };
        HitState hit_state = HITSTATE_IS_NOT_HITTING;
        if( action == ACTION_HITTING || action == ACTION_FIRING || action == ACTION_CASTING ) {
            hit_state = HITSTATE_IS_HITTING;
            int action_delay = 0;
            if( action == ACTION_HITTING ) {
                action_delay = time_to_hit_c;
            }
            else if( action == ACTION_FIRING ) {
                action_delay = time_to_fire_c;
            }
            else if( action == ACTION_CASTING ) {
                action_delay = time_to_cast_c;
            }
            else {
                ASSERT_LOGGER(false);
            }
            if( elapsed_ms > time_last_action_ms + action_delay ) {
                hit_state = HITSTATE_HAS_HIT;
            }
        }

        if( hit_state == HITSTATE_HAS_HIT || hit_state == HITSTATE_IS_NOT_HITTING ) {
            float dist = ( target_npc->getPos() - this->getPos() ).magnitude();
            /* We could use the is_visible flag, but for future use we might want
               to cater for Enemy NPCs shooting friendly NPCs.
               This shouldn't be a performance issue, as this code is only
               executed when firing/hitting, and not every frame.
            */
            bool is_ranged = false;
            const Spell *spell = NULL;
            if( hit_state == HITSTATE_HAS_HIT ) {
                is_ranged = action == ACTION_FIRING || action == ACTION_CASTING;
                if( action == ACTION_CASTING )
                    spell = casting_spell;
            }
            else {
                for(map<string, int>::const_iterator iter = this->spells.begin(); iter != this->spells.end(); ++iter) {
                    if( iter->second > 0 ) {
                        string spell_name = iter->first;
                        const Spell *this_spell = playing_gamestate->findSpell(spell_name);
                        if( this_spell->getType() == "attack") {
                            spell = this_spell;
                            break;
                        }
                    }
                }
                is_ranged = spell != NULL || ( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->isRanged() );
            }
            bool can_hit = false;
            if( is_ranged ) {
                if( dist <= npc_visibility_c ) {
                    // check line of sight
                    Vector2D hit_pos;
                    if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, this->pos, target_npc->getPos(), 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL, this->can_fly) ) {
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
                if( can_hit && !target_npc->is_dead ) {
                    ai_try_moving = false; // no point trying to move, just wait to hit again
                    if( action == ACTION_CASTING ) {
                        // make sure we still have the spell
                        ASSERT_LOGGER(casting_spell != NULL);
                        if( this->getSpellCount(casting_spell->getName()) > 0 ) {
                            stringstream str;
                            str << "Casts ";
                            str << spell->getName();
                            playing_gamestate->addTextEffect(str.str(), this->getPos(), 500);
                            spell->castOn(playing_gamestate, this, target_npc);
                        }
                    }
                    else {
                        int hit_roll = rollDice(2, 6, 0);
                        //int stat = is_ranged ? this->getBS() : this->getFP();
                        int stat = this->getProfileIntProperty(is_ranged ? profile_key_BS_c : profile_key_FP_c);
                        int mod_stat = this->modifyStatForDifficulty(playing_gamestate, stat);
                        if( hit_roll <= mod_stat ) {
                            //LOG("character %s rolled %d, hit %s (ranged? %d)\n", this->getName().c_str(), hit_roll, target_npc->getName().c_str(), is_ranged);
                            bool is_magical = this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->isMagical();
                            if( !is_magical && target_npc->requiresMagical() ) {
                                // weapon has no effect!
                                if( this == playing_gamestate->getPlayer() ) {
                                    playing_gamestate->addTextEffect("Weapon has no effect!", this->getPos(), 2000, 255, 0, 0);
                                }
                            }
                            else {
                                int damage = this->getCurrentWeapon() != NULL ? this->getCurrentWeapon()->getDamage() : this->getNaturalDamage();
                                //if( rollDice(2, 6, 0) <= this->getStrength() ) {
                                if( rollDice(2, 6, 0) <= this->getProfileIntProperty(profile_key_S_c) ) {
                                    //LOG("    extra strong hit!\n");
                                    damage++;
                                }
                                //LOG("    damage: %d\n", damage);
                                if( damage > 0 ) {
                                    if( target_npc->decreaseHealth(playing_gamestate, damage, true, true) ) {
                                        target_npc->addPainTextEffect(playing_gamestate);
                                        if( target_npc->isDead() && this == playing_gamestate->getPlayer() ) {
                                            this->addXP(playing_gamestate, target_npc->getXPWorth());
                                        }
                                    }
                                }
                            }
                        }
                        else{
                            //LOG("character %s rolled %d, missed %s (ranged? %d)\n", this->getName().c_str(), hit_roll, target_npc->getName().c_str(), is_ranged);
                        }
                    }
                }
                this->setStateIdle();
            }
            else if( hit_state == HITSTATE_IS_NOT_HITTING ) {
                if( can_hit ) {
                    ai_try_moving = false; // even if we can't hit yet, we should just wait until we can
                    if( elapsed_ms > time_last_action_ms + 1000 ) {
                        if( spell != NULL ) {
                            // cast spell!
                            action = ACTION_CASTING;
                            casting_spell = spell;
                            has_path = false;
                            time_last_action_ms = elapsed_ms;
                        }
                        else {
                            // take a swing!
                            bool can_hit = true;
                            Ammo *ammo = NULL;
                            string ammo_key;
                            if( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->getRequiresAmmo() ) {
                                ammo_key = this->getCurrentWeapon()->getAmmoKey();
                                Item *item = this->findItem(ammo_key);
                                if( item == NULL ) {
                                    if( this == playing_gamestate->getPlayer() ) {
                                        // this case occurs if the player arms a ranged weapon without having any ammo (as opposed to the check below, where we check for running out of ammo after firing)
                                        //LOG("Character %s has no ammo: %s\n", this->getName().c_str(), ammo_key.c_str());
                                        playing_gamestate->addTextEffect("Run out of " + ammo_key + "!", this->getPos(), 1000);
                                    }
                                    // for NPCs, this shouldn't happen, but put this check just in case!
                                    can_hit = false;
                                    this->armWeapon(NULL); // disarm it
                                }
                                else {
                                    if( item->getType() != ITEMTYPE_AMMO ) {
                                        //LOG("required ammo type %s is not ammo\n", item->getName().c_str());
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
                                            else {
                                                this->armWeapon(NULL); // disarm it (will select the best weapon to use on next hit attempt)
                                            }
                                        }
                                    }
                                }
                                //is_hitting = true;
                                action = is_ranged ? ACTION_FIRING : ACTION_HITTING;
                                //has_destination = false;
                                has_path = false;
                                time_last_action_ms = elapsed_ms;
                                if( this->listener != NULL ) {
                                    string anim = is_ranged ? "ranged" : "attack";
                                    this->listener->characterSetAnimation(this, this->listener_data, anim, true);
                                    Vector2D dir = target_npc->getPos() - this->getPos();
                                    if( dist > 0.0f ) {
                                        dir.normalise();
                                        this->listener->characterTurn(this, this->listener_data, dir);
                                    }
                                }
                                playing_gamestate->playSound("swing");
                            }
                        }
                    }
                }
            }
        }
    }

    //if( is_ai && !is_hitting && ai_try_moving ) {
    if( is_ai && action == ACTION_NONE && ai_try_moving ) {
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
            if( this->has_default_position && this->default_position != this->pos ) {
                this->setDestination(this->default_position.x, this->default_position.y, NULL);
                done_target = true;
            }
            else if( this->has_path ) {
                this->setStateIdle();
            }
        }
    }

    //if( this->has_destination && !is_hitting ) {
    //if( this->has_path && !is_hitting ) {
    if( this->has_path && action == ACTION_NONE ) {
        //Vector2D diff = this->dest - this->pos;
        if( this->path.size() == 0 ) {
            LOG("Character %s has path, but empty path vector\n", this->getName().c_str());
            ASSERT_LOGGER( this->path.size() != 0 );
        }
        if( !this->canMove() ) {
            // can't move!
            if( this->listener != NULL ) {
                this->listener->characterSetAnimation(this, this->listener_data, "", false);
            }
        }
        else {
            Vector2D dest = this->path.at(0);
            Vector2D diff = dest - this->pos;
            int time_ms = game_g->getScreen()->getGameTimeFrameMS();
            //float step = 0.002f * time_ms;
            //float step = (this->getSpeed() * time_ms)/1000.0f;
            float step = (this->getProfileFloatProperty(profile_key_Sp_c) * time_ms)/1000.0f;
            float dist = diff.magnitude();
            Vector2D new_pos = pos;
            bool next_seg = false;
            if( step >= dist ) {
                /*new_pos = this->dest;
                this->setStateIdle();*/
                new_pos = dest;
                next_seg = true;
            }
            else {
                diff /= dist;
                new_pos += diff * step;
            }
            if( location->collideWithTransient(this, new_pos) ) {
                // can't move - so stay where we are
                // though we don't modify the path, so we can continue if the obstacle moves
                if( this->listener != NULL ) {
                    this->listener->characterSetAnimation(this, this->listener_data, "", false);
                }
            }
            else {
                if( this->listener != NULL ) {
                    this->listener->characterSetAnimation(this, this->listener_data, "run", false);
                }
                this->setPos(new_pos.x, new_pos.y);
                if( next_seg ) {
                    this->path.erase(this->path.begin());
                    if( this->path.size() == 0 ) {
                        this->setStateIdle();
                    }
                }
            }
        }
    }

    return false;
}

void Character::addPainTextEffect(PlayingGamestate *playing_gamestate) const {
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
    playing_gamestate->addTextEffect(text, this->getPos(), 500);
}

void Character::setStateIdle() {
    //has_destination = false;
    has_path = false;
    //is_hitting = false;
    action = ACTION_NONE;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "", false);
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

int Character::getNaturalDamage() const {
    int roll = rollDice(natural_damageX, natural_damageY, natural_damageZ);
    return roll;
}

void Character::increaseHealth(int increase) {
    if( this->is_dead ) {
        LOG("tried to increaseHealth of %s by %d - already dead!\n", this->getName().c_str(), increase);
        ASSERT_LOGGER( !this->is_dead );
    }
    else if( increase < 0 ) {
        LOG("increaseHealth: received negative increase: %d\n", increase);
        ASSERT_LOGGER( increase >= 0 );
    }
    this->health += increase;
    if( health > max_health )
        health = max_health;
}

bool Character::decreaseHealth(const PlayingGamestate *playing_gamestate, int decrease, bool armour, bool shield) {
    if( this->is_dead ) {
        LOG("tried to decreaseHealth of %s by %d - already dead!\n", this->getName().c_str(), decrease);
        ASSERT_LOGGER( !this->is_dead );
    }
    else if( decrease < 0 ) {
        LOG("decreaseHealth: received negative decrease: %d\n", decrease);
        ASSERT_LOGGER( decrease >= 0 );
    }
    int armour_value = 0;
    if( armour && this->getCurrentArmour() != NULL )
        armour_value += this->getCurrentArmour()->getRating();
    if( shield && this->getCurrentShield() != NULL )
        armour_value++;
    decrease -= armour_value;
    decrease = std::max(decrease, 0);
    this->health -= decrease;
    if( health <= 0 ) {
        this->kill(playing_gamestate);
    }
    return (decrease > 0);
}

void Character::kill(const PlayingGamestate *playing_gamestate) {
    this->health = 0;
    LOG("%s has died\n", this->getName().c_str());
    int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();
    this->is_dead = true;
    this->time_of_death_ms = elapsed_ms;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "death", false);
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

void Character::armWeapon(Weapon *item) {
    // n.b., must be an item owned by Character!
    // set NULL to disarm
    if( this->current_weapon != item ) {
        this->current_weapon = item;
        //if( this->is_hitting ) {
        if( action == ACTION_HITTING || action == ACTION_FIRING ) {
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

void Character::addItem(Item *item, bool auto_arm) {
    if( item->getType() == ITEMTYPE_CURRENCY ) {
        // special case
        Currency *currency = static_cast<Currency *>(item);
        this->gold += currency->getValue();
        LOG("delete gold item: %d\n", item);
        delete item;
        return;
    }

    this->items.insert(item);
    if( auto_arm && this->current_weapon == NULL && item->getType() == ITEMTYPE_WEAPON ) {
        // automatically arm weapon
        this->armWeapon( static_cast<Weapon *>(item) );
    }
    if( auto_arm && this->current_shield == NULL && item->getType() == ITEMTYPE_SHIELD ) {
        // automatically arm shield
        this->armShield( static_cast<Shield *>(item) );
    }
    if( auto_arm && this->current_armour == NULL && item->getType() == ITEMTYPE_ARMOUR ) {
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

void Character::setGold(int gold) {
    this->gold = gold;
    ASSERT_LOGGER( gold >= 0 );
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

void Character::paralyse(int time_ms) {
    this->setStateIdle();
    this->is_paralysed = true;
    this->paralysed_until = game_g->getScreen()->getGameTimeTotalMS() + time_ms;
    //qDebug("%d, %d", paralysed_until, time_ms);
}

void Character::setPath(vector<Vector2D> &path) {
    //LOG("Character::setPath() for %s\n", this->getName().c_str());
    this->has_path = true;
    this->path = path;
    //this->is_hitting = false;
    this->action = ACTION_NONE;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "run", false);
    }
}

void Character::setDestination(float xdest, float ydest, const Scenery *ignore_scenery) {
    //LOG("Character::setDestination(%f, %f) for %s , currently at %f, %f\n", xdest, ydest, this->getName().c_str(), this->pos.x, this->pos.y);
    if( this->location == NULL ) {
        LOG("can't set destination for character with NULL location");
        ASSERT_LOGGER( location != NULL );
    }

    Vector2D dest(xdest, ydest);

    /*vector<Vector2D> new_path;

    Vector2D hit_pos;
    if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, this->pos, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, ignore_scenery, this->can_fly) ) {
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
                    hit = location->intersectSweptSquareWithBoundaries(&hit_pos, false, A, B, npc_radius_c, Location::INTERSECTTYPE_MOVE, j==end_index ? ignore_scenery : NULL, this->can_fly);
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
            if( location->intersectSweptSquareWithBoundaries(&hit_pos, true, p0, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, NULL, this->can_fly) ) {
                new_path[ new_path.size() - 1 ] = hit_pos;
            }
        }
        this->setPath(new_path);
    }*/

    vector<Vector2D> new_path = location->calculatePathTo(this->pos, dest, ignore_scenery, this->can_fly);
    if( new_path.size() > 0 ) {
        //LOG("set path\n");
        this->setPath(new_path);
    }
}

int Character::getCanCarryWeight() const {
    //return 300;
    //return 10;
    //return 250 + 10 * this->getStrength();
    return 250 + 10 * this->getProfileIntProperty(profile_key_S_c);
}

bool Character::canMove() const {
    bool can_move = true;
    if( can_move && this->calculateItemsWeight() > this->getCanCarryWeight() ) {
        can_move = false;
    }
    return can_move;
}

void Character::useSpell(const string &spell) {
    map<string, int>::iterator iter = this->spells.find(spell);
    ASSERT_LOGGER( iter != this->spells.end() );
    ASSERT_LOGGER( iter->second > 0 );
    iter->second--;
}

void Character::addXP(PlayingGamestate *playing_gamestate, int change) {
    this->xp += change;
    if( this == playing_gamestate->getPlayer() ) {
        stringstream xp_str;
        xp_str << change << " XP";
        playing_gamestate->addTextEffect(xp_str.str(), this->getPos(), 2000, 255, 0, 0);
    }
}

bool Character::canCompleteInteraction(PlayingGamestate *playing_gamestate) const {
    if( this->interaction_completed )
        return false;
    if( this->interaction_type == "WANT_OBJECT" ) {
        if( playing_gamestate->getPlayer()->findItem(this->interaction_data) != NULL ) {
            return true;
        }
    }
    else {
        ASSERT_LOGGER(false);
    }
    return false;
}

void Character::completeInteraction(PlayingGamestate *playing_gamestate) {
    ASSERT_LOGGER( !this->interaction_completed );
    if( this->interaction_type == "WANT_OBJECT" ) {
        Item *item = playing_gamestate->getPlayer()->findItem(this->interaction_data);
        ASSERT_LOGGER(item != NULL);
        if( item != NULL ) {
            playing_gamestate->getPlayer()->takeItem(item);
            delete item;
            if( this->interaction_xp > 0 ) {
                playing_gamestate->getPlayer()->addXP(playing_gamestate, this->interaction_xp);
            }
        }
    }
    else {
        ASSERT_LOGGER(false);
    }
    this->interaction_completed = true;
}

/*string Character::getTalkItem(const string &question) const {
    map<string, string>::const_iterator iter = this->talk_items.find(question);
    if( iter == this->talk_items.end() ) {
        ASSERT_LOGGER(false);
        return "";
    }
    return iter->second;
}*/
