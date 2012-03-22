#include "item.h"

#include <sstream>
using std::stringstream;

Item::Item(string name, string image_name, int weight) :
    name(name), image_name(image_name), user_data_gfx(NULL), weight(weight)
{
}

Item::~Item() {
}

Item *Item::clone() const {
    return new Item(*this);
}

Weapon::Weapon(string name, string image_name, int weight, string animation_name) :
    Item(name, image_name, weight), animation_name(animation_name),
    is_two_handed(false), is_ranged(false), requires_ammo(false)
{
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

Shield::Shield(string name, string image_name, int weight, string animation_name) :
    Item(name, image_name, weight), animation_name(animation_name)
{
}

Shield *Shield::clone() const {
    return new Shield(*this);
}

Armour::Armour(string name, string image_name, int weight, int rating) :
    Item(name, image_name, weight), rating(rating)
{
}

Armour *Armour::clone() const {
    return new Armour(*this);
}

Ammo::Ammo(string name, string image_name, string projectile_image_name, int amount) :
    Item(name, image_name, 0), projectile_image_name(projectile_image_name), amount(amount)
{
}

Ammo *Ammo::clone() const {
    return new Ammo(*this);
}

string Ammo::getName() const {
    stringstream ammo_name;
    ammo_name << this->name << " (" << amount << ")";
    return ammo_name.str();
}

Currency::Currency(string name, string image_name) :
    Item(name, image_name, 0), value(0)
{
}

Currency *Currency::clone() const {
    return new Currency(*this);
}
