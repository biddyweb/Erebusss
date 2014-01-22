#pragma once

#include <string>
using std::string;

#include "character.h"

class RPGEngine {
public:
    static string getAttackerProfileKey(bool is_ranged, bool is_magical);
    static string getDefenderProfileKey(bool is_ranged, bool is_magical);
    static int getDamageBonusFromHatred(const Character *attacker, const Character *defender, bool is_ranged);
    static bool combat(int &weapon_damage, bool &weapon_no_effect_magical, bool &weapon_no_effect_holy, PlayingGamestate *playing_gamestate, const Character *attacker, const Character *defender, bool is_ranged, const Ammo *ammo, bool has_charged);
};
