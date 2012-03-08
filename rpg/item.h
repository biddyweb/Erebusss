#pragma once

#include <string>
using std::string;

enum ItemType {
    ITEMTYPE_GENERAL = 0,
    ITEMTYPE_WEAPON = 1,
    ITEMTYPE_SHIELD = 2,
    ITEMTYPE_ARMOUR = 3
};

class Item {
    string name;

public:
    Item(string name);
    virtual ~Item() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_GENERAL;
    }
    virtual Item *clone() const; // virtual copy constructor

    string getName() const {
        return this->name;
    }
};

class Weapon : public Item {
    string animation_filename;
public:
    Weapon(string name, string animation_filename);
    virtual ~Weapon() {
    }

    virtual ItemType getType() const {
        return ITEMTYPE_WEAPON;
    }
    virtual Weapon *clone() const; // virtual copy constructor

    string getAnimationFilename() const {
        return this->animation_filename;
    }
};
