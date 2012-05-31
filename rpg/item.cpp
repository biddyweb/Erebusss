#include "item.h"
#include "character.h"
#include "../game.h"

#include <sstream>
using std::stringstream;

#ifdef _DEBUG
#include <cassert>
#endif

Item::Item(const string &name, const string &image_name, int weight) :
    name(name), image_name(image_name), user_data_gfx(NULL), icon_width(0.5f), weight(weight),
    /*item_use(ITEMUSE_NONE), */rating(1), is_magical(false)
{
}

Item::~Item() {
}

Item *Item::clone() const {
    return new Item(*this);
}

bool Item::canUse() const {
    //return item_use != ITEMUSE_NONE;
    return use.length() > 0;
}

string Item::getUseVerb() const {
    /*if( this->item_use == ITEMUSE_POTION_HEALING ) {
        return "Drink";
    }*/
    /*if( this->item_use == "ITEMUSE_POTION_HEALING" ) {
        return "Drink";
    }
    //LOG("Item::getUseVerb() unknown item_use: %d\n", this->item_use);
    LOG("Item::getUseVerb() unknown item_use: %s\n", this->item_use.c_str());
    throw string("Unknown ItemUse type");
    return "";*/
    if( use_verb.length() == 0 )
        return "Use";
    return use_verb;
}

bool Item::useItem(PlayingGamestate *playing_gamestate, Character *character) {
    // returns true if item used up
    // n.b., must be an item owned by Character!
    if( !this->canUse() ) {
        LOG("tried to use item that can't be used %s\n", this->getName().c_str());
        throw string("tried to use item that can't be used");
    }

    //if( this->item_use == ITEMUSE_POTION_HEALING ) {
    if( this->use == "ITEMUSE_POTION_HEALING" ) {
        int amount = rollDice(this->rating, 6, 0);
        LOG("Character: %s drinks potion of healing, heal %d\n", character->getName().c_str(), amount);
        character->increaseHealth( amount );
        LOG("    health is now: %d\n", character->getHealth());
        playing_gamestate->addTextEffect("Gulp!", character->getPos(), 1000);
        return true;
    }
    else {
        //LOG("Item::use() unknown item_use: %d\n", this->item_use);
        LOG("Item::use() unknown item_use: %s\n", this->use.c_str());
        throw string("Unknown ItemUse type");
    }
    return false;
}

Weapon::Weapon(const string &name, const string &image_name, int weight, const string &animation_name) :
    Item(name, image_name, weight), animation_name(animation_name),
    is_two_handed(false), is_ranged(false), requires_ammo(false)
{
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

Shield::Shield(const string &name, const string &image_name, int weight, const string &animation_name) :
    Item(name, image_name, weight), animation_name(animation_name)
{
}

Shield *Shield::clone() const {
    return new Shield(*this);
}

Armour::Armour(const string &name, const string &image_name, int weight, int rating) :
    Item(name, image_name, weight)
{
    this->rating = rating;
}

Armour *Armour::clone() const {
    return new Armour(*this);
}

Ammo::Ammo(const string &name, const string &image_name, const string &projectile_image_name, int amount) :
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

Currency::Currency(const string &name, const string &image_name) :
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

Shop::Shop(const string &name) : name(name) {
}

Shop::~Shop() {
    for(vector<const Item *>::const_iterator iter = items.begin(); iter != items.end(); ++iter) {
        const Item *item = *iter;
        delete item;
    }
}

void Shop::addItem(const Item *item, int cost) {
    this->items.push_back(item);
    this->costs.push_back(cost);
}

/*size_t Shop::getNItems() const {
    ASSERT_LOGGER( this->items.size() == this->costs.size() );
    return this->items.size();
}

const Item *Shop::getItem(int *cost, size_t i) const {
    ASSERT_LOGGER( this->items.size() == this->costs.size() );
    ASSERT_LOGGER( i < this->items.size() );
    *cost = this->costs.at(i);
    return this->items.at(i);
}*/
