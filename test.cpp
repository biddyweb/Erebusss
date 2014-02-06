#include "test.h"
#include "game.h"
#include "playinggamestate.h"
#include "logiface.h"
#include "rpg/rpgengine.h"

int Test::test_expected_n_info_dialog = 0;

/**
  TEST_PATHFINDING_0 - find a path
  TEST_PATHFINDING_1 - find a path
  TEST_PATHFINDING_2 - can't find a path as no route available
  TEST_PATHFINDING_3 - can't find a path, as start isn't in valid floor region
  TEST_PATHFINDING_4 - can't find a path, as destination isn't in valid floor region
  TEST_PATHFINDING_5 - find a path, with scenery being ignored
  TEST_PATHFINDING_6 - can't find a path - scenery is ignored but still blocking, as it doesn't block the last segment
  TEST_POINTINPOLYGON_0 - tests that a point is inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_1 - tests that a point is on a convex polygon (triangle)
  TEST_POINTINPOLYGON_2 - tests that a point is not inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_3 - tests that a point is inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_4 - tests that a point is on a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_5 - tests that a point is not inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_6 - tests that a point is inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_7 - tests that a point is on a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_8 - tests that a point is not inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_9 - tests that a point is inside a concave polygon (L-shape)
  TEST_POINTINPOLYGON_10 - tests that a point is on a concave polygon (L-shape)
  TEST_POINTINPOLYGON_11 - tests that a point is not inside a concave polygon (L-shape)
  TEST_PERF_POINTINPOLYGON_0 - performance test version of TEST_POINTINPOLYGON_9
  TEST_PERF_POINTINPOLYGON_1 - performance test version of TEST_POINTINPOLYGON_11
  TEST_PERF_POINTINPOLYGON_2 - as TEST_PERF_POINTINPOLYGON_1, but point is now outside of AABB
  TEST_PERF_DISTANCEGRAPH_0 - performance test for calculating a distance graph
  TEST_PERF_PATHFINDING_0 - performance test for calculating a shortest path
  TEST_PERF_REMOVE_SCENERY_0 - performance test for removing scenery (including recalculating distance graph)
  TEST_PERF_REMOVE_SCENERY_1 - performance test for removing scenery that was blocking important waypoint (including recalculating distance graph)
  TETS_PERF_REMOVE_SCENERY_2 - performance test for removing scenery, also tests the bug (fixed in 0.7) where we weren't updating the nodes correctly
  TEST_PERF_UPDATE_VISIBILITY_0 - performance test for updating visibility
  TEST_PERF_NUDGE_0 - performance test for nudging: clicking far side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_1 - performance test for nudging: clicking near side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_2 - performance test for nudging: clicking far side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_3 - performance test for nudging: clicking near side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_4 - performance test for nudging: clicking far side outside a west/east door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_5 - performance test for nudging: clicking near side outside a west/east door
  TEST_PERF_NUDGE_6 - performance test for nudging: clicking far side outside a north/south door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_7 - performance test for nudging: clicking near side outside a north/south door
  TEST_PERF_NUDGE_8 - performance test for nudging: clicking on west side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_9 - performance test for nudging: clicking on west side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_10 - performance test for nudging: clicking on east side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_11 - performance test for nudging: clicking on east side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_12 - performance test for nudging: clicking on east side of scenery, src is on east side
  TEST_PERF_NUDGE_13 - performance test for nudging: clicking on west side of scenery, src is on east side
  TEST_PERF_NUDGE_14 - performance test for nudging: clicking near 90 degree corner
  TEST_LOADSAVEQUEST_n - tests that we can load the nth quest, then test saving, then test loading the save game
  TEST_LOADSAVERANDOMQUEST_0 - tests that we can create a random quest, then test saving, then test loading the save game
  TEST_MEMORYQUEST_n - loads the nth quest, forces all NPCs and scenery to be instantiated, and checks the memory usage (we do this as a separate test, due to forcing all images to be loaded)
  TEST_LOADSAVE_QUEST_1_COMPLETED - test for when 1st quest is completed, and door unlocked
  TEST_LOADSAVE_ACTION_LAST_TIME_BUG - tests load/save/load cycle for _test_savegames/action_last_time_bug.xml (this protects against a bug where we were writing out invalid html for the action_last_time attribute for Scenery; in this case, the save game file is valid
  TEST_LOADSAVEWRITEQUEST_0_COMPLETE - test for 1st quest: kill all goblins, check quest then complete
  TEST_LOADSAVEWRITEQUEST_0_WARRIOR - test for 1st quest: check FP of player and goblin is as expected, then check again when they are unarmed; also test that Warrior gets FP bonus when armed with shield; also test that Warrior doesn't get +1 damage bonus against Goblin; also tests for getTimeTurn()
  TEST_LOADSAVEWRITEQUEST_0_BARBARIAN - as TEST_LOADSAVEWRITEQUEST_0_WARRIOR, but checks that Barbarian doesn't have FP penalty for being unarmed, and doesn't get FP bonus when armed with shield
  TEST_LOADSAVEWRITEQUEST_0_ELF - test for 1st quest: check that Elf gets +1 damage bonus against Goblin and Orc, but not against Wyvern
  TEST_LOADSAVEWRITEQUEST_0_RANGER - as TEST_LOADSAVEWRITEQUEST_0_WARRIOR, but checks that Ranger has faster ranged attacks
  TEST_LOADSAVEWRITEQUEST_1_COMPLETE - test for 2nd quest: pick up item, check quest then complete
  TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT - test for 2nd quest: interact with Calbert
  TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST - test for 2nd quest: interact with Ghost
  TEST_LOADSAVEWRITEQUEST_1_ELF - test for 2nd quest: Elf has sprint bonus iff outdoors; also check wandering monster rest chance is as expected
  TEST_LOADSAVEWRITEQUEST_1_RANGER - test for 2nd quest: check wandering monster rest chance is as expected, and that doesn't have sprint bonus; also check that ranger can get disease
  TEST_LOADSAVEWRITEQUEST_1_HALFLING - test for 2nd quest: check that halfling can get disease
  TEST_LOADSAVEWRITEQUEST_1_REVEAL - test for 2nd quest: check Location::revealMap() works as expected
  TEST_LOADSAVEWRITEQUEST_1_ITEMS - test for 2nd quest: check attributes for various items
  TEST_LOADSAVEWRITEQUEST_2_COMPLETE - test for 3rd quest: go through the exit, check quest then complete
  TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH - test for 3rd quest: interact with Anmareth
  TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR - test for 3rd quest: interact with Glenthor
  TEST_LOADSAVEWRITEQUEST_2_ITEMS - test for 3rd quest: check attributes for various items
  */

Item *Test::checkFindSingleItem(Scenery **scenery_owner, Character **character_owner, PlayingGamestate *playing_gamestate, Location *location, const string &item_name, bool owned_by_scenery, bool owned_by_npc, bool owned_by_player, bool allow_multiple) {
    LOG("checkFindSingleItem for %s\n", item_name.c_str());
    if( owned_by_npc && owned_by_player ) {
        throw string("test error: item can't be owned by npc and player");
    }
    vector<Scenery *> scenery_owners;
    vector<Character *> character_owners;
    vector<Item *> items = location->getItems(item_name, true, true, &scenery_owners, &character_owners);
    if( items.size() != scenery_owners.size() || items.size() != character_owners.size() ) {
        throw string("mismatched array lengths");
    }
    else if( items.size() == 0 ) {
        throw string("couldn't find any items");
    }
    else if( !allow_multiple && items.size() > 1 ) {
        throw string("too many items");
    }
    else if( owned_by_scenery ) {
        if( scenery_owners[0] == NULL || character_owners[0] != NULL ) {
            throw string("expected item to be owned by scenery");
        }
    }
    else if( owned_by_npc || owned_by_player ) {
        if( scenery_owners[0] != NULL || character_owners[0] == NULL ) {
            throw string("expected item to be owned by character");
        }
        else if( owned_by_npc && character_owners[0] == playing_gamestate->getPlayer() ) {
            throw string("didn't expect item to be owned by player, should be an NPC");
        }
        else if( owned_by_player && character_owners[0] != playing_gamestate->getPlayer() ) {
            throw string("expected item to be owned by player");
        }
    }
    else {
        if( scenery_owners[0] != NULL || character_owners[0] != NULL ) {
            throw string("expected item to be owned by nothing");
        }
    }
    if( scenery_owner != NULL )
        *scenery_owner = scenery_owners[0];
    if( character_owner != NULL )
        *character_owner = character_owners[0];
    return items[0];
}

void Test::checkLockedDoors(PlayingGamestate *playing_gamestate, const string &location_key_name, const string &location_doors_name, const string &key_name, int n_doors, bool key_owned_by_scenery, bool key_owned_by_npc, bool key_owned_by_player) {
    if( key_owned_by_scenery && ( key_owned_by_npc || key_owned_by_player ) ) {
        throw string("test error: key can't be owned by scenery and npc/player");
    }
    if( key_owned_by_npc && key_owned_by_player ) {
        throw string("test error: key can't be owned by npc and player");
    }
    Location *location_key = playing_gamestate->getQuest()->findLocation(location_key_name);
    if( location_key == NULL ) {
        throw string("can't find location for key");
    }
    Location *location_doors = playing_gamestate->getQuest()->findLocation(location_doors_name);
    if( location_doors == NULL ) {
        throw string("can't find location for doors");
    }
    set<Scenery *> scenerys = location_doors->getSceneryUnlockedBy(key_name);
    if( scenerys.size() != n_doors ) {
        throw string("unexpected number of locked scenerys");
    }
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( !scenery->isLocked() ) {
            throw string("didn't expect door to be unlocked");
        }
    }
    checkFindSingleItem(NULL, NULL, playing_gamestate, location_key, key_name, key_owned_by_scenery, key_owned_by_npc, key_owned_by_player, false);
}

void Test::checkCanCompleteNPC(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, int expected_xp, int expected_gold, const string &expected_item, bool can_complete, bool quest_was_item) {
    Location *location = playing_gamestate->getQuest()->findLocation(location_npc_name);
    if( location == NULL ) {
        throw string("can't find location with npc");
    }
    if( location != playing_gamestate->getCLocation() ) {
        playing_gamestate->moveToLocation(location, location_npc_pos);
    }

    Character *npc = location->findCharacter(npc_name);
    if( npc == NULL ) {
        throw string("can't find ") + npc_name;
    }
    if( npc->canCompleteInteraction(playing_gamestate) != can_complete ) {
        if( can_complete )
            throw string("didn't expect to have completed sub-quest for ") + npc_name;
        else
            throw string("expected to have completed quest for ") + npc_name;
    }
    if( can_complete ) {
        const Character *player = playing_gamestate->getPlayer();
        int current_xp = player->getXP();
        int current_gold = player->getGold();
        int current_item = expected_item.length() == 0 ? 0 : player->findItemCount(expected_item);
        int current_total_items = player->getItemCount();
        /*for(set<Item *>::const_iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            const Item *item = *iter;
            LOG("item: %s\n", item->getName().c_str());
        }*/

        npc->completeInteraction(playing_gamestate);

        if( player->getXP() != current_xp + expected_xp ) {
            throw string("unexpected xp reward");
        }
        if( player->getGold() != current_gold + expected_gold ) {
            throw string("unexpected gold reward");
        }
        if( expected_item.length() > 0 && player->findItemCount(expected_item) != current_item+1 ) {
            throw string("didn't get item reward");
        }
        if( expected_item.length() > 0 && player->getItemCount() != (quest_was_item ? current_total_items : current_total_items+1) ) {
            // check we didn't get any other items
            // if quest_was_item: if we got a reward, then number of items should be the same, due to giving up the quest item
            throw string("unexpected additional item reward");
        }
        /*LOG("expected_item: %s", expected_item.c_str());
        LOG("count was %d now %d\n", current_total_items, player->getItemCount());
        for(set<Item *>::const_iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            const Item *item = *iter;
            LOG("item: %s\n", item->getName().c_str());
        }*/
        if( expected_item.length() == 0 && player->getItemCount() != (quest_was_item ? current_total_items-1 : current_total_items)) {
            // if quest_was_item: if no item reward, we should lose one, due to giving up the quest item
            throw string("unexpected item reward");
        }
    }
}

void Test::interactNPCItem(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &location_item_name, const Vector2D &location_item_pos, const string &item_name, bool owned_by_scenery, bool owned_by_npc, int expected_xp, int expected_gold, const string &expected_item) {
    // first check that we can't yet complete the interaction with NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, false, true);

    // now pick up item
    Location *location_item = playing_gamestate->getQuest()->findLocation(location_item_name);
    if( location_item == NULL ) {
        throw string("can't find location with item");
    }
    if( location_item != playing_gamestate->getCLocation() ) {
        playing_gamestate->moveToLocation(location_item, location_item_pos);
    }

    Scenery *scenery = NULL;
    Item *item = checkFindSingleItem(&scenery, NULL, playing_gamestate, location_item, item_name, owned_by_scenery, owned_by_npc, false, false);
    bool move = false;
    void *ignore = NULL;
    if( owned_by_scenery )
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
    playing_gamestate->getPlayer()->pickupItem(item);

    const Character *player = playing_gamestate->getPlayer();
    int current_quest_item = player->findItemCount(item_name);
    if( current_quest_item == 0 ) {
        throw string("player didn't pick up quest item");
    }

    // now return to NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, true, true);

    if( player->findItemCount(item_name) != current_quest_item-1 ) {
        throw string("player didn't give up quest item");
    }
}

void Test::interactNPCKill(PlayingGamestate *playing_gamestate, const string &location_npc_name, const Vector2D &location_npc_pos, const string &npc_name, const string &objective_id, const string &check_kill_location, const string &check_kill_name, int expected_xp, int expected_gold, const string &expected_item) {
    // first check that we can't yet complete the interaction with NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, false, false);

    const Quest *quest = playing_gamestate->getQuest();
    for(vector<Location *>::const_iterator iter = quest->locationsBegin(); iter != quest->locationsEnd(); ++iter) {
        Location *location = *iter;
        while( location->getNCharacters() > 0 ) {
            Character *npc = NULL;
            for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd() && npc==NULL; ++iter) {
                Character *character = *iter;
                if( character->getObjectiveId() == objective_id ) {
                    if( character == playing_gamestate->getPlayer() ) {
                        throw string("player has objective_id!");
                    }
                    if( !character->isHostile() ) {
                        throw string("npc with objective_id isn't hostile");
                    }
                    if( check_kill_location.length() > 0 && check_kill_location != location->getName() ) {
                        throw string("enemies with objective_id in unexpected location");
                    }
                    if( check_kill_name.length() > 0 && check_kill_name != character->getName() ) {
                        throw string("enemy with objective_id has unexpected name");
                    }
                    npc = character;
                }
            }
            if( npc == NULL )
                break;
            location->removeCharacter(npc);
            delete npc;
        }
    }

    // now return to NPC
    checkCanCompleteNPC(playing_gamestate, location_npc_name, location_npc_pos, npc_name, expected_xp, expected_gold, expected_item, true, false);
}

/** Optional read-only checks on a loaded game.
  */
void Test::checkSaveGame(PlayingGamestate *playing_gamestate, int test_id) {
    LOG("checkSaveGame\n");
    if( test_id == TEST_LOADSAVEQUEST_0 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "", "", "Goblin's Key", 1, false, true, false);
    }
    else if( test_id == TEST_LOADSAVEQUEST_1 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "entrance" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "level_1", "level_1", "Vespar's Cell Key", 5, false, true, false);
        checkLockedDoors(playing_gamestate, "level_3", "level_3", "Hand Mirror", 1, false, false, false);
        checkLockedDoors(playing_gamestate, "cave", "level_past", "Missing Teleporter Piece", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "cave", "cave", "Orc Warlord's Key", 2, false, true, false);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Maze Key", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Bull Statuette", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "level_6", "level_6", "Minotaur's Key", 1, false, true, false);
    }
    else if( test_id == TEST_LOADSAVEQUEST_2 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "entrance" ) {
            throw string("unexpected start location");
        }
        checkLockedDoors(playing_gamestate, "level_1", "level_2", "Dwarven Key", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "level_2", "level_3", "Derrin's Ring", 1, false, true, false);
        checkLockedDoors(playing_gamestate, "level_2", "level_4", "Derrin's Ring", 1, false, true, false);
    }
    else if( test_id == TEST_LOADSAVEQUEST_3 ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        /*Location *location = playing_gamestate->getCLocation();
        if( location->getName() != "Axbury" ) {
            throw string("unexpected start location");
        }*/
        checkLockedDoors(playing_gamestate, "Dungeons near Axbury", "Dungeons near Axbury", "Axbury Dungeon Key", 2, false, true, false);
        checkLockedDoors(playing_gamestate, "Upper Level, Wentbridge Fort", "Ground Level, Wentbridge Fort", "Wentbridge Dungeon Key", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "Dungeons, Wentbridge Fort", "Dungeons, Wentbridge Fort", "Wentbridge Cell Key", 1, true, false, false);
        checkLockedDoors(playing_gamestate, "Dungeons, Wentbridge Fort", "Dungeons Lower Level, Wentbridge Fort", "Wentbridge Cell Key", 1, true, false, false);
    }
    else if( test_id == TEST_LOADSAVE_QUEST_1_COMPLETED ) {
        // check quest is completed
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed");
        }
        checkLockedDoors(playing_gamestate, "", "", "Goblin's Key", 0, false, false, true); // key now owned by player, and door should be unlocked
    }
    else if( test_id == TEST_LOADSAVE_ACTION_LAST_TIME_BUG ) {
        // check quest not completed
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
    }
    else {
        throw string("unknown test_id");
    }
}

void Test::checkSaveGameWrite(PlayingGamestate *playing_gamestate, int test_id) {
    LOG("checkSaveGameWrite");
    if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE ) {
        // check quest not completed until all goblins completed
        Location *location = playing_gamestate->getCLocation();
        while( location->getNCharacters() > 0 ) {
            Character *npc = NULL;
            for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd() && npc==NULL; ++iter) {
                Character *character = *iter;
                if( character != playing_gamestate->getPlayer() ) {
                    if( !character->isHostile() ) {
                        throw string("npc isn't hostile");
                    }
                    npc = character;
                }
            }
            if( npc == NULL )
                break;
            if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
                throw string("didn't expect quest to already be completed");
            }
            location->removeCharacter(npc);
            delete npc;
        }
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR || test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN || test_id == TEST_LOADSAVEWRITEQUEST_0_ELF || test_id == TEST_LOADSAVEWRITEQUEST_0_RANGER ) {
        Character *player = playing_gamestate->getPlayer();
        // check skill is as expected
        if( test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR && !player->hasSkill(skill_shield_combat_c) ) {
            throw string("player doesn't have unarmed combat skill");
        }
        if( test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN && !player->hasSkill(skill_unarmed_combat_c) ) {
            throw string("player doesn't have unarmed combat skill");
        }
        int base_fp = player->getBaseProfileIntProperty(profile_key_FP_c);
        int fp = player->getProfileIntProperty(profile_key_FP_c);
        if( fp != base_fp ) {
            throw string("player fp " + numberToString(fp) + " is different to base fp " + numberToString(base_fp));
        }
        // now disarm, and check we get a penalty (except for barbarian)
        player->armWeapon(NULL);
        fp = player->getProfileIntProperty(profile_key_FP_c);
        int exp_fp = (test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN) ? base_fp : base_fp-2;
        if( fp != exp_fp ) {
            throw string("player unarmed fp " + numberToString(fp) + " is not " + numberToString(exp_fp) + " as expected, base fp was " + numberToString(base_fp));
        }
        // now rearm, and add a shield, and check warrior gets a bonus; also check that Elf is too weak for Long Sword
        Weapon *weapon = static_cast<Weapon *>(playing_gamestate->cloneStandardItem("Long Sword"));
        player->addItem(weapon, true);
        player->addItem(playing_gamestate->cloneStandardItem("Shield"), true);
        if( player->getProfileIntProperty(profile_key_S_c) < weapon->getMinStrength() ) {
            if( player->getCurrentWeapon() != NULL ) {
                throw string("shouldn't have been able to arm weapon");
            }
        }
        else {
            if( player->getCurrentWeapon() != weapon ) {
                throw string("failed to arm player with weapon");
            }
        }
        if( player->getCurrentShield() == NULL ) {
            throw string("failed to arm player with shield");
        }
        fp = player->getProfileIntProperty(profile_key_FP_c);
        exp_fp = (test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR) ? base_fp+1 : base_fp;
        if( player->getCurrentWeapon() == NULL )
            exp_fp -= 2;
        if( fp != exp_fp ) {
            throw string("player with shield fp " + numberToString(fp) + " is not " + numberToString(exp_fp) + " as expected, base fp was " + numberToString(base_fp));
        }

        Location *location = playing_gamestate->getCLocation();
        Character *goblin = location->findCharacter("Goblin");
        if( goblin == NULL ) {
            throw string("can't find goblin");
        }
        if( goblin->skillsBegin() != goblin->skillsEnd() ) {
            throw string("goblin shouldn't have any skills");
        }
        base_fp = goblin->getBaseProfileIntProperty(profile_key_FP_c);
        fp = goblin->getProfileIntProperty(profile_key_FP_c);
        if( fp != base_fp ) {
            throw string("goblin fp " + numberToString(fp) + " is different to base fp " + numberToString(base_fp));
        }

        if( test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR ) {
            // test no damage bonus against goblin
            if( RPGEngine::getDamageBonusFromHatred(player, goblin, false) != 0 ) {
                throw string("shouldn't get damage bonus from hatred");
            }
        }
        else if( test_id == TEST_LOADSAVEWRITEQUEST_0_ELF ) {
            if( RPGEngine::getDamageBonusFromHatred(player, goblin, false) != 1 ) {
                throw string("should get damage bonus from hatred");
            }
            else if( RPGEngine::getDamageBonusFromHatred(player, goblin, true) != 0 ) {
                throw string("shouldn't get damage bonus from hatred for ranged");
            }
            else if( RPGEngine::getDamageBonusFromHatred(goblin, player, false) != 0 ) {
                throw string("goblin shouldn't get damage bonus");
            }
            Character *wyvern = playing_gamestate->createCharacter("Wyvern", "Wyvern");
            if( RPGEngine::getDamageBonusFromHatred(player, wyvern, false) != 0 ) {
                throw string("shouldn't get damage bonus from hatred for wyvern");
            }
        }

        const int base_time_turn_c = 1000;
        if( player->getTimeTurn(false, false) != base_time_turn_c ) {
            throw string("unexpected time turn");
        }
        // arm with a bow
        Weapon *bow = static_cast<Weapon *>(playing_gamestate->cloneStandardItem("Shortbow"));
        player->addItem(bow, true);
        player->armWeapon(bow);
        if( player->getCurrentWeapon() != bow ) {
            throw string("failed to arm player with bow");
        }
        if( player->getTimeTurn(false, true) != (test_id==TEST_LOADSAVEWRITEQUEST_0_RANGER ? 700 : base_time_turn_c) ) {
            throw string("unexpected time turn for ranged");
        }
        else if( player->getTimeTurn(true, false) != base_time_turn_c ) {
            throw string("unexpected time turn for spellcasting");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE ) {
        // check quest completed iff item picked up
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getQuest()->findLocation("level_6");
        if( location == NULL ) {
            throw string("can't find location");
        }
        vector<Scenery *> scenery_owners;
        vector<Item *> items = location->getItems("Vespar's Skull", true, false, &scenery_owners, NULL);
        if( items.size() != scenery_owners.size() ) {
            throw string("mismatched array lengths");
        }
        else if( items.size() != 1 ) {
            throw string("unexpected number of items");
        }
        Item *item = items[0];
        Scenery *scenery = scenery_owners[0];
        bool move = false;
        void *ignore = NULL;
        playing_gamestate->moveToLocation(location, Vector2D(0.0f, 0.0f));
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
        playing_gamestate->getPlayer()->pickupItem(item);
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT ) {
        // interact with Calbert
        interactNPCItem(playing_gamestate, "level_3", Vector2D(7.5f, 14.0f), "Calbert", "level_3", Vector2D(7.5f, 14.0f), "Necromancy for Beginners", true, false, 50, 0, "");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST ) {
        // interact with Ghost
        interactNPCItem(playing_gamestate, "level_6", Vector2D(5.9f, 28.0f), "Ghost", "level_6", Vector2D(5.9f, 28.0f), "Ghost's Bones", false, false, 30, 0, "");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_ELF || test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER || test_id == TEST_LOADSAVEWRITEQUEST_1_HALFLING ) {
        Character *player = playing_gamestate->getPlayer();
        // check skill is as expected
        if( test_id == TEST_LOADSAVEWRITEQUEST_1_ELF && !player->hasSkill(skill_sprint_c) ) {
            throw string("player doesn't have sprint skill");
        }
        if( test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER && !player->hasSkill(skill_hideaway_c) ) {
            throw string("player doesn't have hideaway skill");
        }
        // check hideaway skill
        Location *location = playing_gamestate->getCLocation();
        int exp_rest_chance = test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER ? 5 : 10;
        int rest_chance = location->getWanderingMonsterRestChance(player);
        if( rest_chance != exp_rest_chance ) {
            throw string("wandering monster rest chance " + numberToString(rest_chance) + " is not as expected " + numberToString(exp_rest_chance));
        }
        // check sprint skill
        float base_sp = player->getBaseProfileFloatProperty(profile_key_Sp_c);
        float sp = player->getProfileFloatProperty(profile_key_Sp_c);
        if( fabs(base_sp - sp) > E_TOL_LINEAR ) {
            throw string("player sp " + numberToString(sp) + " is different to base sp " + numberToString(base_sp));
        }
        location = playing_gamestate->getQuest()->findLocation("level_past");
        playing_gamestate->moveToLocation(location, Vector2D(25.0f, 24.0f));
        sp = player->getProfileFloatProperty(profile_key_Sp_c);
        float exp_sp = test_id == TEST_LOADSAVEWRITEQUEST_1_ELF ? base_sp + 0.2f : base_sp;
        if( fabs(exp_sp - sp) > E_TOL_LINEAR ) {
            throw string("player sp " + numberToString(sp) + " does not have expected sp " + numberToString(exp_sp) + " base sp was " + numberToString(base_sp));
        }
        // check effects of disease
        bool can_get_disease = test_id != TEST_LOADSAVEWRITEQUEST_1_HALFLING;
        location = playing_gamestate->getQuest()->findLocation("level_5");
        playing_gamestate->moveToLocation(location, Vector2D(21.0f, 2.5f));
        Character *character = location->findCharacter("Zombie");
        if( character == NULL ) {
            throw string("can't find zombie");
        }
        for(int i=0;i<1000;i++) {
            character->handleSpecialHitEffects(playing_gamestate, playing_gamestate->getPlayer());
            if( character->isDiseased() )
                break;
        }
        // n.b., very small chance that non-Halfling characters won't have got a disease! If this ever happens, need to rerun the test.
        if( playing_gamestate->getPlayer()->isDiseased() != can_get_disease ) {
            throw string("unexpected disease status");
        }
        // check effects of mushroom
        for(int i=0;i<100;i++) {
            Item *item = playing_gamestate->cloneStandardItem("Mushroom");
            int old_health = playing_gamestate->getPlayer()->getHealth();
            playing_gamestate->getPlayer()->addItem(item);
            if( !item->useItem(playing_gamestate, playing_gamestate->getPlayer()) ) {
                throw string("Failed to use up mushroom");
            }
            playing_gamestate->getPlayer()->takeItem(item);
            delete item;
            if( playing_gamestate->getPlayer()->getHealth() < old_health ) {
                if( test_id == TEST_LOADSAVEWRITEQUEST_1_HALFLING ) {
                    throw string("Halfling shouldn't have been harmed by mushroom");
                }
                break;
            }
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_REVEAL ) {
        Location *location = playing_gamestate->getCLocation();
        int n_visible = 0;
        QList<QGraphicsItem *> list = playing_gamestate->getView()->scene()->items();
        foreach(QGraphicsItem *item, list) {
            if( item->isVisible() ) {
                n_visible++;
            }
        }
        if( n_visible == 0 ) {
            throw string("no floor_regions initially visible");
        }
        location->revealMap(playing_gamestate);
        int new_n_visible = 0;
        list = playing_gamestate->getView()->scene()->items();
        foreach(QGraphicsItem *item, list) {
            if( item->isVisible() ) {
                new_n_visible++;
            }
        }
        if( new_n_visible <= n_visible ) {
            throw string("new_n_visible " + numberToString(new_n_visible) + " should be greater than n_visible " + numberToString(n_visible));
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_1_ITEMS ) {
        Location *location = playing_gamestate->getQuest()->findLocation("level_2");
        Item *item = checkFindSingleItem(NULL, NULL, playing_gamestate, location, "Shortbow", false, true, false, true);
        if( item->getType() != ITEMTYPE_WEAPON ) {
            throw string("expected a weapon");
        }
        if( item->getImageName() != "longbow" ) {
            throw string("item image name is not as expected");
        }
        if( item->getWeight() != 4 ) {
            throw string("item weight is not as expected");
        }
        if( item->isMagical() ) {
            throw string("item magicalness is not as expected");
        }
        Weapon *weapon = static_cast<Weapon *>(item);
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        if( damageX != 2 || damageY != 8 || damageZ != -2 ) {
            throw string("weapon damage is not as expected");
        }
        if( weapon->getMinStrength() != 0 ) {
            throw string("weapon min strength is not as expected");
        }
        if( weapon->getAnimationName() != "longbow" ) {
            throw string("weapon animation name is not as expected");
        }
        if( weapon->getWeaponClass() != "bow" ) {
            throw string("weapon class is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_FP_c) != 0 ) {
            throw string("weapon bonus FP is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_BS_c) != -1 ) {
            throw string("weapon bonus BS is not as expected");
        }
        if( !weapon->isTwoHanded() ) {
            throw string("weapon two-handedness is not as expected");
        }
        if( !weapon->isRangedOrThrown() ) {
            throw string("weapon ranged/thrown status is not as expected");
        }
        if( weapon->getAmmoKey() != "Arrows") {
            throw string("weapon ammo key is not as expected");
        }
        if( weapon->getBaseTemplate() != "" ) {
            throw string("weapon base template is not as expected");
        }
        if( weapon->getWorthBonus() != 0 ) {
            throw string("weapon worth bonus is not as expected");
        }

        item = checkFindSingleItem(NULL, NULL, playing_gamestate, location, "Magic Dagger", true, false, false, false);
        if( item->getType() != ITEMTYPE_WEAPON ) {
            throw string("expected a weapon");
        }
        if( item->getImageName() != "dagger" ) {
            throw string("item image name is not as expected");
        }
        if( item->getWeight() != 5 ) {
            throw string("item weight is not as expected");
        }
        if( !item->isMagical() ) {
            throw string("item magicalness is not as expected");
        }
        weapon = static_cast<Weapon *>(item);
        weapon->getDamage(&damageX, &damageY, &damageZ);
        if( damageX != 1 || damageY != 6 || damageZ != 1 ) {
            throw string("weapon damage is not as expected");
        }
        if( weapon->getMinStrength() != 0 ) {
            throw string("weapon min strength is not as expected");
        }
        if( weapon->getAnimationName() != "dagger" ) {
            throw string("weapon animation name is not as expected");
        }
        if( weapon->getWeaponClass() != "" ) {
            throw string("weapon class is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_FP_c) != -1 ) {
            throw string("weapon bonus FP is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_BS_c) != 0 ) {
            throw string("weapon bonus BS is not as expected");
        }
        if( weapon->isTwoHanded() ) {
            throw string("weapon two-handedness is not as expected");
        }
        if( weapon->isRangedOrThrown() ) {
            throw string("weapon ranged/thrown status is not as expected");
        }
        if( weapon->getAmmoKey() != "") {
            throw string("weapon ammo key is not as expected");
        }
        if( weapon->getBaseTemplate() != "Dagger" ) {
            throw string("weapon base template is not as expected");
        }
        if( weapon->getWorthBonus() != 200 ) {
            throw string("weapon worth bonus is not as expected");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE ) {
        // check quest completed iff go through exit picked up
        test_expected_n_info_dialog++;
        if( playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("didn't expect quest to already be completed");
        }
        Location *location = playing_gamestate->getQuest()->findLocation("level_5");
        if( location == NULL ) {
            throw string("can't find location");
        }
        Scenery *scenery = location->findScenery("North Exit");
        if( scenery == NULL ) {
            throw string("can't find north exit");
        }
        bool move = false;
        void *ignore = NULL;
        playing_gamestate->moveToLocation(location, Vector2D(0.0f, 0.0f));
        playing_gamestate->interactWithScenery(&move, &ignore, scenery);
        LOG("now check quest complete\n");
        if( !playing_gamestate->getQuest()->testIfComplete(playing_gamestate) ) {
            throw string("expected quest to be completed now");
        }
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH ) {
        // interact with Anmareth
        interactNPCItem(playing_gamestate, "entrance", Vector2D(3.5f, 6.0f), "Anmareth", "level_1", Vector2D(21.0f, 39.0f), "Dire Leaf", true, false, 30, 0, "Potion of Healing");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR ) {
        interactNPCKill(playing_gamestate, "level_1", Vector2D(39.0f, 29.0f), "Glenthor", "Glenthor", "level_1", "Wyvern", 75, 0, "");
    }
    else if( test_id == TEST_LOADSAVEWRITEQUEST_2_ITEMS ) {
        Location *location = playing_gamestate->getQuest()->findLocation("level_1");
        Item *item = checkFindSingleItem(NULL, NULL, playing_gamestate, location, "Long Sword", true, false, false, false);
        if( item->getType() != ITEMTYPE_WEAPON ) {
            throw string("expected a weapon");
        }
        if( item->getImageName() != "longsword" ) {
            throw string("item image name is not as expected");
        }
        if( item->getWeight() != 10 ) {
            throw string("item weight is not as expected");
        }
        if( item->isMagical() ) {
            throw string("item magicalness is not as expected");
        }
        Weapon *weapon = static_cast<Weapon *>(item);
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        if( damageX != 2 || damageY != 10 || damageZ != 2 ) {
            throw string("weapon damage is not as expected");
        }
        if( weapon->getMinStrength() != 7 ) {
            throw string("weapon min strength is not as expected");
        }
        if( weapon->getAnimationName() != "longsword" ) {
            throw string("weapon animation name is not as expected");
        }
        if( weapon->getWeaponClass() != "" ) {
            throw string("weapon class is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_FP_c) != 0 ) {
            throw string("weapon bonus FP is not as expected");
        }
        if( weapon->getRawProfileBonusIntProperty(profile_key_BS_c) != 0 ) {
            throw string("weapon bonus BS is not as expected");
        }
        if( weapon->isTwoHanded() ) {
            throw string("weapon two-handedness is not as expected");
        }
        if( weapon->isRangedOrThrown() ) {
            throw string("weapon ranged/thrown status is not as expected");
        }
        if( weapon->getAmmoKey() != "") {
            throw string("weapon ammo key is not as expected");
        }
        if( weapon->getBaseTemplate() != "Long Sword" ) {
            throw string("weapon base template is not as expected");
        }
        if( weapon->getWorthBonus() != 150 ) {
            throw string("weapon worth bonus is not as expected");
        }

        item = checkFindSingleItem(NULL, NULL, playing_gamestate, location, "Chain Mail Armour", false, true, false, true);
        if( item->getType() != ITEMTYPE_ARMOUR ) {
            throw string("expected an armour");
        }
        if( item->getImageName() != "metal_armour" ) {
            throw string("item image name is not as expected");
        }
        if( item->getWeight() != 150 ) {
            throw string("item weight is not as expected");
        }
        if( item->isMagical() ) {
            throw string("item magicalness is not as expected");
        }
        Armour *armour = static_cast<Armour *>(item);
        if( armour->getMinStrength() != 7 ) {
            throw string("armour min strength is not as expected");
        }
        if( armour->getRating() != 4 ) {
            throw string("armour rating is not as expected");
        }
    }
    else {
        throw string("unknown test_id");
    }
}

void Test::runTest(const string &filename, int test_id) {
    LOG(">>> Run Test: %d\n", test_id);
    ASSERT_LOGGER(test_id >= 0);
    ASSERT_LOGGER(test_id < N_TESTS);

    game_g->startTesting();
    test_expected_n_info_dialog = 0;

    FILE *testfile = fopen(filename.c_str(), "at+");
    if( testfile == NULL ) {
        LOG("### FAILED to open/create %s\n", filename.c_str());
        return;
    }
    fprintf(testfile, "%d,", test_id);

    bool ok = true;
    bool has_score = false;
    double score = 0;

    try {
        if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_1 || test_id == TEST_PATHFINDING_2 || test_id == TEST_PATHFINDING_3 || test_id == TEST_PATHFINDING_4 || test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 10.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id != TEST_PATHFINDING_2 ) {
                    FloorRegion *floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                    location.addFloorRegion(floor_region);
                }
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }

            void *ignore = NULL;
            Scenery *scenery = NULL;
            if( test_id == TEST_PATHFINDING_5 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.0f);
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 8.0f, 8.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 1.5f, 5.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();

            Vector2D src, dest;
            int expected_points = 0;
            if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_2 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = test_id == TEST_PATHFINDING_0 ? 3 : 0;
            }
            else if( test_id == TEST_PATHFINDING_1 ) {
                src = Vector2D(4.0f, 4.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_3 ) {
                src = Vector2D(1.0f, 5.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 0;
            }
            else if( test_id == TEST_PATHFINDING_4 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(15.0f, 2.0f);
                expected_points = 0;
            }
            else if( test_id == TEST_PATHFINDING_5 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(2.5f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(1.5f, 1.0f);
                expected_points = 3;
            }
            else {
                throw string("missing case for this test_id");
            }
            vector<Vector2D> path = location.calculatePathTo(src, dest, ignore, false);

            LOG("path has %d points\n", path.size());
            for(size_t i=0;i<path.size();i++) {
                Vector2D point = path.at(i);
                LOG("    %d : %f, %f\n", i, point.x, point.y);
            }

            if( path.size() != expected_points ) {
                throw string("Unexpected number of path points");
            }
            else if( ignore == NULL && path.size() > 0 && path.at( path.size()-1 ) != dest ) {
                throw string("Unexpected end of path");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_0 || test_id == TEST_POINTINPOLYGON_1 || test_id == TEST_POINTINPOLYGON_2 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(-0.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_0 ) {
                test_pt.set(0.0f, -1.05f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_1 ) {
                test_pt.set(0.5f, -1.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.0f, -1.1f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_3 || test_id == TEST_POINTINPOLYGON_4 || test_id == TEST_POINTINPOLYGON_5 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(0.5f, 0.8f));
            poly.addPoint(Vector2D(-1.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_3 ) {
                test_pt.set(0.49f, 0.75f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_4 ) {
                test_pt.set(0.5f, 0.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.51f, 0.75f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_6 || test_id == TEST_POINTINPOLYGON_7 || test_id == TEST_POINTINPOLYGON_8 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, 1.0f));
            poly.addPoint(Vector2D(0.9f, -0.9f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_6 ) {
                test_pt.set(0.91f, -0.91f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_7 ) {
                test_pt.set(0.95f, 0.05f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.8f, -0.89f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ||
                 test_id == TEST_PERF_POINTINPOLYGON_0 || test_id == TEST_PERF_POINTINPOLYGON_1 || test_id == TEST_PERF_POINTINPOLYGON_2
                 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(0.0f, 0.0f));
            poly.addPoint(Vector2D(0.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 0.0f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_PERF_POINTINPOLYGON_0 ) {
                test_pt.set(0.5f, 2.5f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_10 ) {
                test_pt.set(0.5f, 0.0f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_11 || test_id == TEST_PERF_POINTINPOLYGON_1 ) {
                test_pt.set(1.1f, 0.1f);
                exp_inside = false;
            }
            else {
                test_pt.set(1.1f, -0.1f);
                exp_inside = false;
            }

            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ) {
                bool inside = poly.pointInside(test_pt);
                if( inside != exp_inside ) {
                    throw string("failed point inside polygon test");
                }
            }
            else {
                QElapsedTimer timer;
                timer.start();
                //int time_s = clock();
                int n_times = 1000000;
                for(int i=0;i<n_times;i++) {
                    bool inside = poly.pointInside(test_pt);
                    if( inside != exp_inside ) {
                        throw string("failed point inside polygon test");
                    }
                }
                has_score = true;
                //score = ((double)(clock() - time_s)) / (double)n_times;
                score = ((double)timer.elapsed()) / ((double)n_times);
                score /= 1000.0;
            }
        }
        else if( test_id == TEST_PERF_DISTANCEGRAPH_0 || test_id == TEST_PERF_PATHFINDING_0 || test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 || test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(14.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 5.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 10.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 15.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 25.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(0.0f, 30.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(5.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(15.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(20.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 5.5f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.25f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 3.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                n_times = 1; // test can only be run once!
            }
            for(int i=0;i<n_times;i++) {
                if( test_id == TEST_PERF_DISTANCEGRAPH_0 ) {
                    location.calculateDistanceGraph();
                }
                else if( test_id == TEST_PERF_PATHFINDING_0 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                    Vector2D src(0.5f, 0.5f);
                    Vector2D dest(0.5f, 3.5f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
                    Vector2D pos(1.0f, 1.0f);
                    location.clearVisibility();
                    vector<FloorRegion *> floor_regions = location.updateVisibility(pos);
                    if( floor_regions.size() == 0 ) {
                        throw string("didn't find any floor regions");
                    }
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;

            //vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);

        }
        else if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 || test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 || test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 ) {
                    floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(0.0f, 4.0f, 5.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                floor_region = FloorRegion::createRectangle(2.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 5.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 ) {
                    floor_region = FloorRegion::createRectangle(3.0f, 0.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(4.0f, 0.0f, 1.0f, 5.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 4.0f, 0.01f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.01f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 2.5f);
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                scenery = new Scenery("", "", 1.0f, 0.1f, 0.9f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.95f);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();
            for(size_t i=0;i<location.getNFloorRegions();i++) {
                FloorRegion *floor_region = location.getFloorRegion(i);
                floor_region->setVisible(true);
            }

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            for(int i=0;i<n_times;i++) {
                Vector2D src, pos, expected_nudge;
                if( test_id == TEST_PERF_NUDGE_0 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.91f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_1 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.99f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_2 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.91f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_3 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.99f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_4 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(4.85f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_5 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(5.05f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_6 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 4.85f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_7 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 5.05f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_8 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.5f, 3.0f);
                    expected_nudge = Vector2D(4.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_9 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.9f, 2.0f);
                    expected_nudge = Vector2D(5.0f - npc_radius_c - E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_10 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.5f, 3.0f);
                    expected_nudge = Vector2D(5.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_11 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.11f, 2.0f);
                    expected_nudge = Vector2D(5.01f + npc_radius_c + E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_12 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(3.1f, 2.5f);
                    expected_nudge = Vector2D(3.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_13 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(1.9f, 2.6f);
                    expected_nudge = Vector2D(2.0f - npc_radius_c - E_TOL_LINEAR, 2.6f);
                }
                else if( test_id == TEST_PERF_NUDGE_14 ) {
                    src = Vector2D(2.5f, 2.5f);
                    pos = Vector2D(0.1f, 4.95f);
                    expected_nudge = Vector2D(npc_radius_c + E_TOL_LINEAR, 5.0f - npc_radius_c - E_TOL_LINEAR);
                }
                Vector2D nudge = location.nudgeToFreeSpace(src, pos, npc_radius_c);
                if( (nudge - expected_nudge).magnitude() > E_TOL_LINEAR ) {
                    LOG("src: %f, %f\n", src.x, src.y);
                    LOG("pos: %f, %f\n", pos.x, pos.y);
                    LOG("nudge: %f, %f\n", nudge.x, nudge.y);
                    LOG("expected_nudge: %f, %f\n", expected_nudge.x, expected_nudge.y);
                    throw string("unexpected nudge");
                }
                vector<Vector2D> path = location.calculatePathTo(src, nudge, NULL, false);
                if( path.size() == 0 ) {
                    throw string("failed to find path");
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVEQUEST_0 || test_id == TEST_LOADSAVEQUEST_1 || test_id == TEST_LOADSAVEQUEST_2 || test_id == TEST_LOADSAVEQUEST_3 ) {
            // load, check, save, load, check
            QElapsedTimer timer;
            timer.start();
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, "Warrior", "name", false, false, 0);
            game_g->setGamestate(playing_gamestate);

            QString qt_filename;
            if( test_id == TEST_LOADSAVEQUEST_0 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_1 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_2 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_3 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_necromancer.xml");
            }

            // load
            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            // save
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            // load
            QString full_filename = game_g->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            game_g->setGamestate(playing_gamestate);
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVERANDOMQUEST_0 ) {
            // load, save, load
            QElapsedTimer timer;
            timer.start();

            // load
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_RANDOM, "Warrior", "name", false, false, 0);
            game_g->setGamestate(playing_gamestate);

            playing_gamestate->createRandomQuest();
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            // save
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            // load
            QString full_filename = game_g->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            // n.b., use GAMETYPE_CAMPAIGN as we're loading a game
            game_g->setGamestate(playing_gamestate);
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_MEMORYQUEST_0 || test_id == TEST_MEMORYQUEST_1 || test_id == TEST_MEMORYQUEST_2 || test_id == TEST_MEMORYQUEST_3 ) {
            // load
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, "Warrior", "name", false, false, 0);
            game_g->setGamestate(playing_gamestate);

            QString qt_filename;
            if( test_id == TEST_MEMORYQUEST_0 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_MEMORYQUEST_1 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_MEMORYQUEST_2 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }
            else if( test_id == TEST_MEMORYQUEST_3 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_necromancer.xml");
            }

            // load
            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            const int max_memory_c = 80000000;
            // if this check fails, retest the amount of memory usage on Windows and Android for this quest, to see if the new memory usage is acceptable
            // ideally we should be less than 128MB
            int memory_size = playing_gamestate->getImageMemorySize();
            if( memory_size > max_memory_c ) {
                throw string("image memory size too large: ") + numberToString(memory_size);
            }
        }
        else if( test_id == TEST_LOADSAVE_QUEST_1_COMPLETED || test_id == TEST_LOADSAVE_ACTION_LAST_TIME_BUG ) {
            // load, check, save, load, check
            QElapsedTimer timer;
            timer.start();

            // load
            QString load_filename = "";
            if( test_id == TEST_LOADSAVE_QUEST_1_COMPLETED )
                load_filename = "quest_1_completed.xml";
            else if( test_id == TEST_LOADSAVE_ACTION_LAST_TIME_BUG )
                load_filename = "action_last_time_bug.xml";
            QString full_load_filename = "../erebus/_test_savegames/" + load_filename; // hack to get local directory rather than deployed directory
            LOG("try loading %s\n", full_load_filename.toStdString().c_str());
            PlayingGamestate *playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            game_g->setGamestate(playing_gamestate);
            playing_gamestate->loadQuest(full_load_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            // save
            QString save_filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", save_filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(save_filename, false) ) {
                throw string("failed to save game");
            }

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            // load
            QString full_save_filename = game_g->getApplicationFilename(savegame_folder + save_filename);
            LOG("now try loading %s\n", full_save_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            game_g->setGamestate(playing_gamestate);
            playing_gamestate->loadQuest(full_save_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            checkSaveGame(playing_gamestate, test_id);

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_ELF ||
                 test_id == TEST_LOADSAVEWRITEQUEST_0_RANGER ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_ELF ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_HALFLING ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_REVEAL ||
                 test_id == TEST_LOADSAVEWRITEQUEST_1_ITEMS ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR ||
                 test_id == TEST_LOADSAVEWRITEQUEST_2_ITEMS ) {
            // load, check, load, save, load, check
            QElapsedTimer timer;
            timer.start();

            // load
            LOG("1 load\n");
            string player = "Warrior";
            if( test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN )
                player = "Barbarian";
            else if( test_id == TEST_LOADSAVEWRITEQUEST_0_ELF || test_id == TEST_LOADSAVEWRITEQUEST_1_ELF )
                player = "Elf";
            else if( test_id == TEST_LOADSAVEWRITEQUEST_0_RANGER || test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER )
                player = "Ranger";
            else if( test_id == TEST_LOADSAVEWRITEQUEST_1_HALFLING )
                player = "Halfling";
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, player, "name", false, false, 0);
            game_g->setGamestate(playing_gamestate);

            QString qt_filename;
            if( test_id == TEST_LOADSAVEWRITEQUEST_0_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_0_WARRIOR || test_id == TEST_LOADSAVEWRITEQUEST_0_BARBARIAN || test_id == TEST_LOADSAVEWRITEQUEST_0_ELF || test_id == TEST_LOADSAVEWRITEQUEST_0_RANGER ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_LOADSAVEWRITEQUEST_1_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_CALBERT || test_id == TEST_LOADSAVEWRITEQUEST_1_NPC_GHOST || test_id == TEST_LOADSAVEWRITEQUEST_1_ELF || test_id == TEST_LOADSAVEWRITEQUEST_1_RANGER || test_id == TEST_LOADSAVEWRITEQUEST_1_HALFLING || test_id == TEST_LOADSAVEWRITEQUEST_1_REVEAL || test_id == TEST_LOADSAVEWRITEQUEST_1_ITEMS ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_LOADSAVEWRITEQUEST_2_COMPLETE || test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_ANMARETH || test_id == TEST_LOADSAVEWRITEQUEST_2_NPC_GLENTHOR || test_id == TEST_LOADSAVEWRITEQUEST_2_ITEMS ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }

            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            LOG("2 check\n");
            checkSaveGameWrite(playing_gamestate, test_id);

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            // load
            LOG("3 load\n");
            playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, player, "name", false, false, 0);
            game_g->setGamestate(playing_gamestate);

            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // save
            LOG("4 save\n");
            QString filename = "EREBUSTEST_" + QString::number(test_id) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            // load
            LOG("5 load\n");
            QString full_filename = game_g->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            game_g->setGamestate(playing_gamestate);
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            // check
            LOG("6 check\n");
            checkSaveGameWrite(playing_gamestate, test_id);

            delete playing_gamestate;
            game_g->setGamestate(NULL);
            playing_gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else {
            throw string("unknown test");
        }

        // final checks
        if( game_g->getTestNInfoDialog() != test_expected_n_info_dialog ) {
            throw string("unexpected test_n_info_dialog; expected: ") + numberToString(test_expected_n_info_dialog) + string(" actual: ") + numberToString(game_g->getTestNInfoDialog());
        }

        fprintf(testfile, "PASSED,");
        if( has_score ) {
            if( score < 0.01 ) {
                // use exponential notation for small values
                fprintf(testfile, "%E", score);
            }
            else {
                fprintf(testfile, "%f", score);
            }
        }
        fprintf(testfile, "\n");
    }
    catch(const string &str) {
        LOG("ERROR: %s\n", str.c_str());
        fprintf(testfile, "FAILED,%s\n", str.c_str());
        ok = false;
    }

    if( ok ) {
        LOG("<<< TEST %d PASSED\n", test_id);
    }
    else {
        LOG("<<< TEST %d FAILED\n", test_id);
    }

    fclose(testfile);
    game_g->stopTesting();
}
