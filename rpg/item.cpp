#include "item.h"
#include "../game.h"

Item::Item(string name, Image *image) : name(name), image(image), user_data_gfx(NULL) {
}

Item::~Item() {
    if( image != NULL ) {
        delete image;
    }
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
