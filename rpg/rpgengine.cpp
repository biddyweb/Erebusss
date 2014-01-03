#include "rpgengine.h"
#include "profile.h"
#include "item.h"

string RPGEngine::getAttackerProfileKey(bool is_ranged, bool is_magical) {
    return is_ranged ? profile_key_BS_c : is_magical ? profile_key_M_c : profile_key_FP_c;
}

string RPGEngine::getDefenderProfileKey(bool is_ranged, bool is_magical) {
    return is_ranged ? profile_key_D_c : is_magical ? profile_key_M_c: profile_key_FP_c;
}

bool RPGEngine::combat(int &weapon_damage, bool &weapon_no_effect_magical, bool &weapon_no_effect_holy, PlayingGamestate *playing_gamestate, const Character *attacker, const Character *defender, bool is_ranged, const Ammo *ammo, bool has_charged) {
    bool is_magical = attacker->requiresMagical() || defender->requiresMagical();
    int a_stat = attacker->getProfileIntProperty( RPGEngine::getAttackerProfileKey(is_ranged, is_magical) );
    int d_stat = defender->getProfileIntProperty( RPGEngine::getDefenderProfileKey(is_ranged, is_magical) );
    int mod_a_stat = attacker->modifyStatForDifficulty(playing_gamestate, a_stat);
    int mod_d_stat = defender->modifyStatForDifficulty(playing_gamestate, d_stat);
    int a_str = attacker->getProfileIntProperty(profile_key_S_c);

    bool hits = false;
    weapon_no_effect_magical = false;
    weapon_no_effect_holy = false;
    weapon_damage = 0;

    int hit_roll = rollDice(2, 6, -7);
    qDebug("character %s rolled %d; %d vs %d to hit %s (ranged? %d)", attacker->getName().c_str(), hit_roll, mod_a_stat, mod_d_stat, defender->getName().c_str(), is_ranged);
    if( hit_roll + mod_a_stat > mod_d_stat ) {
        qDebug("    hit");
        hits = true;
        bool weapon_is_magical = attacker->getCurrentWeapon() != NULL && attacker->getCurrentWeapon()->isMagical();
        if( !weapon_is_magical && ammo != NULL && ammo->isMagical() ) {
            weapon_is_magical = true;
        }

        if( !weapon_is_magical && defender->requiresMagical() ) {
            // weapon has no effect!
            weapon_no_effect_magical = true;
        }
        else {
            if( attacker->getCurrentWeapon() != NULL && attacker->getCurrentWeapon()->isUnholyOnly() && !defender->isUnholy() ) {
                weapon_no_effect_holy = true;
            }
            else {
                weapon_damage = attacker->getCurrentWeapon() != NULL ? attacker->getCurrentWeapon()->getDamage() : attacker->getNaturalDamage();
                if( !is_ranged && rollDice(2, 6, 0) <= a_str ) {
                    qDebug("    extra strong hit!");
                    int extra_damage = rollDice(1, 3, 0);
                    weapon_damage += extra_damage;
                }
                if( !is_ranged && has_charged ) {
                    weapon_damage++;
                    qDebug("    extra damage from charge");
                    //playing_gamestate->addTextEffect(PlayingGamestate::tr("ping charge").toStdString(), attacker->getPos(), 1000);
                }
                if( !is_ranged && attacker->hasSkill(skill_hatred_orcs_c) && defender->getType() == "goblinoid" ) {
                    weapon_damage++;
                    qDebug("    extra damage from hatred of orcs");
                    //playing_gamestate->addTextEffect(PlayingGamestate::tr("hatred of orcs").toStdString(), attacker->getPos(), 1000);
                }
                if( ammo != NULL ) {
                    // -1 from rating, as default rating is 1, but this should mean no modification
                    weapon_damage += (ammo->getRating()-1);
                }
                if( attacker->getCurrentWeapon() != NULL && defender->getWeaponResistClass().length() > 0 && defender->getWeaponResistClass() == attacker->getCurrentWeapon()->getWeaponClass() ) {
                    qDebug("weapon resist percentage %d, scale from %d", defender->getWeaponResistPercentage(), weapon_damage);
                    weapon_damage = (weapon_damage * defender->getWeaponResistPercentage() )/100;
                }
                qDebug("weapon_damage %d", weapon_damage);
            }
        }
    }

    return hits;
}
