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
    TEST_PERF_POINTINPOLYGON_0 = 19,
    TEST_PERF_POINTINPOLYGON_1 = 20,
    TEST_PERF_POINTINPOLYGON_2 = 21,
    TEST_PERF_DISTANCEGRAPH_0 = 22,
    TEST_PERF_PATHFINDING_0 = 23,
    TEST_PERF_REMOVE_SCENERY_0 = 24,
    TEST_PERF_REMOVE_SCENERY_1 = 25,
    TEST_PERF_REMOVE_SCENERY_2 = 26,
    TEST_PERF_UPDATE_VISIBILITY_0 = 27,
    TEST_PERF_NUDGE_0 = 28,
    TEST_PERF_NUDGE_1 = 29,
    TEST_PERF_NUDGE_2 = 30,
    TEST_PERF_NUDGE_3 = 31,
    TEST_PERF_NUDGE_4 = 32,
    TEST_PERF_NUDGE_5 = 33,
    TEST_PERF_NUDGE_6 = 34,
    TEST_PERF_NUDGE_7 = 35,
    TEST_PERF_NUDGE_8 = 36,
    TEST_PERF_NUDGE_9 = 37,
    TEST_PERF_NUDGE_10 = 38,
    TEST_PERF_NUDGE_11 = 39,
    TEST_PERF_NUDGE_12 = 40,
    TEST_PERF_NUDGE_13 = 41,
    TEST_PERF_NUDGE_14 = 42,
    TEST_USE_AMMO = 43,
    TEST_LEVEL_UP = 44,
    TEST_LOADSAVEQUEST_0 = 45,
    TEST_LOADSAVEQUEST_1 = 46,
    TEST_LOADSAVEQUEST_2 = 47,
    TEST_LOADSAVEQUEST_3 = 48,
    TEST_LOADSAVERANDOMQUEST_0 = 49,
    TEST_MEMORYQUEST_0 = 50,
    TEST_MEMORYQUEST_1 = 51,
    TEST_MEMORYQUEST_2 = 52,
    TEST_MEMORYQUEST_3 = 53,
    TEST_LOADSAVE_QUEST_1_COMPLETED = 54,
    TEST_LOADSAVE_ACTION_LAST_TIME_BUG = 55,
    TEST_LOADSAVEWRITEQUEST_0_COMPLETE = 56,
    TEST_LOADSAVEWRITEQUEST_0_WARRIOR = 57,
    TEST_LOADSAVEWRITEQUEST_0_BARBARIAN = 58,
    TEST_LOADSAVEWRITEQUEST_0_ELF = 59,
    TEST_LOADSAVEWRITEQUEST_0_RANGER = 60,
    TEST_LOADSAVEWRITEQUEST_1_COMPLETE = 61,
    TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT = 62,
    TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST = 63,
    TEST_LOADSAVEWRITEQUEST_1_ELF = 64,
    TEST_LOADSAVEWRITEQUEST_1_RANGER = 65,
    TEST_LOADSAVEWRITEQUEST_1_HALFLING = 66,
    TEST_LOADSAVEWRITEQUEST_1_REVEAL = 67,
    TEST_LOADSAVEWRITEQUEST_1_ITEMS = 68,
    TEST_LOADSAVEWRITEQUEST_2_COMPLETE = 69,
    TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH = 70,
    TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR = 71,
    TEST_LOADSAVEWRITEQUEST_2_ITEMS = 72,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_0 = 73,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_1 = 74,
    TEST_LOADSAVEWRITEQUEST_2_FIRE_ANT_2 = 75,
    N_TESTS = 76
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
