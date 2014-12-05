#pragma once

#include <string>
using std::string;

#include "common.h"
#include "rpg/utils.h" // for Vector2D

class Item;
class Scenery;
class Character;
class Location;
class PlayingGamestate;

enum TestID {
    TEST_PATHFINDING_0 = 0,
    TEST_PATHFINDING_1 = 1,
    TEST_PATHFINDING_2 = 2,
    TEST_PATHFINDING_3 = 3,
    TEST_PATHFINDING_4 = 4,
    TEST_PATHFINDING_5 = 5,
    TEST_PATHFINDING_6 = 6,
    TEST_POINTINPOLYGON_0 = 7,
    TEST_POINTINPOLYGON_1 = 8,
    TEST_POINTINPOLYGON_2 = 9,
    TEST_POINTINPOLYGON_3 = 10,
    TEST_POINTINPOLYGON_4 = 11,
    TEST_POINTINPOLYGON_5 = 12,
    TEST_POINTINPOLYGON_6 = 13,
    TEST_POINTINPOLYGON_7 = 14,
    TEST_POINTINPOLYGON_8 = 15,
    TEST_POINTINPOLYGON_9 = 16,
    TEST_POINTINPOLYGON_10 = 17,
    TEST_POINTINPOLYGON_11 = 18,
    TEST_POINTINPOLYGON_12 = 19,
    TEST_POINTINPOLYGON_13 = 20,
    TEST_POINTINPOLYGON_14 = 21,
    TEST_FLOORREGIONS_0 = 22,
    TEST_PERF_POINTINPOLYGON_0 = 23,
    TEST_PERF_POINTINPOLYGON_1 = 24,
    TEST_PERF_POINTINPOLYGON_2 = 25,
    TEST_PERF_DISTANCEGRAPH_0 = 26,
    TEST_PERF_PATHFINDING_0 = 27,
    TEST_PERF_REMOVE_SCENERY_0 = 28,
    TEST_PERF_REMOVE_SCENERY_1 = 29,
    TEST_PERF_REMOVE_SCENERY_2 = 30,
    TEST_PERF_UPDATE_VISIBILITY_0 = 31,
    TEST_PERF_NUDGE_0 = 32,
    TEST_PERF_NUDGE_1 = 33,
    TEST_PERF_NUDGE_2 = 34,
    TEST_PERF_NUDGE_3 = 35,
    TEST_PERF_NUDGE_4 = 36,
    TEST_PERF_NUDGE_5 = 37,
    TEST_PERF_NUDGE_6 = 38,
    TEST_PERF_NUDGE_7 = 39,
    TEST_PERF_NUDGE_8 = 40,
    TEST_PERF_NUDGE_9 = 41,
    TEST_PERF_NUDGE_10 = 42,
    TEST_PERF_NUDGE_11 = 43,
    TEST_PERF_NUDGE_12 = 44,
    TEST_PERF_NUDGE_13 = 45,
    TEST_PERF_NUDGE_14 = 46,
    TEST_USE_AMMO = 47,
    TEST_LEVEL_UP = 48,
    TEST_LOADSAVEQUEST_0 = 49,
    TEST_LOADSAVEQUEST_1 = 50,
    TEST_LOADSAVEQUEST_2 = 51,
    TEST_LOADSAVEQUEST_3 = 52,
    TEST_LOADSAVERANDOMQUEST_0 = 53,
    TEST_LOADSAVERANDOMQUEST_1 = 54,
    TEST_LOADSAVERANDOMQUEST_2 = 55,
    TEST_LOADSAVERANDOMQUEST_3 = 56,
    TEST_LOADSAVERANDOMQUEST_4 = 57,
    TEST_LOADSAVERANDOMQUEST_5 = 58,
    TEST_LOADSAVERANDOMQUEST_6 = 59,
    TEST_MEMORYQUEST_0 = 60,
    TEST_MEMORYQUEST_1 = 61,
    TEST_MEMORYQUEST_2 = 62,
    TEST_MEMORYQUEST_3 = 63,
    TEST_LOADSAVE_QUEST_1_COMPLETED = 64,
    TEST_LOADSAVE_ACTION_LAST_TIME_BUG = 65,
    TEST_LOADSAVEWRITEQUEST_0_COMPLETE = 66,
    TEST_LOADSAVEWRITEQUEST_0_WARRIOR = 67,
    TEST_LOADSAVEWRITEQUEST_0_BARBARIAN = 68,
    TEST_LOADSAVEWRITEQUEST_0_ELF = 69,
    TEST_LOADSAVEWRITEQUEST_0_RANGER = 70,
    TEST_LOADSAVEWRITEQUEST_1_COMPLETE = 71,
    TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT = 72,
    TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST = 73,
    TEST_LOADSAVEWRITEQUEST_1_ELF = 74,
    TEST_LOADSAVEWRITEQUEST_1_RANGER = 75,
    TEST_LOADSAVEWRITEQUEST_1_HALFLING = 76,
    TEST_LOADSAVEWRITEQUEST_1_REVEAL = 77,
    TEST_LOADSAVEWRITEQUEST_1_ITEMS = 78,
    TEST_LOADSAVEWRITEQUEST_2_COMPLETE = 79,
    TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH = 80,
    TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR = 81,
    TEST_LOADSAVEWRITEQUEST_2_ITEMS = 82,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_0 = 83,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_1 = 84,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_2 = 85,
    N_TESTS = 86
};

class Test {
    static int test_expected_n_info_dialog;

    static Item *checkFindSingleItem(Scenery **scenery_owner, Character **character_owner, PlayingGamestate *playing_gamestate, Location *location, const string &item_name, bool owned_by_scenery, bool owned_by_npc, bool owned_by_player, bool allow_multiple);
    static void checkLockedDoors(PlayingGamestate *playing_gamestate, const string &location_key_name, const string &location_doors_name, const string &key_name, int n_doors, bool key_owned_by_scenery, bool key_owned_by_npc, bool key_owned_by_player);
    static void checkCanCompleteNPC(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, int expected_xp, int expected_gold, const string &expected_item, bool can_complete, bool quest_was_item);
    static void interactNPCItem(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &location_item_name, const Vector2D &location_item_pos, const string &item_name, bool owned_by_scenery, bool owned_by_npc, int expected_xp, int expected_gold, const string &expected_item);
    static void interactNPCKill(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &objective_id, const string &check_kill_location, const string &check_kill_name, int expected_xp, int expected_gold, const string &expected_item);
    static void checkSaveGame(PlayingGamestate *playing_gamestate, int test_id);
    static void checkSaveGameWrite(PlayingGamestate *playing_gamestate, int test_id);
public:
    static void runTest(const string &filename, int test_id);
};
