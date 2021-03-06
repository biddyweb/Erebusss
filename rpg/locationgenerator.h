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
    NPCGroup& operator=(const NPCGroup &) {
        throw string("NPCGroup assignment operator disallowed");
    }
    NPCGroup(const NPCGroup &) {
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
    NPCTableLevel& operator=(const NPCTableLevel &) {
        throw string("NPCTableLevel assignment operator disallowed");
    }
    NPCTableLevel(const NPCTableLevel &) {
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
    NPCTable& operator=(const NPCTable &) {
        throw string("NPCTable assignment operator disallowed");
    }
    NPCTable(const NPCTable &) {
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

enum RoomType {
    ROOMTYPE_NORMAL = 0,
    ROOMTYPE_HAZARD = 1,
    ROOMTYPE_LAIR = 2,
    ROOMTYPE_QUEST = 3,
    N_ROOMTYPES = 4
};

class LocationGeneratorInfo {
public:
    const map<string, NPCTable *> &npc_tables;

    int room_weights[N_ROOMTYPES];
    int n_room_doorsX, n_room_doorsY, n_room_doorsZ; // not necessarily the actual number of doors, as we may fail to find room, when exploring from the seed
    int percentage_chance_passageway; // percentage chance of room door opening to a passageway (rather than a room)
    vector<int> n_door_weights;
    vector<int> n_door_weights_initial;
    int n_rooms_until_quest;

    int n_rooms_normal;
    int n_rooms_hazard;
    int n_rooms_lair;
    int n_rooms_quest;

    LocationGeneratorInfo(const map<string, NPCTable *> &npc_tables);
    ~LocationGeneratorInfo();

    int nRooms() const {
        return n_rooms_normal + n_rooms_hazard + n_rooms_lair + n_rooms_quest;
    }
};

class LocationGenerator {
    static Item *getRandomItem(const PlayingGamestate *playing_gamestate, int level);
    static Item *getRandomTreasure(const PlayingGamestate *playing_gamestate, int level);

    static bool collidesWithFloorRegions(const vector<Rect2D> *floor_regions_rects, const vector<Rect2D> *ignore_rects, Rect2D rect, float gap);
    static void exploreFromSeedPassagewayPassageway(Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, int level, LocationGeneratorInfo *generator_info);
    static void exploreFromSeedRoomPassageway(Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects);
    static void exploreFromSeedXRoom(Scenery **exit_down, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, int level, int n_levels, LocationGeneratorInfo *generator_info);
    static void exploreFromSeed(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Location *location, const Seed &seed, vector<Seed> *seeds, vector<Rect2D> *floor_regions_rects, bool first, int level, int n_levels, LocationGeneratorInfo *generator_info);

public:
    static Location *generateLocation(Scenery **exit_down, Scenery **exit_up, PlayingGamestate *playing_gamestate, Vector2D *player_start, const map<string, NPCTable *> &npc_tables, int level, int n_levels, bool force_start, bool passageway_start_type, Direction4 start_direction);
};
