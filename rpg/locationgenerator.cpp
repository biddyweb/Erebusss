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

LocationGeneratorInfo::LocationGeneratorInfo(const map<string, NPCTable *> &npc_tables) : npc_tables(npc_tables),
    n_rooms_normal(0), n_rooms_hazard(0), n_rooms_lair(0), n_rooms_quest(0)
{
    this->room_weights[ROOMTYPE_NORMAL] = 3;
    this->room_weights[ROOMTYPE_HAZARD] = 1;
    this->room_weights[ROOMTYPE_LAIR] = 1;
    this->room_weights[ROOMTYPE_QUEST] = 1;

    int type = rollDice(1, 2, 0);
    LOG("type: %d\n", type);
    if( type == 1 ) {
        LOG("default\n");
        this->n_room_doorsX = 1;
        this->n_room_doorsY = 2;
        this->n_room_doorsZ = 0;

        this->percentage_chance_passageway = 50;

        this->n_door_weights.push_back(70);
        this->n_door_weights.push_back(20);
        this->n_door_weights.push_back(10);

        this->n_door_weights_initial.push_back(25);
        this->n_door_weights_initial.push_back(65);
        this->n_door_weights_initial.push_back(10);

        this->n_rooms_until_quest = 5;
    }
    else {
        LOG("room-heavy");
        this->n_room_doorsX = 1;
        this->n_room_doorsY = 2;
        this->n_room_doorsZ = 1;

        this->percentage_chance_passageway = 10;

        this->n_door_weights.push_back(10);
        this->n_door_weights.push_back(30);
        this->n_door_weights.push_back(60);

        this->n_door_weights_initial.push_back(0);
        this->n_door_weights_initial.push_back(5);
        this->n_door_weights_initial.push_back(95);

        this->n_rooms_until_quest = 15;
    }
}

LocationGeneratorInfo::~LocationGeneratorInfo() {
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

void LocationGenerator::exploreFromSeedPassagewayPassageway(Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, int level, LocationGeneratorInfo *generator_info) {
    Vector2D dir_vec = directionFromEnum(seed.dir);
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
            /*int roll = rollDice(1, 100, 0);
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
            }*/
            if( generator_info->nRooms() == 0 ) {
                n_doors = rollDiceChoice(&generator_info->n_door_weights_initial.front(), generator_info->n_door_weights_initial.size());
            }
            else {
                n_doors = rollDiceChoice(&generator_info->n_door_weights.front(), generator_info->n_door_weights.size());
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
                map<string, NPCTable *>::const_iterator iter = generator_info->npc_tables.find("isolated");
                if( iter != generator_info->npc_tables.end() ) {
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
        if( seed.dir == DIRECTION4_WEST || seed.dir == DIRECTION4_EAST )
        {
            // torches
            for(int i=0;i<passage_length_i;i++) {
                if( rollDice(1, 2, 0) == 1 )
                    continue;
                float pos = (i+0.5f)*base_passage_length + 1.0f;
                float xpos = rect_pos.x + pos;
                Scenery *scenery = new Scenery("Torch", "torch", 0.75f, 0.75f, 0.75f, false, 0.0f);
                location->addScenery(scenery, xpos, rect_pos.y - 0.45f);
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
    //bool has_end = false;
    bool left_turn = false;
    bool right_turn = false;
    {
        int roll = first ? 2 : rollDice(2, 12, 0);
        //roll = 2;
        if( roll >= 4 && roll <= 8 ) {
        }
        else if( roll >= 9 && roll <= 11 ) {
            //has_end = true;
            left_turn = true;
        }
        else if( roll >= 15 && roll <= 17 ) {
            //has_end = true;
            right_turn = true;
        }
        else {
            //has_end = true;
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
        //Vector2D passageway_centre = door_centre + dir_vec * ( 0.5f*door_depth + passage_hwidth );
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
    int r = rollDice(1, 27, 0);
    Item *item = NULL;
    if( r <= 3 ) {
        item = playing_gamestate->cloneStandardItem("Arrows");
    }
    else if( r <= 4 ) {
        item = playing_gamestate->cloneStandardItem("Bullets");
    }
    else if( r <= 5 ) {
        item = playing_gamestate->cloneStandardItem("Leather Armour");
    }
    else if( r <= 6 ) {
        item = playing_gamestate->cloneStandardItem(level == 0 ? "Leather Armour" : "Chain Mail Armour");
    }
    else if( r <= 7 ) {
        item = playing_gamestate->cloneStandardItem(level == 0 ? "Chain Mail Armour" : "Plate Armour");
    }
    else if( r <= 9 ) {
        item = playing_gamestate->cloneStandardItem("Shield");
    }
    else if( r <= 11 ) {
        item = playing_gamestate->cloneStandardItem("Dagger");
    }
    else if( r <= 13 ) {
        item = playing_gamestate->cloneStandardItem("Short Sword");
    }
    else if( r <= 14 ) {
        item = playing_gamestate->cloneStandardItem("Long Sword");
    }
    else if( r <= 15 ) {
        item = playing_gamestate->cloneStandardItem(level == 0 ? "Long Sword" : "Two Handed Sword");
    }
    else if( r <= 17 ) {
        item = playing_gamestate->cloneStandardItem("Shortbow");
    }
    else if( r <= 18 ) {
        item = playing_gamestate->cloneStandardItem(level == 0 ? "Shortbow" : "Longbow");
    }
    else if( r <= 19 ) {
        item = playing_gamestate->cloneStandardItem("Sling");
    }
    else if( r <= 20 ) {
        item = playing_gamestate->cloneStandardItem("Plain Ring");
    }
    else if( r <= 21 ) {
        item = playing_gamestate->cloneStandardItem("Gold Ring");
    }
    else if( r <= 22 ) {
        item = playing_gamestate->cloneStandardItem("Wyvern Egg");
    }
    else if( r <= 23 ) {
        item = playing_gamestate->cloneStandardItem("Bowl");
    }
    else if( r <= 24 ) {
        item = playing_gamestate->cloneStandardItem("Plate");
    }
    else if( r <= 25 ) {
        item = playing_gamestate->cloneStandardItem("Pitcher");
    }
    else { // 26, 27
        item = playing_gamestate->cloneStandardItem("Red Gem");
    }
    return item;
}

Item *LocationGenerator::getRandomTreasure(const PlayingGamestate *playing_gamestate, int level) {
    int r = rollDice(1, 17, 0);
    //r = 15;
    Item *item = NULL;
    if( r <= 1 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Healing");
    }
    else if( r <= 2 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Combat");
    }
    else if( r <= 3 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Archery");
    }
    else if( r <= 4 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Strength");
    }
    else if( r <= 5 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Speed");
    }
    else if( r <= 6 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Cure Disease");
    }
    else if( r <= 7 ) {
        item = playing_gamestate->cloneStandardItem("Potion of Rage");
    }
    else if( r <= 8 ) {
        item = playing_gamestate->cloneStandardItem("Holy Water");
    }
    else if( r <= 9 ) {
        item = playing_gamestate->cloneStandardItem("Acid");
    }
    else if( r <= 10 ) {
        item = playing_gamestate->cloneStandardItem("Green Gem");
    }
    else if( r <= 11 ) {
        item = playing_gamestate->cloneStandardItem("Blue Gem");
    }
    else if( r <= 12 ) {
        item = playing_gamestate->cloneStandardItem("Gold Gem");
    }
    else if( r <= 13 ) {
        item = playing_gamestate->cloneStandardItem("White Gem");
    }
    else if( r <= 15 ) {
        item = playing_gamestate->cloneStandardItem("Gold Ring");
        item->setBaseTemplate(item->getName());
        item->setMagical(true);
        int r2 = rollDice(1, 7, 0);
        if( r2 == 1 ) {
            item->setProfileBonusIntProperty(profile_key_FP_c, 1);
            item->setWorthBonus(300);
            item->setName("Ring of Combat");
        }
        else if( r2 == 2 ) {
            item->setProfileBonusIntProperty(profile_key_BS_c, 1);
            item->setWorthBonus(250);
            item->setName("Ring of Accuracy");
        }
        else if( r2 == 3 ) {
            item->setProfileBonusIntProperty(profile_key_S_c, 1);
            item->setWorthBonus(200);
            item->setName("Ring of Strength");
        }
        else if( r2 == 4 ) {
            item->setProfileBonusIntProperty(profile_key_M_c, 1);
            item->setWorthBonus(200);
            item->setName("Ring of Wisdom");
        }
        else if( r2 == 5 ) {
            item->setProfileBonusIntProperty(profile_key_D_c, 1);
            item->setWorthBonus(150);
            item->setName("Ring of Skill");
        }
        else if( r2 == 6 ) {
            item->setProfileBonusIntProperty(profile_key_B_c, 1);
            item->setWorthBonus(150);
            item->setName("Ring of Bravery");
        }
        else {
            item->setProfileBonusFloatProperty(profile_key_Sp_c, 0.2f);
            item->setWorthBonus(200);
            item->setName("Ring of Speed");
        }
    }
    else { // 16, 17
        if( r == 16 ) {
            item = playing_gamestate->cloneStandardItem("Dagger");
            item->setBaseTemplate(item->getName());
            item->setName("Magic Dagger");
        }
        else {
            item = playing_gamestate->cloneStandardItem("Short Sword");
            item->setBaseTemplate(item->getName());
            item->setName("Magic Short Sword");
        }
        item->setMagical(true);
        int damageX = 0, damageY = 0, damageZ = 0;
        Weapon *weapon = static_cast<Weapon *>(item);
        weapon->getDamage(&damageX, &damageY, &damageZ);
        damageZ += level;
        weapon->setDamage(damageX, damageY, damageZ);
        item->setWorthBonus(100*level);
    }
    return item;
}

void LocationGenerator::exploreFromSeedXRoom(Scenery **exit_down, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, int level, int n_levels, LocationGeneratorInfo *generator_info) {
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
        float room_size_w = base_room_size;
        float room_size_h = base_room_size;
        /*RoomType room_type = ROOMTYPE_NORMAL;
        int roll = rollDice(1, 12, 0);
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
        }*/
        RoomType room_type = (RoomType)rollDiceChoice(generator_info->room_weights, N_ROOMTYPES);

        if( room_type == ROOMTYPE_QUEST && generator_info->n_rooms_quest > 0 ) {
            // only 1 quest room per level
            room_type = ROOMTYPE_LAIR;
        }
        else if( room_type == ROOMTYPE_QUEST && ( generator_info->n_rooms_normal == 0 || generator_info->n_rooms_hazard == 0 || generator_info->n_rooms_lair == 0 || generator_info->nRooms() < generator_info->n_rooms_until_quest ) ) {
            // need at least 1 of each of the other room types, and at least before having a quest room
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
            //rounded_rectangle = true;
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
                int r = rollDice(1, 9, 0);
                //r = 9; // test
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
                else if( r == 5 || r == 6 || r == 7 ) {
                    // shrine, bell or pool
                    string scenery_name, scenery_image_name, interact_type;
                    int interact_state = 0;
                    if( r == 5 ) {
                        scenery_name = "Shrine";
                        scenery_image_name = "shrine";
                        interact_type ="INTERACT_TYPE_SHRINE";
                    }
                    else if( r == 6 ) {
                        scenery_name = "Bell";
                        scenery_image_name = "church_bell";
                        interact_type = "INTERACT_TYPE_BELL";
                    }
                    else {
                        interact_state = rollDice(1, 8, 0);
                        scenery_name = "Pool";
                        if( interact_state <= 4 )
                            scenery_image_name = "pool_pink";
                        else
                            scenery_image_name = "pool_blue";
                        interact_type = "INTERACT_TYPE_POOL";
                    }
                    float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_image_name, true, 1.0f, 0.0f, 0.0f, false, 0.0f);
                    Scenery *scenery = new Scenery(scenery_name, scenery_image_name, size_w, size_h, visual_h, false, 0.0f);
                    scenery->setInteractType(interact_type);
                    scenery->setInteractState(interact_state);
                    scenery->setBlocking(true, false);
                    location->addScenery(scenery, room_centre.x, room_centre.y);
                }
                else if( r == 8 ) {
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
                else if( r == 9 ) {
                    // dungeon map
                    string scenery_image_name = "map_dungeon";
                    float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                    playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_image_name, true, 0.5f, 0.0f, 0.0f, false, 0.0f);
                    Scenery *scenery = new Scenery("Map", scenery_image_name, size_w, size_h, visual_h, false, 0.0f);
                    scenery->setInteractType("INTERACT_TYPE_DUNGEON_MAP");
                    float x_pos = rollDice(1, 2, 0)==0 ? room_rect.getX() + 1.5f : room_rect.getX() + room_rect.getWidth() - 1.5f;
                    location->addScenery(scenery, x_pos, room_rect.getY() - 0.5f*scenery->getHeight() - 0.05f);
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
                if( room_type == ROOMTYPE_QUEST || rollDice(1, 2, 0) == 1 )
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
                    else if( roll <= 40 ) {
                        scenery_centre_name = "Campfire";
                        scenery_centre_image_name = "campfire";
                        scenery_centre_size = 1.0;
                    }
                    else if( roll <= 55 ) {
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

                int n_torches = rollDice(1, 2, 0);
                for(int i=0;i<n_torches;i++) {
                    float xpos = room_rect.getX();
                    if( (n_torches == 1 && rollDice(1, 2, 0)==1) || (n_torches==2 && i==0) ) {
                        xpos += 0.25f*room_rect.getWidth();
                    }
                    else {
                        xpos += 0.75f*room_rect.getWidth();
                    }
                    Scenery *scenery = new Scenery("Torch", "torch", 0.75f, 0.75f, 0.75f, false, 0.0f);
                    location->addScenery(scenery, xpos, room_rect.getY() - 0.45f);
                }
            }

            if( scenery_corner != NULL ) {
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
                else if( r <= 35 ) {
                    scenery_name = "bones";
                }
                else if( r <= 65 ) {
                    scenery_name = "rock1";
                    scenery_size = 0.2f;
                }
                else if( r <= 70 ) {
                    scenery_name = "grass";
                    scenery_size = 1.0f;
                }
                else if( r <= 75 ) {
                    scenery_name = "plant";
                    scenery_size = 0.5f;
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

            const int n_slots_w = base_room_size, n_slots_h = base_room_size;
            bool slot_filled[n_slots_w*n_slots_h];
            for(int i=0;i<n_slots_w*n_slots_h;i++) {
                slot_filled[i] = false;
            }
            // fill corners and centre, as may use for scenery, or have a rounded room
            slot_filled[0*n_slots_w + 0] = true;
            slot_filled[0*n_slots_w + (n_slots_w-1)] = true;
            slot_filled[(n_slots_h-1)*n_slots_w + 0] = true;
            slot_filled[(n_slots_h-1)*n_slots_w + (n_slots_w-1)] = true;
            slot_filled[((n_slots_h-1)/2)*n_slots_w + ((n_slots_w-1)/2)] = true;
            // also don't allow blocking of doors
            slot_filled[0*n_slots_w + ((n_slots_w-1)/2)] = true;
            slot_filled[(n_slots_h-1)*n_slots_w + ((n_slots_w-1)/2)] = true;
            slot_filled[((n_slots_h-1)/2)*n_slots_w + 0] = true;
            slot_filled[((n_slots_h-1)/2)*n_slots_w + (n_slots_w-1)] = true;
            float slot_scale_x = room_size_w/(float)base_room_size;
            float slot_scale_y = room_size_h/(float)base_room_size;
            if( rollDice(1, 2, 0) == 1 ) {
                // place some random blocking scenery
                Vector2D scenery_pos;
                while(true) {
                    int slot_x = rand() % n_slots_w;
                    int slot_y = rand() % n_slots_h;
                    if( !slot_filled[slot_y*n_slots_w + slot_x] ) {
                        slot_filled[slot_y*n_slots_w + slot_x] = true;
                        scenery_pos = room_rect.getTopLeft() + Vector2D(1.0f, 0.0f)*(slot_x+0.5f)*slot_scale_x + Vector2D(0.0f, 1.0f)*(slot_y+0.5f)*slot_scale_y;
                        break;
                    }
                }
                string scenery_name;
                float scenery_size = 0.5f;
                int r = rollDice(1, 100, 0);
                if( r <= 20 ) {
                    scenery_name = "bigboulder";
                    scenery_size = 0.58f;
                }
                else if( r <= 60 ) {
                    scenery_name = "boulders";
                    scenery_size = 0.56f;
                }
                else {
                    scenery_name = "boulders2";
                    scenery_size = 0.56f;
                }
                float size_w = 0.0f, size_h = 0.0f, visual_h = 0.0f;
                playing_gamestate->querySceneryImage(&size_w, &size_h, &visual_h, scenery_name, true, scenery_size, 0.0f, 0.0f, false, 0.0f);
                Scenery *scenery = new Scenery(scenery_name, scenery_name, size_w, size_h, visual_h, false, 0.0f);
                scenery->setBlocking(true, false);
                location->addScenery(scenery, scenery_pos.x, scenery_pos.y);
            }
            if( enemy_table.length() > 0 ) {
                map<string, NPCTable *>::const_iterator iter = generator_info->npc_tables.find(enemy_table);
                if( iter != generator_info->npc_tables.end() ) {
                    const NPCTable *npc_table = iter->second;
                    const NPCGroup *npc_group = npc_table->chooseGroup(level);
                    int count = 0;
                    for(vector<Character *>::const_iterator iter2 = npc_group->charactersBegin(); iter2 != npc_group->charactersEnd(); ++iter2, count++) {
                        Vector2D npc_pos;
                        while(true) {
                            int slot_x = rand() % n_slots_w;
                            int slot_y = rand() % n_slots_h;
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
            //int n_room_doors = rollDice(1, 2, 0); // not necessarily the actual number of doors, as we may fail to find room, when exploring from the seed
            // not necessarily the actual number of doors, as we may fail to find room, when exploring from the seed
            int n_room_doors = rollDice(generator_info->n_room_doorsX, generator_info->n_room_doorsY, generator_info->n_room_doorsZ);
            vector<Direction4> done_dirs;
            done_dirs.push_back( rotateDirection4(seed.dir, 2) );
            for(int j=0;j<n_room_doors;j++) {
                //qDebug("j = %d / %d", j, n_room_doors);
                Direction4 new_room_dir;
                bool done = false;
                while( !done ) {
                    //new_room_dir = (Direction4)(rollDice(1, 4, -1));
                    if( j == 0 )
                        new_room_dir = seed.dir;
                    else {
                        int turn = rollDice(1, 2, 0) == 1 ? 1 : -1;
                        new_room_dir = rotateDirection4(seed.dir, turn);
                    }
                    //qDebug("    chose dir %d", new_room_dir);
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
                Seed::Type seed_type = rollDice(1, 100, 0) <= generator_info->percentage_chance_passageway ? Seed::TYPE_ROOM_PASSAGEWAY : Seed::TYPE_X_ROOM;
                Seed new_seed(seed_type, door_pos, new_room_dir);
                new_seed.addIgnoreRect(room_rect);
                qDebug("    add new room from room at %f, %f", new_seed.pos.x, new_seed.pos.y);
                seeds->push_back(new_seed);
            }
        }
    }
}

void LocationGenerator::exploreFromSeed(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, int level, int n_levels, LocationGeneratorInfo *generator_info) {
    Vector2D dir_vec = directionFromEnum(seed.dir);
    qDebug("explore from seed type %d at %f, %f ; direction %d: %f, %f", seed.type, seed.pos.x, seed.pos.y, seed.dir, dir_vec.x, dir_vec.y);
    if( seed.type == Seed::TYPE_PASSAGEWAY_PASSAGEWAY ) {
        exploreFromSeedPassagewayPassageway(exit_up, playing_gamestate, location, seed, seeds, floor_regions_rects, first, level, generator_info);
        return;
    }
    else if( seed.type == Seed::TYPE_ROOM_PASSAGEWAY ) {
        exploreFromSeedRoomPassageway(location, seed, seeds, floor_regions_rects);
        return;
    }
    else if( seed.type == Seed::TYPE_X_ROOM ) {
        exploreFromSeedXRoom(exit_down, playing_gamestate, location, seed, seeds, floor_regions_rects, level, n_levels, generator_info);
        return;
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

        vector<Seed> seeds;
        Direction4 direction = rollDice(1, 2, 0) == 1 ? DIRECTION4_EAST : DIRECTION4_SOUTH;
        Vector2D start_pos(100.0f, 100.0f);
        start_pos -= directionFromEnum(direction) * 100.0f;
        Seed seed(Seed::TYPE_PASSAGEWAY_PASSAGEWAY, start_pos, direction);
        seeds.push_back(seed);
        *player_start = Vector2D(start_pos + directionFromEnum(direction) * 1.5f);

        vector<Rect2D> floor_regions_rects;

        LocationGeneratorInfo generator_info(npc_tables);

        for(int count=0;count<8 && seeds.size() > 0;count++) {
            vector<Seed> c_seeds;
            for(vector<Seed>::iterator iter = seeds.begin(); iter != seeds.end(); ++iter) {
                Seed seed = *iter;
                c_seeds.push_back(seed);
            }
            seeds.clear();

            for(vector<Seed>::iterator iter = c_seeds.begin(); iter != c_seeds.end(); ++iter) {
                Seed seed = *iter;
                exploreFromSeed(exit_down, exit_up, playing_gamestate, location, seed, &seeds, &floor_regions_rects, count==0, level, n_levels, &generator_info);
            }
            if( generator_info.n_rooms_quest > 0 ) {
                // quit to limit size
                break;
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
