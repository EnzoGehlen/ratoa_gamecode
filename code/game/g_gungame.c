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
        level = GG_NUM_WEAPONS;
    }

    return g_gunGameWeaponOrder[level];
}

// Initialize a client for GunGame
void G_GunGame_InitClient(gclient_t *client) {
    int i;
    int weapon;
    
    // Clear all weapons
    for (i = 0; i < WP_NUM_WEAPONS; i++) {
        client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
        client->ps.ammo[i] = 0;
    }
    
    // Give the first weapon in the progression
    weapon = G_GunGame_GetWeaponForLevel(client->ps.persistant[PERS_SCORE] -1);
    client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
    
    // Set ammo (except for gauntlet which doesn't need ammo)
    if (weapon != WP_GAUNTLET) {
        client->ps.ammo[weapon] = 999;
    } else {
        client->ps.ammo[weapon] = -1;
    }
    
    // Set the current weapon
    client->ps.weapon = weapon;
}

// Handle a player kill in GunGame
void G_GunGame_PlayerKilled(gentity_t *attacker, gentity_t *target, int meansOfDeath) {
    int currentLevel;
    int i;
    int weapon;

    G_Printf("GunGame: Player killed\n");
    G_Printf("GunGame: pers_killed: %d\n", attacker->client->ps.persistant[PERS_KILLED]);
    G_Printf("GunGame: kills: %d\n", attacker->client->pers.kills);
    G_Printf("GunGame: deaths: %d\n", attacker->client->pers.deaths);

    
    // if (!attacker || !attacker->client || !target || !target->client || attacker == target) {
    //     return;
    // }
    
    // If killed with gauntlet, check for win condition
    if (meansOfDeath == MOD_GAUNTLET && attacker->client->ps.weapon == WP_GAUNTLET) {
        // Player won the game with a gauntlet kill
        trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " é o diabo mesmo!\n\"", 
                                     attacker->client->pers.netname));
        
        // End the round
        LogExit("GunGame round won.", qtrue);
        return;
    }

    // Log the current level
    
    // Progress the attacker to the next weapon (which is a worse weapon)
    currentLevel = attacker->client->pers.kills - attacker->client->pers.deaths;
    if (currentLevel < 0) {
        currentLevel = 0;
    }
    G_Printf("GunGame: Player %s is at level %d\n", attacker->client->pers.netname, currentLevel);
    // currentLevel++;
    
    if (currentLevel < GG_NUM_WEAPONS) {
        // Clear all weapons
        for (i = 0; i < WP_NUM_WEAPONS; i++) {
            attacker->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
            attacker->client->ps.ammo[i] = 0;
        }
        
        // Give the next weapon in the progression
        weapon = G_GunGame_GetWeaponForLevel(currentLevel);
        attacker->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);
        
        // Set ammo (except for gauntlet which doesn't need ammo)
        if (weapon != WP_GAUNTLET) {
            attacker->client->ps.ammo[weapon] = 999;
        } else {
            attacker->client->ps.ammo[weapon] = -1;
        }
        
        // Set the current weapon
        attacker->client->ps.weapon = weapon;
        
        // Update the score (which represents the level)
        attacker->client->ps.persistant[PERS_SCORE] = currentLevel;
        
        // Inform the player
        trap_SendServerCommand(attacker->s.number, va("cp \"vc upou pra %s\n\"", 
                                                    BG_FindItemForWeapon(weapon)->pickup_name));
    }

    // progress the target to the previous weapon (which is a better weapon)
    currentLevel = target->client->pers.kills - target->client->pers.deaths;
    if (currentLevel < 0) {
        currentLevel = 0;
    }

    if (currentLevel < GG_NUM_WEAPONS) {
        // Clear all weapons
        for (i = 0; i < WP_NUM_WEAPONS; i++) {
            target->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
            target->client->ps.ammo[i] = 0;
        }

        // Give the next weapon in the progression
        weapon = G_GunGame_GetWeaponForLevel(currentLevel);
        target->client->ps.stats[STAT_WEAPONS] |= (1 << weapon);

        // Set ammo (except for gauntlet which doesn't need ammo)
        if (weapon != WP_GAUNTLET) {
            target->client->ps.ammo[weapon] = 999;
        } else {
            target->client->ps.ammo[weapon] = -1;
        }
        
        // Set the current weapon
        target->client->ps.weapon = weapon;
        
        // Update the score (which represents the level)
        target->client->ps.persistant[PERS_SCORE] = currentLevel;
        
        // Inform the player
        trap_SendServerCommand(target->s.number, va("cp \"vc caiu de level pra %s\n\"", 
                                                    BG_FindItemForWeapon(weapon)->pickup_name));
    }
    G_Printf("GunGame: Player %s is at level %d\n", target->client->pers.netname, currentLevel);

}
    

// Check if a player has won the GunGame round
qboolean G_GunGame_CheckWinner(void) {
    // We don't need to check for winners here, as the win condition
    // is handled in G_GunGame_PlayerKilled when a player gets a kill with the gauntlet
    return qfalse;
} 
