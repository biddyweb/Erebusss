#include "item.h"

Item::Item(string name, Image *image) : name(name), image(image) {
}

Item *Item::clone() const {
    return new Item(*this);
}

Weapon::Weapon(string name, Image *image, string animation_filename) : Item(name, image), animation_filename(animation_filename) {
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

Armour::Armour(string name, Image *image, int rating) : Item(name, image), rating(rating) {
}

Armour *Armour::clone() const {
    return new Armour(*this);
}
