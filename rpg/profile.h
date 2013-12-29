#pragma once

#include <string>
using std::string;

#include <map>
using std::map;

const string profile_key_FP_c = "FP";
const string profile_key_BS_c = "BS";
const string profile_key_S_c = "S";
const string profile_key_A_c = "A";
const string profile_key_M_c = "M";
const string profile_key_D_c = "D";
const string profile_key_B_c = "B";
const string profile_key_Sp_c = "Sp";

string getProfileLongString(const string &key);

class Profile {
    map<string, int> int_properties;
    map<string, float> float_properties;
public:

    Profile() {
        // create dummy profile - should call set() before reading any of the properties
    }
    Profile(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->set(FP, BS, S, A, M, D, B, Sp);
    }

    void setIntProperty(const string &key, int value) {
        this->int_properties[key] = value;
    }
    void setFloatProperty(const string &key, float value) {
        this->float_properties[key] = value;
    }
    void addIntProperty(const string &key, int value) {
        this->int_properties[key] += value;
    }
    void addFloatProperty(const string &key, float value) {
        this->float_properties[key] += value;
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

    // convenient wrappers to set all properties
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
    void add(int FP, int BS, int S, int A, int M, int D, int B, float Sp) {
        this->addIntProperty(profile_key_FP_c, FP);
        this->addIntProperty(profile_key_BS_c, BS);
        this->addIntProperty(profile_key_S_c, S);
        this->addIntProperty(profile_key_A_c, A);
        this->addIntProperty(profile_key_M_c, M);
        this->addIntProperty(profile_key_D_c, D);
        this->addIntProperty(profile_key_B_c, B);
        this->addFloatProperty(profile_key_Sp_c, Sp);
    }
};
