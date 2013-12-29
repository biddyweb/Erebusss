#pragma once

#include <vector>
using std::vector;
#include <map>
using std::map;

#include "../common.h"

#include "utils.h"

class Character;
class Location;
class Scenery;
class Item;
class PlayingGamestate;

enum Direction4 {
    DIRECTION4_NORTH = 0,
    DIRECTION4_EAST = 1,
    DIRECTION4_SOUTH = 2,
    DIRECTION4_WEST = 3
};

class Seed {
public:
    enum Type {
        TYPE_PASSAGEWAY_PASSAGEWAY = 0,
        TYPE_ROOM_PASSAGEWAY = 1,
        TYPE_X_ROOM = 2
    };
    Type type;
    Vector2D pos;
    Direction4 dir;
    vector<Rect2D> ignore_rects;

    Seed(Type type, Vector2D pos, Direction4 dir) : type(type), pos(pos), dir(dir) {
    }
    void addIgnoreRect(const Rect2D &ignore_rect ) {
        this->ignore_rects.push_back(ignore_rect);
    }
};

class NPCGroup {
    vector<Character *> npcs;

    // rule of three
    NPCGroup& operator=(const NPCGroup &npc_group) {
        throw string("NPCGroup assignment operator disallowed");
    }
    NPCGroup(const NPCGroup &npc_group) {
        throw string("NPCGroup copy constructor disallowed");
    }
public:
    NPCGroup() {
    }
    ~NPCGroup();

    void addNPC(Character *npc) {
        this->npcs.push_back(npc);
    }
    vector<Character *>::iterator charactersBegin() {
        return this->npcs.begin();
    }
    vector<Character *>::const_iterator charactersBegin() const {
        return this->npcs.begin();
    }
    vector<Character *>::iterator charactersEnd() {
        return this->npcs.end();
    }
    vector<Character *>::const_iterator charactersEnd() const {
        return this->npcs.end();
    }
    size_t size() const {
        return this->npcs.size();
    }
};

class NPCTableLevel {
    vector<NPCGroup *> npc_groups;

    // rule of three
    NPCTableLevel& operator=(const NPCTableLevel &npc_table_level) {
        throw string("NPCTableLevel assignment operator disallowed");
    }
    NPCTableLevel(const NPCTableLevel &npc_table_level) {
        throw string("NPCTableLevel copy constructor disallowed");
    }
public:
    NPCTableLevel() {
    }
    ~NPCTableLevel();

    void addNPCGroup(NPCGroup *npc_group) {
        this->npc_groups.push_back(npc_group);
    }
    const NPCGroup *chooseGroup() const {
        int r = rand() % npc_groups.size();
        const NPCGroup *npc_group = npc_groups.at(r);
        return npc_group;
    }
};

class NPCTable {
    map<int, NPCTableLevel *> levels;

    const NPCTableLevel *getLevel(int level) const {
        map<int, NPCTableLevel *>::const_iterator iter = levels.find(level);
        if( iter == levels.end() ) {
            return NULL;
        }
        return iter->second;
    }

    // rule of three
    NPCTable& operator=(const NPCTable &npc_table) {
        throw string("NPCTable assignment operator disallowed");
    }
    NPCTable(const NPCTable &npc_table) {
        throw string("NPCTable copy constructor disallowed");
    }
public:
    NPCTable() {
    }
    ~NPCTable();

    void addLevel(int value, NPCTableLevel *level) {
        this->levels[value] = level;
    }
    const NPCGroup *chooseGroup(int level) const;
};

class LocationGeneratorInfo {
public:
    int n_rooms_normal;
    int n_rooms_hazard;
    int n_rooms_lair;
    int n_rooms_quest;

    LocationGeneratorInfo() {
        n_rooms_normal = 0;
        n_rooms_hazard = 0;
        n_rooms_lair = 0;
        n_rooms_quest = 0;
    }

    int nRooms() const {
        return n_rooms_normal + n_rooms_hazard + n_rooms_lair + n_rooms_quest;
    }
};

class LocationGenerator {
    static Item *getRandomItem(const PlayingGamestate *playing_gamestate, int level);
    static Item *getRandomTreasure(const PlayingGamestate *playing_gamestate, int level);

    static bool collidesWithFloorRegions(const vector<Rect2D> *floor_regions_rects, const vector<Rect2D> *ignore_rects, Rect2D rect, float gap);
    static void exploreFromSeedRoomPassageway(Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects);
    static void exploreFromSeedXRoom(Scenery **exit_down, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, const map<string, NPCTable *> &npc_tables, int level, int n_levels, LocationGeneratorInfo *generator_info);
    static void exploreFromSeed(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, const map<string, NPCTable *> &npc_tables, int level, int n_levels, LocationGeneratorInfo *generator_info);

public:
    static Location *generateLocation(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Vector2D *player_start, const map<string, NPCTable *> &npc_tables, int level, int n_levels);
};
