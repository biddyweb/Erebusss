#include "item.h"

Item::Item(string name, string image_name) : name(name), image_name(image_name), user_data_gfx(NULL) {
}

Item::~Item() {
}

Item *Item::clone() const {
    return new Item(*this);
}

Weapon::Weapon(string name, string image_name, string animation_name) : Item(name, image_name), animation_name(animation_name) {
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

Shield::Shield(string name, string image_name, string animation_name) : Item(name, image_name), animation_name(animation_name) {
}

Shield *Shield::clone() const {
    return new Shield(*this);
}

Armour::Armour(string name, string image_name, int rating) : Item(name, image_name), rating(rating) {
}

Armour *Armour::clone() const {
    return new Armour(*this);
}

Currency::Currency(string name, string image_name) : Item(name, image_name) {
}

Currency *Currency::clone() const {
    return new Currency(*this);
}
