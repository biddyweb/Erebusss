#include "locationgenerator.h"
#include "location.h"
#include "item.h"
#include "../playinggamestate.h"
#include "../logiface.h"

NPCGroup::~NPCGroup() {
    for(vector<Character *>::iterator iter = this->npcs.begin(); iter != this->npcs.end(); ++iter) {
        Character *npc = *iter;
        delete npc;
    }
}

NPCTableLevel::~NPCTableLevel() {
    for(vector<NPCGroup *>::iterator iter = this->npc_groups.begin(); iter != this->npc_groups.end(); ++iter) {
        NPCGroup *npc_group = *iter;
        delete npc_group;
    }
}

NPCTable::~NPCTable() {
    for(map<int, NPCTableLevel *>::iterator iter = this->levels.begin(); iter != this->levels.end(); ++iter) {
        NPCTableLevel *level = iter->second;
        delete level;
    }
}

const NPCGroup *NPCTable::chooseGroup(int level) const {
    const NPCTableLevel *npc_table_level = this->getLevel(level);
    if( npc_table_level == NULL ) {
        LOG("NPCTable::chooseGroup no table for level %d\n", level);
        throw string("no table for this level");
    }
    const NPCGroup *npc_group = npc_table_level->chooseGroup();
    return npc_group;
}

Direction4 rotateDirection4(Direction4 dir, int turn) {
    int i_dir = (int)dir;
    i_dir += turn;
    while( i_dir < 0 )
        i_dir += 4;
    while( i_dir >= 4 )
        i_dir -= 4;
    dir = (Direction4)i_dir;
    return dir;
}

Vector2D directionFromEnum(Direction4 dir) {
    if( dir == DIRECTION4_NORTH )
        return Vector2D(0.0f, -1.0f);
    else if( dir == DIRECTION4_EAST )
        return Vector2D(1.0f, 0.0f);
    else if( dir == DIRECTION4_SOUTH )
        return Vector2D(0.0f, 1.0f);
    return Vector2D(-1.0f, 0.0f);
}

bool LocationGenerator::collidesWithFloorRegions(const vector<Rect2D> *floor_regions_rects, const vector<Rect2D> *ignore_rects, Rect2D rect, float gap) {
    rect.expand(gap);
    for(vector<Rect2D>::const_iterator iter = floor_regions_rects->begin(); iter != floor_regions_rects->end(); ++iter) {
        Rect2D test_rect = *iter;
        bool ignore = false;
        if( ignore_rects != NULL ) {
            for(vector<Rect2D>::const_iterator iter2 = ignore_rects->begin(); iter2 != ignore_rects->end() && !ignore; ++iter2) {
                Rect2D ignore_rect = *iter2;
                if( test_rect == ignore_rect ) {
                    ignore = true;
                }
            }
        }
        if( ignore ) {
            continue;
        }
        else if( test_rect.overlaps(rect) ) {
            return true;
        }
    }
    return false;
}

const float base_passage_length = 7.5f;
const int max_passage_enemies = 6;
//const float base_room_size = base_passage_length - 2.5f;
const int base_room_size = 5;
const float passage_width = 1.0f;
const float passage_hwidth = 0.5f * passage_width;
const float door_width = 1.0f;
const float door_depth = 1.25f;

void LocationGenerator::exploreFromSeedRoomPassageway(Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects) {
    Vector2D dir_vec = directionFromEnum(seed.dir);
    //Vector2D door_centre = seed.pos + dir_vec * 0.5f * door_depth;
    //Vector2D door_size = ( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) ? Vector2D(door_width, door_depth) : Vector2D(door_depth, door_width);
    Vector2D door_centre = seed.pos + dir_vec * 0.5f * ( door_depth + passage_width );
    Vector2D door_size = ( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) ? Vector2D(door_width, door_depth + passage_width) : Vector2D(door_depth + passage_width, door_width);
    Rect2D door_rect(door_centre - door_size*0.5f, door_centre + door_size*0.5f);
    if( door_rect.getX() < 0.0f )
        return;
    if( door_rect.getY() < 0.0f )
        return;
    bool collides_door = LocationGenerator::collidesWithFloorRegions(floor_regions_rects, &seed.ignore_rects, door_rect, 1.0f);
    if( !collides_door ) {
        Vector2D passageway_centre = door_centre + dir_vec * ( 0.5f*door_depth + passage_hwidth );
        FloorRegion *floor_region = FloorRegion::createRectangle(door_rect);
        location->addFloorRegion(floor_region);
        floor_regions_rects->push_back(door_rect);

        Direction4 l_dir = rotateDirection4(seed.dir, -1);
        Direction4 r_dir = rotateDirection4(seed.dir, 1);
        Vector2D l_dir_vec = directionFromEnum(l_dir);
        Vector2D r_dir_vec = directionFromEnum(r_dir);
        {
            Seed new_seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, door_centre + dir_vec * passage_hwidth + l_dir_vec*passage_hwidth, l_dir);
            new_seed.addIgnoreRect(door_rect);
            qDebug("    add left turn at %f, %f", new_seed.pos.x, new_seed.pos.y);
            seeds->push_back(new_seed);
        }
        {
            Seed new_seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, door_centre + dir_vec * passage_hwidth + r_dir_vec*passage_hwidth, r_dir);
            new_seed.addIgnoreRect(door_rect);
            qDebug("    add right turn at %f, %f", new_seed.pos.x, new_seed.pos.y);
            seeds->push_back(new_seed);
        }
    }
}

Item *LocationGenerator::getRandomItem(const PlayingGamestate *playing_gamestate, int level) {
    int r = rollDice(1, 10, 0);
    Item *item = NULL;
    if( r <= 2 ) {
        item = playing_gamestate->cloneStandardItem("Arrows");
    }
    else if( r == 3 ) {
        item = playing_gamestate->cloneStandardItem("Leather Armour");
    }
    else if( r == 4 ) {
        item = playing_gamestate->cloneStandardItem("Shield");
    }
    else if( r == 5 || r == 6 ) {
        item = playing_gamestate->cloneStandardItem("Dagger");
    }
    else if( r == 7 ) {
        item = playing_gamestate->cloneStandardItem("Short Sword");
    }
    else if( r == 8 ) {
        item = playing_gamestate->cloneStandardItem("Gold Ring");
    }
    else if( r == 9 ) {
        item = playing_gamestate->cloneStandardItem("Wyvern Egg");
    }
    else {
        item = playing_gamestate->cloneStandardItem("Red Gem");
    }
    return item;
}

Item *LocationGenerator::getRandomTreasure(const PlayingGamestate *playing_gamestate, int level) {
    int r = rollDice(1, 14, 0);
    //r = 13;
    Item *item = NULL;
    if( r == 1 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Healing");
    }
    else if( r == 2 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Combat");
    }
    else if( r == 3 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Archery");
    }
    else if( r == 4 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Strength");
    }
    else if( r == 5 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Speed");
    }
    else if( r == 6 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Cure Disease");
    }
    else if( r == 7 ) {
        item = playing_gamestate->cloneStandardItem("Holy Water");
    }
    else if( r == 8 ) {
        item = playing_gamestate->cloneStandardItem("Acid");
    }
    else if( r == 9 ) {
        item = playing_gamestate->cloneStandardItem("Green Gem");
    }
    else if( r == 10 ) {
        item = playing_gamestate->cloneStandardItem("Blue Gem");
    }
    else if( r == 11 ) {
        item = playing_gamestate->cloneStandardItem("Gold Gem");
    }
    else if( r == 12 ) {
        item = playing_gamestate->cloneStandardItem("White Gem");
    }
    else if( r == 13 || r == 14 ) {
        if( r == 13 ) {
            item = playing_gamestate->cloneStandardItem("Dagger");
            item->setName("Magic Dagger");
        }
        else {
            item = playing_gamestate->cloneStandardItem("Short Sword");
            item->setName("Magic Short Sword");
        }
        item->setMagical(true);
        int damageX = 0, damageY = 0, damageZ = 0;
        Weapon *weapon = static_cast<Weapon *>(item);
        weapon->getDamage(&damageX, &damageY, &damageZ);
        damageZ += level;
        weapon->setDamage(damageX, damageY, damageZ);
    }
    return item;
}

void LocationGenerator::exploreFromSeedXRoom(Scenery **exit_down, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, const map<string, NPCTable *> &npc_tables, int level, int n_levels, LocationGeneratorInfo *generator_info) {
    Vector2D room_dir_vec = directionFromEnum(seed.dir);
    Vector2D door_centre = seed.pos + room_dir_vec * 0.5f * door_depth;
    Vector2D door_size = ( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) ? Vector2D(door_width, door_depth) : Vector2D(door_depth, door_width);
    Rect2D door_rect(door_centre - door_size*0.5f, door_centre + door_size*0.5f);
    if( door_rect.getX() < 0.0f )
        return;
    if( door_rect.getY() < 0.0f )
        return;
    bool collides_door = LocationGenerator::collidesWithFloorRegions(floor_regions_rects, &seed.ignore_rects, door_rect, 1.0f);
    if( !collides_door ) {
        enum RoomType {
            ROOMTYPE_NORMAL = 0,
            ROOMTYPE_HAZARD = 1,
            ROOMTYPE_LAIR = 2,
            ROOMTYPE_QUEST = 3
        };
        RoomType room_type = ROOMTYPE_NORMAL;
        float room_size_w = base_room_size;
        float room_size_h = base_room_size;
        int roll = rollDice(1, 12, 0);
        /*if( rollDice(1, 2, 0) == 1 ) {
            roll = 8; // test
        }*/
        if( roll <= 6 ) {
            // normal room
            room_type = ROOMTYPE_NORMAL;
        }
        else if( roll <= 8 ) {
            // hazard
            room_type = ROOMTYPE_HAZARD;
        }
        else if( roll <= 10 ) {
            // lair
            room_type = ROOMTYPE_LAIR;
        }
        else {
            // quest room
            room_type = ROOMTYPE_QUEST;
        }

        if( room_type == ROOMTYPE_QUEST && generator_info->n_rooms_quest > 0 ) {
            // only 1 quest room per level
            room_type = ROOMTYPE_LAIR;
        }
        else if( room_type == ROOMTYPE_QUEST && ( generator_info->n_rooms_normal == 0 || generator_info->n_rooms_hazard == 0 || generator_info->n_rooms_lair == 0 ) ) {
            // need at least 1 of each of the other room types, before having a quest room
            if( generator_info->n_rooms_normal == 0 ) {
                room_type = ROOMTYPE_NORMAL;
            }
            else if( generator_info->n_rooms_hazard == 0 ) {
                room_type = ROOMTYPE_HAZARD;
            }
            else {
                room_type = ROOMTYPE_LAIR;
            }
        }

        if( room_type == ROOMTYPE_LAIR ) {
            if( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) {
                room_size_h *= 2.0f;
            }
            else {
                room_size_w *= 2.0f;
            }
        }
        else if( room_type == ROOMTYPE_QUEST ) {
            if( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) {
                room_size_h *= 2.0f;
            }
            else {
                room_size_w *= 2.0f;
            }
        }

        float into_room_size = ( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) ? room_size_h : room_size_w;
        Vector2D room_centre = door_centre + room_dir_vec * ( 0.5f*door_depth + 0.5f*into_room_size );
        Rect2D room_rect(room_centre - Vector2D(0.5f*room_size_w, 0.5f*room_size_h), room_centre + Vector2D(0.5f*room_size_w, 0.5f*room_size_h));
        if( room_rect.getX() < 0.0f )
            return;
        if( room_rect.getY() < 0.0f )
            return;
        bool collides_room = LocationGenerator::collidesWithFloorRegions(floor_regions_rects, NULL, room_rect, 1.0f);
        if( !collides_room ) {
            FloorRegion *floor_region = FloorRegion::createRectangle(door_rect);
            location->addFloorRegion(floor_region);
            floor_regions_rects->push_back(door_rect);

            bool rounded_rectangle = false;
            if( (room_type == ROOMTYPE_NORMAL || room_type == ROOMTYPE_HAZARD) && rollDice(1, 2, 0) == 1 ) {
                rounded_rectangle = true;
            }
            floor_region = NULL;
            if( rounded_rectangle ) {
                const float corner_dist = 1.0f;
                floor_region = new FloorRegion();
                floor_region->addPoint(room_rect.getTopLeft() + Vector2D(0.0f, corner_dist));
                floor_region->addPoint(room_rect.getBottomLeft() - Vector2D(0.0f, corner_dist));
                floor_region->addPoint(room_rect.getBottomLeft() + Vector2D(corner_dist, 0.0f));
                floor_region->addPoint(room_rect.getBottomRight() - Vector2D(corner_dist, 0.0f));
                floor_region->addPoint(room_rect.getBottomRight() - Vector2D(0.0f, corner_dist));
                floor_region->addPoint(room_rect.getTopRight() + Vector2D(0.0f, corner_dist));
                floor_region->addPoint(room_rect.getTopRight() - Vector2D(corner_dist, 0.0f));
                floor_region->addPoint(room_rect.getTopLeft() + Vector2D(corner_dist, 0.0f));

                /*for(size_t i=0;i<floor_region->getNPoints();i++) {
                    Vector2D vec = floor_region->getPoint(i);
                    qDebug("%d : %f, %f", i, vec.x, vec.y);
                }*/
            }
            else {
                floor_region = FloorRegion::createRectangle(room_rect);
            }
            location->addFloorRegion(floor_region);
            floor_regions_rects->push_back(room_rect);

            if( room_type == ROOMTYPE_NORMAL ) {
                generator_info->n_rooms_normal++;
            }
            else if( room_type == ROOMTYPE_HAZARD ) {
                generator_info->n_rooms_hazard++;
            }
            else if( room_type == ROOMTYPE_LAIR ) {
                generator_info->n_rooms_lair++;
            }
            else if( room_type == ROOMTYPE_QUEST ) {
                generator_info->n_rooms_quest++;
            }

            // TODO: place door scenery itself
            Scenery *scenery_corner = NULL;
            string enemy_table;
            if( room_type == ROOMTYPE_NORMAL ) {
                // normal room
                // 50% chance of barrel or crate in corner
                if( rollDice(1, 2, 0) == 1 )
                {
                    string name, image_name;
                    if( rollDice(1, 2, 0) == 1 ) {
                        name = "Barrel";
                        image_name = "barrel";
                    }
                    else {
                        name = "Crate";
                        image_name = "crate";
                    }
                    float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, image_name, true, 0.8f, 0.0f, 0.0f, false, 0.0f);
                    scenery_corner = new Scenery(name, image_name, size_w, size_h, visual_h, false, 0.0f);
                    scenery_corner->setBlocking(true, false);
                    scenery_corner->setCanBeOpened(true);
                    int gold = rollDice(1, 12, 0);
                    scenery_corner->addItem( playing_gamestate->cloneGoldItem(gold) );
                    if( rollDice(1, 4, 0) == 1 ) {
                        // also add an item
                        Item *item = getRandomItem(playing_gamestate, level);
                        scenery_corner->addItem(item);
                    }
                }
            }
            else if( room_type == ROOMTYPE_HAZARD ) {
                // hazard
                int r = rollDice(1, 7, 0);
                //r = 2; // test
                if( r == 1 ) {
                    // wandering monster
                    enemy_table = "isolated";
                }
                else if( r == 2 ) {
                    // npc - trader
                    string npc_animation_name = (rollDice(1, 2, 0) == 1) ? "man" : "peasant_woman";
                    Character *npc = new Character("Trader", npc_animation_name, true);
                    npc->setHostile(false);
                    const Shop *shop = playing_gamestate->getRandomShop(true);
                    npc->setShop(shop->getName());
                    location->addCharacter(npc, room_centre.x, room_centre.y);
                }
                else if( r == 3 ) {
                    // hazard monster
                    enemy_table = "hazard";
                }
                else if( r == 4 ) {
                    // mushrooms
                    int n_mushrooms = rollDice(2, 4, 0);
                    for(int i=0;i<n_mushrooms;i++) {
                        int pos_x = rand() % (int)room_size_w;
                        int pos_y = rand() % (int)room_size_h;
                        if( rounded_rectangle ) {
                            if( pos_x == 0 && pos_y == 0 ) {
                                pos_x++;
                            }
                            else if( pos_x == room_size_w-1 && pos_y == 0 ) {
                                pos_x--;
                            }
                            else if( pos_x == 0 && pos_y == room_size_h-1 ) {
                                pos_x++;
                            }
                            else if( pos_x == room_size_w-1 && pos_y == room_size_h-1 ) {
                                pos_x--;
                            }
                        }
                        Vector2D item_pos = room_rect.getTopLeft() + Vector2D(1.0f, 0.0f)*(pos_x + 0.5f) + Vector2D(0.0f, 1.0f)*(pos_y + 0.5f);
                        Item *item = playing_gamestate->cloneStandardItem("Mushroom");
                        location->addItem(item, item_pos.x, item_pos.y);
                    }
                }
                else if( r == 5 || r == 6 ) {
                    // shrine or bell
                    string scenery_name, scenery_image_name, interact_type;
                    if( r == 5 ) {
                        scenery_name = "Shrine";
                        scenery_image_name = "shrine";
                        interact_type ="INTERACT_TYPE_SHRINE";
                    }
                    else {
                        scenery_name = "Bell";
                        scenery_image_name = "church_bell";
                        interact_type = "INTERACT_TYPE_BELL";
                    }
                    float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_image_name, true, 1.0f, 0.0f, 0.0f, false, 0.0f);
                    Scenery *scenery = new Scenery(scenery_name, scenery_image_name, size_w, size_h, visual_h, false, 0.0f);
                    scenery->setInteractType(interact_type);
                    scenery->setBlocking(true, false);
                    location->addScenery(scenery, room_centre.x, room_centre.y);
                }
                else if( r == 7 ) {
                    // tomb
                    string scenery_image_name = "tomb";
                    float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_image_name, true, 1.0f, 0.0f, 0.0f, false, 0.0f);
                    Scenery *scenery = new Scenery("Tomb", scenery_image_name, size_w, size_h, visual_h, false, 0.0f);
                    scenery->setBlocking(true, false);
                    if( rollDice(1, 2, 0) == 1 ) {
                        int gold = rollDice(3, 6, 0);
                        scenery->addItem( playing_gamestate->cloneGoldItem(gold) );
                    }
                    if( rollDice(1, 2, 0) == 1 ) {
                        int rating = level+1;
                        int difficulty = level;
                        Trap *trap = new Trap("acid", passage_width, passage_width);
                        trap->setRating(rating);
                        trap->setDifficulty(difficulty);
                        scenery->setTrap(trap);
                    }
                    location->addScenery(scenery, room_centre.x, room_centre.y);
                }
            }
            else if( room_type == ROOMTYPE_LAIR || room_type == ROOMTYPE_QUEST ) {
                // lair/quest
                enemy_table = "lair"; // using same table as lairs for now

                string name = "Chest", image_name = "chest";
                float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, image_name, true, 0.8f, 0.0f, 0.0f, false, 0.0f);
                scenery_corner = new Scenery(name, image_name, size_w, size_h, visual_h, false, 0.0f);
                scenery_corner->setBlocking(true, false);
                scenery_corner->setCanBeOpened(true);
                int gold = room_type == ROOMTYPE_LAIR ? rollDice(4, 10, 10) : rollDice(5, 10, 50);
                if( room_type == ROOMTYPE_QUEST && level == n_levels-1 ) {
                    gold += 300;
                }
                scenery_corner->addItem( playing_gamestate->cloneGoldItem(gold) );
                if( rollDice(1, 2, 0) == 1 )
                {
                    // also add an item
                    Item *item = getRandomTreasure(playing_gamestate, level);
                    scenery_corner->addItem(item);
                }

                if( room_type == ROOMTYPE_LAIR ) {
                    string scenery_centre_name, scenery_centre_image_name;
                    float scenery_centre_size = 0.0f;
                    bool size_flat = false;
                    bool scenery_centre_is_blocking = true;
                    bool scenery_centre_blocks_visibility = false;
                    Scenery::DrawType draw_type = Scenery::DRAWTYPE_NORMAL;
                    string scenery_centre_description;
                    int roll = rollDice(1, 100, 0);
                    //roll = 50;
                    if( roll <= 25 ) {
                        scenery_centre_name = "Fire";
                        scenery_centre_image_name = "fire";
                        scenery_centre_size = 1.0;
                    }
                    else if( roll <= 50 ) {
                        scenery_centre_name = "Trapdoor";
                        scenery_centre_image_name = "grate";
                        scenery_centre_size = 0.5;
                        size_flat = true;
                        draw_type = Scenery::DRAWTYPE_BACKGROUND;
                        scenery_centre_is_blocking = false;
                        if( rollDice(1, 2, 0) == 1 ) {
                            scenery_centre_description = "This grate seems stuck or locked, and you are unable to budge it. Peering down, in the darkness you make out a chamber with bones strewn across the floor.";
                        }
                        else {
                            scenery_centre_description = "This grate seems stuck or locked, and you are unable to budge it. You see nothing but blackness beneath.";
                        }
                    }

                    if( scenery_centre_name.length() > 0 ) {
                        float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                        if( size_flat ) {
                            size_w = scenery_centre_size;
                            size_h = scenery_centre_size;
                            visual_h = scenery_centre_size;
                        }
                        else {
                            playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_centre_image_name, true, scenery_centre_size, 0.0f, 0.0f, false, 0.0f);
                        }
                        Scenery *scenery_centre = new Scenery(scenery_centre_name, scenery_centre_image_name, size_w, size_h, visual_h, false, 0.0f);
                        scenery_centre->setDrawType(draw_type);
                        scenery_centre->setBlocking(scenery_centre_is_blocking, scenery_centre_blocks_visibility);
                        if( scenery_centre_description.length() > 0 ) {
                            scenery_centre->setDescription(scenery_centre_description);
                        }
                        Vector2D scenery_pos = room_centre;
                        location->addScenery(scenery_centre, scenery_pos.x, scenery_pos.y);
                    }
                }
                else {
                    if( level == n_levels-1 ) {
                        // final quest location
                    }
                    else if( *exit_down == NULL ) {
                        // also place stairs down, if not already found
                        name = "Stairs";
                        image_name = "stairsdown_indoors";
                        playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, image_name, true, 1.0f, 0.0f, 0.0f, false, 0.0f);
                        Scenery *scenery_stairs_down = new Scenery(name, image_name, size_w, size_h, visual_h, false, 0.0f);
                        scenery_stairs_down->setBlocking(true, false);
                        location->addScenery(scenery_stairs_down, room_centre.x, room_centre.y);
                        *exit_down = scenery_stairs_down;
                    }
                }
            }

            if( scenery_corner != NULL ) {
                int dir_i = rand() % 4;
                int sign_x = (rand() % 2)==0 ? -1 : 1;
                int sign_y = (rand() % 2)==0 ? -1 : 1;
                Vector2D scenery_pos;
                if( rounded_rectangle ) {
                    scenery_pos = room_centre;
                }
                else {
                    const float offset = 0.5f;
                    scenery_pos = room_centre + Vector2D(1.0f, 0.0f) * ( 0.5f*room_size_w - offset ) * sign_x + Vector2D(0.0f, 1.0f) * ( 0.5f*room_size_h - offset ) * sign_y;
                }
                location->addScenery(scenery_corner, scenery_pos.x, scenery_pos.y);
            }

            if( rollDice(1, 2, 0) == 1 )
            {
                // place random background scenery
                string scenery_name;
                float scenery_size = 0.5f;
                bool size_flat = false;
                int r = rollDice(1, 100, 0);
                if( r <= 5 ) {
                    scenery_name = "bloodstain";
                    scenery_size = 0.7f;
                    size_flat = true;
                }
                else if( r <= 10 ) {
                    scenery_name = "bloodstain2";
                    scenery_size = 0.7f;
                    size_flat = true;
                }
                else if( r <= 40 ) {
                    scenery_name = "bones";
                }
                else if( r <= 70 ) {
                    scenery_name = "rock1";
                    scenery_size = 0.2f;
                }
                else {
                    scenery_name = "skulls";
                    scenery_size = 0.3f;
                }
                int pos_x = rand() % (int)room_size_w;
                int pos_y = rand() % (int)room_size_h;
                if( rounded_rectangle ) {
                    if( pos_x == 0 && pos_y == 0 ) {
                        pos_x++;
                    }
                    else if( pos_x == room_size_w-1 && pos_y == 0 ) {
                        pos_x--;
                    }
                    else if( pos_x == 0 && pos_y == room_size_h-1 ) {
                        pos_x++;
                    }
                    else if( pos_x == room_size_w-1 && pos_y == room_size_h-1 ) {
                        pos_x--;
                    }
                }
                Vector2D scenery_pos = room_rect.getTopLeft() + Vector2D(1.0f, 0.0f)*(pos_x + 0.5f) + Vector2D(0.0f, 1.0f)*(pos_y + 0.5f);
                float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                if( size_flat ) {
                    size_w = scenery_size;
                    size_h = scenery_size;
                    visual_h = scenery_size;
                }
                else {
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_name, true, scenery_size, 0.0f, 0.0f, false, 0.0f);
                }
                Scenery *scenery = new Scenery(scenery_name, scenery_name, size_w, size_h, visual_h, false, 0.0f);
                scenery->setDrawType(Scenery::DRAWTYPE_BACKGROUND);
                location->addScenery(scenery, scenery_pos.x, scenery_pos.y);
            }
            if( enemy_table.length() > 0 ) {
                const int n_slots_w = base_room_size, n_slots_h = base_room_size;
                bool slot_filled[n_slots_w*n_slots_h];
                for(int i=0;i<n_slots_w*n_slots_h;i++) {
                    slot_filled[i] = false;
                }
                float slot_scale_x = room_size_w/(float)base_room_size;
                float slot_scale_y = room_size_h/(float)base_room_size;
                map<string, NPCTable *>::const_iterator iter = npc_tables.find(enemy_table);
                if( iter != npc_tables.end() ) {
                    const NPCTable *npc_table = iter->second;
                    const NPCGroup *npc_group = npc_table->chooseGroup(level);
                    int count = 0;
                    for(vector<Character *>::const_iterator iter2 = npc_group->charactersBegin(); iter2 != npc_group->charactersEnd(); ++iter2, count++) {
                        Vector2D npc_pos;
                        while(true) {
                            int slot_x = rand() % n_slots_w;
                            int slot_y = rand() % n_slots_h;
                            if( (slot_x == 0 || slot_x == n_slots_w-1) && (slot_x == 0 || slot_x == n_slots_w-1) ) {
                                // don't put in corners, as we may use them for scenery
                                continue;
                            }
                            if( slot_x == (n_slots_w-1)/2 && slot_y == (n_slots_h-1)/2 ) {
                                // don't put in centre, as we may use it for scenery
                                continue;
                            }
                            if( !slot_filled[slot_y*n_slots_w + slot_x] ) {
                                slot_filled[slot_y*n_slots_w + slot_x] = true;
                                npc_pos = room_rect.getTopLeft() + Vector2D(1.0f, 0.0f)*(slot_x+0.5f)*slot_scale_x + Vector2D(0.0f, 1.0f)*(slot_y+0.5f)*slot_scale_y;
                                break;
                            }
                        }

                        if( npc_pos.x >= room_rect.getBottomRight().x || npc_pos.y >= room_rect.getBottomRight().y ) {
                            LOG("about to place NPC at %f, %f in room %f, %f to %f, %f\n", npc_pos.x, npc_pos.y, room_rect.getTopLeft().x, room_rect.getTopLeft().y, room_rect.getBottomRight().x, room_rect.getBottomRight().y);
                            throw string("invalid location for NPC");
                        }
                        const Character *npc = *iter2;
                        Character *copy = new Character(*npc);
                        location->addCharacter(copy, npc_pos.x, npc_pos.y);
                        if( count == n_slots_w*n_slots_h - 4 - 1 ) { // -4 for corners, -1 for centre
                            break;
                        }
                    }
                }
            }

            //int n_room_doors = rollDice(1, 3, -1);
            int n_room_doors = rollDice(1, 2, 0); // not necessarily the actual number of doors, as we may fail to find room, when exploring from the seed
            vector<Direction4> done_dirs;
            done_dirs.push_back( rotateDirection4(seed.dir, 2) );
            for(int j=0;j<n_room_doors;j++) {
                Direction4 new_room_dir;
                bool done = false;
                while( !done ) {
                    //new_room_dir = (Direction4)(rollDice(1, 4, -1));
                    if( j == 0 )
                        new_room_dir = seed.dir;
                    else {
                        int turn = rollDice(1, 2, 1) == 1 ? 1 : -1;
                        new_room_dir = rotateDirection4(seed.dir, turn);
                    }
                    done = true;
                    for(vector<Direction4>::iterator iter = done_dirs.begin(); iter != done_dirs.end() && done; ++iter) {
                        Direction4 done_dir = *iter;
                        if( done_dir == new_room_dir )
                            done = false;
                    }
                }
                done_dirs.push_back(new_room_dir);

                Vector2D new_room_dir_vec = directionFromEnum(new_room_dir);
                float door_dist = ( new_room_dir == DIRECTION4_NORTH || new_room_dir == DIRECTION4_SOUTH ) ? room_size_h : room_size_w;
                Vector2D door_pos = room_centre + new_room_dir_vec * 0.5 * door_dist;
                Seed::Type seed_type = rollDice(1, 2, 0) == 1 ? Seed::TYPE_X_ROOM : Seed::TYPE_ROOM_PASSAGEWAY;
                Seed new_seed(seed_type, door_pos, new_room_dir);
                new_seed.addIgnoreRect(room_rect);
                qDebug("    add new room from room at %f, %f", new_seed.pos.x, new_seed.pos.y);
                seeds->push_back(new_seed);
            }
        }
    }
}

void LocationGenerator::exploreFromSeed(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, const map<string, NPCTable *> &npc_tables, int level, int n_levels, LocationGeneratorInfo *generator_info) {
    Vector2D dir_vec = directionFromEnum(seed.dir);
    qDebug("explore from seed type %d at %f, %f ; direction %d: %f, %f", seed.type, seed.pos.x, seed.pos.y, seed.dir, dir_vec.x, dir_vec.y);
    if( seed.type == Seed::TYPE_X_ROOM ) {
        exploreFromSeedXRoom(exit_down, playing_gamestate, location, seed, seeds, floor_regions_rects, npc_tables, level, n_levels, generator_info);
        return;
    }
    else if( seed.type == Seed::TYPE_ROOM_PASSAGEWAY ) {
        exploreFromSeedRoomPassageway(location, seed, seeds, floor_regions_rects);
        return;
    }

    vector<Rect2D> ignore_rects = seed.ignore_rects;
    int passage_length_i = 0;
    {
        int roll = first ? 1 : rollDice(1, 10, 0);

        if( roll <= 6 )
            passage_length_i = 1;
        else if( roll <= 9 )
            passage_length_i = 2;
        else
            passage_length_i = 3;
    }
    Vector2D rect_pos, rect_size;
    Vector2D end_pos;
    Rect2D floor_region_rect;
    bool room_for_junction = true;
    while(true) {
        room_for_junction = true;

        bool length_ok = true;
        float passage_length = passage_length_i * base_passage_length;
        end_pos = seed.pos + dir_vec * passage_length;
        if( seed.dir == DIRECTION4_NORTH ) {
            if( seed.pos.y - passage_length < 0.0f ) {
                length_ok = false;
            }
            if( seed.pos.y - (passage_length+passage_width) < 0.0f ) {
                room_for_junction = false;
            }
            rect_pos = Vector2D(seed.pos.x - passage_hwidth, seed.pos.y - passage_length);
            rect_size = Vector2D(passage_width, passage_length);
        }
        else if( seed.dir == DIRECTION4_EAST ) {
            rect_pos = Vector2D(seed.pos.x, seed.pos.y - passage_hwidth);
            rect_size = Vector2D(passage_length, passage_width);
        }
        else if( seed.dir == DIRECTION4_SOUTH ) {
            rect_pos = Vector2D(seed.pos.x - passage_hwidth, seed.pos.y);
            rect_size = Vector2D(passage_width, passage_length);
        }
        else if( seed.dir == DIRECTION4_WEST ) {
            if( seed.pos.x - passage_length < 0.0f ) {
                length_ok = false;
            }
            if( seed.pos.x - (passage_length+passage_width) < 0.0f ) {
                room_for_junction = false;
            }
            rect_pos = Vector2D(seed.pos.x - passage_length, seed.pos.y - passage_hwidth);
            rect_size = Vector2D(passage_length, passage_width);
        }
        if( length_ok ) {
            floor_region_rect = Rect2D(rect_pos.x, rect_pos.y, rect_size.x, rect_size.y);
            qDebug("    test passage: %f, %f w %f h %f", rect_pos.x, rect_pos.y, rect_size.x, rect_size.y);
            bool collides = LocationGenerator::collidesWithFloorRegions(floor_regions_rects, &ignore_rects, floor_region_rect, 1.0f);
            if( collides ) {
                length_ok = false;
            }
        }
        if( length_ok ) {
            break;
        }
        else {
            passage_length_i--;
            if( passage_length_i == 0 ) {
                qDebug("    ### passageway collided!");
                return;
            }
        }
    }
    qDebug("    add passage?: %f, %f w %f h %f", rect_pos.x, rect_pos.y, rect_size.x, rect_size.y);
    {
        FloorRegion *floor_region = FloorRegion::createRectangle(rect_pos.x, rect_pos.y, rect_size.x, rect_size.y);
        location->addFloorRegion(floor_region);
        floor_regions_rects->push_back(floor_region_rect);
        ignore_rects.push_back(floor_region_rect);
    }
    if( first ) {
        string name = "Stairs";
        string image_name = "stairsup_indoors";
        float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
        playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, image_name, true, 1.0f, 0.0f, 0.0f, false, 0.0f);
        Scenery *scenery_stairs_up = new Scenery(name, image_name, size_w, size_h, visual_h, false, 0.0f);
        scenery_stairs_up->setBlocking(true, false);
        Vector2D scenery_pos = seed.pos + dir_vec*size_w*0.5f;
        location->addScenery(scenery_stairs_up, scenery_pos.x, scenery_pos.y);
        *exit_up = scenery_stairs_up;
    }

    // contents
    int n_doors = 0;
    if( !first )
    {
        {
            // doors
            int roll = rollDice(1, 100, 0);
            if( roll <= 20 ) {
                n_doors = 1;
            }
            else if( roll <= 30 ) {
                n_doors = 2;
            }
            else if( roll <= 75 ) {
                // doors more likely if haven't found any rooms yet
                if( generator_info->nRooms() == 0 ) {
                    n_doors = 1;
                }
            }
        }
        {
            // traps
            int roll = rollDice(1, 100, 0);
            if( roll <= 10 ) {
                string trap_type;
                int rating = 1;
                roll = rollDice(1, 9, 0);
                if( roll <= 2 ) {
                    trap_type = "arrow";
                    rating = level+1;
                }
                else if( roll <= 4 ) {
                    trap_type = "mantrap";
                    rating = level+1;
                }
                else if( roll <= 6 ) {
                    trap_type = "acid";
                    rating = level+1;
                }
                else if( roll <= 8 ) {
                    trap_type = "darts";
                    rating = level+1;
                }
                else {
                    trap_type = "gas";
                    rating = 2;
                }
                int difficulty = level;
                Trap *trap = new Trap(trap_type, passage_width, passage_width);
                trap->setRating(rating);
                trap->setDifficulty(difficulty);
                float pos_x = rect_pos.x;
                float pos_y = rect_pos.y;
                int passage_section = rand() % (int)(passage_length_i*base_passage_length);
                if( seed.dir == DIRECTION4_NORTH || seed.dir == DIRECTION4_SOUTH ) {
                    pos_y += passage_section;
                }
                else {
                    pos_x += passage_section;
                }
                location->addTrap(trap, pos_x, pos_y);
            }
        }
        {
            // wandering monsters
            int roll = rollDice(1, 100, 0);
            if( roll <= 25 ) {
                int passage_section = rand() % passage_length_i;
                map<string, NPCTable *>::const_iterator iter = npc_tables.find("isolated");
                if( iter != npc_tables.end() ) {
                    const NPCTable *npc_table = iter->second;
                    const NPCGroup *npc_group = npc_table->chooseGroup(level);
                    int count = 0;
                    int spare = max_passage_enemies - npc_group->size();
                    int shift = 0;
                    if( spare > 0 ) {
                        shift = rand() % (spare+1);
                    }
                    for(vector<Character *>::const_iterator iter2 = npc_group->charactersBegin(); iter2 != npc_group->charactersEnd(); ++iter2, count++) {
                        const Character *npc = *iter2;
                        Character *copy = new Character(*npc);
                        float pos = passage_section*base_passage_length + (float)(count + shift) + 0.5f;
                        Vector2D npc_pos = seed.pos + dir_vec * pos;
                        location->addCharacter(copy, npc_pos.x, npc_pos.y);
                        if( count == max_passage_enemies ) {
                            break;
                        }
                    }
                }
            }
        }
    }
    for(int i=0;i<n_doors;i++) {
        int passage_section = rand() % passage_length_i;
        float pos = (passage_section+0.5f)*base_passage_length;
        bool side = rand() % 2 == 0;
        Direction4 room_dir = rotateDirection4(seed.dir, side ? -1 : 1);
        Vector2D room_dir_vec = directionFromEnum(room_dir);
        //Vector2D door_pos = seed.pos + dir_vec * ( pos + 0.5f*door_width ) + room_dir_vec * passage_hwidth;
        Vector2D door_pos = seed.pos + dir_vec * pos + room_dir_vec * passage_hwidth;
        Seed new_seed(Seed::TYPE_X_ROOM, door_pos, room_dir);
        new_seed.addIgnoreRect(floor_region_rect);
        qDebug("    add new room from passage at %f, %f", new_seed.pos.x, new_seed.pos.y);
        seeds->push_back(new_seed);
    }

    if( !room_for_junction ) {
        return;
    }

    // passage end
    bool has_end = false;
    bool left_turn = false;
    bool right_turn = false;
    {
        int roll = first ? 2 : rollDice(2, 12, 0);
        //roll = 2;
        if( roll >= 4 && roll <= 8 ) {
        }
        else if( roll >= 9 && roll <= 11 ) {
            has_end = true;
            left_turn = true;
        }
        else if( roll >= 15 && roll <= 17 ) {
            has_end = true;
            right_turn = true;
        }
        else {
            has_end = true;
            left_turn = true;
            right_turn = true;
        }
    }

    Direction4 l_dir = rotateDirection4(seed.dir, -1);
    Direction4 r_dir = rotateDirection4(seed.dir, 1);
    Vector2D l_dir_vec = directionFromEnum(l_dir);
    Vector2D r_dir_vec = directionFromEnum(r_dir);

    Rect2D floor_region_junction_rect;
    if( seed.dir == DIRECTION4_NORTH ) {
        floor_region_junction_rect = Rect2D(end_pos.x - passage_hwidth, end_pos.y - passage_width, passage_width, passage_width);
    }
    else if( seed.dir == DIRECTION4_EAST ) {
        floor_region_junction_rect = Rect2D(end_pos.x, end_pos.y - passage_hwidth, passage_width, passage_width);
    }
    else if( seed.dir == DIRECTION4_SOUTH ) {
        floor_region_junction_rect = Rect2D(end_pos.x - passage_hwidth, end_pos.y, passage_width, passage_width);
    }
    else if( seed.dir == DIRECTION4_WEST ) {
        floor_region_junction_rect = Rect2D(end_pos.x - passage_width, end_pos.y - passage_hwidth, passage_width, passage_width);
    }
    else {
        throw string("unknown direction");
    }
    qDebug("    add junction?: %f, %f w %f h %f", floor_region_junction_rect.getX(), floor_region_junction_rect.getY(), floor_region_junction_rect.getWidth(), floor_region_junction_rect.getHeight());
    {
        /*bool collides = false;
        for(vector<Rect2D>::const_iterator iter = floor_regions_rects->begin(); iter != floor_regions_rects->end() && !collides; ++iter) {
            Rect2D rect = *iter;
            if( floor_region_junction_rect.overlaps(rect) ) {
                collides = true;
            }
        }*/
        bool collides = LocationGenerator::collidesWithFloorRegions(floor_regions_rects, &ignore_rects, floor_region_junction_rect, 1.0f);
        if( collides ) {
            qDebug("    ### junction collided!");
            return;
        }
        FloorRegion *floor_region = FloorRegion::createRectangle(floor_region_junction_rect);
        location->addFloorRegion(floor_region);
    }

    floor_regions_rects->push_back(floor_region_junction_rect);

    if( left_turn ) {
        Seed new_seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, end_pos + dir_vec*passage_hwidth + l_dir_vec*passage_hwidth, l_dir);
        new_seed.addIgnoreRect(floor_region_rect);
        new_seed.addIgnoreRect(floor_region_junction_rect);
        qDebug("    add left turn at %f, %f", new_seed.pos.x, new_seed.pos.y);
        seeds->push_back(new_seed);
    }
    if( right_turn ) {
        Seed new_seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, end_pos + dir_vec*passage_hwidth + r_dir_vec*passage_hwidth, r_dir);
        new_seed.addIgnoreRect(floor_region_rect);
        new_seed.addIgnoreRect(floor_region_junction_rect);
        qDebug("    add right turn at %f, %f", new_seed.pos.x, new_seed.pos.y);
        seeds->push_back(new_seed);
    }
}

Location *LocationGenerator::generateLocation(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Vector2D *player_start, const map<string, NPCTable *> &npc_tables, int level, int n_levels) {
    LOG("LocationGenerator::generateLocation create level %d\n", level);
    stringstream str;
    str << "Level " << (level+1);
    Location *location = NULL;

    for(;;) {
        *exit_down = *exit_up = NULL;
        location = new Location(str.str());
        location->setDisplayName(true);

        string background_name, floor_name, wall_name;
        int back_r = rollDice(1, 3, 0);
        if( back_r == 1 ) {
            background_name = "background_brown";
        }
        else if( back_r == 2 ) {
            background_name = "background_dark";
        }
        else if( back_r == 3 ) {
            background_name = "background_black";
        }
        int floor_r = rollDice(1, 10, 0);
        if( floor_r <= 3 ) {
            floor_name = "floor_dirt";
        }
        else if( floor_r <= 6 ) {
            floor_name = "floor_rock";
        }
        else if( floor_r <= 9 ) {
            floor_name = "floor_paved";
        }
        else {
            floor_name = "floor_paved_blood";
        }
        int wall_r = rollDice(1, 6, 0);
        if( wall_r <= 3 ) {
            wall_name = "wall";
        }
        else if( wall_r <= 5 ) {
            wall_name = "wall_rock";
        }
        else {
            wall_name = "wall_redbricks";
        }
        location->setBackgroundImageName(background_name);
        location->setFloorImageName(floor_name);
        location->setWallImageName(wall_name);

        Vector2D start_pos(0.0f, 100.0f);
        Seed seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, start_pos, DIRECTION4_EAST);
        *player_start = Vector2D(start_pos + Vector2D(1.5f, 0.0f));

        vector<Seed> seeds;
        seeds.push_back(seed);
        vector<Rect2D> floor_regions_rects;

        LocationGeneratorInfo generator_info;

        for(int count=0;count<8 && seeds.size() > 0;count++) {
            vector<Seed> c_seeds;
            for(vector<Seed>::iterator iter = seeds.begin(); iter != seeds.end(); ++iter) {
                Seed seed = *iter;
                c_seeds.push_back(seed);
            }
            seeds.clear();

            for(vector<Seed>::iterator iter = c_seeds.begin(); iter != c_seeds.end(); ++iter) {
                Seed seed = *iter;
                exploreFromSeed(exit_down, exit_up, playing_gamestate, location, seed, &seeds, &floor_regions_rects, count==0, npc_tables, level, n_levels, &generator_info);
            }
        }

        if( generator_info.n_rooms_quest > 0 ) {
            // okay!
            break;
        }
        LOG("level %d didn't create a quest room, delete and try again\n", level);
        delete location;
    }

    return location;
}
