#include "item.h"
#include "character.h"
#include "../game.h"

#include <sstream>
using std::stringstream;

Item::Item(string name, string image_name, int weight) :
    name(name), image_name(image_name), user_data_gfx(NULL), weight(weight),
    item_use(ITEMUSE_NONE), rating(1)
{
}

Item::~Item() {
}

Item *Item::clone() const {
    return new Item(*this);
}

string Item::getUseVerb() const {
    if( this->item_use == ITEMUSE_POTION_HEALING ) {
        return "Drink";
    }
    LOG("Item::getUseVerb() unknown item_use: %d\n", this->item_use);
    throw string("Unknown ItemUse type");
    return "";
}

bool Item::use(PlayingGamestate *playing_gamestate, Character *character) {
    // returns true if item used up
    // n.b., must be an item owned by Character!
    if( !this->canUse() ) {
        LOG("tried to use item that can't be used %s\n", this->getName().c_str());
        throw string("tried to use item that can't be used");
    }

    if( this->item_use == ITEMUSE_POTION_HEALING ) {
        LOG("Character: %s drinks potion of healing\n", character->getName().c_str());
        character->increaseHealth( this->rating );
        playing_gamestate->addTextEffect("Gulp!", character->getPos(), 1000);
        return true;
    }
    else {
        LOG("Item::use() unknown item_use: %d\n", this->item_use);
        throw string("Unknown ItemUse type");
    }
    return false;
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

string Currency::getName() const {
    stringstream currency_name;
    currency_name << value << " " << this->name;
    return currency_name.str();
}
