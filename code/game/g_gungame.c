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

// Weapon progression order from best to worst
static const int g_gunGameWeaponOrder[] = {
    WP_LIGHTNING,
    WP_RAILGUN,
    WP_ROCKET_LAUNCHER,
    WP_PLASMAGUN,
    WP_NAILGUN,
    WP_CHAINGUN,
    WP_MACHINEGUN,
    WP_SHOTGUN,
    WP_GAUNTLET
};

#define GG_NUM_WEAPONS (sizeof(g_gunGameWeaponOrder) / sizeof(g_gunGameWeaponOrder[0]))

// Get the weapon for the specified level (0 is the best weapon, GG_NUM_WEAPONS-1 is the worst)
int G_GunGame_GetWeaponForLevel(int level) {
    if (level < 0) {
        level = 0;
    }
    if (level >= GG_NUM_WEAPONS) {
        level = GG_NUM_WEAPONS - 1;
    }

    return g_gunGameWeaponOrder[level];
}

// Initialize a client for GunGame
void G_GunGame_InitClient(gclient_t *client) {
    int i;
    int weapon;
    int score = client->ps.persistant[PERS_SCORE];
    
    // Clear all weapons
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
        client->ps.ammo[i] = 0;
    }
    
    // Give the first weapon in the progression based on score
    weapon = G_GunGame_GetWeaponForLevel(score);
    client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
    
    // Always give the gauntlet as well
    client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);
    
    // Set ammo for the main weapon
    if (weapon != WP_GAUNTLET) {
        client->ps.ammo[weapon] = 999;
    }
    
    // Set gauntlet ammo
    client->ps.ammo[WP_GAUNTLET] = -1;
    
    // Set the current weapon
    client->ps.weapon = weapon;
}

// Handle a player kill in GunGame
void G_GunGame_PlayerKilled(gentity_t *attacker, gentity_t *target, int meansOfDeath) {
    int currentLevel;
    int i;
    int weapon;
    int score;

    G_Printf("GunGame: Player killed\n");
    
    // if (!attacker || !attacker->client || !target || !target->client || attacker == target) {
    //     return;
    // }
    
    // Check for win condition: player is at the last level AND kills with a gauntlet
    if (meansOfDeath == MOD_GAUNTLET && attacker->client->ps.weapon == WP_GAUNTLET) {
        // Check if player is at the last level (using score)
        score = attacker->client->ps.persistant[PERS_SCORE];
        if (score >= GG_NUM_WEAPONS - 2) { // Last level is GG_NUM_WEAPONS - 1, so check for one before
            // Player won the game with a gauntlet kill
            trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " é o diabo mesmo!\n\"", 
                                        attacker->client->pers.netname));
            
            // End the round
            LogExit("GunGame round won.", qtrue);
            return;
        }
    }

    // Progress the attacker to the next weapon (which is a worse weapon)
    score = attacker->client->ps.persistant[PERS_SCORE];
    score++; // Increment score
    
    G_Printf("GunGame: Player %s is at score %d\n", attacker->client->pers.netname, score);
    
    if (score < GG_NUM_WEAPONS) {
        // Clear all weapons
        for (i = 0; i < WP_NUM_WEAPONS; i++) {
            attacker->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
            attacker->client->ps.ammo[i] = 0;
        }
        
        // Give the next weapon in the progression
        weapon = G_GunGame_GetWeaponForLevel(score);
        attacker->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
        
        // Always give the gauntlet as well
        attacker->client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);
        
        // Set ammo for the main weapon
        if (weapon != WP_GAUNTLET) {
            attacker->client->ps.ammo[weapon] = 999;
        }
        
        // Set gauntlet ammo
        attacker->client->ps.ammo[WP_GAUNTLET] = -1;
        
        // Set the current weapon
        attacker->client->ps.weapon = weapon;
        
        // Update the score
        attacker->client->ps.persistant[PERS_SCORE] = score;
        
        // Inform the player
        trap_SendServerCommand(attacker->s.number, va("cp \"vc upou pra %s\n\"", 
                                                    BG_FindItemForWeapon(weapon)->pickup_name));
    }

    // Only drop the target's score if killed by a gauntlet
    if (meansOfDeath == MOD_GAUNTLET && attacker->client->ps.weapon == WP_GAUNTLET) {
        score = target->client->ps.persistant[PERS_SCORE];
        if (score > 0) {
            score--; // Decrement score
            
            // Clear all weapons
            for (i = 0; i < WP_NUM_WEAPONS; i++) {
                target->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
                target->client->ps.ammo[i] = 0;
            }

            // Give the previous weapon in the progression
            weapon = G_GunGame_GetWeaponForLevel(score);
            target->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
            
            // Always give the gauntlet as well
            target->client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);

            // Set ammo for the main weapon
            if (weapon != WP_GAUNTLET) {
                target->client->ps.ammo[weapon] = 999;
            }
            
            // Set gauntlet ammo
            target->client->ps.ammo[WP_GAUNTLET] = -1;
            
            // Set the current weapon
            target->client->ps.weapon = weapon;
            
            // Update the score
            target->client->ps.persistant[PERS_SCORE] = score;
            
            // Inform the player
            trap_SendServerCommand(target->s.number, va("cp \"vc caiu de level pra %s\n\"", 
                                                    BG_FindItemForWeapon(weapon)->pickup_name));
            
            G_Printf("GunGame: Player %s dropped to score %d\n", target->client->pers.netname, score);
        }
    } else {
        // If not killed by gauntlet, just update the weapon without changing score
        score = target->client->ps.persistant[PERS_SCORE];
        
        // Clear all weapons
        for (i = 0; i < WP_NUM_WEAPONS; i++) {
            target->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
            target->client->ps.ammo[i] = 0;
        }

        // Give the weapon for current level
        weapon = G_GunGame_GetWeaponForLevel(score);
        target->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
        
        // Always give the gauntlet as well
        target->client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);

        // Set ammo for the main weapon
        if (weapon != WP_GAUNTLET) {
            target->client->ps.ammo[weapon] = 999;
        }
        
        // Set gauntlet ammo
        target->client->ps.ammo[WP_GAUNTLET] = -1;
        
        // Set the current weapon
        target->client->ps.weapon = weapon;
    }
}
    

// Check if a player has won the GunGame round
qboolean G_GunGame_CheckWinner(void) {
    // We don't need to check for winners here, as the win condition
    // is handled in G_GunGame_PlayerKilled when a player at the last level gets a kill with the gauntlet
    return qfalse;
} 
