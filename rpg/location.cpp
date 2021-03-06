#include <qdebug.h>

#include <algorithm>
using std::min;
using std::max;
using std::swap;

#ifdef _DEBUG
#include <cassert>
#endif

#include "location.h"
#include "character.h"
#include "item.h"

#include "../playinggamestate.h"
#include "../logiface.h"

Scenery::Scenery(const string &name, const string &image_name, float width, float height, float visual_height, bool boundary_iso, float boundary_iso_ratio) :
    location(NULL), name(name), image_name(image_name),
    is_blocking(false), blocks_visibility(false), is_door(false), is_exit(false), exit_travel_time(0), is_locked(false), locked_silent(false), locked_used_up(false), key_always_needed(false), unlock_xp(20),
    draw_type(DRAWTYPE_NORMAL), opacity(1.0f), has_smoke(false), width(width), height(height), visual_height(visual_height), boundary_iso(boundary_iso), boundary_iso_ratio(boundary_iso_ratio),
    action_last_time(0), action_delay(0), action_value(0),
    interact_state(0),
    can_be_opened(false), opened(false),
    trap(NULL)
{
}

Scenery::~Scenery() {
    for(set<Item *>::iterator iter = items.begin(); iter != items.end(); ++iter) {
        Item *item = *iter;
        delete item;
    }
    if( trap != NULL ) {
        delete trap;
    }
}

Scenery *Scenery::clone() const {
    return new Scenery(*this);
}

void Scenery::setPos(float xpos, float ypos) {
    this->pos.set(xpos, ypos);

    // set boundary polygons
    boundary_base = Polygon2D();
    boundary_visual = Polygon2D();
    // clockwise, as "inside" should be on the left
    Vector2D p0 = pos; p0.x -= 0.5f*width; p0.y -= 0.5f*height;
    Vector2D p1 = pos; p1.x += 0.5f*width; p1.y -= 0.5f*height;
    Vector2D p2 = pos; p2.x += 0.5f*width; p2.y += 0.5f*height;
    Vector2D p3 = pos; p3.x -= 0.5f*width; p3.y += 0.5f*height;
    if( boundary_iso ) {
        p0.y += (1.0f-boundary_iso_ratio)*height;
        p1.x -= boundary_iso_ratio*width;
        p2.y -= (1.0f-boundary_iso_ratio)*height;
        p3.x += boundary_iso_ratio*width;
    }
    boundary_base.addPoint(p0);
    boundary_base.addPoint(p1);
    boundary_base.addPoint(p2);
    boundary_base.addPoint(p3);

    float extra_height = this->visual_height - this->height;
    if( fabs(extra_height) < E_TOL_LINEAR ) {
        this->boundary_visual = boundary_base;
    }
    else {
        if( boundary_iso ) {
            Vector2D p0_2 = p0;
            p0_2.y -= extra_height;

            p1.y -= extra_height;

            Vector2D p2_2 = p2;
            p2_2.y -= extra_height;

            boundary_visual.addPoint(p0);
            boundary_visual.addPoint(p0_2);
            boundary_visual.addPoint(p1);
            boundary_visual.addPoint(p2_2);
            boundary_visual.addPoint(p2);
            boundary_visual.addPoint(p3);
        }
        else {
            p0.y -= extra_height;
            p1.y -= extra_height;
            boundary_visual.addPoint(p0);
            boundary_visual.addPoint(p1);
            boundary_visual.addPoint(p2);
            boundary_visual.addPoint(p3);
        }
    }
    //this->boundary_base = this->boundary_visual; // TEST
}

void Scenery::setBlocking(bool is_blocking, bool blocks_visibility) {
    this->is_blocking = is_blocking;
    this->blocks_visibility = blocks_visibility;
    if( !this->is_blocking && this->blocks_visibility ) {
        throw string("scenery that blocks movement, but not visibility, is not supported");
    }
}

/*void Scenery::getBox(Vector2D *box_centre, float *box_width, float *box_height, bool include_visual) const {
    *box_centre = this->pos;
    *box_width = this->width;
    *box_height = include_visual ? this->visual_height : this->height;
    if( include_visual ) {
        // adjust for the "visual" part being purely above the centre (i.e., the scenery pos is not the centre of the visual box region)
        box_centre->y -= 0.5f*(this->visual_height - this->height);
    }
}*/

float Scenery::distFromPoint(Vector2D point, bool include_visual) const {
    /*Vector2D scenery_pos;
    float scenery_width = 0.0f, scenery_height = 0.0f;
    this->getBox(&scenery_pos, &scenery_width, &scenery_height, include_visual);
    float dist = distFromBox2D(scenery_pos, scenery_width, scenery_height, point);
    return dist;*/
    float dist = include_visual ? this->boundary_visual.distanceFrom(point) : this->boundary_base.distanceFrom(point);
    return dist;
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
    return this->boundary_base.pointInside(ch_pos);
}

vector<string> Scenery::getInteractionText(PlayingGamestate *, string *dialog_text) const {
    vector<string> options;
    if( this->interact_type == "INTERACT_TYPE_THRONE_FP" ) {
        *dialog_text = PlayingGamestate::tr("One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of a knife, gripped by a fist.\n\nDo you wish to sit on the throne?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, sit on the throne.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_GOLD" ) {
        *dialog_text = PlayingGamestate::tr("One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of a gold coin.\n\nDo you wish to sit on the throne?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, sit on the throne.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_M" ) {
        *dialog_text = PlayingGamestate::tr("One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of an eye.\n\nDo you wish to sit on the throne?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, sit on the throne.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_H" ) {
        *dialog_text = PlayingGamestate::tr("One of four manificant thrones in this room. They look out of place in this otherwise ruined location, and the settled dust suggests they have not been used in a long time. On the back of this chair is a symbol of an tree.\n\nDo you wish to sit on the throne?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, sit on the throne.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_SHRINE" ) {
        *dialog_text = PlayingGamestate::tr("An old shrine, to some unknown forgotten diety. The wording on the stone has long since faded away. Do you wish to take a few moments to offer a prayer?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, pray.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_BELL" ) {
        *dialog_text = PlayingGamestate::tr("A large bell hangs here. Do you want to try ringing it?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, ring the bell.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER_EMPTY" ) {
        *dialog_text = PlayingGamestate::tr("A large glass chamber filled with a murky liquid. As you look closely, you can see dark shapes floating inside, though you are unable to identify them.").toStdString();
        options.push_back(PlayingGamestate::tr("Okay").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER" ) {
        if( this->interact_state == 0 ) {
            *dialog_text = PlayingGamestate::tr("A large glass chamber filled with a murky liquid. As you look closely, suddenly the shape of a figure appears! It speaks, in a quiet, drawn out voice - \"Please...,\" it begs to you, \"End my suffering\".\n\nYou see that the glass chamber has a panel at the bottom with two buttons, red and green. You could press one - although you could also try smashing the glass.").toStdString();
            options.push_back(PlayingGamestate::tr("Press the red button.").toStdString());
            options.push_back(PlayingGamestate::tr("Press the green button.").toStdString());
            options.push_back(PlayingGamestate::tr("Smash the glass.").toStdString());
            options.push_back(PlayingGamestate::tr("Leave the creature to suffer.").toStdString());
        }
        else {
            *dialog_text = PlayingGamestate::tr("A large glass chamber filled with a murky liquid. As you look closely, you can see dark shapes floating inside, though you are unable to identify them.").toStdString();
            options.push_back(PlayingGamestate::tr("Okay").toStdString());
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_PAINTING_SHATTER" ) {
        // no options - go straight to interaction
    }
    else if( this->interact_type == "INTERACT_TYPE_POOL" ) {
        *dialog_text = PlayingGamestate::tr("A large pool of murky liquid is here. Do you wish to drink from it?").toStdString();
        options.push_back(PlayingGamestate::tr("Yes, drink.").toStdString());
        options.push_back(PlayingGamestate::tr("No.").toStdString());
    }
    else if( this->interact_type == "INTERACT_TYPE_DUNGEON_MAP" ) {
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
            result_text = PlayingGamestate::tr("As you sit, the chair buzzes, and you feel magical energy run into you. Your fighting prowess has increased!").toStdString();
            //int FP = playing_gamestate->getPlayer()->getFP();
            //playing_gamestate->getPlayer()->setFP( FP + 1 );
            //playing_gamestate->getPlayer()->changeBaseFP(1);
            //playing_gamestate->getPlayer()->changeBaseProfileIntProperty(profile_key_FP_c, 1);
            Profile profile;
            profile.setIntProperty(profile_key_FP_c, 1);
            ProfileEffect profile_effect(profile, 3*60*1000);
            playing_gamestate->getPlayer()->addProfileEffect(profile_effect);
        }
        else {
            result_text = PlayingGamestate::tr("As you sit, you hear a click and feel something uncomfortable in your back. You look down in horror to see a knife protruding out from your chest, stained with your blood.\nThen everything goes dark...").toStdString();
            playing_gamestate->getPlayer()->kill(playing_gamestate);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_GOLD" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            result_text = PlayingGamestate::tr("As you sit, you suddenly feel something magically appear in your hands. Gold - 50 gold pieces!").toStdString();
            playing_gamestate->getPlayer()->addGold(50);
        }
        else {
            if( playing_gamestate->getPlayer()->getGold() > 0 ) {
                result_text = PlayingGamestate::tr("You sit, but nothing seems to happen this time. It is only later that you notice your gold has crumbled into a fine worthless dust!").toStdString();
                playing_gamestate->getPlayer()->setGold(0);
            }
            else {
                result_text = PlayingGamestate::tr("You sit, but nothing seems to happen this time.").toStdString();
            }
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_M" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            result_text = PlayingGamestate::tr("As you sit, you see a flash before your eyes. You have gained great intellectual insight!").toStdString();
            //int M = playing_gamestate->getPlayer()->getMind();
            //playing_gamestate->getPlayer()->setMind( M + 1 );
            //playing_gamestate->getPlayer()->changeBaseM(1);
            //playing_gamestate->getPlayer()->changeBaseProfileIntProperty(profile_key_M_c, 1);
            Profile profile;
            profile.setIntProperty(profile_key_M_c, 1);
            ProfileEffect profile_effect(profile, 3*60*1000);
            playing_gamestate->getPlayer()->addProfileEffect(profile_effect);
        }
        else {
            result_text = PlayingGamestate::tr("As you sit, you see a flash again before your eyes, but this time it is followed by darkness. You rub your eyes, but it remains. As you stand up, you realise you have been blinded!\nYour adventure ends here.").toStdString();
            playing_gamestate->getPlayer()->kill(playing_gamestate);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_THRONE_H" ) {
        //dialog_title = "Throne";
        if( this->interact_state == 0 ) {
            if( playing_gamestate->getPlayer()->getHealth() < playing_gamestate->getPlayer()->getMaxHealth() ) {
                this->interact_state = 1; // only set the state once the health benefit is used
                result_text = PlayingGamestate::tr("As you sit, you feel energy rush into you, and you see your wounds magically close up before your eyes!").toStdString();
                playing_gamestate->getPlayer()->restoreHealth();
            }
            else {
                result_text = PlayingGamestate::tr("You sit, but nothing seems to happen.").toStdString();
            }
        }
        else {
            result_text = PlayingGamestate::tr("As you sit, you are suddenly gripped by a terrible pain over your entire body. Your watch in horror as old wounds open up before your eyes.").toStdString();
            int damage = rollDice(10, 6, 0);
            playing_gamestate->getPlayer()->decreaseHealth(playing_gamestate, damage, false, false);
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_SHRINE" ) {
        result_text = PlayingGamestate::tr("You pray, but nothing seems to happen.").toStdString();
        if( this->interact_state == 0 ) {
            int roll = rollDice(1, 3, 0);
            LOG("shrine: roll a %d\n", roll);
            this->interact_state = 1;
            if( roll == 1 ) {
                if( playing_gamestate->getPlayer()->getHealth() < playing_gamestate->getPlayer()->getMaxHealth() ) {
                    result_text = PlayingGamestate::tr("You have been blessed with healing!").toStdString();
                    playing_gamestate->getPlayer()->restoreHealth();
                }
            }
            else if( roll == 2 ) {
                result_text = PlayingGamestate::tr("You are rewarded by the diety for your courage on this quest, with a gift of gold.").toStdString();
                int gold = rollDice(3, 10, 0);
                playing_gamestate->getPlayer()->addGold(gold);
            }
            else if( roll == 3 ) {
                result_text = PlayingGamestate::tr("You are granted the gift of wisdom by the diety.").toStdString();
                playing_gamestate->getPlayer()->addXP(playing_gamestate, 30);
            }
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_BELL" ) {
        if( this->interact_state == 0 ) {
            Vector2D free_pvec;
            if( this->location->findFreeWayPoint(&free_pvec, playing_gamestate->getPlayer()->getPos(), true, false) ) {
                int roll = rollDice(1, 3, 0);
                LOG("bell: roll a %d\n", roll);
                this->interact_state = 1;
                Character *enemy = NULL;
                if( roll == 1 ) {
                    result_text = PlayingGamestate::tr("You ring the bell, which makes an almighty clang. Suddenly a Goblin materialises out of thin air!").toStdString();
                    enemy = playing_gamestate->createCharacter("Goblin", "Goblin");
                }
                else if( roll == 2 ) {
                    result_text = PlayingGamestate::tr("You ring the bell, which makes an almighty clang. Suddenly an Orc materialises out of thin air!").toStdString();
                    enemy = playing_gamestate->createCharacter("Orc", "Orc");
                }
                else if( roll == 3 ) {
                    result_text = PlayingGamestate::tr("You ring the bell, which makes an almighty clang. Suddenly a Goblin materialises out of thin air!").toStdString();
                    enemy = playing_gamestate->createCharacter("Goblin Mage", "Goblin Mage");
                    enemy->addSpell("Fire bolt", 6);
                }

                if( enemy != NULL ) {
                    Location *c_location = playing_gamestate->getCLocation();
                    enemy->setDefaultPosition(free_pvec.x, free_pvec.y);
                    c_location->addCharacter(enemy, free_pvec.x, free_pvec.y);
                }
            }
            else {
                result_text = PlayingGamestate::tr("You ring the bell, but nothing seems to happen.").toStdString();
            }
        }
        else {
            result_text = PlayingGamestate::tr("You ring the bell again, but nothing seems to happen this time.").toStdString();
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER_EMPTY" ) {
        ASSERT_LOGGER(false);
    }
    else if( this->interact_type == "INTERACT_TYPE_EXPERIMENTAL_CHAMBER" ) {
        if( this->interact_state == 0 ) {
            this->interact_state = 1;
            if( option == 0 ) {
                result_text = PlayingGamestate::tr("The chamber flashes red. You hear a sudden but short scream from within, then it is over. At last, you have put the poor creature to rest.").toStdString();
                playing_gamestate->getPlayer()->addXP(playing_gamestate, 100);
            }
            else if( option == 1 ) {
                result_text = PlayingGamestate::tr("You hear strange whirring sounds. After some moments, the chamber starts to change different colours - green, then blue, then purple, and several more. As this happens, the creature inside starts to scream. The chamber starts to shake, causing a sound that becomes increasingly loud, though it is unable to cover the volume of the poor creature's screams. Eventually the sounds subside, and it seem the creature is dead.\n\nYou have at least granted its wish, though you wonder if there was a better way to do it.").toStdString();
            }
            else if( option == 2 ) {
                result_text = PlayingGamestate::tr("You smash the glass, and liquid drains out. Some of it spatters on you, causing you pain! The creature also screams as this happens, but then it seems the poor creature has died. At last, you have put the poor creature to rest.").toStdString();
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
            result_text = PlayingGamestate::tr("As you cast your eyes on the painting, there is suddenly a smashing sound, and to your amazement, your weapon shatters!").toStdString();
            playing_gamestate->getPlayer()->takeItem(item);
            delete item;
        }
        else {
            result_text = PlayingGamestate::tr("You look at the interesting painting.").toStdString();
        }
        picture = this->big_image_name;
    }
    else if( this->interact_type == "INTERACT_TYPE_POOL" ) {
        if( !playing_gamestate->getPlayer()->hasProfileEffects() ) {
            Profile pool_profile;
            switch( interact_state ) {
            case 1:
                pool_profile.setIntProperty(profile_key_FP_c, 1);
                break;
            case 2:
                pool_profile.setIntProperty(profile_key_BS_c, 1);
                break;
            case 3:
                pool_profile.setIntProperty(profile_key_S_c, 1);
                break;
            case 4:
                pool_profile.setIntProperty(profile_key_A_c, 1);
                break;
            case 5:
                pool_profile.setIntProperty(profile_key_M_c, 1);
                break;
            case 6:
                pool_profile.setIntProperty(profile_key_D_c, 1);
                break;
            case 7:
                pool_profile.setIntProperty(profile_key_B_c, 1);
                break;
            case 8:
                pool_profile.setFloatProperty(profile_key_Sp_c, 0.2f);
                break;
            }
            ProfileEffect profile_effect(pool_profile, 30000);
            playing_gamestate->getPlayer()->addProfileEffect(profile_effect);
            result_text = PlayingGamestate::tr("You drink from the pool and feel a boost of energy.").toStdString();
        }
        else {
            result_text = PlayingGamestate::tr("You drink from the pool but feel no effects.").toStdString();
        }
    }
    else if( this->interact_type == "INTERACT_TYPE_DUNGEON_MAP" ) {
        result_text = PlayingGamestate::tr("The painting shows a map of the current dungeon level!").toStdString();
        playing_gamestate->getCLocation()->revealMap(playing_gamestate);
    }
    else {
        ASSERT_LOGGER(false);
    }

    playing_gamestate->showInfoDialog(result_text, picture);
}

void Scenery::setTrap(Trap *trap) {
    if( this->trap != NULL ) {
        delete this->trap;
    }
    this->trap = trap;
}

Trap::Trap(const string &type) : type(type), has_position(false), width(-1.0f), height(-1.0f), rating(0), difficulty(0)
{
}

Trap::Trap(const string &type, float width, float height) : type(type), has_position(true), width(width), height(height), rating(0), difficulty(0)
{
}

bool Trap::isSetOff(const Character *character) const {
    ASSERT_LOGGER( has_position );
    if( !has_position ) {
        return false;
    }
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
        //if( rollD + difficulty <= character->getDexterity() ) {
        if( rollD + difficulty <= character->getProfileIntProperty(profile_key_D_c) ) {
            LOG("avoided\n");
            text = PlayingGamestate::tr("You have set off a trap!\nAn arrow shoots out from the wall,\nbut you manage to avoid it!").toStdString();
        }
        else {
            LOG("affected\n");
            text = PlayingGamestate::tr("You have set off a trap!\nAn arrow shoots out from the\nwall and hits you!").toStdString();
            int damage = rollDice(2, 12, rating-1);
            character->decreaseHealth(playing_gamestate, damage, true, true);
        }
    }
    else if( type == "darts" ) {
        text = PlayingGamestate::tr("You have set off a trap!\nDarts shoot out from the wall,\nhitting you multiple times!").toStdString();
        int damage = rollDice(rating+1, 12, 0);
        character->decreaseHealth(playing_gamestate, damage, true, true);
    }
    else if( type == "acid" ) {
        if( character->getCurrentShield() != NULL ) {
            text = PlayingGamestate::tr("You have set off a trap!\nAcid shoots out from jets in the walls!\nYour shield protects you,\nbut is destroyed in the process.").toStdString();
            Item *item = character->getCurrentShield();
            character->takeItem(item);
            delete item;
        }
        else {
            text = PlayingGamestate::tr("You have set off a trap!\nA painful acid shoots out\nfrom jets in the walls,\nburning your flesh!").toStdString();
            int damage = rollDice(4, 20, rating);
            character->decreaseHealth(playing_gamestate, damage, false, true);
        }
    }
    else if( type == "mantrap" ) {
        if( rollD + difficulty <= character->getProfileIntProperty(profile_key_D_c) ) {
            LOG("avoided\n");
            text = PlayingGamestate::tr("You manage to avoid the\nvicious bite of a mantrap that\nyou spot laying on the ground!").toStdString();
        }
        else {
            LOG("affected\n");
            text = PlayingGamestate::tr("You have set off a trap!\nYou feel agony in your leg, as you realise\nyou have stepped into a mantrap!").toStdString();
            int damage = rollDice(4, 12, rating);
            character->decreaseHealth(playing_gamestate, damage, true, false);
            character->setStateIdle();
        }
    }
    else if( type == "gas" ) {
        text = PlayingGamestate::tr("You have set off a trap!\nGas shoots from jets in the wall,\nand suddenly you find you are paralysed!").toStdString();
        character->paralyse((rating+1)*1000);
    }
    else if( type == "death" ) {
        text = PlayingGamestate::tr("You have set off a trap!\nThe last thing you hear is a\nmassive explosion in your ears!\nA bomb blows your body to pieces...").toStdString();
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

void FloorRegion::removeItem(Item *item) {
    if( this->items.find(item) == items.end() ) {
        LOG("failed to find item %s in floor region\n", item->getName().c_str());
        throw string("failed to find item in this floor region");
    }
    this->items.erase(item);
}

FloorRegion *FloorRegion::createRectangle(float x, float y, float w, float h) {
    FloorRegion *floor_regions = new FloorRegion();
    if( x < 0.0f ) {
        throw string("x coord too small");
    }
    else if( y < 0.0f ) {
        throw string("y coord too small");
    }
    floor_regions->addPoint(Vector2D(x, y));
    floor_regions->addPoint(Vector2D(x, y+h));
    floor_regions->addPoint(Vector2D(x+w, y+h));
    floor_regions->addPoint(Vector2D(x+w, y));
    return floor_regions;
}

FloorRegion *FloorRegion::createRectangle(const Rect2D &rect) {
    return createRectangle(rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight());
}

/*const float tile_width = 1.0f;
const float tile_height = 1.0f;*/

Tilemap::Tilemap(float x, float y, const string &imagemap, int tile_width, int tile_height, vector<string> rows) : pos(x, y), /*width(0.0f), height(0.0f),*/ imagemap(imagemap), tile_width(tile_width), tile_height(tile_height), rows(rows), max_length(0) {
    //height = rows.size() * tile_height;
    for(vector<string>::const_iterator iter = rows.begin(); iter != rows.end(); ++iter) {
        string str = *iter;
        int length = str.length();
        max_length = std::max(max_length, length);
    }
    //width = max_length * tile_width;
}

char Tilemap::getTileAt(int x, int y) const {
    ASSERT_LOGGER(x >= 0);
    ASSERT_LOGGER(x < max_length);
    ASSERT_LOGGER(y >= 0);
    ASSERT_LOGGER(y < rows.size());
    if( x >= rows.at(y).length() ) {
        return ' ';
    }
    return rows.at(y).at(x);
}

Location::Location(const string &name) :
    name(name), display_name(false), type(TYPE_INDOORS), geo_type(GEOTYPE_DUNGEON), listener(NULL), listener_data(NULL),
    distance_graph(NULL), wall_x_scale(3.0f), lighting_min(55), wandering_monster_time_ms(0), wandering_monster_rest_chance(0)
{
}

Location::~Location() {
    if( distance_graph != NULL ) {
        delete distance_graph;
    }
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        delete floor_region;
    }
    for(vector<Tilemap *>::iterator iter = tilemaps.begin(); iter != tilemaps.end(); ++iter) {
        Tilemap *tilemap = *iter;
        delete tilemap;
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
    for(set<Trap *>::iterator iter = traps.begin(); iter != traps.end(); ++iter) {
        Trap *trap = *iter;
        delete trap;
    }
}

int Location::getWanderingMonsterRestChance(const Character *player) const {
    int chance = this->wandering_monster_rest_chance;
    if( player->hasSkill(skill_hideaway_c) )
        chance /= 2;
    return chance;
}

void Location::addFloorRegion(FloorRegion *floorRegion) {
    Vector2D top_left = floorRegion->getTopLeft();
    Vector2D bottom_right = floorRegion->getTopLeft();
    // floor region should be within sensible limits - important for floating point precision
    if( top_left.x < -E_TOL_LINEAR || bottom_right.x > E_TOL_LARGE || top_left.y < -E_TOL_LINEAR || bottom_right.y > E_TOL_LARGE ) {
        throw string("floor region outside of allowed range");
    }
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

FloorRegion *Location::findFloorRegionInside(Vector2D pos, float width, float height) const {
    width *= 0.5f;
    height *= 0.5f;
    // n.b., finds a floor region that fully contains the box
    FloorRegion *result = NULL;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end() && result==NULL; ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->pointInside(pos) &&
                floor_region->pointInside(pos + Vector2D(-width, -height)) &&
                floor_region->pointInside(pos + Vector2D(width, -height)) &&
                floor_region->pointInside(pos + Vector2D(-width, height)) &&
                floor_region->pointInside(pos + Vector2D(width, height)) ) {
            result = floor_region;
        }
    }
    return result;
}

FloorRegion *Location::findFloorRegionAt(Vector2D pos) const {
    FloorRegion *result = NULL;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end() && result==NULL; ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->pointInside(pos) ) {
            result = floor_region;
        }
    }
    return result;
}

vector<FloorRegion *> Location::findFloorRegionsAt(Vector2D pos) const {
    vector<FloorRegion *> result_floor_regions;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->pointInside(pos) ) {
            result_floor_regions.push_back(floor_region);
        }
    }
    return result_floor_regions;
}

vector<FloorRegion *> Location::findFloorRegionsAt(Vector2D pos, float width, float height) const {
    width *= 0.5f;
    height *= 0.5f;
    // n.b., also includes floor regions that the scenery only just touches
    vector<FloorRegion *> result_floor_regions;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->pointInside(pos) ||
                floor_region->pointInside(pos + Vector2D(-width, -height)) ||
                floor_region->pointInside(pos + Vector2D(width, -height)) ||
                floor_region->pointInside(pos + Vector2D(-width, height)) ||
                floor_region->pointInside(pos + Vector2D(width, height)) ) {
            result_floor_regions.push_back(floor_region);
        }
    }
    return result_floor_regions;
}

vector<FloorRegion *> Location::findFloorRegionsAt(const Scenery *scenery) const {
    vector<FloorRegion *> result_floor_regions = this->findFloorRegionsAt(scenery->getPos(), scenery->getWidth(), scenery->getHeight());
    if( result_floor_regions.size() == 0 ) {
        // for sceneries on a wall
        result_floor_regions = this->findFloorRegionsAt(scenery->getPos() + Vector2D(0.0f, 0.5f*scenery->getHeight() + 0.5f));
    }
    if( result_floor_regions.size() == 0 ) {
        LOG("add: failed to find floor region for scenery %s at %f, %f\n", scenery->getName().c_str(), scenery->getX(), scenery->getY());
        throw string("add: failed to find floor region for scenery");
    }
    return result_floor_regions;
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

float Location::distanceOfPath(Vector2D src, const vector<Vector2D> &path, bool has_max_dist, float max_dist) {
    float dist = 0.0f;
    if( path.size() > 0 ) {
        Vector2D last_pos = src;
        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
            Vector2D pos = *iter;
            dist += (pos - last_pos).magnitude();
            last_pos = pos;
            if( has_max_dist && dist > max_dist )
                break;
        }
    }
    return dist;
}

bool Location::hasEnemies(const PlayingGamestate *playing_gamestate) const {
    bool has_enemies = false;
    //qDebug("Location:hasEnemies()");
    if( playing_gamestate->getPlayer() == NULL ) {
        // protect against RTE!
        return false;
    }
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !has_enemies; ++iter) {
        const Character *character = *iter;
        //qDebug("    character: %d", character);
        //qDebug("    character: %s", character->getName().c_str());
        if( character->isHostile() ) {
            // n.b., we don't use the visibility test - so it isn't sufficient to just be out of sight, but we also need to be a sufficient distance
            // we care about the distance by path, rather than euclidean distance - but we can check the euclidean distance first as a quick check
            float dist = (character->getPos() - playing_gamestate->getPlayer()->getPos()).magnitude();
            if( dist <= npc_visibility_c ) {
                vector<Vector2D> new_path = this->calculatePathTo(character->getPos(), playing_gamestate->getPlayer()->getPos(), NULL, character->canFly());
                if( new_path.size() > 0 ) {
                    /*float dist = 0.0f;
                    Vector2D last_pos = character->getPos();
                    for(vector<Vector2D>::const_iterator iter = new_path.begin(); iter != new_path.end(); ++iter) {
                        Vector2D pos = *iter;
                        dist += (pos - last_pos).magnitude();
                        last_pos = pos;
                        if( dist > npc_visibility_c )
                            break;
                    }*/
                    float dist = Location::distanceOfPath(character->getPos(), new_path, true, npc_visibility_c);
                    if( dist <= npc_visibility_c ) {
                        has_enemies = true;
                    }
                }
            }
        }
    }
    return has_enemies;
}

void Location::addItem(Item *item, float xpos, float ypos) {
    qDebug("add item %s to %s at %f, %f", item->getName().c_str(), this->name.c_str(), xpos, ypos);
    item->setPos(xpos, ypos);
    this->items.insert(item);

    FloorRegion *floor_region = this->findFloorRegionAt(item->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for item %s at %f, %f\n", item->getName().c_str(), item->getX(), item->getY());
        throw string("failed to find floor region for item");
    }
    floor_region->addItem(item);

    if( this->listener != NULL ) {
        this->listener->locationAddItem(this, item, floor_region->isVisible());
    }
}

void Location::removeItem(Item *item) {
    qDebug("remove item %s from %s", item->getName().c_str(), this->name.c_str());
    if( this->items.find(item) == items.end() ) {
        LOG("failed to find item %s in location %s\n", item->getName().c_str(), name.c_str());
        throw string("failed to find item in this location");
    }
    this->items.erase(item);

    FloorRegion *floor_region = this->findFloorRegionAt(item->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for item %s at %f, %f in location %s\n", item->getName().c_str(), item->getX(), item->getY(), this->name.c_str());
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
  * Similarly for adding scenery to floor regions, done in addSceneryToFloorRegions().
  */
void Location::addScenery(Scenery *scenery, float xpos, float ypos) {
    scenery->setLocation(this);
    scenery->setPos(xpos, ypos);
    this->scenerys.insert(scenery);

    if( this->listener != NULL ) {
        this->listener->locationAddScenery(this, scenery);
    }
}

void Location::removeScenery(Scenery *scenery) {
    // remove corresponding boundary
    //qDebug("Location::removeScenery(%d)", scenery);
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

    /*FloorRegion *floor_region = this->findFloorRegionAt(scenery->getPos());
    if( floor_region == NULL ) {
        LOG("failed to find floor region for scenery %s at %f, %f\n", scenery->getName().c_str(), scenery->getX(), scenery->getY());
        throw string("failed to find floor region for scenery");
    }
    floor_region->removeScenery(scenery);*/
    vector<FloorRegion *> scenery_floor_regions = this->findFloorRegionsAt(scenery);
    for(vector<FloorRegion *>::iterator iter = scenery_floor_regions.begin();iter != scenery_floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        floor_region->removeScenery(scenery);
    }

    // now remove corresponding nodes from the graph
    /*if( this->distance_graph != NULL ) {
        for(size_t i=0;i<this->distance_graph->getNVertices();i++) {
            GraphVertex *graph_vertex = this->distance_graph->getVertex(i);
            //qDebug("%d : %d", i, graph_vertex->getUserData());
            if( graph_vertex->getUserData() == scenery ) {
                //qDebug("### node %d can be removed", i);
                // todo
            }
        }
    }*/
    //this->calculateDistanceGraph();

    if( this->distance_graph != NULL ) {
        // update distance graph

        // need to see if additional path_way_points can be activated
        for(size_t i=0;i<path_way_points.size();i++) {
            PathWayPoint path_way_point = path_way_points.at(i);
            if( !path_way_point.active ) {
                this->testActivatePathWayPoint(&path_way_point);
                if( path_way_point.active ) {
                    Vector2D A = path_way_point.point;
                    GraphVertex vertex(A, path_way_point.source);
                    this->distance_graph->addVertex(vertex);
                }
            }
        }

        Vector2D top_left = scenery->getPos();
        Vector2D bottom_right = scenery->getPos();
        top_left.x -= (0.5f*scenery->getWidth()+npc_radius_c+E_TOL_LINEAR);
        bottom_right.x += (0.5f*scenery->getWidth()+npc_radius_c+E_TOL_LINEAR);
        top_left.y -= (0.5f*scenery->getHeight()+npc_radius_c+E_TOL_LINEAR);
        bottom_right.y += (0.5f*scenery->getHeight()+npc_radius_c+E_TOL_LINEAR);
        for(size_t i=0;i<this->distance_graph->getNVertices();i++) {
            GraphVertex *v_A = this->distance_graph->getVertex(i);
            Vector2D A_pos = v_A->getPos();
            for(size_t j=i+1;j<this->distance_graph->getNVertices();j++) {
                //qDebug("### update %d vs %d?", i, j);
                GraphVertex *v_B = this->distance_graph->getVertex(j);
                Vector2D B_pos = v_B->getPos();
                if( A_pos.x < top_left.x && B_pos.x < top_left.x ) {
                    continue;
                }
                else if( A_pos.x > bottom_right.x && B_pos.x > bottom_right.x ) {
                    continue;
                }
                else if( A_pos.y < top_left.y && B_pos.y < top_left.y ) {
                    continue;
                }
                else if( A_pos.y > bottom_right.y && B_pos.y > bottom_right.y ) {
                    continue;
                }
                //qDebug("    updating %d vs %d", i, j);
                bool already_visible = false;
                for(size_t k=0;k<v_A->getNNeighbours() && !already_visible;k++) {
                    if( v_A->getNeighbour(this->distance_graph, k) == v_B ) {
                        already_visible = true;
                    }
                }
                // only need to test if not already visible (since we're moving scenery)
                if( !already_visible ) {
                    float dist = 0.0f;
                    bool hit = testGraphVerticesHit(&dist, v_A, v_B, NULL, false);
                    if( !hit ) {
                        v_A->addNeighbour(j, dist);
                        v_B->addNeighbour(i, dist);
                    }
                }
            }
        }

    }

    if( this->listener != NULL ) {
        this->listener->locationRemoveScenery(this, scenery);
    }
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
    delete trap;
}

void Location::createBoundariesForRegions() {
    qDebug("Location::createBoundariesForRegions()");

#ifdef _DEBUG
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        /*for(vector<FloorRegion *>::iterator iter2 = iter+1; iter2 != floor_regions.end(); ++iter2) {
            FloorRegion *floor_region2 = *iter2;
        }*/
        qDebug("Floor region:");
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            qDebug("    %d : %f, %f", j, p0.x, p0.y);
        }
    }
#endif
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

    qDebug("calculate which boundary edges are internal");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint((j+1) % floor_region->getNPoints());
            //qDebug("check if edge is internal: %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
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
                    //qDebug("    compare to: %f, %f to %f, %f", p2.x, p2.y, p3.x, p3.y);
                    //qDebug("    dists: %f, %f", dist0, dist1);
                    if( dist0 <= E_TOL_LINEAR && dist1 <= E_TOL_LINEAR ) {
                        //qDebug("edge is internal with: %f, %f to %f, %f", p2.x, p2.y, p3.x, p3.y);
                        floor_region->setEdgeType(j, FloorRegion::EDGETYPE_INTERNAL);
                        floor_region2->setEdgeType(j2, FloorRegion::EDGETYPE_INTERNAL);
                    }
                }
            }
        }
    }

    qDebug("find boundaries");
    for(;;) {
        //qDebug("find an unmarked edge");
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
            //qDebug("all done");
            break;
        }
        FloorRegion *start_floor_region = c_floor_region;
        int start_indx = c_indx;
        Polygon2D boundary;
        boundary.setSourceType((int)SOURCETYPE_FLOOR);
        // first point actually added as last point at end
        for(;;) {
            int n_indx = (c_indx+1) % c_floor_region->getNPoints();
            //Vector2D back_pvec = c_floor_region->getPoint( c_indx );
            Vector2D pvec = c_floor_region->getPoint( n_indx );
            //qDebug("on edge %f, %f to %f, %f", back_pvec.x, back_pvec.y, pvec.x, pvec.y);
            if( c_floor_region->getTempMark(c_indx) != 0 ) {
                LOG("moved onto boundary already marked: %d, %d\n", c_floor_region, c_indx);
                throw string("moved onto boundary already marked");
            }
            boundary.addPoint(pvec);
            c_floor_region->setTempMark(c_indx, 1);
            if( c_floor_region == start_floor_region && n_indx == start_indx ) {
                break;
            }
            //qDebug("move on to next edge");
            if( c_floor_region->getEdgeType(n_indx) == FloorRegion::EDGETYPE_EXTERNAL ) {
                c_indx = n_indx;
            }
            else {
                //qDebug("need to find a new boundary");
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
                    //qDebug("can't find new boundary");
                    break;
                }
            }
        }
        this->addBoundary(boundary);
    }
    qDebug("reset temp marks");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            floor_region->setTempMark(j, 0);
        }
    }
}

void Location::createBoundaryForRect(Vector2D pos, float width, float height, bool boundary_iso, float boundary_iso_ratio, void *source, int source_type) {
    Polygon2D boundary;
    boundary.setSourceType(source_type);
    boundary.setSource(source);
    // clockwise, as "inside" should be on the left
    Vector2D p0 = pos; p0.x -= 0.5f*width; p0.y -= 0.5f*height;
    Vector2D p1 = pos; p1.x += 0.5f*width; p1.y -= 0.5f*height;
    Vector2D p2 = pos; p2.x += 0.5f*width; p2.y += 0.5f*height;
    Vector2D p3 = pos; p3.x -= 0.5f*width; p3.y += 0.5f*height;

    if( boundary_iso ) {
        p0.y += (1.0f-boundary_iso_ratio)*height;
        p1.x -= boundary_iso_ratio*width;
        p2.y -= (1.0f-boundary_iso_ratio)*height;
        p3.x += boundary_iso_ratio*width;
    }

    if( this->findFloorRegionAt(p0) == NULL ) {
        LOG("can't find floor region for p0 at: %f, %f\n", p0.x, p0.y);
        LOG("rect at %f, %f: %d\n", pos.x, pos.y, source);
        throw string("can't find floor region for p0");
    }
    if( this->findFloorRegionAt(p1) == NULL ) {
        LOG("can't find floor region for p1 at: %f, %f\n", p1.x, p1.y);
        LOG("rect at %f, %f: %d\n", pos.x, pos.y, source);
        throw string("can't find floor region for p1");
    }
    if( this->findFloorRegionAt(p2) == NULL ) {
        LOG("can't find floor region for p2 at: %f, %f\n", p2.x, p2.y);
        LOG("rect at %f, %f: %d\n", pos.x, pos.y, source);
        throw string("can't find floor region for p2");
    }
    if( this->findFloorRegionAt(p3) == NULL ) {
        LOG("can't find floor region for p3 at: %f, %f\n", p3.x, p3.y);
        LOG("rect at %f, %f: %d\n", pos.x, pos.y, source);
        throw string("can't find floor region for p3");
    }
    boundary.addPoint(p0);
    boundary.addPoint(p1);
    boundary.addPoint(p2);
    boundary.addPoint(p3);
    this->addBoundary(boundary);
}

void Location::createBoundariesForScenery() {
    //qDebug("Location::createBoundariesForScenery()");
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( !scenery->isBlocking() ) {
            continue;
        }
        //qDebug(">>> set scenery %d", scenery);
        /*float width = scenery->getWidth(), height = scenery->getHeight();
        Vector2D pos = scenery->getPos();
        this->createBoundaryForRect(pos, width, height, scenery->isBoundaryIso(), scenery->getBoundaryIsoRatio(), scenery, (int)SOURCETYPE_SCENERY);*/
        Polygon2D boundary = scenery->getBoundary(false);
        boundary.setSourceType((int)SOURCETYPE_SCENERY);
        boundary.setSource(scenery);
        for(size_t i=0;i<boundary.getNPoints();i++) {
            Vector2D pvec = boundary.getPoint(i);
            if( this->findFloorRegionAt(pvec) == NULL ) {
                LOG("can't find floor region for scenery %s , %d th boundary point at: %f, %f\n", scenery->getName().c_str(), i, pvec.x, pvec.y);
                LOG("scenery at %f, %f\n", scenery->getX(), scenery->getY());
                throw string("can't find floor region for scenery boundary point");
            }
        }
        this->addBoundary(boundary);
        //qDebug("scenery at %f, %f", pos.x, pos.y);
    }
    qDebug("    done");
}

void Location::createBoundariesForFixedNPCs() {
    for(set<Character *>::iterator iter = characters.begin(); iter != characters.end(); ++iter) {
        Character *character = *iter;
        if( !character->isFixed() ) {
            continue;
        }
        Vector2D pos = character->getPos();
        this->createBoundaryForRect(pos, 2.0f*npc_radius_c, 2.0f*npc_radius_c, false, 0.0f, character, (int)SOURCETYPE_FIXED_NPC);
    }
    qDebug("    done");
}

void Location::addSceneryToFloorRegions() {
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        vector<FloorRegion *> scenery_floor_regions = this->findFloorRegionsAt(scenery);
        for(vector<FloorRegion *>::iterator iter = scenery_floor_regions.begin();iter != scenery_floor_regions.end(); ++iter) {
            FloorRegion *floor_region = *iter;
            floor_region->addScenery(scenery);
        }
    }
}

void Location::intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, bool find_earliest, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) const {
    //Vector2D saved_p0 = p0; // for debugging
    //Vector2D saved_p1 = p1;
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

void Location::intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, bool find_earliest, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, IntersectType intersect_type, const void *ignore_one, bool flying) const {
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
        else if( boundary->getSourceType() == (int)SOURCETYPE_FIXED_NPC ) {
            if( intersect_type == INTERSECTTYPE_VISIBILITY )
                continue; // can always see past NPCs
        }
        if( ignore_one != NULL && boundary->getSource() == ignore_one ) {
            continue;
        }
        Vector2D top_left = boundary->getTopLeft();
        Vector2D bottom_right = boundary->getBottomRight();
        top_left.x -= E_TOL_LINEAR;
        top_left.y -= E_TOL_LINEAR;
        bottom_right.x += E_TOL_LINEAR;
        bottom_right.y += E_TOL_LINEAR;
        if( start.x + width < top_left.x && end.x + width < top_left.x ) {
            continue;
        }
        else if( start.x - width > bottom_right.x && end.x - width > bottom_right.x ) {
            continue;
        }
        else if( start.y + width < top_left.y && end.y + width < top_left.y ) {
            continue;
        }
        else if( start.y - width > bottom_right.y && end.y - width > bottom_right.y ) {
            continue;
        }
        /*bool aabb_test = false;
        if( start.x + width < top_left.x && end.x + width < top_left.x ) {
            aabb_test = true;
        }
        else if( start.x - width > bottom_right.x && end.x - width > bottom_right.x ) {
            aabb_test = true;
        }
        else if( start.y + width < top_left.y && end.y + width < top_left.y ) {
            aabb_test = true;
        }
        else if( start.y - width > bottom_right.y && end.y - width > bottom_right.y ) {
            aabb_test = true;
        }*/
        for(size_t j=0;j<boundary->getNPoints() && !(*done);j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            intersectSweptSquareWithBoundarySeg(hit, hit_dist, done, find_earliest, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
            /*if( *done && aabb_test ) {
                qDebug("start: %f, %f", start.x, start.y);
                qDebug("end: %f, %f", end.x, end.y);
                qDebug("top_left: %f, %f", top_left.x, top_left.y);
                qDebug("bottom_right: %f, %f", bottom_right.x, bottom_right.y);
                qDebug("width: %f", width);
                throw string("###");
            }*/
        }
    }
}

bool Location::intersectSweptSquareWithBoundaries(Vector2D *hit_pos, bool find_earliest, Vector2D start, Vector2D end, float width, IntersectType intersect_type, const void *ignore_one, bool flying) const {
    //LOG("Location::intersectSweptSquareWithBoundaries from %f, %f to %f, %f, width %f\n", start.x, start.y, end.x, end.y, width);
    bool done = false;
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        qDebug("Location::intersectSweptSquareWithBoundaries received equal start and end");
    }
    ASSERT_LOGGER(dv_length != 0.0f);
    if( dv_length == 0.0f ) {
        return false;
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

    intersectSweptSquareWithBoundaries(&done, &hit, &hit_dist, find_earliest, start, end, du, dv, width, xmin, xmax, ymin, ymax, intersect_type, ignore_one, flying);

    if( hit ) {
        *hit_pos = start + dv * hit_dist;
    }
    return hit;
}

//#if 0
Vector2D Location::nudgeToFreeSpace(Vector2D src, Vector2D pos, float width) const {
    {
        // rule out cases that we can move to without needing any nudges - must be done first
        vector<Vector2D> new_path = this->calculatePathTo(src, pos, NULL, false);
        if( new_path.size() > 0 ) {
            return pos;
        }
    }
    Vector2D saved_pos = pos;
    bool nudged = false;
    vector<Vector2D> internal_nudges;
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
                //qDebug("    closest_pt %f, %f; dist = %f", closest_pt.x, closest_pt.y, dist);
                //if( fabs(dist) <= width ) {
                if( dist >= -0.9f && dist <= width ) {
                    // we allow as far as -0.9f "inside" the wall, to handle doors
                    float move_dist = width - dist;
                    move_dist += E_TOL_LINEAR;
                    Vector2D test_pos = pos + normal_from_wall * move_dist;
                    //qDebug("    nudge to %f, %f?", test_pos.x, test_pos.y);

                    if( dist < -E_TOL_LINEAR ) {
                        // internal?
                        internal_nudges.push_back(test_pos);
                        //qDebug("    candidate internal nudge %f, %f to %f, %f (dist %f)", saved_pos.x, saved_pos.y, pos.x, pos.y, dist);
                        //qDebug("        due to %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
                    }
                    else {
                        pos = test_pos;
                        //qDebug("    nudged %f, %f to %f, %f (dist %f)", saved_pos.x, saved_pos.y, pos.x, pos.y, dist);
                        //qDebug("        due to %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
                        nudged = true;
                    }
                }
            }
            // n.b., only care about the closest point on the line seg, and not the end points of the line seg - as we'll look at the adjacent boundary segments to find the real closest point
        }
    }

    if( nudged ) {
        // test we can get to the nudged position - otherwise we may meant to be looking at internal nudges
        vector<Vector2D> new_path = this->calculatePathTo(src, pos, NULL, false);
        if( new_path.size() == 0 ) {
            nudged = false;
            pos = saved_pos;
        }
    }
    if( !nudged && internal_nudges.size() > 0 ) {
        // if the point has been nudged, then this wasn't really a click point in an "internal" region
        // if it was internal, we pick the closest
        float closest_dist = -1.0f;
        //qDebug("    internal nudge");
        for(vector<Vector2D>::const_iterator iter = internal_nudges.begin(); iter!= internal_nudges.end(); ++iter) {
            Vector2D test_pos = *iter;
            //double dist = (test_pos - saved_pos).magnitude();
            vector<Vector2D> new_path = this->calculatePathTo(src, test_pos, NULL, false);
            if( new_path.size() == 0 ) {
                continue; // can't even get to this position
            }
            // pick closest point to get to
            float dist = Location::distanceOfPath(src, new_path, false, 0.0f);

            if( !nudged || dist < closest_dist - E_TOL_LINEAR ) {
                nudged = true;
                pos = test_pos;
                closest_dist = dist;
            }
        }
    }

    if( nudged ) {
        //qDebug("nudged %f, %f to %f, %f", saved_pos.x, saved_pos.y, pos.x, pos.y);
    }
    return pos;
}
//#endif

#if 0
Vector2D Location::nudgeToFreeSpace(Vector2D src, Vector2D pos, float width) const {
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
                //qDebug("    closest_pt %f, %f; dist = %f", closest_pt.x, closest_pt.y, dist);
                //if( fabs(dist) <= width ) {
                if( dist >= -0.9f && dist <= width ) {
                    // we allow as far as -0.9f "inside" the wall, to handle doors
                    float move_dist = width - dist;
                    move_dist += E_TOL_LINEAR;
                    Vector2D test_pos = pos + normal_from_wall * move_dist;
                    //qDebug("    nudge to %f, %f?", test_pos.x, test_pos.y);

                    vector<Vector2D> new_path = this->calculatePathTo(src, test_pos, NULL, false);
                    if( new_path.size() > 0 ) {
                        pos = test_pos;
                        /*qDebug("    nudged %f, %f to %f, %f (dist %f)", saved_pos.x, saved_pos.y, pos.x, pos.y, dist);
                        qDebug("        due to %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);*/
                        nudged = true;
                    }
                }
            }
            // n.b., only care about the closest point on the line seg, and not the end points of the line seg - as we'll look at the adjacent boundary segments to find the real closest point
        }
    }


    if( nudged ) {
        //qDebug("nudged %f, %f to %f, %f", saved_pos.x, saved_pos.y, pos.x, pos.y);
    }
    return pos;
}
#endif

bool Location::findFleePoint(Vector2D *result, Vector2D from, Vector2D fleeing_from, bool can_fly) const {
    //qDebug("findFleePoint");
    //QElapsedTimer timer;
    //timer.start();

    /*bool found = false;
    bool stop_search = false;
    Vector2D flee_pos;
    float max_dist = 0.0f;
    for(vector<PathWayPoint>::const_iterator iter = path_way_points.begin(); iter != path_way_points.end() && !stop_search; ++iter) {
        const PathWayPoint path_way_point = *iter;
        QElapsedTimer timer1;
        timer1.start();
        vector<Vector2D> new_path = this->calculatePathTo(from, path_way_point.point, NULL, can_fly);
        if( timer1.elapsed() > 0 ) {
            qDebug("    time1: %d", timer1.elapsed());
            qDebug("    path length %d", new_path.size());
        }
        //qDebug("from: %f, %f", from.x, from.y);
        //qDebug("fleeing_from: %f, %f", fleeing_from.x, fleeing_from.y);
        //qDebug("path_way_point.point: %f, %f", path_way_point.point.x, path_way_point.point.y);
        if( new_path.size() > 0 ) {
            //qDebug(">>> new_path size %d", new_path.size());
            float dist = (new_path.at(0) - from).magnitude();
            for(size_t i=0;i<new_path.size()-1;i++) {
                Vector2D p0 = new_path.at(i);
                Vector2D p1 = new_path.at(i+1);
                float this_dist = (p1 - p0).magnitude();
                dist += this_dist;
            }
            for(size_t i=0;i<new_path.size();i++) {
                Vector2D p0 = new_path.at(i);
                //qDebug("    %d : %f, %f", i, p0.x, p0.y);
            }

            if( !found || dist > max_dist + E_TOL_LINEAR ) {
                QElapsedTimer timer2;
                timer2.start();
                vector<Vector2D> new_path2 = this->calculatePathTo(fleeing_from, path_way_point.point, NULL, can_fly); // note, still use can_fly flag here
                if( timer2.elapsed() > 0 ) {
                    qDebug("    time2: %d", timer2.elapsed());
                    qDebug("    path length %d", new_path2.size());
                }
                float dist2 = 0.0f;
                if( new_path2.size() > 0 ) {
                    dist2 += (new_path2.at(0) - fleeing_from).magnitude();
                }
                for(size_t i=0;i<new_path2.size()-1;i++) {
                    Vector2D p0 = new_path2.at(i);
                    Vector2D p1 = new_path2.at(i+1);
                    float this_dist = (p1 - p0).magnitude();
                    dist2 += this_dist;
                }
                for(size_t i=0;i<new_path2.size();i++) {
                    Vector2D p0 = new_path2.at(i);
                    //qDebug("    %d : %f, %f", i, p0.x, p0.y);
                }
                //qDebug("%f : %f", dist, dist2);
                if( dist < dist2 )
                {
                    found = true;
                    flee_pos = path_way_point.point;
                    max_dist = dist;
                    if( new_path.size() > 1 ) {
                        // needed for performance - the flee point is good enough if it's not in direct line of sight
                        stop_search = true;
                    }
                }
            }
        }
    }

    qDebug("time: %d", timer.elapsed());
    *result = flee_pos;
    return found;
    */

    // needs to be fast!
    Vector2D diff = from - fleeing_from;
    bool coi = true;
    if( diff.magnitude() > E_TOL_LINEAR ) {
        diff.normalise();
        coi = false;
    }
    vector<PathWayPoint> candidates;
    for(vector<PathWayPoint>::const_iterator iter = path_way_points.begin(); iter != path_way_points.end(); ++iter) {
        const PathWayPoint path_way_point = *iter;
        Vector2D dir = path_way_point.point - from;
        if( dir.magnitude() > npc_radius_c ) {
            dir.normalise();
            if( coi || diff % dir > -E_TOL_LINEAR ) {
                candidates.push_back(path_way_point);
            }
        }
    }
    if( candidates.size() == 0 ) {
        return false;
    }
    int r = rand() % candidates.size();
    Vector2D flee_pos = candidates.at(r).point;

    // check that the route won't take us past the flee_from point
    // note we shouldn't do this when collecting candidates, as would be too slow to do on every way point!
    vector<Vector2D> new_path = this->calculatePathTo(from, flee_pos, NULL, can_fly);
    if( new_path.size() == 0 ) {
        //qDebug("can't get to flee point");
        return false;
    }
    vector<Vector2D> new_path2 = this->calculatePathTo(fleeing_from, flee_pos, NULL, can_fly); // note, still use can_fly flag here
    bool ok = true;
    if( new_path2.size() > 0 ) {
        /*float dist = (new_path.at(0) - from).magnitude();
        for(size_t i=0;i<new_path.size()-1;i++) {
            Vector2D p0 = new_path.at(i);
            Vector2D p1 = new_path.at(i+1);
            float this_dist = (p1 - p0).magnitude();
            dist += this_dist;
        }
        float dist2 = (new_path2.at(0) - fleeing_from).magnitude();
        for(size_t i=0;i<new_path2.size()-1;i++) {
            Vector2D p0 = new_path2.at(i);
            Vector2D p1 = new_path2.at(i+1);
            float this_dist = (p1 - p0).magnitude();
            dist2 += this_dist;
        }*/
        float dist = Location::distanceOfPath(from, new_path, false, 0.0f);
        float dist2 = Location::distanceOfPath(fleeing_from, new_path2, false, 0.0f);
        //qDebug("%f : %f", dist, dist2);
        if( dist > dist2 + E_TOL_LINEAR ) {
            ok = false;
        }
    }
    if( !ok ) {
        return false;
    }

    //qDebug("time: %d", timer.elapsed());
    *result = flee_pos;
    return true;
}

bool Location::findFreeWayPoint(Vector2D *result, Vector2D from, bool visible, bool can_fly) const {
    vector<Vector2D> candidates;
    for(vector<PathWayPoint>::const_iterator iter = path_way_points.begin(); iter != path_way_points.end(); ++iter) {
        const PathWayPoint path_way_point = *iter;
        if( this->collideWithTransient(NULL, path_way_point.point) ) {
            continue;
        }
        Vector2D hit_pos;
        //bool is_visible = !this->intersectSweptSquareWithBoundaries(&hit_pos, false, from, path_way_point.point, 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL, false);
        // remember that visibility test also limits by a range of npc_visibility_c
        bool is_visible = this->visibilityTest(from, path_way_point.point);
        if( is_visible == visible ) {
            // test we can actually get to the "from" position from this way point (avoids problem of wandering monsters appearing trapped behind scenery etc)
            vector<Vector2D> new_path = this->calculatePathTo(path_way_point.point, from, NULL, can_fly);
            if( new_path.size() > 0 ) {
                candidates.push_back(path_way_point.point);
            }
        }
    }

    if( candidates.size() == 0 ) {
        return false;
    }
    int r = rand() % candidates.size();
    *result = candidates.at(r);
    return true;
}

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

bool Location::visibilityTest(Vector2D src, Vector2D dest) const {
    bool is_visible = false;
    float dist = ( dest - src ).magnitude();
    if( dist <= hit_range_c ) {
        // assume always visible
        is_visible = true;
    }
    else if( dist <= npc_visibility_c ) {
        // check line of sight
        Vector2D hit_pos;
        if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, src, dest, 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL, false) ) {
            is_visible = true;
        }
    }
    return is_visible;
}

/*bool Location::pointInside(Vector2D point) const {
    // tests whether a point is within the valid location region
}*/

void Location::testActivatePathWayPoint(PathWayPoint *path_way_point) const {
    // test we get get to the way point
    // important to use E_TOL_LINEAR for the width, otherwise can cause problems for scenery right next to the wall
    Vector2D hit_pos;
    if( !this->intersectSweptSquareWithBoundaries(&hit_pos, false, path_way_point->origin_point, path_way_point->point, E_TOL_LINEAR, Location::INTERSECTTYPE_MOVE, NULL, false) ) {
        path_way_point->active = true;
    }
}

void Location::calculatePathWayPoints() {
    //qDebug("Location::calculatePathWayPoints()");
    path_way_points.clear();
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
                Vector2D path_way_point_pos = point + inwards * offset;

                PathWayPoint path_way_point(point, path_way_point_pos, boundary->getSource());
                float turn_sign = d0 % normal_from_wall1;
                if( turn_sign < E_TOL_LINEAR ) {
                    path_way_point.used_for_pathfinding = false;
                }
                this->testActivatePathWayPoint(&path_way_point);
                path_way_points.push_back( path_way_point );
            }
        }
    }
}

/*void Location::refreshDistanceGraph() {
    LOG("Location::refreshDistanceGraph()\n");
}*/

bool Location::testGraphVerticesHit(float *dist, GraphVertex *v_A, GraphVertex *v_B, const void *ignore, bool can_fly) const {
    bool hit = false;
    Vector2D A = v_A->getPos();
    Vector2D B = v_B->getPos();

    *dist = (A - B).magnitude();
    if( *dist <= E_TOL_LINEAR ) {
        // needed for coi way points?
        hit = false;
    }
    else {
        Vector2D hit_pos;
        hit = this->intersectSweptSquareWithBoundaries(&hit_pos, false, A, B, npc_radius_c, INTERSECTTYPE_MOVE, ignore, can_fly);
    }
    return hit;
}

void Location::calculateDistanceGraph() {
    //qDebug("Location::calculateDistanceGraph()");
    //int time_s = clock();
    if( this->distance_graph != NULL ) {
        delete this->distance_graph;
    }
    this->distance_graph = new Graph();

    this->calculatePathWayPoints();
    //LOG("Location::calculateDistanceGraph() calculatePathWayPoints() time taken: %d\n", clock() - time_s);

    //int n_hits = 0;
    for(size_t i=0;i<path_way_points.size();i++) {
        PathWayPoint path_way_point = path_way_points.at(i);
        if( path_way_point.active && path_way_point.used_for_pathfinding ) {
            Vector2D A = path_way_point.point;
            GraphVertex vertex(A, path_way_point.source);
            this->distance_graph->addVertex(vertex);
        }
    }

    for(size_t i=0;i<this->distance_graph->getNVertices();i++) {
        GraphVertex *v_A = this->distance_graph->getVertex(i);
        for(size_t j=i+1;j<this->distance_graph->getNVertices();j++) {
            GraphVertex *v_B = this->distance_graph->getVertex(j);
            float dist = 0.0f;
            bool hit = testGraphVerticesHit(&dist, v_A, v_B, NULL, false);
            if( !hit ) {
                v_A->addNeighbour(j, dist);
                v_B->addNeighbour(i, dist);
                //n_hits++;
            }
        }
    }
    //qDebug("Location::calculateDistanceGraph(): %d hits", n_hits);
    //qDebug("Location::calculateDistanceGraph() total time taken: %d", clock() - time_s);
}

vector<Vector2D> Location::calculatePathTo(Vector2D src, Vector2D dest, const void *ignore, bool can_fly) const {
    vector<Vector2D> new_path;
    //qDebug("ignore: %d", ignore);

    Vector2D hit_pos;
    if( src == dest || !this->intersectSweptSquareWithBoundaries(&hit_pos, false, src, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, ignore, can_fly) ) {
        // easy
        //qDebug("direct path from %f, %f to %f, %f", src.x, src.y, dest.x, dest.y);
        new_path.push_back(dest);
    }
    else {
        //qDebug("    calculate path\n");
        const Graph *distance_graph = this->getDistanceGraph();
        Graph *graph = distance_graph->clone();

        size_t n_old_vertices = graph->getNVertices();
        GraphVertex start_vertex(src, NULL);
        GraphVertex end_vertex(dest, NULL);
        size_t start_index = graph->addVertex(start_vertex);
        size_t end_index = graph->addVertex(end_vertex);

        // n.b., don't need to check for link between start_vertex and end_vertex, as this code path is only for where we can't walk directly between the start and end!
        for(size_t i=0;i<n_old_vertices;i++) {
            GraphVertex *v_A = graph->getVertex(i);
            for(size_t j=n_old_vertices;j<graph->getNVertices();j++) {
                GraphVertex *v_B = graph->getVertex(j);
                float dist = 0.0f;
                bool hit = testGraphVerticesHit(&dist, v_A, v_B, j==end_index ? ignore : NULL, can_fly); // only the last segment of the path should ignore the "ignore"
                if( !hit ) {
                    v_A->addNeighbour(j, dist);
                    v_B->addNeighbour(i, dist);
                }
            }
        }

        vector<GraphVertex *> shortest_path = graph->shortestPath(start_index, end_index);
        if( shortest_path.size() == 0 ) {
            // can't reach destination (or already at it)
            //qDebug("    can't reach destination (or already at it)\n");
        }
        else {
            for(vector<GraphVertex *>::const_iterator iter = shortest_path.begin(); iter != shortest_path.end(); ++iter) {
                const GraphVertex *vertex = *iter;
                new_path.push_back(vertex->getPos());
            }
        }

        delete graph;
    }

    if( new_path.size() > 0 ) {
        //LOG("set path\n");
        if( ignore != NULL ) {
            Vector2D p0 = src;
            if( new_path.size() >= 2 ) {
                p0 = new_path.at( new_path.size() - 2 );
            }
            if( this->intersectSweptSquareWithBoundaries(&hit_pos, true, p0, dest, npc_radius_c, Location::INTERSECTTYPE_MOVE, NULL, can_fly) ) {
                new_path[ new_path.size() - 1 ] = hit_pos;
            }
        }
    }

    return new_path;
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

#if 0
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
#endif

void Location::clearVisibility() {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        floor_region->setVisible(false);
    }
}

void Location::revealMap(PlayingGamestate *playing_gamestate) {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->isVisible() ) {
            continue;
        }
        floor_region->setVisible(true);
        playing_gamestate->updateVisibilityForFloorRegion(floor_region);
    }
}

vector<FloorRegion *> Location::updateVisibility(Vector2D pos) {
    //qDebug("Location::updateVisibility for %f, %f", pos.x, pos.y);
    vector<FloorRegion *> update_floor_regions;
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        if( floor_region->isVisible() ) {
            continue;
        }

        Vector2D top_left = floor_region->getTopLeft();
        Vector2D bottom_right = floor_region->getBottomRight();
        float xdiff = 0.0f, ydiff = 0.0f;
        if( pos.x < top_left.x )
            xdiff = top_left.x - pos.x;
        else if( pos.x > bottom_right.x )
            xdiff = pos.x - bottom_right.x;
        if( pos.y < top_left.y )
            ydiff = top_left.y - pos.y;
        else if( pos.y > bottom_right.y )
            ydiff = pos.y - bottom_right.y;
        float dist2 = xdiff*xdiff + ydiff*ydiff;
        if( dist2 - E_TOL_LINEAR > npc_visibility_c*npc_visibility_c ) {
            continue;
        }
        //qDebug("TEST");

        for(size_t j=0;j<floor_region->getNPoints() && !floor_region->isVisible();j++) {
            //Vector2D point = floor_region->getPoint(j);
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
        if( !floor_region->isVisible() ) {
            // also test centre point
            Vector2D centre_pos = floor_region->findCentre();
            Vector2D hit_pos;
            // use E_TOL_LINEAR, to avoid line of sight slipping between two adjacent items
            if( pos == centre_pos || !this->intersectSweptSquareWithBoundaries(&hit_pos, false, pos, centre_pos, E_TOL_LINEAR, INTERSECTTYPE_VISIBILITY, NULL, false) ) {
                floor_region->setVisible(true);
                update_floor_regions.push_back(floor_region);
            }
        }
        if( !floor_region->isVisible() ) {
            // also test if we're inside this region!
            if( floor_region->pointInside(pos) ) {
                floor_region->setVisible(true);
                update_floor_regions.push_back(floor_region);
            }
        }
    }
    //qDebug("Location::updateVisibility done");
    return update_floor_regions;
}

set<Scenery *> Location::getSceneryUnlockedBy(const string &unlocked_by_template) {
    set<Scenery *> ret_scenery;
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( scenery->getUnlockItemName() == unlocked_by_template ) {
           ret_scenery.insert(scenery);
        }
    }
    return ret_scenery;
}

Scenery *Location::findScenery(const string &scenery_name) {
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( scenery->getName() == scenery_name ) {
            return scenery;
        }
    }
    return NULL;
}

Character *Location::findCharacter(const string &character_name) {
    for(set<Character *>::iterator iter = characters.begin(); iter != characters.end(); ++iter) {
        Character *character = *iter;
        if( character->getName() == character_name ) {
            return character;
        }
    }
    return NULL;
}

vector<Item *> Location::getItems(const string &item_name, bool include_scenery, bool include_characters, vector<Scenery *> *scenery_owners, vector<Character *> *character_owners) {
    vector<Item *> ret_items;
    for(set<Item *>::iterator iter = items.begin(); iter != items.end(); ++iter) {
        Item *item = *iter;
        if( item->getName() == item_name ) {
            ret_items.push_back(item);
            if( scenery_owners != NULL )
                scenery_owners->push_back(NULL);
            if( character_owners != NULL )
                character_owners->push_back(NULL);
        }
    }
    if( include_scenery ) {
        for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
            Scenery *scenery = *iter;
            for(set<Item *>::iterator iter2 = scenery->itemsBegin(); iter2 != scenery->itemsEnd(); ++iter2) {
                Item *item = *iter2;
                if( item->getName() == item_name ) {
                    ret_items.push_back(item);
                    if( scenery_owners != NULL )
                        scenery_owners->push_back(scenery);
                    if( character_owners != NULL )
                        character_owners->push_back(NULL);
                }
            }
        }
    }
    if( include_characters ) {
        for(set<Character *>::iterator iter = characters.begin(); iter != characters.end(); ++iter) {
            Character *character = *iter;
            for(set<Item *>::iterator iter2 = character->itemsBegin(); iter2 != character->itemsEnd(); ++iter2) {
                Item *item = *iter2;
                if( item->getName() == item_name ) {
                    ret_items.push_back(item);
                    if( scenery_owners != NULL )
                        scenery_owners->push_back(NULL);
                    if( character_owners != NULL )
                        character_owners->push_back(character);
                }
            }
        }
    }
    return ret_items;
}

QuestObjective::QuestObjective(const string &type, const string &arg1, int gold) : type(type), arg1(arg1), gold(gold) {
}

bool QuestObjective::testIfComplete(const PlayingGamestate *playing_gamestate, const Quest *quest) const {
    ASSERT_LOGGER( playing_gamestate->getPlayer() != NULL );
    bool complete = false;
    if( type == "kill" ) {
        if( arg1 == "all_hostile" ) {
            complete = true;
            for(vector<Location *>::const_iterator iter = quest->locationsBegin(); iter != quest->locationsEnd() && complete; ++iter) {
                const Location *location = *iter;
                for(set<Character *>::const_iterator iter2 = location->charactersBegin(); iter2 != location->charactersEnd() && complete; ++iter2) {
                    const Character *character = *iter2;
                    if( character->isHostile() && !character->isDead() ) {
                        //qDebug("ping still character: %s", character->getName().c_str());
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
    else if( type == "find_item" ) {
        if( playing_gamestate->getPlayer()->findItem(arg1) ) {
            complete = true;
        }
    }
    else if( type == "find_exit" ) {
        // always set externally
        complete = quest->isCompleted();
    }
    else {
        LOG("unknown type: %s\n", type.c_str());
        ASSERT_LOGGER(false);
    }
    return complete;
}

void QuestObjective::completeQuest(PlayingGamestate *playing_gamestate) const {
    Character *player = playing_gamestate->getPlayer();
    player->addGold(this->gold);

    if( type == "kill" ) {
    }
    else if( type == "find_item" ) {
        Item *item = player->findItem(arg1);
        ASSERT_LOGGER(item != NULL);
        if( item != NULL ) {
            player->takeItem(item);
            delete item;
        }
    }
    else if( type == "find_exit" ) {
    }
    else {
        LOG("unknown type: %s\n", type.c_str());
        ASSERT_LOGGER(false);
    }
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

bool Quest::testIfComplete(const PlayingGamestate *playing_gamestate) {
    if( quest_objective != NULL ) {
        this->is_completed = quest_objective->testIfComplete(playing_gamestate, this);
        return this->is_completed;
    }
    return false;
}
