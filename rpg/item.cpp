#include "item.h"
#include "character.h"
#include "../playinggamestate.h"
#include "../logiface.h"

#include <sstream>
using std::stringstream;

#ifdef _DEBUG
#include <cassert>
#endif

Item::Item(const string &name, const string &image_name, int weight) :
    name(name), image_name(image_name), user_data_gfx(NULL), icon_width(0.5f), weight(weight),
    arg1(0), arg2(0), rating(1), is_magical(false), worth_bonus(0)
{
}

Item *Item::clone() const {
    return new Item(*this);
}

bool Item::canUse() const {
    //return item_use != ITEMUSE_NONE;
    return use.length() > 0;
}

string Item::getUseVerb() const {
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

    if( this->use == "ITEMUSE_POTION_HEALING" ) {
        int amount = rollDice(this->rating, 6, 0);
        LOG("Character: %s drinks potion of healing, heal %d\n", character->getName().c_str(), amount);
        character->increaseHealth( amount );
        LOG("    health is now: %d\n", character->getHealth());
        playing_gamestate->addTextEffect(PlayingGamestate::tr("Gulp!").toStdString(), character->getPos(), 1000);
        if( character == playing_gamestate->getPlayer() ) {
            playing_gamestate->playSound("drink");
        }
        return true;
    }
    else if( this->use == "ITEMUSE_POTION_EFFECT" ) {
        LOG("Character: %s drinks potion, effect: %s\n", character->getName().c_str(), this->arg1_s.c_str());
        if( this->arg1_s == "cure_disease" ) {
            if( character->isDiseased() ) {
                character->setDiseased(false);
            }
        }
        else {
            // arg1_s gives statistic affect, rating gives increase, arg1 gives time in ms
            LOG("Character: %s drinks potion, change %s by %d for %d\n", character->getName().c_str(), this->arg1_s.c_str(), this->rating, this->arg1);
            Profile potion_profile;
            if( character->hasBaseProfileIntProperty(this->arg1_s) ) {
                potion_profile.setIntProperty(this->arg1_s, this->rating);
            }
            else if( character->hasBaseProfileFloatProperty(this->arg1_s) ) {
                potion_profile.setFloatProperty(this->arg1_s, static_cast<float>(this->rating));
            }
            else {
                LOG("### unknown property type!\n");
                ASSERT_LOGGER(false);
            }
            ProfileEffect profile_effect(potion_profile, this->arg1);
            character->addProfileEffect(profile_effect);
        }
        playing_gamestate->addTextEffect(PlayingGamestate::tr("Gulp!").toStdString(), character->getPos(), 1000);
        if( character == playing_gamestate->getPlayer() ) {
            playing_gamestate->playSound("drink");
        }
        return true;
    }
    else if( this->use == "ITEMUSE_HARM" ) {
        int amount = rollDice(this->rating, 6, 0);
        LOG("Character: %s uses harmful item, damage %d\n", character->getName().c_str(), amount);
        character->decreaseHealth(playing_gamestate, amount, false, false);
        LOG("    health is now: %d\n", character->getHealth());
        playing_gamestate->addTextEffect(PlayingGamestate::tr("Yuck!").toStdString(), character->getPos(), 1000);
        return true;
    }
    else if( this->use == "ITEMUSE_MUSHROOM" ) {
        int roll = rollDice(1, 6, 0);
        LOG("Character: %s eats mushroom, rolls %d\n", character->getName().c_str(), roll);
        if( roll <= 4 ) {
            // heals
            int amount = rollDice(1, 6, 0);
            LOG("    heal %d\n", amount);
            character->increaseHealth( amount );
            LOG("    health is now: %d\n", character->getHealth());
            playing_gamestate->addTextEffect(PlayingGamestate::tr("Yum!").toStdString(), character->getPos(), 1000);
        }
        else {
            // harms
            int amount = rollDice(1, 6, 0);
            LOG("    harm %d\n", amount);
            character->decreaseHealth(playing_gamestate, amount, false, false);
            LOG("    health is now: %d\n", character->getHealth());
            playing_gamestate->addTextEffect(PlayingGamestate::tr("Yuck!").toStdString(), character->getPos(), 1000);
        }
        return true;
    }
    else {
        //LOG("Item::use() unknown item_use: %d\n", this->item_use);
        LOG("Item::use() unknown item_use: %s\n", this->use.c_str());
        ASSERT_LOGGER(false);
    }
    return false;
}

string Item::getProfileBonusDescriptionInt(const string &key) const {
    int value = this->getRawProfileBonusIntProperty(key);
    if( value == 0 ) {
        return "";
    }
    stringstream str;
    str << "<b>" << key << " Bonus</b>: " << value << "<br/>";
    return str.str();
}

string Item::getProfileBonusDescriptionFloat(const string &key) const {
    float value = this->getRawProfileBonusFloatProperty(key);
    if( value == 0.0f ) {
        return "";
    }
    stringstream str;
    str << "<b>" << key << " Bonus</b>: " << value << "<br/>";
    return str.str();
}

string Item::getDetailedDescription() const {
    stringstream str;
    str << "<html><body>";
    str << "<h2>" << this->getName() << "</h2>";
    str << "<p>";
    if( this->getType() == ITEMTYPE_WEAPON ) {
        str << "<b>Type:</b> Weapon<br/>";
        const Weapon *weapon = static_cast<const Weapon *>(this);
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        if( damageZ != 0 ) {
            char sign = damageZ > 0 ? '+' : '-';
            str << "<b>Damage:</b> " << damageX << "D" << damageY << sign << abs(damageZ) << "<br/>";
        }
        else {
            str << "<b>Damage:</b> " << damageX << "D" << damageY << "<br/>";
        }
        str << "<b>Two Handed?:</b> " << (weapon->isTwoHanded() ? "Yes" : "No") << "<br/>";
        if( weapon->getMinStrength() > 0 ) {
            str << "<b>Min Strength:</b> " << weapon->getMinStrength() << "<br/>";
        }
        if( weapon->getWeaponType() == Weapon::WEAPONTYPE_HAND ) {
            str << "<b>Weapon Type:</b> Hand to Hand<br/>";
        }
        else if( weapon->getWeaponType() == Weapon::WEAPONTYPE_RANGED ) {
            str << "<b>Weapon Type:</b> Ranged<br/>";
            str << "<b>Ammo:</b> " << weapon->getAmmoKey() << "<br/>";
        }
        else if( weapon->getWeaponType() == Weapon::WEAPONTYPE_THROWN ) {
            str << "<b>Weapon Type:</b> Thrown<br/>";
        }
    }
    else if( this->getType() == ITEMTYPE_SHIELD ) {
        str << "<b>Type:</b> Shield<br/>";
        //const Shield *shield = static_cast<const Shield *>(this);
    }
    else if( this->getType() == ITEMTYPE_ARMOUR ) {
        str << "<b>Type:</b> Armour<br/>";
        const Armour *armour = static_cast<const Armour *>(this);
        if( armour->getMinStrength() > 0 ) {
            str << "<b>Min Strength:</b> " << armour->getMinStrength() << "<br/>";
        }
    }
    else if( this->getType() == ITEMTYPE_RING ) {
        str << "<b>Type:</b> Ring<br/>";
        //const Ring *ring = static_cast<const Ring *>(this);
    }
    else if( this->getType() == ITEMTYPE_AMMO ) {
        str << "<b>Type:</b> Ammo<br/>";
        const Ammo *ammo = static_cast<const Ammo *>(this);
        str << "<b>Base Ammo Type:</b> " << ammo->getAmmoType() << "<br/>";
        str << "<b>Amount:</b> " << ammo->getAmount() << "<br/>";
    }
    str << "<b>Weight: </b>" << this->getWeight() << "<br/>";
    str << "<b>Magical: </b>" << (this->isMagical() ? "Yes" : "No") << "<br/>";
    if( this->getRating() > 0 ) {
        str << "<b>Rating:</b> " << this->getRating() << "<br/>";
    }
    str << this->getProfileBonusDescriptionInt(profile_key_FP_c);
    str << this->getProfileBonusDescriptionInt(profile_key_BS_c);
    str << this->getProfileBonusDescriptionInt(profile_key_S_c);
    str << this->getProfileBonusDescriptionInt(profile_key_A_c);
    str << this->getProfileBonusDescriptionInt(profile_key_M_c);
    str << this->getProfileBonusDescriptionInt(profile_key_D_c);
    str << this->getProfileBonusDescriptionInt(profile_key_B_c);
    str << this->getProfileBonusDescriptionFloat(profile_key_Sp_c);
    str << "</p>";
    if( this->getDescription().length() > 0 ) {
        /*str << "<pre>";
        str << this->getDescription();
        str << "</pre>";*/
        /*QString desc = this->getDescription().c_str();
        //desc.replace(" ", "&nbsp;"); // commented out as causes text to no longer word-wrap!
        desc.replace("\n", "<br/>");
        str << desc.toStdString();*/
        str << this->getDescription();
        /*str << "initial text<br/>initial text with new line<br/>";
        //str << this->getDescription();
        QString desc = this->getDescription().c_str();
        //desc.replace(" ", "&nbsp;");
        desc.replace("\n", "<br/>");
        str << desc.toStdString();*/
    }
    str << "</body></html>";
    return str.str();
}

int Item::getRawProfileBonusIntProperty(const string &key) const {
    int value = this->profile_bonus.getIntProperty(key);
    return value;
}

float Item::getRawProfileBonusFloatProperty(const string &key) const {
    float value = this->profile_bonus.getFloatProperty(key);
    return value;
}

int Item::getProfileBonusIntProperty(const Character *character, const string &key) const {
    // default for item is that profile bonus is always active
    return this->getRawProfileBonusIntProperty(key);
}

float Item::getProfileBonusFloatProperty(const Character *character, const string &key) const {
    // default for item is that profile bonus is always active
    return this->getRawProfileBonusFloatProperty(key);
}

bool ItemCompare::operator()(const Item *lhs, const Item *rhs) const {
    // class that can be used for sorting
    // we prefer to sort by type, then by name
    // n.b., we can't simply add an operator< to Item class, as we typically sort lists of pointers to Item, rather than lists of Items
    if( lhs->getType() != rhs->getType() ) {
        return lhs->getType() < rhs->getType();
    }
    return lhs->getName() < rhs->getName();
}

Weapon::Weapon(const string &name, const string &image_name, int weight, const string &animation_name, int damageX, int damageY, int damageZ) :
    Item(name, image_name, weight), animation_name(animation_name),
    is_two_handed(false), weapon_type(WEAPONTYPE_HAND), requires_ammo(false),
    damageX(damageX), damageY(damageY), damageZ(damageZ),
    min_strength(0), unholy_only(false)
{
}

Weapon *Weapon::clone() const {
    return new Weapon(*this);
}

int Weapon::getDamage() const {
    int roll = rollDice(damageX, damageY, damageZ);
    return roll;
}

int Weapon::getProfileBonusIntProperty(const Character *character, const string &key) const {
    // profile bonus only active if weapon is armed
    if( this != character->getCurrentWeapon() )
        return 0;
    return Item::getProfileBonusIntProperty(character, key);
}

float Weapon::getProfileBonusFloatProperty(const Character *character, const string &key) const {
    // profile bonus only active if weapon is armed
    if( this != character->getCurrentWeapon() )
        return 0;
    return Item::getProfileBonusFloatProperty(character, key);
}

Shield::Shield(const string &name, const string &image_name, int weight, const string &animation_name) :
    Item(name, image_name, weight), animation_name(animation_name)
{
}

Shield *Shield::clone() const {
    return new Shield(*this);
}

int Shield::getProfileBonusIntProperty(const Character *character, const string &key) const {
    // profile bonus only active if shield is armed
    if( this != character->getCurrentShield() )
        return 0;
    return Item::getProfileBonusIntProperty(character, key);
}

float Shield::getProfileBonusFloatProperty(const Character *character, const string &key) const {
    // profile bonus only active if shield is armed
    if( this != character->getCurrentShield() )
        return 0;
    return Item::getProfileBonusFloatProperty(character, key);
}

Armour::Armour(const string &name, const string &image_name, int weight, int rating) :
    Item(name, image_name, weight),
    min_strength(0)
{
    this->rating = rating;
}

Armour *Armour::clone() const {
    return new Armour(*this);
}

int Armour::getProfileBonusIntProperty(const Character *character, const string &key) const {
    // profile bonus only active if armour is worn
    if( this != character->getCurrentArmour() )
        return 0;
    return Item::getProfileBonusIntProperty(character, key);
}

float Armour::getProfileBonusFloatProperty(const Character *character, const string &key) const {
    // profile bonus only active if armour is worn
    if( this != character->getCurrentArmour() )
        return 0;
    return Item::getProfileBonusFloatProperty(character, key);
}

Ring::Ring(const string &name, const string &image_name, int weight) :
    Item(name, image_name, weight)
{
}

Ring *Ring::clone() const {
    return new Ring(*this);
}

int Ring::getProfileBonusIntProperty(const Character *character, const string &key) const {
    // profile bonus only active if ring is worn
    if( this != character->getCurrentRing() )
        return 0;
    return Item::getProfileBonusIntProperty(character, key);
}

float Ring::getProfileBonusFloatProperty(const Character *character, const string &key) const {
    // profile bonus only active if ring is worn
    if( this != character->getCurrentRing() )
        return 0;
    return Item::getProfileBonusFloatProperty(character, key);
}

Ammo::Ammo(const string &name, const string &image_name, const string &ammo_type, const string &projectile_image_name, int weight, int amount) :
    Item(name, image_name, weight), ammo_type(ammo_type), projectile_image_name(projectile_image_name), amount(amount)
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

Shop::Shop(const string &name) : name(name), allow_random_npc(true), campaign(true) {
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
