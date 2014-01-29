#include <cmath>

#ifdef _DEBUG
#include <cassert>
#endif

#include "character.h"
#include "location.h"
#include "item.h"
#include "rpgengine.h"

#include "../game.h"
#include "../playinggamestate.h"
#include "../logiface.h"

const int default_natural_damageX = 1;
const int default_natural_damageY = 3;
const int default_natural_damageZ = -1;

const int base_time_turn_c = 1000;
const int time_to_die_c = 400;
const int time_to_hit_c = 400;
const int time_to_fire_c = 400;
const int time_to_cast_c = 2000;

string getSkillLongString(const string &key) {
    if( key == skill_unarmed_combat_c )
        return "Unarmed Combat";
    else if( key == skill_sprint_c )
        return "Sprint";
    else if( key == skill_disease_resistance_c )
        return "Disease Resistance";
    else if( key == skill_hideaway_c )
        return "Hideaway";
    else if( key == skill_shield_combat_c )
        return "Shield Combat";
    else if( key == skill_luck_c )
        return "Luck";
    else if( key == skill_fast_shooter_c )
        return "Fast Shooter";
    else if( key == skill_charge_c )
        return "Charge";
    else if( key == skill_hatred_orcs_c )
        return "Hatred of Orcs";
    LOG("getSkillLongString: unknown key: %s\n", key.c_str());
    throw string("unknown key");
}

string getSkillDescription(const string &key) {
    if( key == skill_unarmed_combat_c )
        return "You do not suffer any penalty to Fighting Prowess when fighting without weapons.";
    else if( key == skill_sprint_c )
        return "Your speed is 0.2 greater when outdoors.";
    else if( key == skill_disease_resistance_c )
        return "You are immune to many common diseases, including disease from zombies, as well as mushrooms.";
    else if( key == skill_hideaway_c )
        return "You are skilled at building secretive hideaways. Your chance of being disturbed by wandering monsters when resting is reduced by 50%.";
    else if( key == skill_shield_combat_c )
        return "You are skilled at using a weapon and shield in combat. When using a shield, your Fighting Prowess is increased by 1.";
    else if( key == skill_luck_c )
        return "You are luckier than most. If something causes you enough injury to kill you, there is a 50% chance you are not harmed.";
    else if( key == skill_fast_shooter_c )
        return "You can fire a bow more quickly than most. You have +1 Attacks when using a bow.";
    else if( key == skill_charge_c )
        return "You get +1 damage in hand-to-hand combat if you hit on your first strike in a battle.";
    else if( key == skill_hatred_orcs_c )
        return "You get +1 damage against goblinoids (Goblins, Orcs, etc) in hand-to-hand combat.";
    LOG("getSkillDescription: unknown key: %s\n", key.c_str());
    throw string("unknown key");
}

void Spell::castOn(PlayingGamestate *playing_gamestate, Character *source, Character *target) const {
    // source may be NULL, if caster is no longer alive
    // source may also equal the target, if casting on self
    // n.b., caller should have called useSpell()
    if( this->type == "attack" ) {
        bool success = true;
        if( mind_test ) {
            int a_stat = source->getProfileIntProperty(profile_key_M_c);
            int d_stat = target->getProfileIntProperty(profile_key_M_c);
            int mod_a_stat = source->modifyStatForDifficulty(playing_gamestate, a_stat);
            int mod_d_stat = target->modifyStatForDifficulty(playing_gamestate, d_stat);

            int hit_roll = rollDice(2, 6, -7);
            qDebug("spell mind test: %d vs %d (%d vs %d) : roll %d", a_stat, d_stat, mod_a_stat, mod_d_stat, hit_roll);
            if( hit_roll + mod_a_stat > mod_d_stat ) {
                // hits
                qDebug("    attacker wins");
            }
            else {
                success = false;
                qDebug("    defender wins");
            }
        }
        if( success ) {
            int damage = rollDice(rollX, rollY, rollZ);
            if( damage > 0 ) {
                qDebug("cast attack spell: %d; %d, %d", damage, damage_armour, damage_shield);
                if( target->decreaseHealth(playing_gamestate, damage, damage_armour, damage_shield) ) {
                    target->addPainTextEffect(playing_gamestate);
                    if( target->isDead() && source != NULL && source == playing_gamestate->getPlayer() ) {
                        source->addXP(playing_gamestate, target->getXPWorth());
                    }
                }
            }
            if( effect == "paralyse" ) {
                target->paralyse(10000);
                if( target == playing_gamestate->getPlayer() ) {
                    playing_gamestate->addTextEffect("The spell paralyses you!", playing_gamestate->getPlayer()->getPos(), 5000);
                }
            }
        }
    }
    else if( this->type == "heal" ) {
        int heal = rollDice(rollX, rollY, rollZ);
        if( heal > 0 ) {
            qDebug("cast heal spell: %d", heal);
            target->increaseHealth(heal);
        }
    }
    else {
        LOG("unknown spell type: %s\n", this->type.c_str());
        ASSERT_LOGGER(false);
    }
}

CharacterTemplate::CharacterTemplate(const string &animation_name, int FP, int BS, int S, int A, int M, int D, int B, float Sp, int health_min, int health_max, int gold_min, int gold_max) :
    profile(FP, BS, S, A, M, D, B, Sp), health_min(health_min), health_max(health_max),
    //has_natural_damage(false), natural_damageX(0), natural_damageY(0), natural_damageZ(0),
    natural_damageX(default_natural_damageX), natural_damageY(default_natural_damageY), natural_damageZ(default_natural_damageZ),
    can_fly(false), gold_min(gold_min), gold_max(gold_max), xp_worth(0), causes_terror(false), terror_effect(0), causes_disease(false), causes_paralysis(false), requires_magical(false), unholy(false), animation_name(animation_name), static_image(false), bounce(false), weapon_resist_percentage(50)
{
}

int CharacterTemplate::getTemplateHealth() const {
    if( health_min == health_max )
        return health_min;
    int r = rand() % (health_max - health_min + 1);
    return health_min + r;
}

int CharacterTemplate::getTemplateGold() const {
    if( gold_min == gold_max )
        return gold_min;
    int r = rand() % (gold_max - gold_min + 1);
    return gold_min + r;
}

ProfileEffect::ProfileEffect(const Profile &profile, int duration_ms) : profile(profile), expires_ms(0) {
    this->expires_ms = game_g->getGameTimeTotalMS() + duration_ms;
}

// this is not the only Character constructor!
Character::Character(const string &name, string animation_name, bool is_ai) :
    name(name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    is_fixed(false),
    animation_name(animation_name), static_image(false), bounce(false), weapon_resist_percentage(50),
    location(NULL), listener(NULL), listener_data(NULL),
    is_dead(false), time_of_death_ms(0), direction(Vector2D(1.0f, 0.0f)), has_charge_pos(false), is_visible(false),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), action(ACTION_NONE), has_charged(false), casting_spell(NULL), casting_spell_target(NULL), has_default_position(false), has_last_known_player_position(false), time_last_complex_update_ms(0),
    //FP(0), BS(0), S(0), A(0), M(0), D(0), B(0), Sp(0.0f),
    health(0), max_health(0),
    natural_damageX(default_natural_damageX), natural_damageY(default_natural_damageY), natural_damageZ(default_natural_damageZ),
    can_fly(false),
    is_paralysed(false), paralysed_until(0),
    is_diseased(false),
    initial_level(0),
    current_weapon(NULL), current_ammo(NULL), current_shield(NULL), current_armour(NULL), current_ring(NULL), gold(0), level(1), xp(0), xp_worth(0), causes_terror(false), terror_effect(0), done_terror(false), is_fleeing(false), causes_disease(0), causes_paralysis(0), requires_magical(false), unholy(false),
    can_talk(false), has_talked(false), interaction_xp(0), interaction_reward_gold(0), interaction_completed(false)
{
    // ensure we always have default properties set
    this->initialiseProfile(1, 0, 0, 0, 0, 0, 0, 0, 0.0f);
}

// this is not the only Character constructor!
Character::Character(const string &name, bool is_ai, const CharacterTemplate &character_template) :
    name(name),
    is_ai(is_ai), is_hostile(is_ai), // AI NPCs default to being hostile
    is_fixed(false),
    static_image(character_template.isStaticImage()),
    bounce(character_template.isBounce()), weapon_resist_class(character_template.getWeaponResistClass()), weapon_resist_percentage(character_template.getWeaponResistPercentage()),
    location(NULL), listener(NULL), listener_data(NULL),
    is_dead(false), time_of_death_ms(0), direction(Vector2D(1.0f, 0.0f)), has_charge_pos(false), is_visible(false),
    //has_destination(false),
    has_path(false),
    target_npc(NULL), time_last_action_ms(0), action(ACTION_NONE), has_charged(false), casting_spell(NULL), casting_spell_target(NULL), has_default_position(false), has_last_known_player_position(false), time_last_complex_update_ms(0),
    profile(*character_template.getProfile()),
    health(0), max_health(0),
    //natural_damageX(default_natural_damageX), natural_damageY(default_natural_damageY), natural_damageZ(default_natural_damageZ),
    natural_damageX(character_template.getNaturalDamageX()), natural_damageY(character_template.getNaturalDamageY()), natural_damageZ(character_template.getNaturalDamageZ()),
    can_fly(character_template.canFly()),
    is_paralysed(false), paralysed_until(0),
    is_diseased(false),
    initial_level(0),
    current_weapon(NULL), current_ammo(NULL), current_shield(NULL), current_armour(NULL), current_ring(NULL), gold(0), level(1), xp(0), xp_worth(0), causes_terror(false), terror_effect(0), done_terror(false), is_fleeing(false), causes_disease(0), causes_paralysis(0), requires_magical(false), unholy(false),
    can_talk(false), has_talked(false), interaction_xp(0), interaction_reward_gold(0), interaction_completed(false)
{
    this->animation_name = character_template.getAnimationName();
    this->type = character_template.getType();
    this->initialiseHealth( character_template.getTemplateHealth() );
    /*if( character_template.hasNaturalDamage() ) {
        character_template.getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);
    }*/
    this->gold = character_template.getTemplateGold();
    this->xp_worth = character_template.getXPWorth();
    this->causes_terror = character_template.getCausesTerror();
    this->terror_effect = character_template.getTerrorEffect();
    this->causes_disease = character_template.getCausesDisease();
    this->causes_paralysis = character_template.getCausesParalysis();
    this->requires_magical = character_template.requiresMagical();
    this->unholy = character_template.isUnholy();
    this->initial_level = this->level;
    this->initial_profile = this->profile;
}

Character::Character(const Character &character)
{
    qDebug("Character::Character(): copy constructor: %s", character.name.c_str());
    *this = character;

    this->listener = NULL;
    this->listener_data = NULL;

    this->current_weapon = NULL;
    this->current_armour = NULL;
    this->current_shield = NULL;
    this->current_ammo = NULL;
    this->current_ring = NULL;
    this->items.clear();
    for(set<Item *>::const_iterator iter = character.items.begin(); iter != character.items.end(); ++iter) {
        const Item *item = *iter;
        Item *item_copy = item->clone();
        this->items.insert(item_copy);
    }
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

int Character::findItemCount(const string &key) const {
    int count = 0;
    for(set<Item *>::const_iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        const Item *item = *iter;
        if( item->getKey() == key )
            count++;
    }
    return count;
}


Ammo *Character::findAmmo(const string &key) {
    if( this->current_ammo != NULL ) {
        if( this->current_ammo->getAmmoType() == key ) {
            return this->current_ammo;
        }
        this->current_ammo = NULL;
    }
    for(set<Item *>::iterator iter = this->items.begin(); iter != this->items.end(); ++iter) {
        Item *item = *iter;
        if( item->getType() == ITEMTYPE_AMMO ) {
            Ammo *ammo = static_cast<Ammo *>(item);
            if( ammo->getAmmoType() == key ) {
                return ammo;
            }
        }
    }
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
    //qDebug("key: %s", key.c_str());
    int value = this->getBaseProfileIntProperty(key);
    // effects from profile bonues
    for(vector<ProfileEffect>::const_iterator iter = this->profile_effects.begin(); iter != this->profile_effects.end(); ++iter) {
        const ProfileEffect &profile_effect = *iter;
        int effect_bonus = profile_effect.getProfile()->getIntProperty(key);
        //qDebug("    increase by %d\n", effect_bonus);
        value += effect_bonus;
    }
    // effect from items
    for(set<Item *>::const_iterator iter = this->items.begin();iter != this->items.end(); ++iter) {
        const Item *item = *iter;
        int item_bonus = item->getProfileBonusIntProperty(this, key);
        value += item_bonus;
    }
    if( this->is_diseased ) {
        // effect of disease
        if( key == profile_key_FP_c || key == profile_key_S_c ) {
            value--;
        }
    }
    if( key == profile_key_FP_c && this->current_weapon == NULL && !this->is_hostile && !this->hasSkill(skill_unarmed_combat_c) ) {
        // modifier for player being unarmed
        value -= 2;
    }
    if( key == profile_key_A_c && this->current_weapon != NULL && this->current_weapon->getWeaponClass() == "bow" && this->hasSkill(skill_fast_shooter_c) ) {
        value++;
    }
    if( key == profile_key_FP_c && this->current_shield != NULL && this->hasSkill(skill_shield_combat_c) ) {
        value++;
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
    for(set<Item *>::const_iterator iter = this->items.begin();iter != this->items.end(); ++iter) {
        const Item *item = *iter;
        float item_bonus = item->getProfileBonusFloatProperty(this, key);
        value += item_bonus;
    }
    if( key == profile_key_Sp_c && this->hasSkill(skill_sprint_c) && this->location != NULL && this->location->getGeoType() == Location::GEOTYPE_OUTDOORS ) {
        value += 0.2f;
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
    /*if( this == playing_gamestate->getPlayer() ) {
        qDebug("Character::update() for the player");
    }*/

    int elapsed_ms = game_g->getGameTimeTotalMS();

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

    if( elapsed_ms - this->time_last_complex_update_ms > 100 ) {
        //qDebug("complex update for: %s", this->name.c_str());
        this->time_last_complex_update_ms = elapsed_ms;

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
        Character *target = action == ACTION_CASTING ? casting_spell_target : target_npc;
        if( target != NULL && ( action == ACTION_CASTING || are_enemies ) ) {
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
            //qDebug("action: %d", action);
            //qDebug("hit_state: %d", hit_state);

            if( hit_state == HITSTATE_HAS_HIT || ( hit_state == HITSTATE_IS_NOT_HITTING && !is_fleeing ) ) {
                float dist = ( target->getPos() - this->getPos() ).magnitude();
                bool is_ranged = false; // also includes thrown weapons, and spell attacks
                const Spell *spell = NULL;
                Character *spell_target = NULL;
                if( hit_state == HITSTATE_HAS_HIT ) {
                    is_ranged = action == ACTION_FIRING || action == ACTION_CASTING;
                    if( action == ACTION_CASTING ) {
                        spell = casting_spell;
                        spell_target = casting_spell_target;
                    }
                }
                else {
                    if( this->health < 0.5f*this->max_health ) {
                        // try casting a healing spell
                        for(map<string, int>::const_iterator iter = this->spells.begin(); iter != this->spells.end() && spell == NULL; ++iter) {
                            if( iter->second > 0 ) {
                                string spell_name = iter->first;
                                const Spell *this_spell = playing_gamestate->findSpell(spell_name);
                                if( this_spell->getType() == "heal") {
                                    spell = this_spell;
                                    spell_target = this;
                                }
                            }
                        }
                    }
                    if( spell == NULL ) {
                        // try casting an attack spell
                        vector<const Spell *> candidate_spells;
                        for(map<string, int>::const_iterator iter = this->spells.begin(); iter != this->spells.end() && spell == NULL; ++iter) {
                            if( iter->second > 0 ) {
                                string spell_name = iter->first;
                                const Spell *this_spell = playing_gamestate->findSpell(spell_name);
                                if( this_spell->getType() == "attack") {
                                    candidate_spells.push_back(this_spell);
                                }
                            }
                        }
                        if( candidate_spells.size() > 0 ) {
                            int r = rand() % candidate_spells.size();
                            spell = candidate_spells.at(r);
                            spell_target = target_npc;
                        }
                    }
                    is_ranged = spell != NULL || ( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->isRangedOrThrown() );
                }
                bool can_hit = false;
                /* We could use the is_visible flag, but for future use we might want
                   to cater for Enemy NPCs shooting friendly NPCs.
                   This shouldn't be a performance issue, as this code is only
                   executed when firing/hitting, and not every frame.
                */
                if( spell != NULL ) {
                    if( spell_target == this || location->visibilityTest(this->pos, spell_target->getPos()) ) {
                        can_hit = true;
                    }
                }
                else if( is_ranged ) {
                    if( location->visibilityTest(this->pos, target_npc->getPos()) ) {
                        can_hit = true;
                    }
                }
                else {
                    can_hit = dist <= hit_range_c;
                }

                if( hit_state == HITSTATE_HAS_HIT ) {
                    bool casting_on_self = spell != NULL && spell_target == this;
                    if( can_hit && ( casting_on_self || !target_npc->is_dead ) ) {
                        ai_try_moving = false; // no point trying to move, just wait to hit again
                        if( action == ACTION_CASTING ) {
                            // make sure we still have the spell
                            ASSERT_LOGGER(casting_spell != NULL);
                            ASSERT_LOGGER(casting_spell_target != NULL);
                            if( this->getSpellCount(casting_spell->getName()) > 0 ) {
                                stringstream str;
                                str << "Casts ";
                                str << casting_spell->getName();
                                playing_gamestate->addTextEffect(str.str(), this->getPos(), 500);
                                this->useSpell(spell->getName());
                                if( this == casting_spell_target ) {
                                    // can cast straight away
                                    spell->castOn(playing_gamestate, this, casting_spell_target);
                                }
                                else {
                                    // fire off an action
                                    CharacterAction *action = CharacterAction::createSpellAction(playing_gamestate, this, casting_spell_target, casting_spell);
                                    playing_gamestate->addCharacterAction(action);
                                    playing_gamestate->playSound("spell_attack");
                                }
                            }
                        }
                        else {
                            Ammo *ammo = NULL;
                            if( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->getRequiresAmmo() ) {
                                // obtain the ammo - and make sure we still have some!
                                // note, it's better to use up the ammo now, rather than we first decide to fire, so that we still have the information on the ammo
                                string ammo_key = this->getCurrentWeapon()->getAmmoKey();
                                //Item *item = this->findItem(ammo_key);
                                ammo = this->findAmmo(ammo_key);
                                if( ammo == NULL ) {
                                    // run out of ammo!
                                    can_hit = false;
                                    // no need to change weapon, this will happen when Character next tries to hit
                                }
                                /*else {
                                    if( item->getType() != ITEMTYPE_AMMO ) {
                                        LOG("required ammo type %s is not ammo\n", item->getName().c_str());
                                        ASSERT_LOGGER( item->getType() == ITEMTYPE_AMMO );
                                    }
                                    ammo = static_cast<Ammo *>(item);
                                }*/
                            }

                            if( can_hit ) {
                                bool weapon_no_effect_magical = false;
                                bool weapon_no_effect_holy = false;
                                int weapon_damage = 0;
                                bool hits = RPGEngine::combat(weapon_damage, weapon_no_effect_magical, weapon_no_effect_holy, playing_gamestate, this, target_npc, is_ranged, ammo, has_charged);

                                if( is_ranged ) {
                                    // fire off an action
                                    string projectile_key;
                                    float projectile_icon_width = 0.0f;
                                    ASSERT_LOGGER( this->getCurrentWeapon() != NULL );
                                    if( this->getCurrentWeapon()->getWeaponType() == Weapon::WEAPONTYPE_RANGED ) {
                                        projectile_key = this->getCurrentWeapon()->getAmmoKey();
                                        const Item *projectile_item = playing_gamestate->getStandardItem(projectile_key);
                                        ASSERT_LOGGER( ammo != NULL );
                                        projectile_icon_width = ammo->getIconWidth();
                                    }
                                    else {
                                        projectile_key = this->getCurrentWeapon()->getKey();
                                        projectile_icon_width = this->getCurrentWeapon()->getIconWidth();
                                    }
                                    CharacterAction *action = CharacterAction::createProjectileAction(playing_gamestate, this, target_npc, hits, weapon_no_effect_magical, weapon_no_effect_holy, weapon_damage, projectile_key, projectile_icon_width);
                                    playing_gamestate->addCharacterAction(action);
                                }
                                else {
                                    // do it straight away
                                    if( hits ) {
                                        playing_gamestate->hitEnemy(this, target_npc, is_ranged, weapon_no_effect_magical, weapon_no_effect_holy, weapon_damage);
                                    }
                                }

                                if( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->getWeaponType() == Weapon::WEAPONTYPE_THROWN ) {
                                    // use up the weapon itself!
                                    Item *item = this->getCurrentWeapon();
                                    this->takeItem( item );
                                    delete item;
                                }
                                if( ammo != NULL ) {
                                    if( this->useAmmo(ammo) ) {
                                        ammo = NULL; // just to be safe, as pointer now deleted
                                        string ammo_key = this->getCurrentWeapon()->getAmmoKey();
                                        //if( this->findItem(ammo_key) == NULL && this == playing_gamestate->getPlayer() ) {
                                        if( this->findAmmo(ammo_key) == NULL && this == playing_gamestate->getPlayer() ) {
                                            // really has used up all available ammo
                                            LOG("Character %s has run out of ammo: %s\n", this->getName().c_str(), ammo_key.c_str());
                                            playing_gamestate->addTextEffect(PlayingGamestate::tr("Run out of").toStdString() + " " + ammo_key + "!", this->getPos(), 1000);
                                            this->armWeapon(NULL); // disarm it
                                        }
                                    }
                                }
                            }
                        }
                    }
                    this->setStateIdle();
                }
                else if( hit_state == HITSTATE_IS_NOT_HITTING ) {
                    if( can_hit ) {
                        ai_try_moving = false; // even if we can't hit yet, we should just wait until we can
                        int time_turn = getTimeTurn(spell != NULL, is_ranged);
                        if( elapsed_ms > time_last_action_ms + time_turn ) {
                            if( spell != NULL ) {
                                // cast spell!
                                action = ACTION_CASTING;
                                casting_spell = spell;
                                casting_spell_target = spell_target;
                                has_path = false;
                                time_last_action_ms = elapsed_ms;
                                if( this->listener != NULL ) {
                                    // we don't have an animation specific for spell-casting, but should still explicitly set to the idle animation, in case previously set to something else
                                    this->listener->characterSetAnimation(this, this->listener_data, "", false);
                                }
                            }
                            else {
                                // take a swing!
                                bool can_hit = true;
                                // make sure we have enough ammo (ammo is used up when we actually fire - see above)
                                if( this->getCurrentWeapon() != NULL && this->getCurrentWeapon()->getRequiresAmmo() ) {
                                    string ammo_key = this->getCurrentWeapon()->getAmmoKey();
                                    //Item *item = this->findItem(ammo_key);
                                    Ammo *item = this->findAmmo(ammo_key);
                                    if( item == NULL ) {
                                        if( this == playing_gamestate->getPlayer() ) {
                                            // this case occurs if the player arms a ranged weapon without having any ammo (as opposed to the check below, where we check for running out of ammo after firing)
                                            //LOG("Character %s has no ammo: %s\n", this->getName().c_str(), ammo_key.c_str());
                                            playing_gamestate->addTextEffect(PlayingGamestate::tr("Run out of").toStdString() + " " + ammo_key + "!", this->getPos(), 1000);
                                        }
                                        // for NPCs, this shouldn't happen, but put this check just in case!
                                        can_hit = false;
                                        this->armWeapon(NULL); // disarm it
                                    }
                                }
                                if( can_hit && this->tooWeakForWeapon() ) {
                                    can_hit = false;
                                    if( this == playing_gamestate->getPlayer() ) {
                                        playing_gamestate->addTextEffect(PlayingGamestate::tr("You are not strong enough to use this weapon!").toStdString(), this->getPos(), 1000);
                                        this->armWeapon(NULL); // disarm it
                                    }
                                }

                                if( can_hit ) {
                                    action = is_ranged ? ACTION_FIRING : ACTION_HITTING;
                                    qDebug("action is now: %d", action);
                                    has_path = false;
                                    time_last_action_ms = elapsed_ms;
                                    if( this->listener != NULL ) {
                                        string anim = is_ranged ? "ranged" : "attack";
                                        this->listener->characterSetAnimation(this, this->listener_data, anim, true);
                                        Vector2D dir = target_npc->getPos() - this->getPos();
                                        this->setDirection(dir);
                                    }
                                    playing_gamestate->playSound("swing");
                                    if( !is_ranged && has_charge_pos && (charge_pos - pos).magnitude() > 1.0f && this->hasSkill(skill_charge_c) ) {
                                        playing_gamestate->addTextEffect(PlayingGamestate::tr("Charge!").toStdString(), this->getPos(), 1000);
                                        // weapon damage increased if/when we actually hit
                                        has_charged = true;
                                        qDebug("has_charged");
                                        charge_pos = pos;
                                    }
                                    /*else {
                                        qDebug("is_ranged? %d", is_ranged);
                                        qDebug("has_charge_pos? %d", has_charge_pos);
                                        qDebug("charge_pos? %f, %f", charge_pos.x, charge_pos.y);
                                        qDebug("pos? %f, %f", pos.x, pos.y);
                                        qDebug("dist %f", (charge_pos - pos).magnitude());
                                    }*/
                                }
                            }
                        }
                    }
                }
            }
        }

        if( is_ai && action == ACTION_NONE && ai_try_moving ) {
            bool done_target = false;
            if( is_hostile && !is_fleeing && playing_gamestate->getPlayer() != NULL ) {
                if( this->is_visible ) {
                    this->setTargetNPC( playing_gamestate->getPlayer() );
                    Vector2D dest = playing_gamestate->getPlayer()->getPos();
                    this->setDestination(dest.x, dest.y, NULL);
                    done_target = true;
                    this->has_last_known_player_position = true;
                    this->last_known_player_position = dest;
                }
            }

            if( !done_target && is_fleeing ) {
                // run away!
                done_target = true;
                if( playing_gamestate->getPlayer() != NULL && !has_path ) {
                    /*Vector2D dir = this->getPos() - playing_gamestate->getPlayer()->getPos();
                    if( dir.magnitude() > 0 ) {
                        dir.normalise();
                        Vector2D new_pos = this->pos + dir*1.0f;
                        qDebug("### flee to %f, %f", new_pos.x, new_pos.y);
                        this->setDestination(new_pos.x, new_pos.y, NULL);
                        this->location->findFreeWayPoint()
                    }*/
                    Vector2D flee_pos;
                    //qDebug("findFreePoint");
                    if( this->location->findFleePoint(&flee_pos, this->pos, playing_gamestate->getPlayer()->getPos(), this->can_fly) ) {
                        //qDebug("### flee to %f, %f", flee_pos.x, flee_pos.y);
                        this->setDestination(flee_pos.x, flee_pos.y, NULL);
                    }
                    else {
                        // can't run away, return to fighting
                        this->is_fleeing = false;
                    }
                }
            }

            if( !done_target ) {
                this->setTargetNPC(NULL);

                if( this->has_last_known_player_position ) {
                    if( this->last_known_player_position == this->pos ) {
                        this->has_last_known_player_position = false;
                    }
                    else {
                        // move to player's last known position
                        this->setDestination(this->last_known_player_position.x, this->last_known_player_position.y, NULL);
                        done_target = true;
                    }
                }
            }

            if( !done_target ) {
                // return to default position
                if( this->has_default_position && this->default_position != this->pos ) {
                    this->setDestination(this->default_position.x, this->default_position.y, NULL);
                    done_target = true;
                }
                else if( this->has_path ) {
                    this->setStateIdle();
                }
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
        else if( this->path.size() > 0 ) {
            Vector2D dest = this->path.at(0);
            Vector2D diff = dest - this->pos;
            int time_ms = game_g->getGameTimeFrameMS();
            //float step = 0.002f * time_ms;
            //float step = (this->getSpeed() * time_ms)/1000.0f;
            float step = (this->getProfileFloatProperty(profile_key_Sp_c) * time_ms)/1000.0f;
            float dist = diff.magnitude();
            diff /= dist;
            Vector2D new_pos = pos;
            bool next_seg = false;
            if( step >= dist ) {
                /*new_pos = this->dest;
                this->setStateIdle();*/
                new_pos = dest;
                next_seg = true;
            }
            else {
                new_pos += diff * step;
            }
            if( location->collideWithTransient(this, new_pos) ) {
                is_fleeing = false; // stop NPC getting trapped when trying to flee
                if( this->path.size() == 1 && dist <= 2.0f*npc_radius_c + E_TOL_LINEAR ) {
                    // close enough, so stay where we are (to avoid aimlessly circling round a point that we can't reach
                    if( this->listener != NULL ) {
                        this->listener->characterSetAnimation(this, this->listener_data, "", false);
                    }
                }
                else {
                    Vector2D perp = diff.perpendicularYToX();
                    Vector2D p1 = pos + perp * step;
                    Vector2D p2 = pos - perp * step;

                    Vector2D hit_pos;
                    if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, p1, npc_radius_c, Location::INTERSECTTYPE_MOVE, NULL, this->can_fly) &&
                            !location->collideWithTransient(this, p1) ) {
                        // managed to avoid obstacle
                        if( this->listener != NULL ) {
                            this->listener->characterSetAnimation(this, this->listener_data, "run", false);
                        }
                        if( this == playing_gamestate->getPlayer() ) {
                            playing_gamestate->playSound("footsteps");
                        }
                        this->setPos(p1.x, p1.y);
                    }
                    else if( !location->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, p2, npc_radius_c, Location::INTERSECTTYPE_MOVE, NULL, this->can_fly) &&
                            !location->collideWithTransient(this, p2) ) {
                        // managed to avoid obstacle
                        if( this->listener != NULL ) {
                            this->listener->characterSetAnimation(this, this->listener_data, "run", false);
                        }
                        if( this == playing_gamestate->getPlayer() ) {
                            playing_gamestate->playSound("footsteps");
                        }
                        this->setPos(p2.x, p2.y);
                    }
                    else {
                        // can't move - so stay where we are
                        // though we don't modify the path, so we can continue if the obstacle moves
                        if( this->listener != NULL ) {
                            this->listener->characterSetAnimation(this, this->listener_data, "", false);
                        }
                    }
                }
            }
            else {
                if( this->listener != NULL ) {
                    this->listener->characterSetAnimation(this, this->listener_data, "run", false);
                }
                if( this == playing_gamestate->getPlayer() ) {
                    playing_gamestate->playSound("footsteps");
                }
                this->setPos(new_pos.x, new_pos.y);
                if( next_seg ) {
                    this->path.erase(this->path.begin());
                    if( this->path.size() == 0 ) {
                        if( this == playing_gamestate->getPlayer() && playing_gamestate->isKeyboardMoving() ) {
                            // don't set to idle, as player is still requesting keyboard movement
                            this->has_path = false;
                        }
                        else {
                            this->setStateIdle();
                        }
                    }
                }
            }
        }
    }

    //qDebug("Character::update() done: %s", this->name.c_str());
    return false;
}

void Character::handleSpecialHitEffects(PlayingGamestate *playing_gamestate, Character *target) const {
    // called when this character has hit the target character
    if( this->getCausesDisease() > 0 && !target->isDiseased() && !target->hasSkill(skill_disease_resistance_c) ) {
        int roll = rollDice(1, 100, 0);
        qDebug("roll for causing disease: %d vs %d", roll, this->getCausesDisease());
        if( roll < this->getCausesDisease() ) {
            // infect!
            target->setDiseased(true);
            if( target == playing_gamestate->getPlayer() ) {
                playing_gamestate->addTextEffect(PlayingGamestate::tr("You have been infected with a disease!").toStdString(), playing_gamestate->getPlayer()->getPos(), 5000);
            }
        }
    }
    if( this->getCausesParalysis() > 0 && !target->isParalysed() ) {
        // note, although we could paralyse someone who's already paralysed (the effect being to extend the length of paralysis), it seems fairer to the player to not do this, to avoid the risk of a player being unable to ever do anything!
        int roll = rollDice(1, 100, 0);
        qDebug("roll for causing paralysis: %d vs %d", roll, this->getCausesParalysis());
        if( roll < this->getCausesParalysis() ) {
            // paralyse!
            target->paralyse(5000);
            if( target == playing_gamestate->getPlayer() ) {
                playing_gamestate->addTextEffect(PlayingGamestate::tr("You are paralysed by the enemy!").toStdString(), playing_gamestate->getPlayer()->getPos(), 5000);
            }
        }
    }
}

int Character::getTimeTurn(bool is_casting, bool is_ranged) {
    int time_turn = base_time_turn_c;
    if( !is_casting ) {
        int action_time = is_ranged ? time_to_fire_c : time_to_hit_c;
        // note, increase attacks only speed up the delay between actions, not counting the action_time required to carry out the action
        if( time_turn > action_time ) { // should always be true, but just to be safe
            time_turn -= action_time;
            time_turn /= this->getProfileIntProperty(profile_key_A_c);
            time_turn += action_time;
        }
    }
    //qDebug("time turn: %d", time_turn);
    return time_turn;
}

void Character::setTargetNPC(Character *target_npc) {
    if( this->target_npc != target_npc ) {
        this->target_npc = target_npc;
        if( this->action != ACTION_NONE ) {
            this->action = ACTION_NONE;
            this->has_charged = false;
            qDebug("%s: setTargetNPC to different target: %d", this->name.c_str(), target_npc);
            if( this->listener != NULL ) {
                this->listener->characterSetAnimation(this, this->listener_data, "", true);
            }
        }
    }
}

void Character::notifyDead(const Character *character) {
    if( character == this->target_npc ) {
        this->setTargetNPC(NULL);
    }
    if( character == this->casting_spell_target )
        this->casting_spell_target = NULL;
}

void Character::addPainTextEffect(PlayingGamestate *playing_gamestate) const {
    string text;
    int r = rand() % 4;
    if( r == 0 )
        text = PlayingGamestate::tr("Argh!").toStdString();
    else if( r == 1 )
        text = PlayingGamestate::tr("Ow!").toStdString();
    else if( r == 2 )
        text = PlayingGamestate::tr("Ouch!").toStdString();
    else
        text = PlayingGamestate::tr("Eek!").toStdString();
    playing_gamestate->addTextEffect(text, this->getPos(), 500);
}

void Character::setStateIdle() {
    qDebug("Character::setStateIdle() for %s", this->getName().c_str());
    //has_destination = false;
    has_path = false;
    //is_hitting = false;
    action = ACTION_NONE;
    this->has_charged = false;
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
    qDebug("    natural damage: %d, %d, %d", natural_damageX, natural_damageY, natural_damageZ);
    int roll = rollDice(natural_damageX, natural_damageY, natural_damageZ);
    return roll;
}

int Character::getArmourRating(bool armour, bool shield) const {
    int armour_value = 0;
    if( armour && this->getCurrentArmour() != NULL )
        armour_value += this->getCurrentArmour()->getRating();
    if( shield && this->getCurrentShield() != NULL )
        armour_value++;
    return armour_value;
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

bool Character::decreaseHealth(PlayingGamestate *playing_gamestate, int decrease, bool armour, bool shield) {
    if( this->is_dead ) {
        LOG("tried to decreaseHealth of %s by %d - already dead!\n", this->getName().c_str(), decrease);
        ASSERT_LOGGER( !this->is_dead );
    }
    else if( decrease < 0 ) {
        LOG("decreaseHealth: received negative decrease: %d\n", decrease);
        ASSERT_LOGGER( decrease >= 0 );
    }
    int armour_value = this->getArmourRating(armour, shield);
    decrease -= armour_value;
    decrease = std::max(decrease, 0);
    if( this->health - decrease <= 0 && this->hasSkill(skill_luck_c) ) {
        if( rand() % 2 == 0 ) {
            decrease = 0;
            playing_gamestate->addTextEffect("Saved by luck", this->getPos(), 500);
        }
    }
    this->health -= decrease;
    if( health <= 0 ) {
        this->kill(playing_gamestate);
    }
    else if( this->is_ai && !this->is_fleeing && ( this->health <= 2 || this->health <= 0.1f*this->max_health ) ) {
        // NPC flees if fails a bravery test
        int r = rollDice(2, 6, 0);
        //r = 13;
        if( r > this->getProfileIntProperty(profile_key_B_c) ) {
            qDebug("NPC %s decides to flee", this->getName().c_str());
            this->is_fleeing = true;
        }
    }
    return (decrease > 0);
}

void Character::setDirection(Vector2D dir) {
    if( dir.magnitude() > 0.0f ) {
        dir.normalise();
        this->direction = dir;
        this->listener->characterTurn(this, this->listener_data);
    }
}

void Character::kill(PlayingGamestate *playing_gamestate) {
    this->health = 0;
    LOG("%s has died\n", this->getName().c_str());
    int elapsed_ms = game_g->getGameTimeTotalMS();
    this->is_dead = true;
    this->time_of_death_ms = elapsed_ms;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "death", false);
    }
    if( this->location != NULL ) {
        bool any_items = false;
        bool any_gold = false;
        while( this->items.size() > 0 ) {
            Item *item = *this->items.begin();
            this->dropItem(item);
            any_items = true;
        }
        if( this->gold > 0 ) {
            Currency *currency = playing_gamestate->cloneGoldItem(this->gold);
            location->addItem(currency, this->pos.x, this->pos.y);
            this->gold = 0;
            any_gold = true;
        }
        if( any_items || any_gold ) {
            playing_gamestate->addTextEffect(!any_items ? "Dropped some gold!" : "Dropped some items!", this->getPos(), 2000);
        }
    }
    if( this == playing_gamestate->getPlayer() ) {
        playing_gamestate->getView()->getGUIOverlay()->setFadeOut();
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

void Character::selectAmmo(Ammo *item) {
    // n.b., must be an item owned by Character!
    // set NULL to disarm
    if( this->current_ammo != item ) {
        this->current_ammo = item;
        // n.b., doesn't matter if the ammo is no longer suitable for the weapon, as we'll still find one that is when required in useAmmo()
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
    // set NULL to take off
    if( this->current_armour != item ) {
        this->current_armour = item;
        /*if( this->listener != NULL ) {
            this->listener->characterUpdateGraphics(this, this->listener_data);
        }*/
    }
}

void Character::wearRing(Ring *item) {
    // n.b., must be an item owned by Character!
    // set NULL to take off
    if( this->current_ring != item ) {
        this->current_ring = item;
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
        if( this->tooWeakForWeapon() ) {
            // too heavy to use, need to disarm again!
            this->armWeapon(NULL);
        }
    }
    // n.b., no need to automatically select ammo, as useAmmo() will find ammo when required
    if( auto_arm && this->current_shield == NULL && item->getType() == ITEMTYPE_SHIELD ) {
        // automatically arm shield
        this->armShield( static_cast<Shield *>(item) );
    }
    if( auto_arm && this->current_armour == NULL && item->getType() == ITEMTYPE_ARMOUR ) {
        // automatically wear armour
        //qDebug("wear armour: %s , %s", this->name.c_str(), item->getName().c_str());
        this->wearArmour( static_cast<Armour *>(item) );
        if( this->tooWeakForArmour() ) {
            // too heavy to wear, need to disarm again!
            this->wearArmour(NULL);
        }
    }
    // n.b., don't automatically wear rings
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
    if( this->current_ammo == item ) {
        this->current_ammo = NULL;
    }
    if( this->current_shield == item ) {
        this->current_shield = NULL;
        graphics_changed = true;
    }
    if( this->current_armour == item ) {
        this->current_armour = NULL;
        //graphics_changed = true;
    }
    if( this->current_ring == item ) {
        this->current_ring = NULL;
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

void Character::deleteItem(const string &key) {
    Item *item = this->findItem(key);
    if( item != NULL ) {
        this->takeItem(item);
        delete item;
    }
}

set<Item *>::iterator Character::itemsBegin() {
    return this->items.begin();
}

set<Item *>::const_iterator Character::itemsBegin() const {
    return this->items.begin();
}

set<Item *>::iterator Character::itemsEnd() {
    return this->items.end();
}

set<Item *>::const_iterator Character::itemsEnd() const {
    return this->items.end();
}

int Character::getItemCount() const {
    return this->items.size();
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
    this->paralysed_until = game_g->getGameTimeTotalMS() + time_ms;
    //qDebug("%d, %d", paralysed_until, time_ms);
}

void Character::setPath(vector<Vector2D> &path) {
    //qDebug("Character::setPath() for %s", this->getName().c_str());
    this->has_path = true;
    this->path = path;
    //this->is_hitting = false;
    this->action = ACTION_NONE;
    this->has_charged = false;
    if( this->listener != NULL ) {
        this->listener->characterSetAnimation(this, this->listener_data, "run", false);
    }
}

Vector2D Character::getDestination() const {
    if( this->has_path && path.size() > 0 ) {
        return path.at( path.size() - 1 );
    }
    ASSERT_LOGGER(false);
    return Vector2D();
}

void Character::setDestination(float xdest, float ydest, const void *ignore) {
    //qDebug("Character::setDestination(%f, %f) for %s , currently at %f, %f", xdest, ydest, this->getName().c_str(), this->pos.x, this->pos.y);
    /*if( is_fleeing ) {
        qDebug("Character::setDestination(%f, %f) for %s , currently at %f, %f", xdest, ydest, this->getName().c_str(), this->pos.x, this->pos.y);
    }*/
    if( this->location == NULL ) {
        LOG("can't set destination for character with NULL location");
        ASSERT_LOGGER( location != NULL );
    }

    Vector2D dest(xdest, ydest);

    vector<Vector2D> new_path = location->calculatePathTo(this->pos, dest, ignore, this->can_fly);
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

bool Character::carryingTooMuch() const {
    if( this->calculateItemsWeight() > this->getCanCarryWeight() ) {
        return true;
    }
    return false;
}

string Character::getWeightString() const {
    int weight = this->calculateItemsWeight();
    stringstream str;
    str << "Weight: ";
    str << weight;
    str << " / ";
    str << this->getCanCarryWeight();
    return str.str();
}

bool Character::tooWeakForArmour() const {
    if( this->getCurrentArmour() != NULL && this->getProfileIntProperty(profile_key_S_c) < this->getCurrentArmour()->getMinStrength() ) {
        return true;
    }
    return false;

}

bool Character::tooWeakForWeapon() const {
    if( this->getCurrentWeapon() != NULL && this->getProfileIntProperty(profile_key_S_c) < this->getCurrentWeapon()->getMinStrength() ) {
        return true;
    }
    return false;
}

bool Character::canMove() const {
    bool can_move = true;
    if( can_move && this->carryingTooMuch() ) {
        can_move = false;
    }
    if( can_move && this->getCurrentArmour() != NULL && this->getProfileIntProperty(profile_key_S_c) < this->getCurrentArmour()->getMinStrength() ) {
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
    /*int next_level_xp = this->getXPForNextLevel();
    if( xp >= next_level_xp ) {
        // we only advance one level at any given increase
        this->advanceLevel(playing_gamestate);
    }*/
    // levelling up is done in main loop (useful for cheat mode where we can advance several levels in one go - don't want to open several level up windows all at once!)
}

void Character::advanceLevel(PlayingGamestate *playing_gamestate) {
    /*if( level % 3 == 1 ) {
        this->changeBaseProfileIntProperty(profile_key_FP_c, 1);
        this->changeBaseProfileIntProperty(profile_key_M_c, 1);
    }
    else if( level % 3 == 2 ) {
        this->changeBaseProfileIntProperty(profile_key_BS_c, 1);
        this->changeBaseProfileIntProperty(profile_key_B_c, 1);
    }
    else if( level % 3 == 0 ) {
        this->changeBaseProfileIntProperty(profile_key_S_c, 1);
        this->changeBaseProfileIntProperty(profile_key_D_c, 1);
    }*/
    //qDebug("speed was: %f", this->getBaseProfileFloatProperty(profile_key_Sp_c));
    if( this->getBaseProfileFloatProperty(profile_key_Sp_c) < 2.5f - E_TOL_LINEAR ) {
        this->changeBaseProfileFloatProperty(profile_key_Sp_c, 0.1f);
    }
    else {
        this->changeBaseProfileFloatProperty(profile_key_Sp_c, 0.02f);
    }
    //qDebug("speed is now: %f", this->getBaseProfileFloatProperty(profile_key_Sp_c));
    int health_bonus = rollDice(1, 6, 0);
    this->increaseMaxHealth(health_bonus);

    this->level++;
    if( this == playing_gamestate->getPlayer() ) {
        LOG("player advances to level %d (xp %d)\n", level, xp);
        new LevelUpWindow(playing_gamestate);
        game_g->setPaused(true, true);
    }
}

int Character::getXPForNextLevel() const {
    int value = 100;
    //int value = 5;
    for(int i=0;i<level-1;i++) {
        value *= 2;
    }
    return value;
}

bool Character::canCompleteInteraction(PlayingGamestate *playing_gamestate) const {
    if( this->interaction_completed )
        return false;
    if( this->interaction_type == "WANT_OBJECT" ) {
        if( playing_gamestate->getPlayer()->findItem(this->interaction_data) != NULL ) {
            return true;
        }
    }
    else if( this->interaction_type == "KILL_NPCS" ) {
        bool all_dead = true;
        /*for(set<Character *>::iterator iter = playing_gamestate->getCLocation()->charactersBegin(); iter != playing_gamestate->getCLocation()->charactersEnd() && all_dead; ++iter) {
            Character *character = *iter;
            if( character != playing_gamestate->getPlayer() && character != this && character->getObjectiveId() == this->getInteractionData() && !character->isDead()) {
                //qDebug("not dead: %s at %f, %f, objective id %s equals %s", character->getName().c_str(), character->getX(), character->getY(), character->getObjectiveId().c_str(), this->getInteractionData().c_str());
                all_dead = false;
            }
        }*/
        for(vector<Location *>::const_iterator iter = playing_gamestate->getQuest()->locationsBegin(); iter != playing_gamestate->getQuest()->locationsEnd() && all_dead; ++iter) {
            const Location *location = *iter;
            for(set<Character *>::const_iterator iter = location->charactersBegin(); iter != location->charactersEnd() && all_dead; ++iter) {
                const Character *character = *iter;
                if( character != playing_gamestate->getPlayer() && character != this && character->getObjectiveId() == this->getInteractionData() && !character->isDead()) {
                    //qDebug("not dead: %s at %f, %f, objective id %s equals %s", character->getName().c_str(), character->getX(), character->getY(), character->getObjectiveId().c_str(), this->getInteractionData().c_str());
                    all_dead = false;
                }
            }
        }
        if( all_dead ) {
            return true;
        }
    }
    else {
        ASSERT_LOGGER(false);
    }
    return false;
}

void Character::completeInteraction(PlayingGamestate *playing_gamestate) {
    LOG("Character::completeInteration(): %s\n", this->name.c_str());
    ASSERT_LOGGER( !this->interaction_completed );
    if( this->interaction_type == "WANT_OBJECT" ) {
        Item *item = playing_gamestate->getPlayer()->findItem(this->interaction_data);
        ASSERT_LOGGER(item != NULL);
        if( item != NULL ) {
            playing_gamestate->getPlayer()->takeItem(item);
            delete item;
        }
    }
    else if( this->interaction_type == "WANT_OBJECT" ) {
        // no special code
    }
    else {
        ASSERT_LOGGER(false);
    }

    if( this->interaction_xp > 0 ) {
        playing_gamestate->getPlayer()->addXP(playing_gamestate, this->interaction_xp);
    }
    if( this->interaction_reward_item.length() > 0 ) {
        Item *item = this->findItem(this->interaction_reward_item);
        if( item != NULL ) {
            this->takeItem(item);
        }
        else {
            try {
                item = playing_gamestate->cloneStandardItem(this->interaction_reward_item);
            }
            catch(const string &err) {
                // catch it, as better than crashing at runtime if the data isn't correct
                LOG("### error, unknown interaction_reward_item: %s\n", interaction_reward_item.c_str());
                ASSERT_LOGGER(false);
            }
        }
        playing_gamestate->getPlayer()->addItem(item);
        stringstream str;
        str << "Received " << item->getName() << " as a reward";
        playing_gamestate->addTextEffect(str.str(), 2000);
    }
    if( this->interaction_reward_gold > 0 ) {
        playing_gamestate->getPlayer()->addGold(this->interaction_reward_gold);
        stringstream str;
        str << "Received " << this->interaction_reward_gold << " gold pieces as a reward";
        playing_gamestate->addTextEffect(str.str(), 2000);
    }
    if( this->interaction_set_flag.length() > 0 ) {
        LOG("add quest flag: %s\n", this->interaction_set_flag.c_str());
        playing_gamestate->getQuest()->addFlag(this->interaction_set_flag);
    }
    if( this->interaction_journal.length() > 0 ) {
        playing_gamestate->writeJournal("<hr/><p>");
        playing_gamestate->writeJournalDate();
        playing_gamestate->writeJournal(this->interaction_journal);
        playing_gamestate->writeJournal("</p>");
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

QString Character::writeStat(const string &stat_key, bool is_float, bool want_base) const {
    string visual_name = getProfileLongString(stat_key);
    QString html;
    html += "<tr>";
    html += "<td>";
    html += "<b>";
    html += visual_name.c_str();
    html += ":</b> ";
    html += "</td>";
    html += "<td>";
    if( is_float ) {
        float stat = this->getProfileFloatProperty(stat_key);
        float base_stat = this->getBaseProfileFloatProperty(stat_key);
        if( want_base )
            stat = base_stat;
        if( stat > base_stat ) {
            html += "<font color=\"#00ff00\">";
        }
        else if( stat < base_stat ) {
            html += "<font color=\"#ff0000\">";
        }
        html += QString::number(stat);
        if( stat != base_stat ) {
            html += "</font>";
        }
    }
    else {
        int stat = this->getProfileIntProperty(stat_key);
        int base_stat = this->getBaseProfileIntProperty(stat_key);
        if( want_base )
            stat = base_stat;
        if( stat > base_stat ) {
            html += "<font color=\"#00ff00\">";
        }
        else if( stat < base_stat ) {
            html += "<font color=\"#ff0000\">";
        }
        html += QString::number(stat);
        if( stat != base_stat ) {
            html += "</font>";
        }
    }
    //html += "<br/>";
    html += " ";
    html += "</td>";
    html += "</tr>";
    return html;
}

QString Character::writeSkills() const {
    QString html = "";
    bool any_skills = false;
    for(set<string>::const_iterator iter = this->skillsBegin(); iter != this->skillsEnd(); ++iter) {
        const string skill = *iter;
        if( !any_skills ) {
            any_skills = true;
            html += "<b>Skills:</b><br/><br/>";
        }
        html += "<i>";
        html += getSkillLongString(skill).c_str();
        html += ":</i> ";
        html += getSkillDescription(skill).c_str();
        html += "<br/>";
    }
    if( any_skills ) {
        html += "<br/>";
    }
    return html;
}
