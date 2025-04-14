/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"

// Global variable to track if the initial catcher has been selected
static qboolean g_tagGameCatcherSelected = qfalse;
static qboolean g_tagGameInProgress = qfalse;

/*
=================
G_TagGame_SelectInitialCatcher
=================
*/
void G_TagGame_SelectInitialCatcher(void) {
    int i;
    int numPlayers = 0;
    int randomPlayerNum;
    gentity_t *selectedPlayer = NULL;
    int playerList[MAX_CLIENTS];

    // Only select catcher once per round
    if (g_tagGameCatcherSelected) {
        return;
    }

    // Count active players and build a list
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED && 
            level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) {
            // Make sure the player is actually in the game (not connecting)
            if (g_entities[i].inuse) {
                playerList[numPlayers] = i;
                numPlayers++;
            }
        }
    }

    // Make sure we have enough players to start (minimum 3)
    if (numPlayers < 3) {
        return;
    }

    // Choose a random player as the catcher
    randomPlayerNum = rand() % numPlayers;
    selectedPlayer = &g_entities[playerList[randomPlayerNum]];

    if (selectedPlayer && selectedPlayer->client) {
        // Unlock teams before changing them
        level.RedTeamLocked = qfalse;
        level.BlueTeamLocked = qfalse;
        
        // Log initial team assignments
        G_Printf("TagGame: Initial team assignments starting. Catcher: %s\n", 
                selectedPlayer->client->pers.netname);
        
        // First, make sure all players are on free team to avoid issues
        for (i = 0; i < level.maxclients; i++) {
            gentity_t *player = &g_entities[i];
            if (player->inuse && player->client && player->client->pers.connected == CON_CONNECTED && 
                player->client->sess.sessionTeam != TEAM_SPECTATOR) {
                // Set to free team first
                player->client->sess.sessionTeam = TEAM_FREE;
                player->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
            }
        }
        
        // Then announce the initial catcher
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " is the catcher!\n\"", 
                                    selectedPlayer->client->pers.netname));
        
        // First set the internal team state for the catcher
        selectedPlayer->client->sess.sessionTeam = TEAM_RED;
        selectedPlayer->client->ps.persistant[PERS_TEAM] = TEAM_RED;
        
        // Then call SetTeam_Force to handle UI and other team logic
        SetTeam_Force(selectedPlayer, "r", NULL, qtrue);
        
        // Force team update again to ensure it persists
        selectedPlayer->client->sess.sessionTeam = TEAM_RED;
        selectedPlayer->client->ps.persistant[PERS_TEAM] = TEAM_RED;
        
        // Initialize catcher with proper weapons
        G_TagGame_InitClient(selectedPlayer->client);
        
        // For bots, need extra handling
        if (selectedPlayer->r.svFlags & SVF_BOT) {
            ClientSpawn(selectedPlayer);
            // Re-apply after spawn
            selectedPlayer->client->sess.sessionTeam = TEAM_RED;
            selectedPlayer->client->ps.persistant[PERS_TEAM] = TEAM_RED;
            G_TagGame_InitClient(selectedPlayer->client);
        }
        
        // Move all other players to the blue team (runners)
        for (i = 0; i < level.maxclients; i++) {
            gentity_t *player = &g_entities[i];
            if (player->inuse && player->client && player->client->pers.connected == CON_CONNECTED && 
                player->client->sess.sessionTeam != TEAM_SPECTATOR && 
                player != selectedPlayer) {
                
                // First set the internal team state directly
                player->client->sess.sessionTeam = TEAM_BLUE;
                player->client->ps.persistant[PERS_TEAM] = TEAM_BLUE;
                
                // Then use SetTeam_Force for UI updates and other team logic
                SetTeam_Force(player, "b", NULL, qtrue);
                
                // Force state update again to ensure it persists
                player->client->sess.sessionTeam = TEAM_BLUE;
                player->client->ps.persistant[PERS_TEAM] = TEAM_BLUE;
                
                // Initialize all runners with proper weapons
                G_TagGame_InitClient(player->client);
                
                // Extra handling for bots to ensure team state is correct
                if (player->r.svFlags & SVF_BOT) {
                    // Reinitialize the bot at the current position
                    vec3_t origin, angles;
                    VectorCopy(player->client->ps.origin, origin);
                    VectorCopy(player->client->ps.viewangles, angles);
                    ClientSpawn(player);
                    VectorCopy(origin, player->client->ps.origin);
                    VectorCopy(angles, player->client->ps.viewangles);
                    
                    // Re-apply team state after spawn
                    player->client->sess.sessionTeam = TEAM_BLUE;
                    player->client->ps.persistant[PERS_TEAM] = TEAM_BLUE;
                    
                    // Make sure they have the right weapons
                    G_TagGame_InitClient(player->client);
                }
            }
        }
        
        // Set game state variables
        g_tagGameCatcherSelected = qtrue;
        g_tagGameInProgress = qtrue;
        
        // Lock teams to prevent players from switching
        level.RedTeamLocked = qtrue;
        level.BlueTeamLocked = qtrue;
        
        // Log completion
        G_Printf("TagGame: Team assignments complete. Catcher: %s (team %d)\n", 
                selectedPlayer->client->pers.netname, 
                selectedPlayer->client->sess.sessionTeam);
        
        // Log all team assignments for debugging
        for (i = 0; i < level.maxclients; i++) {
            gentity_t *player = &g_entities[i];
            if (player->inuse && player->client && player->client->pers.connected == CON_CONNECTED && 
                player->client->sess.sessionTeam != TEAM_SPECTATOR) {
                G_Printf("TagGame: Player %s is on team %d\n", 
                        player->client->pers.netname, 
                        player->client->sess.sessionTeam);
            }
        }
    }
}

/*
=================
G_TagGame_InitClient
=================
*/
void G_TagGame_InitClient(gclient_t *client) {
    int i;
    gentity_t *ent = &g_entities[client - level.clients];
    
    // Debug message to track client initialization
    G_Printf("TagGame: Initializing client %d (team %d)\n", 
             (int)(client - level.clients), client->sess.sessionTeam);
    
    // Clear all weapons
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
        client->ps.ammo[i] = 0;
    }
    
    // Only give the gauntlet
    client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);
    client->ps.ammo[WP_GAUNTLET] = -1; // Unlimited ammo
    client->ps.weapon = WP_GAUNTLET;   // Set active weapon
    
    // Always ensure team state is consistent in all team-related fields
    if (client->sess.sessionTeam == TEAM_RED) {
        client->ps.persistant[PERS_TEAM] = TEAM_RED;
        // These may be redundant but ensure all team flags are consistent
        ent->client->sess.sessionTeam = TEAM_RED;
    } else if (client->sess.sessionTeam == TEAM_BLUE) {
        client->ps.persistant[PERS_TEAM] = TEAM_BLUE;
        // These may be redundant but ensure all team flags are consistent
        ent->client->sess.sessionTeam = TEAM_BLUE;
    }
    
    // For bots, make sure their AI knows their team
    if (ent->r.svFlags & SVF_BOT) {
        // Force respawn to update bot team state
        client->ps.pm_type = PM_NORMAL;
        client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];
        client->ps.stats[STAT_ARMOR] = 100;
        
        // Log bot state after initialization
        G_Printf("TagGame: Bot %s initialized with team %d (ps team: %d)\n", 
                 client->pers.netname, client->sess.sessionTeam, 
                 client->ps.persistant[PERS_TEAM]);
    }
}

/*
=================
G_TagGame_PlayerKilled
=================
*/
void G_TagGame_PlayerKilled(gentity_t *attacker, gentity_t *target, int meansOfDeath) {
    // Only process if the game is in progress
    if (!g_tagGameInProgress || level.intermissiontime) {
        return;
    }
    
    // Check team status for debug purposes
    G_Printf("TagGame Kill: Attacker %s (team %d), Target %s (team %d)\n", 
            attacker->client->pers.netname, 
            attacker->client->sess.sessionTeam,
            target->client->pers.netname,
            target->client->sess.sessionTeam);
    
    // Check if a player was killed by a catcher (red team player kills blue team player)
    if (attacker && attacker->client && target && target->client && 
        attacker->client->sess.sessionTeam == TEAM_RED && 
        target->client->sess.sessionTeam == TEAM_BLUE) {
        
        // Update score for the catcher
        attacker->client->ps.persistant[PERS_SCORE]++;
        
        // Make sure the killed player is not a spectator
        if (target->client->sess.sessionTeam == TEAM_BLUE) {
            // Announce the conversion
            trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " was caught and is now a catcher!\n\"", 
                                        target->client->pers.netname));
            
            G_Printf("TagGame: %s was converted to a catcher\n", target->client->pers.netname);
            
            // First force the internal team state to ensure it's updated
            target->client->sess.sessionTeam = TEAM_RED;
            target->client->ps.persistant[PERS_TEAM] = TEAM_RED;
            
            // Then call SetTeam_Force to handle UI updates and other team logic
            SetTeam_Force(target, "r", NULL, qtrue);
            
            // Ensure team state is correctly set after SetTeam_Force
            target->client->sess.sessionTeam = TEAM_RED;
            target->client->ps.persistant[PERS_TEAM] = TEAM_RED;
            
            // For all players, ensure proper initialization
            G_TagGame_InitClient(target->client);
            
            // For bots, need more explicit handling
            if (target->r.svFlags & SVF_BOT) {
                // Force a respawn to ensure all bot AI knows about the team change
                ClientSpawn(target);
                
                // Re-apply TagGame settings after spawn
                G_TagGame_InitClient(target->client);
            }
            
            // Schedule end condition check for next frame to avoid race conditions
            level.tagGameCheckEndNextFrame = qtrue;
        }
    }
}

/*
=================
G_TagGame_CheckEndCondition
=================
*/
qboolean G_TagGame_CheckEndCondition(void) {
    int i;
    int blueCount = 0;
    int redCount = 0;
    int totalPlayers = 0;
    gentity_t *lastBluePlayer = NULL;
    
    // Don't check end conditions if we're still in warmup or teams haven't been assigned yet
    if (level.warmupTime > 0 || !g_tagGameCatcherSelected || !g_tagGameInProgress || level.intermissiontime) {
        return qfalse;
    }
    
    // Count players on both teams
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            if (level.clients[i].sess.sessionTeam == TEAM_BLUE) {
                blueCount++;
                lastBluePlayer = &g_entities[i];
                totalPlayers++;
            } else if (level.clients[i].sess.sessionTeam == TEAM_RED) {
                redCount++;
                totalPlayers++;
            }
        }
    }
    
    // If we don't have enough players to continue (minimum 3)
    if (totalPlayers < 3) {
        // Announce insufficient players
        trap_SendServerCommand(-1, "cp \"Not enough players to continue Tag Game!\n\"");
        G_Printf("TagGame: Not enough players to continue (%d)\n", totalPlayers);
        
        // Reset the game state
        g_tagGameCatcherSelected = qfalse;
        g_tagGameInProgress = qfalse;
        
        // Schedule map restart instead of immediate exit to prevent race conditions
        level.exitTime = level.time + 5000; // 5 second delay
        return qtrue;
    }
    
    // If only one player remains on blue team, end the round
    if (blueCount == 1 && lastBluePlayer) {
        // Announce the winner
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " is the last survivor!\n\"", 
                                    lastBluePlayer->client->pers.netname));
        
        G_Printf("TagGame: %s is the last survivor\n", lastBluePlayer->client->pers.netname);
        
        // Award bonus points to the winner (3 points for surviving)
        lastBluePlayer->client->ps.persistant[PERS_SCORE] += 3;
        
        // Reset the game state
        g_tagGameCatcherSelected = qfalse;
        g_tagGameInProgress = qfalse;
        
        // Schedule map restart instead of immediate exit
        level.exitTime = level.time + 5000; // 5 second delay
        return qtrue;
    } else if (blueCount == 0) {
        // All players have been caught
        trap_SendServerCommand(-1, "cp \"All players have been caught!\n\"");
        G_Printf("TagGame: All players have been caught\n");
        
        // Reset the game state
        g_tagGameCatcherSelected = qfalse;
        g_tagGameInProgress = qfalse;
        
        // Schedule map restart instead of immediate exit
        level.exitTime = level.time + 5000; // 5 second delay
        return qtrue;
    }
    
    return qfalse;
}

/*
=================
G_TagGame_Reset
=================
*/
void G_TagGame_Reset(void) {
    int i;
    
    g_tagGameCatcherSelected = qfalse;
    g_tagGameInProgress = qfalse;
    
    // Unlock teams
    level.RedTeamLocked = qfalse;
    level.BlueTeamLocked = qfalse;
    
    // Notify players that the game has been reset
    trap_SendServerCommand(-1, "cp \"Tag Game has been reset!\n\"");
    
    // Reset team assignments if teams were assigned for the tag game
    // Only do this if we're not in intermission
    if (!level.intermissiontime) {
        for (i = 0; i < level.maxclients; i++) {
            gentity_t *player = &g_entities[i];
            if (player->inuse && player->client && player->client->pers.connected == CON_CONNECTED && 
                (player->client->sess.sessionTeam == TEAM_RED || 
                 player->client->sess.sessionTeam == TEAM_BLUE)) {
                 
                // Handle differently for bots and humans
                if (player->r.svFlags & SVF_BOT) {
                    // For bots, directly set team
                    player->client->sess.sessionTeam = TEAM_FREE;
                    player->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
                    ClientBegin(player->s.number);
                } else {
                    // Reset to free team (if not team-based gametype) or balance teams
                    if (!G_IsTeamGametype() || g_gametype.integer == GT_TAGGAME) {
                        SetTeam_Force(player, "f", NULL, qtrue);
                    }
                }
            }
        }
    }
    
    G_Printf("TagGame: Game reset complete\n");
}

/*
=================
G_TagGame_IsActive
=================
*/
qboolean G_TagGame_IsActive(void) {
    return g_tagGameInProgress;
}

/*
=================
G_TagGame_CheckRoundStart
=================
*/
void G_TagGame_CheckRoundStart(void) {
    int numPlayers = 0;
    int i;
    int bots = 0;
    int humans = 0;
    
    // Don't do anything during intermission or if game is already in progress
    if (level.intermissiontime || g_tagGameCatcherSelected || g_tagGameInProgress) {
        return;
    }

    // Only start if warmup is over and the game has been running for a few seconds
    if (level.warmupTime == 0 && (level.time - level.startTime) > 3000) {
        // Count players not in spectator mode
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].pers.connected == CON_CONNECTED && 
                level.clients[i].sess.sessionTeam != TEAM_SPECTATOR &&
                g_entities[i].inuse) {
                
                numPlayers++;
                
                // Track bots vs humans separately for better diagnosis
                if (g_entities[i].r.svFlags & SVF_BOT) {
                    bots++;
                } else {
                    humans++;
                }
            }
        }
        
        // Make sure we have enough players (minimum 3) before starting
        if (numPlayers >= 3) {
            // Make sure teams aren't locked before starting
            level.RedTeamLocked = qfalse;
            level.BlueTeamLocked = qfalse;
            
            // Log player makeup for diagnostics
            G_Printf("TagGame: Starting with %d players (%d humans, %d bots)\n", 
                    numPlayers, humans, bots);
                    
            // Select initial catcher and start the round
            G_TagGame_SelectInitialCatcher();
            
            // If catcher was successfully selected, lock teams to prevent players from switching
            if (g_tagGameCatcherSelected) {
                level.RedTeamLocked = qtrue;
                level.BlueTeamLocked = qtrue;
            }
        } else if (numPlayers > 0) {
            // If we have some players but not enough, inform them
            if ((level.time / 10000) % 3 == 0) { // Show message every 30 seconds
                trap_SendServerCommand(-1, va("cp \"Need at least 3 players to start Tag Game. Currently: %d (%d humans, %d bots)\n\"", 
                                          numPlayers, humans, bots));
                // Log player makeup for diagnostics
                G_Printf("TagGame: Waiting for players. Currently: %d (%d humans, %d bots)\n", 
                        numPlayers, humans, bots);
            }
        }
    }
}

/*
=================
G_TagGame_CheckRound
=================
*/
void G_TagGame_CheckRound(void) {
    // Reset game state during intermission
    if (level.intermissiontime) {
        if (g_tagGameCatcherSelected || g_tagGameInProgress) {
            g_tagGameCatcherSelected = qfalse;
            g_tagGameInProgress = qfalse;
            // Unlock teams when game ends
            level.RedTeamLocked = qfalse;
            level.BlueTeamLocked = qfalse;
        }
        return;
    }

    // Check if we should start a new round
    G_TagGame_CheckRoundStart();

    // Process delayed end condition check if scheduled from player kill
    if (level.tagGameCheckEndNextFrame) {
        level.tagGameCheckEndNextFrame = qfalse;
        G_TagGame_CheckEndCondition();
    }
    
    // Periodically check end conditions during gameplay
    if (g_tagGameInProgress && (level.time % 1000) < 50) {  // Check roughly every second
        G_TagGame_CheckEndCondition();
    }
}
