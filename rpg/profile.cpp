#include "profile.h"

#include "../logiface.h"

string getProfileLongString(const string &key) {
    if( key == profile_key_FP_c )
        return "Fighting Prowess";
    else if( key == profile_key_BS_c )
        return "Bow Skill";
    else if( key == profile_key_S_c )
        return "Strength";
    else if( key == profile_key_A_c )
        return "Attacks";
    else if( key == profile_key_M_c )
        return "Mind";
    else if( key == profile_key_D_c )
        return "Dexterity";
    else if( key == profile_key_B_c )
        return "Bravery";
    else if( key == profile_key_Sp_c )
        return "Speed";
    LOG("getLongString: unknown key: %s\n", key.c_str());
    throw string("unknown key");
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
