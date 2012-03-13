#include "item.h"

Item::Item(string name) : name(name) {
}

Item *Item::clone() const {
    return new Item(*this);
}

Weapon::Weapon(string name, string animation_filename) : Item(name), animation_filename(animation_filename) {
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

Armour::Armour(string name, int rating) : Item(name), rating(rating) {
}

Armour *Armour::clone() const {
    return new Armour(*this);
}
