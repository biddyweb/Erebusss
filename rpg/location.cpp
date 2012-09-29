#include "location.h"
#include "character.h"
#include "item.h"
#include "../playinggamestate.h"
#include "../logiface.h"

#include <qdebug.h>

#include <algorithm>
using std::min;
using std::max;
using std::swap;

#include <ctime>

#ifdef _DEBUG
#include <cassert>
#endif

Scenery::Scenery(const string &name, const string &image_name, float width, float height) :
    location(NULL), name(name), image_name(image_name), user_data_gfx(NULL),
    is_blocking(false), blocks_visibility(false), is_door(false), is_exit(false), is_locked(false), draw_type(DRAWTYPE_NORMAL), opacity(1.0f), width(width), height(height),
    action_last_time(0), action_delay(0), action_value(0),
    interact_state(0),
    can_be_opened(false), opened(false)
{
}

void Scenery::setBlocking(bool is_blocking, bool blocks_visibility) {
    this->is_blocking = is_blocking;
    this->blocks_visibility = blocks_visibility;
    if( !this->is_blocking && this->blocks_visibility ) {
        throw string("scenery that blocks movement, but not visibility, is not supported");
    }
}

void Scenery::addItem(Item *item) {
    this->items.insert(item);
}

void Scenery::removeItem(Item *item) {
    this->items.erase(item);
}

void Scenery::setOpened(bool opened) {
    if( opened != this->opened ) {
        ASSERT_LOGGER( this->can_be_opened );
        this->opened = opened;
        if( this->location != NULL ) {
            this->location->updateScenery(this);
        }
    }
}

bool Scenery::isOn(const Character *character) const {
    Vector2D ch_pos = character->getPos();
    float hw = 0.5f * this->width;
    float hh = 0.5f * this->height;
    if( ch_pos.x >= this->pos.x - hw && ch_pos.x <= this->pos.x + hw &&
            ch_pos.y >= this->pos.y - hh && ch_pos.y <= this->pos.y +hh ) {
        return true;
    }
    return false;
}

vector<string> Scenery::getInteractionText(string *dialog_text) const {
    vector<string> options;
    if( this->interact_type == "INTERACT_TYPE_THRONE_FP" ) {
        *dialog_text = "One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of a knife, gripped by a fist.\n\nDo you wish to sit on the throne?";
        options.push_back("Yes, sit on the throne.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_GOLD" ) {
        *dialog_text = "One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of a gold coin.\n\nDo you wish to sit on the throne?";
        options.push_back("Yes, sit on the throne.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_M" ) {
        *dialog_text = "One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of an eye.\n\nDo you wish to sit on the throne?";
        options.push_back("Yes, sit on the throne.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_H" ) {
        *dialog_text = "One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of an tree.\n\nDo you wish to sit on the throne?";
        options.push_back("Yes, sit on the throne.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_SHRINE" ) {
        *dialog_text = "An old shrine, to some unknown forgotten diety. The wording on the stone has long since faded away. Do you wish to take a few moments to offer a prayer?";
        options.push_back("Yes, pray.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_BELL" ) {
        *dialog_text = "A large bell hangs here. Do you want to try ringing it?";
        options.push_back("Yes, ring the bell.");
        options.push_back("No.");
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER_EMPTY" ) {
        *dialog_text = "A large glass chamber filled with a murky liquid. As you look closely, you can see dark shapes floating inside, though you are unable to identify them.";
        options.push_back("Okay");
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER" ) {
        if( this->interact_state == 0 ) {
            *dialog_text = "A large glass chamber filled with a murky liquid. As you look closely, suddenly the shape of a figure appears! It speaks, in a quiet, drawn out voice - \"Please...,\" it begs to you, \"End my suffering\".\n\nYou see that the glass chamber has a panel at the bottom with two buttons, red and green. You could press one - although you could also try smashing the glass.";
            options.push_back("Press the red button.");
            options.push_back("Press the green button.");
            options.push_back("Smash the glass.");
            options.push_back("Leave the creature to suffer.");
        }
        else {
            *dialog_text = "A large glass chamber filled with a murky liquid. As you look closely, you can see dark shapes floating inside, though you are unable to identify them.";
            options.push_back("Okay");
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_PAINTING_SHATTER" ) {
        // no options - go straight to interaction
    }
    else {
        ASSERT_LOGGER(false);
    }
    return options;
}

void Scenery::interact(PlayingGamestate *playing_gamestate, int option) {
    //string dialog_title, result_text;
    string result_text, picture;
    if( this->interact_type == "INTERACT_TYPE_THRONE_FP" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            result_text = "As you sit, the chair buzzes, and you feel magical energy run into you. Your fighting prowess has increased!";
            int FP = playing_gamestate->getPlayer()->getFP();
            playing_gamestate->getPlayer()->setFP( FP + 1 );
        }
        else {
            result_text = "As you sit, you hear a click and feel something uncomfortable in your back. You look down in horror to see a knife protruding out from your chest, stained with your blood.\nThen everything goes dark...";
            playing_gamestate->getPlayer()->kill(playing_gamestate);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_GOLD" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            result_text = "As you sit, you suddenly feel something magically appear in your hands. Gold - 50 gold pieces!";
            playing_gamestate->getPlayer()->addGold(50);
        }
        else {
            if( playing_gamestate->getPlayer()->getGold() > 0 ) {
                result_text = "You sit, but nothing seems to happen this time. It is only later that you notice your gold has crumbled into a fine worthless dust!";
                playing_gamestate->getPlayer()->setGold(0);
            }
            else {
                result_text = "You sit, but nothing seems to happen this time.";
            }
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_M" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            result_text = "As you sit, you see a flash before your eyes. You have gained great intellectual insight!";
            int M = playing_gamestate->getPlayer()->getMind();
            playing_gamestate->getPlayer()->setMind( M + 1 );
        }
        else {
            result_text = "As you sit, you see a flash again before your eyes, but this time it is followed by darkness. You rub your eyes, but it remains. As you stand up, you realise you have been blinded!\nYour adventure ends here.";
            playing_gamestate->getPlayer()->kill(playing_gamestate);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_H" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            if( playing_gamestate->getPlayer()->getHealth() < playing_gamestate->getPlayer()->getMaxHealth() ) {
                this->interact_state = 1; // only set the state once the health benefit is used
                result_text = "As you sit, you feel energy rush into you, and you see your wounds magically close up before your eyes!";
                playing_gamestate->getPlayer()->restoreHealth();
            }
            else {
                result_text = "You sit, but nothing seems to happen.";
            }
        }
        else {
            result_text = "As you sit, you are suddenly gripped by a terrible pain over your entire body. Your watch in horror as old wounds open up before your eyes.";
            int damage = rollDice(10, 6, 0);
            playing_gamestate->getPlayer()->decreaseHealth(playing_gamestate, damage, false, false);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_SHRINE" ) {
        result_text = "You pray, but nothing seems to happen.";
        if( this->interact_state == 0 ) {
            int roll = rollDice(1, 3, 0);
            LOG("shrine: roll a %d\n", roll);
            this->interact_state = 1;
            if( roll == 1 ) {
                if( playing_gamestate->getPlayer()->getHealth() < playing_gamestate->getPlayer()->getMaxHealth() ) {
                    result_text = "You have been blessed with healing!";
                    playing_gamestate->getPlayer()->restoreHealth();
                }
            }
            else if( roll == 2 ) {
                result_text = "You are rewarded by the diety for your courage on this quest, with a gift of gold.";
                int gold = rollDice(3, 6, 0);
                playing_gamestate->getPlayer()->addGold(gold);
            }
            else if( roll == 3 ) {
                result_text = "You are granted the gift of wisdom by the diety.";
                playing_gamestate->getPlayer()->addXP(playing_gamestate, 100);
            }
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_BELL" ) {
        if( this->interact_state == 0 ) {
            int roll = rollDice(1, 3, 0);
            LOG("bell: roll a %d\n", roll);
            this->interact_state = 1;
            Character *enemy = NULL;
            if( roll == 1 ) {
                result_text = "You ring the bell, which makes an almighty clang. Suddenly a Goblin materialises out of thin air!";
                enemy = playing_gamestate->createCharacter("Goblin");
            }
            else if( roll == 2 ) {
                result_text = "You ring the bell, which makes an almighty clang. Suddenly an Orc materialises out of thin air!";
                enemy = playing_gamestate->createCharacter("Orc");
            }
            else if( roll == 3 ) {
                result_text = "You ring the bell, which makes an almighty clang. Suddenly a Goblin materialises out of thin air!";
                enemy = playing_gamestate->createCharacter("Goblin Mage");
                enemy->addSpell("Fire bolt", 6);
            }

            if( enemy != NULL ) {
                Location *c_location = playing_gamestate->getCLocation();
                c_location->addCharacter(enemy, this->pos.x + 2.0f, this->pos.y);
            }
        }
        else {
            result_text = "You ring the bell again, but nothing seems to happen this time.";
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER_EMPTY" ) {
        ASSERT_LOGGER(false);
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER" ) {
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            if( option == 0 ) {
                result_text = "The chamber flashes red. You hear a sudden but short scream from within, then it is over. At last, you have put the poor creature to rest.";
                playing_gamestate->getPlayer()->addXP(playing_gamestate, 100);
            }
            else if( option == 1 ) {
                result_text = "You hear strange whirring sounds. After some moments, the chamber starts to change different colours - green, then blue, then purple, and several more. As this happens, the creature inside starts to scream. The chamber starts to shake, causing a sound that becomes increasingly loud, though it is unable to cover the volume of the poor creature's screams. Eventually the sounds subside, and it seem the creature is dead.\n\nYou have at least granted its wish, though you wonder if there was a better way to do it.";
            }
            else if( option == 2 ) {
                result_text = "You smash the glass, and liquid drains out. Some of it spatters on you, causing you pain! The creature also screams as this happens, but then it seems the poor creature has died. At last, you have put the poor creature to rest.";
                int damage = rollDice(4, 10, 0);
                playing_gamestate->getPlayer()->decreaseHealth(playing_gamestate, damage, false, false);
                playing_gamestate->getPlayer()->addXP(playing_gamestate, 100);
            }
            else {
                ASSERT_LOGGER(false);
            }
        }
        else {
            ASSERT_LOGGER(false);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_PAINTING_SHATTER" ) {
        if( playing_gamestate->getPlayer()->getCurrentWeapon() != NULL ) {
            Item *item = playing_gamestate->getPlayer()->getCurrentWeapon();
            result_text = "As you cast your eyes on the painting, there is suddenly a smashing sound, and to your amazement, your " + item->getName() + " shatters!";
            playing_gamestate->getPlayer()->takeItem(item);
            delete item;
        }
        else {
            result_text = "You look at the interesting painting.";
        }
        picture = ":/gfx/scenes/painting_sword.jpg";
    }
    else {
        ASSERT_LOGGER(false);
    }

    playing_gamestate->showInfoDialog(result_text, picture);
}

Trap::Trap(const string &type, float width, float height) : type(type), width(width), height(height), rating(0), difficulty(0)
{
}

bool Trap::isSetOff(const Character *character) const {
    Vector2D ch_pos = character->getPos();
    if( ch_pos.x >= this->pos.x && ch_pos.x <= this->pos.x + this->width &&
            ch_pos.y >= this->pos.y && ch_pos.y <= this->pos.y + this->height ) {
        return true;
    }
    return false;
}

void Trap::setOff(PlayingGamestate *playing_gamestate, Character *character) const {
    int rollD = rollDice(2, 6, 0);
    LOG("character: %s has set off trap at %f, %f roll %d\n", character->getName().c_str(), this->getX(), this->getY(), rollD);
    string text;
    if( type == "arrow" ) {
        if( rollD + difficulty <= character->getDexterity() ) {
            LOG("avoided\n");
            text = "You have set off a trap!\nAn arrow shoots out from the wall,\nbut you manage to avoid it!";
        }
        else {
            LOG("affected\n");
            text = "You have set off a trap!\nAn arrow shoots out from the wall and hits you!";
            int damage = rollDice(2, 12, rating-1);
            character->decreaseHealth(playing_gamestate, damage, true, true);
        }
    }
    else if( type == "darts" ) {
        text = "You have set off a trap!\nDarts shoot out from the wall, hitting you multiple times!";
        int damage = rollDice(rating+1, 12, 0);
        character->decreaseHealth(playing_gamestate, damage, true, true);
    }
    else if( type == "acid" ) {
        if( character->getCurrentShield() != NULL ) {
            text = "You have set off a trap!\nAcid shoots out from jets in the walls!\nYour shield protects you, but is destroyed in the process.";
            Item *item = character->getCurrentShield();
            character->takeItem(item);
            delete item;
        }
        else {
            text = "You have set off a trap!\nA painful acid shoots out from jets in the walls,\nburning your flesh!";
            int damage = rollDice(4, 20, rating);
            character->decreaseHealth(playing_gamestate, damage, false, true);
        }
    }
    else if( type == "mantrap" ) {
        if( rollD + difficulty <= character->getDexterity() ) {
            LOG("avoided\n");
            text = "You manage to avoid the vicious bite of a mantrap that\nyou spot laying on the ground!";
        }
        else {
            LOG("affected\n");
            text = "You have set off a trap!\nYou feel agony in your leg, as you realise\nyou have stepped into a mantrap!";
            int damage = rollDice(4, 12, rating);
            character->decreaseHealth(playing_gamestate, damage, true, false);
        }
    }
    else if( type == "gas" ) {
        text = "You have set off a trap!\nGas shoots from jets in the wall, and suddenly you find you are paralysed!";
        character->paralyse((rating+1)*1000);
    }
    else if( type == "death" ) {
        text = "You have set off a trap!\nThe last thing you hear is a massive explosion in your ears!\nA bomb blows your body to pieces...";
        character->kill(playing_gamestate);
    }
    else {
        LOG("unknown trap: %s\n", type.c_str());
        ASSERT_LOGGER(false);
    }

    if( character == playing_gamestate->getPlayer() ) {
        playing_gamestate->addTextEffect(text, character->getPos(), 4000);
        playing_gamestate->playSound("click");
    }
}

void FloorRegion::insertPoint(size_t indx, Vector2D point) {
    Polygon2D::insertPoint(indx, point);
    EdgeType edge_type = edge_types.at(indx-1);
    this->edge_types.insert( this->edge_types.begin() + indx, edge_type );
    this->temp_marks.insert( this->temp_marks.begin() + indx, 0 );
}

FloorRegion *FloorRegion::createRectangle(float x, float y, float w, float h) {
    FloorRegion *floor_regions = new FloorRegion();
    floor_regions->addPoint(Vector2D(x, y));
    floor_regions->addPoint(Vector2D(x, y+h));
    floor_regions->addPoint(Vector2D(x+w, y+h));
    floor_regions->addPoint(Vector2D(x+w, y));
    return floor_regions;
}

Location::Location(const string &name) :
    name(name), type(TYPE_INDOORS), listener(NULL), listener_data(NULL),
    distance_graph(NULL)
{
}

Location::~Location() {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        delete floor_region;
    }
    for(set<Character *>::iterator iter = characters.begin(); iter != characters.end(); ++iter) {
        Character *character = *iter;
        delete character;
    }
    for(set<Item *>::iterator iter = items.begin(); iter != items.end(); ++iter) {
        Item *item = *iter;
        delete item;
    }
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        delete scenery;
    }
    if( distance_graph != NULL ) {
        delete distance_graph;
    }
}

void Location::addFloorRegion(FloorRegion *floorRegion) {
    this->floor_regions.push_back(floorRegion);
}

void Location::calculateSize(float *w, float *h) const {
    *w = 0.0f;
    *h = 0.0f;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        const FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            *w = max(*w, point.x);
            *h = max(*h, point.y);
        }
    }
}

FloorRegion *Location::findFloorRegionAt(Vector2D pos) {
    FloorRegion *result = NULL;
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end() && result==NULL; ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->pointInsideConvex(pos) ) {
            result = floor_region;
        }
    }
    return result;
}

void Location::addCharacter(Character *character, float xpos, float ypos) {
    character->setLocation(this);
    character->setPos(xpos, ypos);
    this->characters.insert(character);

    if( this->listener != NULL ) {
        this->listener->locationAddCharacter(this, character);
    }
}

void Location::removeCharacter(Character *character) {
    ASSERT_LOGGER(character->getLocation() == this);
    character->setStateIdle();
    character->setLocation(NULL);
    this->characters.erase(character);
}

bool Location::hasEnemies(const PlayingGamestate *playing_gamestate) const {
    bool has_enemies = false;
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !has_enemies; ++iter) {
        const Character *character = *iter;
        if( character->isHostile() ) {
            // n.b., we don't use the visibility test - so it isn't sufficient to just be out of sight, but we also need to be a sufficient distance
            float dist = (character->getPos() - playing_gamestate->getPlayer()->getPos()).magnitude();
            if( dist <= npc_visibility_c ) {
                has_enemies = true;
            }
        }
    }
    return has_enemies;
}

void Location::addItem(Item *item, float xpos, float ypos) {
    item->setPos(xpos, ypos);
    this->items.insert(item);

    FloorRegion *floor_region = this->findFloorRegionAt(item->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for item %s at %f, %f\n", item->getName().c_str(), item->getX(), item->getY());
        throw string("failed to find floor region for item");
    }
    floor_region->addItem(item);

    if( this->listener != NULL ) {
        this->listener->locationAddItem(this, item);
    }
}

void Location::removeItem(Item *item) {
    this->items.erase(item);

    FloorRegion *floor_region = this->findFloorRegionAt(item->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for item %s at %f, %f\n", item->getName().c_str(), item->getX(), item->getY());
        throw string("failed to find floor region for item");
    }
    floor_region->removeItem(item);

    if( this->listener != NULL ) {
        this->listener->locationRemoveItem(this, item);
    }
}

/** N.B., boundary for scenery should be created in createBoundariesForScenery(). We'll
  * need to update this function if we ever want to add scenery dynamically to a
  * location.
  */
void Location::addScenery(Scenery *scenery, float xpos, float ypos) {
    scenery->setLocation(this);
    scenery->setPos(xpos, ypos);
    this->scenerys.insert(scenery);

    FloorRegion *floor_region = this->findFloorRegionAt(scenery->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for scenery %s at %f, %f\n", scenery->getName().c_str(), scenery->getX(), scenery->getY());
        throw string("failed to find floor region for scenery");
    }
    /*LOG("scenery at %f, %f added to floor region:\n", scenery->getX(), scenery->getY());
    for(size_t i=0;i<floor_region->getNPoints();i++) {
        Vector2D p = floor_region->getPoint(i);
        LOG("    %d: %f, %f\n", i, p.x, p.y);
    }*/
    floor_region->addScenery(scenery);

    if( this->listener != NULL ) {
        this->listener->locationAddScenery(this, scenery);
    }
}

void Location::removeScenery(Scenery *scenery) {
    // remove corresponding boundary
    for(vector<Polygon2D>::iterator iter = this->boundaries.begin(); iter != this->boundaries.end();) {
        Polygon2D *boundary = &*iter;
        if( boundary->getSource() == scenery ) {
            boundary->setSourceType(0);
            boundary->setSource(NULL);
            iter = boundaries.erase(iter);
        }
        else {
            ++iter;
        }
    }

    scenery->setLocation(NULL);
    this->scenerys.erase(scenery);

    FloorRegion *floor_region = this->findFloorRegionAt(scenery->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for scenery %s at %f, %f\n", scenery->getName().c_str(), scenery->getX(), scenery->getY());
        throw string("failed to find floor region for scenery");
    }
    floor_region->removeScenery(scenery);

    if( this->listener != NULL ) {
        this->listener->locationRemoveScenery(this, scenery);
    }

    this->calculateDistanceGraph();
}

void Location::updateScenery(Scenery *scenery) {
    if( this->listener != NULL ) {
        this->listener->locationUpdateScenery(scenery);
    }
}

void Location::addTrap(Trap *trap, float xpos, float ypos) {
    //trap->setLocation(this);
    trap->setPos(xpos, ypos);
    this->traps.insert(trap);
}

void Location::removeTrap(Trap *trap) {
    //trap->setLocation(NULL);
    this->traps.erase(trap);
}

void Location::createBoundariesForRegions() {
    qDebug("Location::createBoundariesForRegions()");
    // imprint coi vertices
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint((j+1) % floor_region->getNPoints());
            //LOG("check edge %f, %f to %f, %f\n", p0.x, p0.y, p1.x, p1.y);
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            dp /= dp_length;
            for(vector<FloorRegion *>::const_iterator iter2 = floor_regions.begin(); iter2 != floor_regions.end(); ++iter2) {
                const FloorRegion *floor_region2 = *iter2;
                if( floor_region == floor_region2 ) {
                    continue;
                }
                for(size_t j2=0;j2<floor_region2->getNPoints();j2++) {
                    Vector2D p = floor_region2->getPoint(j2);
                    Vector2D imp_p = p;
                    imp_p.dropOnLine(p0, dp);
                    float dist_p = (imp_p - p).magnitude();
                    //LOG("    %f, %f is dist: %f\n", p.x, p.y, dist_p);
                    if( dist_p <= E_TOL_LINEAR ) {
                        float dot = ( p - p0 ) % dp;
                        if( dot >= E_TOL_LINEAR && dot <= dp_length - E_TOL_LINEAR ) {
                            // imprint coi vertex
                            //LOG("imprint between %f, %f to %f, %f at: %f, %f\n", p0.x, p0.y, p1.x, p1.y, imp_p.x, imp_p.y);
                            floor_region->insertPoint(j+1, imp_p);
                            p1 = imp_p;
                            dp_length = dot;
                        }
                    }
                }
            }
        }
    }

    LOG("calculate which boundary edges are internal\n");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint((j+1) % floor_region->getNPoints());
            //LOG("check if edge is internal: %f, %f to %f, %f\n", p0.x, p0.y, p1.x, p1.y);
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            dp /= dp_length;
            bool done = false; // only one other edge should be internal!
            for(vector<FloorRegion *>::const_iterator iter2 = floor_regions.begin(); iter2 != floor_regions.end() && !done; ++iter2) {
                FloorRegion *floor_region2 = *iter2;
                if( floor_region == floor_region2 ) {
                    continue;
                }
                for(size_t j2=0;j2<floor_region2->getNPoints() && !done;j2++) {
                    Vector2D p2 = floor_region2->getPoint(j2);
                    Vector2D p3 = floor_region2->getPoint((j2+1) % floor_region2->getNPoints());
                    // we only need to test for opposed, due to the way floor_region polygons should be ordered
                    float dist0 = (p0 - p3).magnitude();
                    float dist1 = (p1 - p2).magnitude();
                    //LOG("    compare to: %f, %f to %f, %f\n", p2.x, p2.y, p3.x, p3.y);
                    //LOG("    dists: %f, %f", dist0, dist1);
                    if( dist0 <= E_TOL_LINEAR && dist1 <= E_TOL_LINEAR ) {
                        //LOG("edge is internal with: %f, %f to %f, %f\n", p2.x, p2.y, p3.x, p3.y);
                        floor_region->setEdgeType(j, FloorRegion::EDGETYPE_INTERNAL);
                        floor_region2->setEdgeType(j2, FloorRegion::EDGETYPE_INTERNAL);
                    }
                }
            }
        }
    }

    LOG("find boundaries\n");
    for(;;) {
        //LOG("find an unmarked edge\n");
        FloorRegion *c_floor_region = NULL;
        int c_indx = -1;
        for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end() && c_floor_region==NULL; ++iter) {
            FloorRegion *floor_region = *iter;
            for(size_t j=0;j<floor_region->getNPoints() && c_floor_region==NULL;j++) {
                if( floor_region->getTempMark(j) == 0 && floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_EXTERNAL ) {
                    c_floor_region = floor_region;
                    c_indx = j;
                }
            }
        }
        if( c_floor_region == NULL ) {
            //LOG("all done\n");
            break;
        }
        FloorRegion *start_floor_region = c_floor_region;
        int start_indx = c_indx;
        Polygon2D boundary;
        boundary.setSourceType((int)SOURCETYPE_FLOOR);
        // first point actually added as last point at end
        for(;;) {
            int n_indx = (c_indx+1) % c_floor_region->getNPoints();
            Vector2D back_pvec = c_floor_region->getPoint( c_indx );
            Vector2D pvec = c_floor_region->getPoint( n_indx );
            //LOG("on edge %f, %f to %f, %f\n", back_pvec.x, back_pvec.y, pvec.x, pvec.y);
            if( c_floor_region->getTempMark(c_indx) != 0 ) {
                LOG("moved onto boundary already marked: %d, %d\n", c_floor_region, c_indx);
                throw string("moved onto boundary already marked");
            }
            boundary.addPoint(pvec);
            c_floor_region->setTempMark(c_indx, 1);
            if( c_floor_region == start_floor_region && n_indx == start_indx ) {
                break;
            }
            //LOG("move on to next edge\n");
            if( c_floor_region->getEdgeType(n_indx) == FloorRegion::EDGETYPE_EXTERNAL ) {
                c_indx = n_indx;
            }
            else {
                //LOG("need to find a new boundary\n");
                bool found = false;
                for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end() && !found; ++iter) {
                    FloorRegion *floor_region = *iter;
                    for(size_t j=0;j<floor_region->getNPoints() && !found;j++) {
                        if( floor_region->getTempMark(j) == 0 && floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_EXTERNAL ) {
                            // we only need to check the first pvec, as we want the edge that leaves this point
                            Vector2D o_pvec = floor_region->getPoint(j);
                            float dist = (pvec - o_pvec).magnitude();
                            if( dist <= E_TOL_LINEAR ) {
                                c_floor_region = floor_region;
                                c_indx = j;
                                found = true;
                            }
                        }
                    }
                }
                if( !found ) {
                    //LOG("can't find new boundary\n");
                    break;
                }
            }
        }
        this->addBoundary(boundary);
    }
    LOG("reset temp marks\n");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            floor_region->setTempMark(j, 0);
        }
    }
}

void Location::createBoundariesForScenery() {
    qDebug("Location::createBoundariesForScenery()");
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( !scenery->isBlocking() ) {
            continue;
        }
        Polygon2D boundary;
        boundary.setSourceType((int)SOURCETYPE_SCENERY);
        boundary.setSource(scenery);
        /*Vector2D pos = scenery->getPos();
        // clockwise, as "inside" should be on the left
        boundary.addPoint(pos);
        boundary.addPoint(pos + Vector2D(1.0f, 0.0f));
        boundary.addPoint(pos + Vector2D(1.0f, 1.0f));
        boundary.addPoint(pos + Vector2D(0.0f, 1.0f));*/
        float width = scenery->getWidth(), height = scenery->getHeight();
        Vector2D pos = scenery->getPos();
        //qDebug("scenery at %f, %f", pos.x, pos.y);
        // clockwise, as "inside" should be on the left
        Vector2D p0 = pos; p0.x -= 0.5f*width; p0.y -= 0.5f*height;
        Vector2D p1 = pos; p1.x += 0.5f*width; p1.y -= 0.5f*height;
        Vector2D p2 = pos; p2.x += 0.5f*width; p2.y += 0.5f*height;
        Vector2D p3 = pos; p3.x -= 0.5f*width; p3.y += 0.5f*height;
        if( this->findFloorRegionAt(p0) == NULL ) {
            LOG("can't find floor region for p0 at: %f, %f\n", p0.x, p0.y);
            LOG("scenery at %f, %f: %s\n", pos.x, pos.y, scenery->getName().c_str());
            throw string("can't find floor region for p0");
        }
        if( this->findFloorRegionAt(p1) == NULL ) {
            LOG("can't find floor region for p1 at: %f, %f\n", p1.x, p1.y);
            LOG("scenery at %f, %f: %s\n", pos.x, pos.y, scenery->getName().c_str());
            throw string("can't find floor region for p1");
        }
        if( this->findFloorRegionAt(p2) == NULL ) {
            LOG("can't find floor region for p2 at: %f, %f\n", p2.x, p2.y);
            LOG("scenery at %f, %f: %s\n", pos.x, pos.y, scenery->getName().c_str());
            throw string("can't find floor region for p2");
        }
        if( this->findFloorRegionAt(p3) == NULL ) {
            LOG("can't find floor region for p3 at: %f, %f\n", p3.x, p3.y);
            LOG("scenery at %f, %f: %s\n", pos.x, pos.y, scenery->getName().c_str());
            throw string("can't find floor region for p3");
        }
        boundary.addPoint(p0);
        boundary.addPoint(p1);
        boundary.addPoint(p2);
        boundary.addPoint(p3);
        this->addBoundary(boundary);
    }
    qDebug("    done");
}

void Location::intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const {
    Vector2D saved_p0 = p0; // for debugging
    Vector2D saved_p1 = p1;
    // transform into the space of the swept square
    p0 -= start;
    p0 = Vector2D( p0 % du, p0 % dv );
    p1 -= start;
    p1 = Vector2D( p1 % du, p1 % dv );
    Vector2D dp = p1 - p0;
    float dp_length = dp.magnitude();
    if( dp_length == 0.0f ) {
        return;
    }
    // n.b., dp is kept as non-unit vector
    Vector2D normal_from_wall = dp.perpendicularYToX(); // since walls are defined anti-clockwise, this vector must point away from the wall
    // n.b., normal_from_wall is also non-unit!!!
    //qDebug("candidate hit wall from %f, %f to %f, %f:", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y);
    //qDebug("    transformed to: %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
    //qDebug("    normal_from_wall = %f, %f", normal_from_wall.x, normal_from_wall.y);
    if( normal_from_wall.y >= 0.0f ) {
        return; // okay to move along or away from wall (if there are any intersections, it means we've already started from an intersection point, so best to allow us to move away!)
    }

    // trivial rejections:
    if( p0.x < xmin && p1.x < xmin )
        return;
    if( p0.x > xmax && p1.x > xmax )
        return;
    if( p0.y < ymin && p1.y < ymin )
        return;
    if( p0.y > ymax + width && p1.y > ymax + width )
        return;

    if( p0.y <= ymax || p1.y <= ymax ) {
        // check for collision with main 2D box

        // use Liang-Barsky Line Clipping to intersect line segment with axis-aligned 2D box
        // see http://www.cs.helsinki.fi/group/goa/viewing/leikkaus/intro.html
        // and http://www.skytopia.com/project/articles/compsci/clipping.html
        double tmin = 0.0f, tmax = 1.0f;
        const int n_edges_c = 4;
        // left, right, bottom, top
        bool has_t_value[n_edges_c] = {false, false, false, false};
        double t_value[n_edges_c] = {0.0f, 0.0f, 0.0f, 0.0f};
        if( p1.x != p0.x ) {
            has_t_value[0] = true;
            t_value[0] = ( xmin - p0.x ) / (double)( p1.x - p0.x );
            has_t_value[1] = true;
            t_value[1] = ( xmax - p0.x ) / (double)( p1.x - p0.x );
        }
        if( p1.y != p0.y ) {
            has_t_value[2] = true;
            t_value[2] = ( ymin - p0.y ) / (double)( p1.y - p0.y );
            has_t_value[3] = true;
            t_value[3] = ( ymax - p0.y ) / (double)( p1.y - p0.y );
        }
        Vector2D normals[n_edges_c];
        normals[0] = Vector2D(-1.0f, 0.0f);
        normals[1] = Vector2D(1.0f, 0.0f);
        normals[2] = Vector2D(0.0f, -1.0f);
        normals[3] = Vector2D(0.0f, 1.0f);
        for(int k=0;k<n_edges_c;k++) {
            if( !has_t_value[k] )
                continue;
            /*if( t_value[k] < tmin || t_value[k] > tmax )
                continue;*/
            bool entering = ( dp % normals[k] ) < 0.0f;
            //qDebug("    edge %d: t value %f, is entering? %d", k, t_value[k], entering?1:0);
            // modification from standard algorithm - we only want to clip if it's hit the main box!
            /*Vector2D ix = p0 + dp * t_value[k];
            if( ix.x < xmin || ix.x > xmax || ix.y < ymin || ix.y > ymax ) {
                continue;
            }*/
            //qDebug("        intersects against swept square");
            if( entering ) {
                if( t_value[k] > tmin )
                    tmin = t_value[k];
            }
            else {
                if( t_value[k] < tmax )
                    tmax = t_value[k];
            }
        }
        if( tmin <= tmax ) {
            // n.b., intersection positions still in the swept square space
            Vector2D i0 = p0 + dp * tmin;
            Vector2D i1 = p0 + dp * tmax;
            /*if( p0.x == 0.250071f && p0.y == 4.749929f && p1.x == 0.249929f && p1.y == 2.750071f )
            {
                qDebug("hit wall from %f, %f to %f, %f: parms %f, %f", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y, tmin, tmax);
                qDebug("    in swept square space: %f, %f and %f, %f", i0.x, i0.y, i1.x, i1.y);
            }*/
            float iy = min(i0.y, i1.y);
            iy -= width;
            iy = max(iy, 0.0f);
            if( iy > ymax + E_TOL_LINEAR ) {
                LOG("intersection is beyond end of swept box: %f, %f\n", iy, ymax);
                throw string("intersection is beyond end of swept box");
            }
            if( !(*hit) || iy < *hit_dist ) {
                *hit = true;
                *hit_dist = iy;
                //qDebug("    hit dist is now %f", *hit_dist);
                if( *hit_dist == 0.0f || !find_earliest )
                    *done = true; // no point doing anymore collisions
            }
        }
    }

    if( !(*done) ) {
        // check for collision with end circle
        Vector2D end(0.0f, ymax);
        Vector2D diff = end - p0;
        float dt = ( diff % dp ) / (dp_length*dp_length);
        /*qDebug("    saved: %f, %f to %f, %f", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y);
        qDebug("    dp: %f, %f", dp.x, dp.y);
        qDebug("    dt = %f", dt);*/
        if( dt >= 0.0f && dt <= 1.0f ) {
            Vector2D closest_pt = p0 + dp * dt; // point on line seg closest to end
            //qDebug("    closest_pt: %f, %f", closest_pt.x, closest_pt.y);
            if( closest_pt.y > ymax ) {
                double dist = (closest_pt - end).magnitude();
                //qDebug("    dist = %f", dist);
                if( dist <= width ) {
                    // we don't bother calculating the exact collision point - just move back far enough
                    float this_hit_dist = ymax - width;
                    this_hit_dist = max(this_hit_dist, 0.0f);
                    if( !(*hit) || this_hit_dist < *hit_dist ) {
                        *hit = true;
                        *hit_dist = this_hit_dist;
                        if( *hit_dist == 0.0f || !find_earliest )
                            *done = true; // no point doing anymore collisions
                    }
                }
            }
        }
        // n.b., only care about the closest point on the line seg, and not the end points of the line seg - as we'll look at the adjacent boundary segments to find the real closest point
        /*else {
            // need to look at ends of line seg!
            for(int i=0;i<2 && !(*done);i++) {
                Vector2D pt = i==0 ? p0 : p1;
                if( pt.y > ymax ) {
                    double dist = (pt - end).magnitude();
                    //qDebug("    dist = %f", dist);
                    if( dist <= width ) {
                        // we don't bother calculating the exact collision point - just move back far enough
                        float this_hit_dist = ymax - width;
                        this_hit_dist = max(this_hit_dist, 0.0f);
                        if( !(*hit) || this_hit_dist < *hit_dist ) {
                            *hit = true;
                            *hit_dist = this_hit_dist;
                            if( *hit_dist == 0.0f || !find_earliest )
                                *done = true; // no point doing anymore collisions
                        }
                    }
                }
            }
        }*/
    }
}

void Location::intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const Scenery *ignore_one_scenery, bool flying) const {
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end() && !(*done); ++iter) {
        const Polygon2D *boundary = &*iter;
        /*if( ignore_all_scenery && boundary->getSourceType() == (int)SOURCETYPE_SCENERY ) {
            continue;
        }*/
        if( boundary->getSourceType() == (int)SOURCETYPE_SCENERY ) {
            const Scenery *scenery = static_cast<const Scenery *>(boundary->getSource());
            if( intersect_type == INTERSECTTYPE_MOVE && !scenery->isBlocking() ) {
                continue;
            }
            else if( intersect_type == INTERSECTTYPE_VISIBILITY && !scenery->blocksVisibility() ) {
                continue;
            }
        }
        else if( boundary->getSourceType() == (int)SOURCETYPE_FLOOR ) {
            if( this->type == TYPE_OUTDOORS && ( flying || intersect_type == INTERSECTTYPE_VISIBILITY ) ) {
                continue;
            }
        }
        if( ignore_one_scenery != NULL && boundary->getSource() == ignore_one_scenery ) {
            continue;
        }
        for(size_t j=0;j<boundary->getNPoints() && !(*done);j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            intersectSweptSquareWithBoundarySeg(hit, hit_dist, done, find_earliest, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
        }
    }
}

bool Location::intersectSweptSquareWithBoundaries(Vector2D *hit_pos, bool find_earliest, Vector2D start, Vector2D end, float width, IntersectType intersect_type, const Scenery *ignore_one_scenery, bool flying) const {
    //LOG("Location::intersectSweptSquareWithBoundaries from %f, %f to %f, %f, width %f\n", start.x, start.y, end.x, end.y, width);
    bool done = false;
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        LOG("Location::intersectSweptSquareWithBoundaries received equal start and end\n");
        throw string("Location::intersectSweptSquareWithBoundaries received equal start and end");
    }
    dv /= dv_length;
    Vector2D du = dv.perpendicularYToX();
    //qDebug("du = %f, %f", du.x, du.y);
    //qDebug("dv = %f, %f", dv.x, dv.y);
    //qDebug("dv_length: %f", dv_length);
    float xmin = - width;
    float xmax = width;
    //float ymin = - width;
    float ymin = 0.0f;
    //float ymax = dv_length + width;
    float ymax = dv_length;

    intersectSweptSquareWithBoundaries(&done, &hit, &hit_dist, find_earliest, start, end, du, dv, width, xmin, xmax, ymin, ymax, intersect_type, ignore_one_scenery, flying);

    if( hit ) {
        *hit_pos = start + dv * hit_dist;
    }
    return hit;
}

Vector2D Location::nudgeToFreeSpace(Vector2D pos, float width) const {
    Vector2D saved_pos = pos; // saved for debugging
    bool nudged = false;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        for(size_t j=0;j<boundary->getNPoints();j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            // n.b., dp is kept as non-unit vector

            Vector2D diff = pos - p0;
            float dt = ( diff % dp ) / (dp_length*dp_length);
            if( dt >= 0.0f && dt <= 1.0f ) {
                Vector2D closest_pt = p0 + dp * dt; // point on line seg closest to end
                //double dist = (closest_pt - pos).magnitude();
                Vector2D normal_from_wall = dp.perpendicularYToX(); // since walls are defined anti-clockwise, this vector must point away from the wall
                normal_from_wall /= dp_length;
                double dist = (pos - closest_pt) % normal_from_wall;
                //qDebug("    dist = %f", dist);
                if( fabs(dist) <= width ) {
                    float move_dist = width - dist;
                    move_dist += E_TOL_LINEAR;
                    pos += normal_from_wall * move_dist;
                    /*qDebug("    nudged %f, %f to %f, %f (dist %f)", saved_pos.x, saved_pos.y, pos.x, pos.y, dist);
                    qDebug("        due to %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);*/
                    nudged = true;
                }
            }
            // n.b., only care about the closest point on the line seg, and not the end points of the line seg - as we'll look at the adjacent boundary segments to find the real closest point
        }
    }

    if( nudged ) {
        qDebug("nudged %f, %f to %f, %f", saved_pos.x, saved_pos.y, pos.x, pos.y);
    }
    return pos;
}

/*bool Location::intersectSweptSquareWithBoundariesAndNPCs(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width) const {
    bool done = false;
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        LOG("Location::intersectSweptSquareWithBoundaries received equal start and end\n");
        throw string("Location::intersectSweptSquareWithBoundaries received equal start and end");
    }
    dv /= dv_length;
    Vector2D du = dv.perpendicularYToX();
    //qDebug("du = %f, %f", du.x, du.y);
    //qDebug("dv = %f, %f", dv.x, dv.y);
    //qDebug("dv_length: %f", dv_length);
    float xmin = - width;
    float xmax = width;
    //float ymin = - width;
    float ymin = 0.0f;
    float ymax = dv_length + width;

    intersectSweptSquareWithBoundaries(&done, &hit, &hit_dist, start, end, du, dv, width, xmin, xmax, ymin, ymax, false, NULL);

    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !done; ++iter) {
        const Character *npc = *iter;
        //if( character == playing_gamestate->getPlayer() ) {
        if( npc == character ) {
            continue;
        }
        float npc_width = npc_radius_c;
        Vector2D npc_pos = npc->getPos();
        //qDebug("npc at %f, %f", npc_pos.x, npc_pos.y);
        // clockwise, as "inside" should be on the left
        Vector2D p0 = npc_pos; p0.x -= npc_width; p0.y -= npc_width;
        Vector2D p1 = npc_pos; p1.x += npc_width; p1.y -= npc_width;
        Vector2D p2 = npc_pos; p2.x += npc_width; p2.y += npc_width;
        Vector2D p3 = npc_pos; p3.x -= npc_width; p3.y += npc_width;
        intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p1, p2, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p2, p3, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p3, p0, start, du, dv, width, xmin, xmax, ymin, ymax);
    }

    if( hit ) {
        *hit_pos = start + dv * hit_dist;
    }
    return hit;
}*/

bool Location::collideWithTransient(const Character *character, Vector2D pos) const {
    // does collision detection with entities like NPCs, that do not have boundaries due to not having a permanent position (and hence can't be avoided in pathfinding)
    bool hit = false;
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !hit; ++iter) {
        const Character *npc = *iter;
        //if( character == playing_gamestate->getPlayer() ) {
        if( npc == character ) {
            continue;
        }
        Vector2D npc_pos = npc->getPos();
        float dist = (pos - npc_pos).magnitude();
        if( dist <= 2.0f * npc_radius_c ) {
            hit = true;
        }
    }
    return hit;
}

/*bool Location::pointInside(Vector2D point) const {
    // tests whether a point is within the valid location region
}*/

vector<Vector2D> Location::calculatePathWayPoints() const {
    qDebug("Location::calculatePathWayPoints()");
    vector<Vector2D> path_way_points;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        size_t n_points = boundary->getNPoints();
        for(size_t j=0;j<n_points;j++) {
            Vector2D p_point = boundary->getPoint((j+n_points-1) % n_points);
            Vector2D point = boundary->getPoint(j);
            Vector2D n_point = boundary->getPoint((j+1) % n_points);
            Vector2D d0 = point - p_point;
            Vector2D d1 = n_point - point;
            if( d0.magnitude() < E_TOL_LINEAR || d1.magnitude() < E_TOL_LINEAR ) {
                LOG("p_point: %f, %f\n", p_point.x, p_point.y);
                LOG("point: %f, %f\n", point.x, point.y);
                LOG("n_point: %f, %f\n", n_point.x, n_point.y);
                throw string("coi boundary points not allowed");
            }
            d0.normalise();
            d1.normalise();
            // these vectors must point away from the wall
            Vector2D normal_from_wall0 = d0.perpendicularYToX();
            Vector2D normal_from_wall1 = d1.perpendicularYToX();
            Vector2D inwards = normal_from_wall0 + normal_from_wall1;
            if( inwards.magnitude() > E_TOL_MACHINE ) {
                float angle = acos(normal_from_wall0 % normal_from_wall1);
                float ratio = cos( 0.5f * angle );
                inwards.normalise();
                //const float offset = npc_radius_c/ratio;
                const float offset = npc_radius_c/ratio + E_TOL_LINEAR;
                Vector2D path_way_point = point + inwards * offset;

                // test we get get to the way point
                // important to use E_TOL_LINEAR for the width, otherwise can cause problems for scenery right next to the wall
                Vector2D hit_pos;
                if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, point, path_way_point, E_TOL_LINEAR, Location::INTERSECTTYPE_MOVE, NULL, false) ) {
                    path_way_points.push_back( path_way_point );
                }
            }
        }
    }

    return path_way_points;
}

/*void Location::refreshDistanceGraph() {
    LOG("Location::refreshDistanceGraph()\n");
}*/

void Location::calculateDistanceGraph() {
    qDebug("Location::calculateDistanceGraph()");
    int time_s = clock();
    if( this->distance_graph != NULL ) {
        delete this->distance_graph;
    }
    this->distance_graph = new Graph();

    vector<Vector2D> path_way_points = this->calculatePathWayPoints();
    //LOG("Location::calculateDistanceGraph() calculatePathWayPoints() time taken: %d\n", clock() - time_s);

    for(size_t i=0;i<path_way_points.size();i++) {
        Vector2D A = path_way_points.at(i);
        GraphVertex vertex(A, NULL);
        this->distance_graph->addVertex(vertex);
    }

    for(size_t i=0;i<this->distance_graph->getNVertices();i++) {
        GraphVertex *v_A = this->distance_graph->getVertex(i);
        Vector2D A = v_A->getPos();
        for(size_t j=i+1;j<this->distance_graph->getNVertices();j++) {
            GraphVertex *v_B = this->distance_graph->getVertex(j);
            Vector2D B = v_B->getPos();
            Vector2D hit_pos;
            float dist = (A - B).magnitude();
            bool hit = false;
            if( dist <= E_TOL_LINEAR ) {
                // needed for coi way points?
                hit = false;
            }
            else {
                hit = this->intersectSweptSquareWithBoundaries(&hit_pos, false, A, B, npc_radius_c, INTERSECTTYPE_MOVE, NULL, false);
            }
            if( !hit ) {
                v_A->addNeighbour(j, dist);
                v_B->addNeighbour(i, dist);
            }
        }
    }
    qDebug("Location::calculateDistanceGraph() total time taken: %d", clock() - time_s);
}

bool Location::testVisibility(Vector2D pos, const FloorRegion *floor_region, size_t j) const {
    //Vector2D point = floor_region->getPoint(j);
    Vector2D point = floor_region->offsetInwards(j, 2.0f*E_TOL_LINEAR); // offset, so we don't collide with the floor region we've sampled from!
    // test we get get to the way point
    Vector2D hit_pos;
    // use E_TOL_LINEAR, to avoid line of sight slipping between two adjacent items
    if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, point, E_TOL_LINEAR, INTERSECTTYPE_VISIBILITY, NULL, false) ) {
        return true;
    }
    return false;
}

void Location::initVisibility(Vector2D pos) {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints() && !floor_region->isVisible();j++) {
            /*Vector2D point = floor_region->getPoint(j);
            // test we get get to the way point
            Vector2D hit_pos;
            // use E_TOL_LINEAR, to avoid line of sight slipping between two adjacent items
            if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, point, E_TOL_LINEAR, INTERSECTTYPE_VISIBILITY, NULL) ) {
                floor_region->setVisible(true);
            }*/
            if( this->testVisibility(pos, floor_region, j) ) {
                floor_region->setVisible(true);
            }
        }
    }
}

vector<FloorRegion *> Location::updateVisibility(Vector2D pos) {
    //LOG("Location::updateVisibility for %f, %f\n", pos.x, pos.y);
    vector<FloorRegion *> update_floor_regions;
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints() && !floor_region->isVisible();j++) {
            Vector2D point = floor_region->getPoint(j);
            int p_j = j==0 ? floor_region->getNPoints()-1 : j-1;
            // now only need to check for points on internal edges
            if( floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_INTERNAL || floor_region->getEdgeType(p_j) == FloorRegion::EDGETYPE_INTERNAL )
            {
                /*
                // test we get get to the way point
                Vector2D hit_pos;
                // use E_TOL_LINEAR, to avoid line of sight slipping between two adjacent items
                if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, point, E_TOL_LINEAR, INTERSECTTYPE_VISIBILITY, NULL) ) {
                    floor_region->setVisible(true);
                    update_floor_regions.push_back(floor_region);
                }*/
                if( this->testVisibility(pos, floor_region, j) ) {
                    floor_region->setVisible(true);
                    update_floor_regions.push_back(floor_region);
                }
            }
        }
    }
    return update_floor_regions;
}

QuestObjective::QuestObjective(const string &type, const string &arg1) : type(type), arg1(arg1) {
}

bool QuestObjective::testIfComplete(const Quest *quest) const {
    bool complete = false;
    if( type == "kill" ) {
        if( arg1 == "all_hostile" ) {
            complete = true;
            for(vector<Location *>::const_iterator iter = quest->locationsBegin(); iter != quest->locationsEnd() && complete; ++iter) {
                const Location *location = *iter;
                for(set<Character *>::const_iterator iter2 = location->charactersBegin(); iter2 != location->charactersEnd() && complete; ++iter2) {
                    const Character *character = *iter2;
                    if( character->isHostile() && !character->isDead() ) {
                        complete = false;
                    }
                }
            }
        }
        else {
            LOG("unknown arg1: %s\n", arg1.c_str());
            ASSERT_LOGGER(false);
        }
    }
    else {
        LOG("unknown type: %s\n", type.c_str());
        ASSERT_LOGGER(false);
    }
    return complete;
}

Quest::Quest() : quest_objective(NULL), is_completed(false) {
}

Quest::~Quest() {
    if( quest_objective != NULL ) {
        delete quest_objective;
    }
    for(vector<Location *>::iterator iter = this->locations.begin(); iter != this->locations.end(); ++iter) {
        Location *location = *iter;
        delete location;
    }
}

void Quest::addLocation(Location *location) {
    this->locations.push_back(location);
}

Location *Quest::findLocation(const string &name) {
    for(vector<Location *>::iterator iter = this->locations.begin(); iter != this->locations.end(); ++iter) {
        Location *location = *iter;
        if( location->getName() == name ) {
            return location;
        }
    }
    return NULL;
}

bool Quest::testIfComplete() {
    ASSERT_LOGGER( !is_completed ); // if already complete, shouldn't be asking
    if( quest_objective != NULL ) {
        this->is_completed = quest_objective->testIfComplete(this);
        return this->is_completed;
    }
    return false;
}

QuestInfo::QuestInfo(const string &filename) : filename(filename) {
}
