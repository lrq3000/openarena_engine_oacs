/*
===========================================================================
Copyright (C) 2013 Stephen Larroque <lrq3000@gmail.com>

This file is part of OpenArena.

OpenArena is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenArena is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// sv_oacs.h

//#include "../libjson/cJSON.h"
#include <math.h> // for NAN

#define MAX_STRING_CSV 2048

#ifdef NAN
#define featureDefaultValue NAN
#else
#define featureDefaultValue 0
#endif

// List all indexes for any feature.
// This is necessary in order to allow for a quick access to a feature (for updating purposes)
// You can add here your own features
//
// ACC (accumulators) and INARROW (actually the same thing as an accumulator) features are differential + counter features (meaning that it continues to increment as long as the difference of the feature it's based upon changes value)
typedef enum {
    FEATURE_PLAYERID,
    FEATURE_TIMESTAMP,

    // Human-specific features
    FEATURE_SVSTIME, // persistent/static server time (svs.time)
    FEATURE_REACTIONTIME, // this is svs.time delta (difference between the last interframe and this one)
    FEATURE_SVTIME, // non-persistent server time (sv.time), can be used to check when a new game starts
    FEATURE_LASTCOMMANDTIME, // ps->commandTime
    FEATURE_COMMANDTIME_REACTIONTIME,
    FEATURE_ANGLEINAFRAME, // abs(ps->viewangles[0] - prev_ps->viewangles[0]) + abs(ps->viewangles[1] - prev_ps->viewangles[1]) + abs(ps->viewangles[2] - prev_ps->viewangles[2]) // PITCH=0/YAW=1/ROLL=2 see q_shared.h
    FEATURE_LASTMOUSEEVENTTIME, // change when FEATURE_ANGLEINONEFRAME changes (based on svs.time)
    FEATURE_MOUSEEVENTTIME_REACTIONTIME, // FIXME: maybe add usercmd_t->button type in client_t: svs.clients[id].lastUsercmd.buttons & BUTTON_ATTACK or & BUTTON_ANY ? because for the moment only the angle change accounts for a reaction time. Maybe simply store the previous cmd and when we update, check for a change by comparing every fields of usercmd_t
    FEATURE_MOVEMENTDIR, // ps->movementDir

    // Semi human-specific and semi game-specific. These will be declared as human-specific.
    FEATURE_SCORE, // ps->persistant[PERS_SCORE] (it's a persEnum_t)
    FEATURE_SCOREACC,
    FEATURE_HITS,
    FEATURE_HITSACC,
    FEATURE_DEATH, // ps->persistant[PERS_KILLED]
    FEATURE_DEATHACC,
    FEATURE_CAPTURES,
    FEATURE_CAPTURESACC,
    FEATURE_IMPRESSIVE_COUNT,			// two railgun hits in a row
    FEATURE_IMPRESSIVE_COUNTACC,
    FEATURE_EXCELLENT_COUNT,			// two successive kills in a short amount of time
    FEATURE_EXCELLENT_COUNTACC,
	FEATURE_DEFEND_COUNT,				// defend awards
    FEATURE_DEFEND_COUNTACC,
	FEATURE_ASSIST_COUNT,				// assist awards
    FEATURE_ASSIST_COUNTACC,
	FEATURE_GAUNTLET_FRAG_COUNT,		// kills with the guantlet
    FEATURE_GAUNTLET_FRAG_COUNTACC,

    FEATURE_FRAGS, // number of kills a player did, incremented in regard to when a player gets killed (we can then get the killer's id)
    FEATURE_FRAGSINAROW, // accumulator

    FEATURE_DAMAGEEVENT_COUNT, // damage one receive (not inflicted to the opponent). ps.damageEvent is an incremented counter
    FEATURE_DAMAGEEVENT_COUNTACC,

    FEATURE_DUCKED, // ps->pm_flags & PMF_DUCKED
    FEATURE_MIDAIR, // ps->groundEntityNum == ENTITYNUM_NONE

    FEATURE_WEAPON, // ps->weapon
    FEATURE_WEAPONSTATE, // ps->weaponstate
    FEATURE_WEAPONINSTANTHIT, // is weapon a long-range instant-hit weapon? (which can be delaggued and easier to use by aimbots?)

    FEATURE_POWERUP_QUAD, // ps->powerups[PW_QUAD] (powerup_t enum type), beware, it contains either 0 (does not have) or > 0 which would be the time the powerup is still active or was enabled. This is the same for all powerups.
    FEATURE_POWERUP_BATTLESUIT,
    FEATURE_POWERUP_HASTE,
    FEATURE_POWERUP_INVIS,
    FEATURE_POWERUP_REGEN,
    FEATURE_POWERUP_FLIGHT,
#ifdef MISSIONPACK
    FEATURE_POWERUP_SCOUT,
    FEATURE_POWERUP_GUARD,
    FEATURE_POWERUP_DOUBLER,
    FEATURE_POWERUP_AMMOREGEN,
    FEATURE_POWERUP_INVULNERABILITY,

    FEATURE_PERSISTANT_POWERUP, // ps->stats[STAT_PERSISTANT_POWERUP], missionpack only
#endif

    // For CTF
    FEATURE_HASFLAG, // PW_REDFLAG || PW_BLUEFLAG || PW_NEUTRALFLAG
    FEATURE_HOLYSHIT, // ps->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_HOLYSHIT but you must compare with the previous value because PERS_PLAYEREVENTS is always XORed! // only attacker get holyshit
    FEATURE_RANK, // ps->persistant[PERS_RANK]
    FEATURE_ENEMYHADFLAG, // compute when victim gets hit and has flag, then PERS_ATTACKER and set his FEATURE_ENEMYHASFLAG to 1 // enemy we shot had flag (maybe we killed him or not)
    
    // Game-specific features (dependent on the rules set on the game, which may change! thus you have to relearn these features for each gametype or mod you want to detect cheats on)
    FEATURE_HEALTH, // ps->stats[STAT_HEALTH] from statIndex_t enum
    FEATURE_MAX_HEALTH,
    FEATURE_ARMOR,
    FEATURE_SPEED, // abs(ps->velocity[0]) + abs(ps->velocity[1]) + abs(ps->velocity[2])
    FEATURE_SPEEDRATIO, // ( abs(ps->velocity[0]) + abs(ps->velocity[1]) + abs(ps->velocity[2]) ) / ps->speed; // ps->speed is the maximum speed the client should have
    FEATURE_DAMAGE_COUNT, // total amount damage one has received (not inflicted to the opponent).

    // Do not modify the rest of the features below
    FEATURE_FRAMEREPEAT, // Do not modify: this frame counts the number of times an interframe is repeated. This feature is necessary to keep storage space.
    LABEL_CHEATER, // Not a feature: this is used as the label Y for the features. Usually, this will be set at 0 for everyone, and set to 1 only by using the /cheater command for development purposes or to generate a dataset when you are using a cheating system.
    FEATURES_COUNT // Important: always place this at the very end! This is used to count the total number of features
} interframeIndex_t;

// List of features types
// You should not modify this unless you know what you do (you'll have to code a new input parser inside OACS)
typedef enum {
	FEATURE_ID,			// Identifier features
	FEATURE_HUMAN,			// Human-specific features
	FEATURE_GAMESPECIFIC,				// Game-specific features (game rules)
    FEATURE_PHYSICS,                     // Physics limitation features (to avoid!!!)
    FEATURE_METADATA,                   // Feature containing meta data that are useful for other features or just for post-analysis but are not to be used for the learning nor detection process unless post-processed into higher order features (such as svtime)
    FEATURE_METAINTERFRAME,           // Feature containing meta data about the interframe (like the framerepeat, which should be used as a ponderation factor for all the others features)
    FEATURE_LABEL // Not a feature, this is a label for the data
} featureType_t;

// Structure of one feature
typedef struct feature_s
{
    char *key;
	featureType_t type;
    qboolean modifier; // if true, change the modify flag (means that this feature modifies meaningfully the content of this interframe, and thus write a new separate interframe). Else if false, it will concatenate consequent interframes into only one but increments FEATURE_FRAMEREPEAT (this avoids storing multiple same frames taking up a lot of storage space uselessly).
    double value[MAX_CLIENTS]; // one value for each player (the key and type do not change, they are the same for every player. This saves some memory space.)
} feature_t;

// Struct containing the list of all features that will be collected
// Note: in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.
/*
typedef struct interframe_s
{
    feature_t armor;
} interframe_t;
*/

// Structure of one player entry in the players' table
typedef struct playerstable_s
{
    double playerid;
    char ip[NET_ADDRSTRMAXLEN];
    char *guid;
    double timestamp;
    char datetime[MAX_STRING_CSV];
    char *nickname;
} playerstable_t;

// Declare the sv.interframe global variable, which will contain the array of all features
//extern feature_t interframe[FEATURES_COUNT];

// Cvars to configure OACS behavior
extern cvar_t  *sv_oacsEnable; // enable the extended logging facility?
extern cvar_t  *sv_oacsPlayersTableEnable; // enable the extended player identification logging?
extern cvar_t  *sv_oacsTypesFile; // where to save the features types
extern cvar_t  *sv_oacsDataFile; // where to save the features data
extern cvar_t  *sv_oacsPlayersTable; // where to save the players table (if enabled)
extern cvar_t  *sv_oacsMinPlayers; // minimum number of human players required to begin logging data
extern cvar_t  *sv_oacsLabelPassword; // password necessary for a player to label himself
extern cvar_t  *sv_oacsMaxPing; // max ping to accept interframes (above, the interframe will be dropped until the ping goes down)
extern cvar_t  *sv_oacsMaxLastPacketTime; // max last packet time to accept interframes (above, the interframe will be dropped until the LastPacketTime goes down)

// OACS extended recording variables (necessary for functionning)
feature_t sv_interframe[FEATURES_COUNT];
feature_t sv_previnterframe[FEATURES_COUNT]; // previous interframe
qboolean sv_interframeModified[MAX_CLIENTS]; // was the current interframe modified from the previous one?
playerstable_t sv_playerstable; // extended player identification data (we only need to store one in memory at a time, since we only need it at client connection)
char *sv_playerstable_keys; // key names, edit this (in sv_oacs.c) if you want to add more infos in the playerstable
int sv_oacshumanplayers; // oacs implementation of g_humanplayers (but we also count privateclients too!)
extern char *sv_interframe_keys[]; // names of the features, array of string keys to output in the typesfile and datafile
extern int sv_interframe_types[]; // types of the features, will be outputted in the typesfile
extern qboolean sv_interframe_modifiers[]; // modifiers for the features, array of boolean that specifies if a feature should commit the interframe on change or not
playerState_t	prev_ps[MAX_CLIENTS]; // previous frame's playerstate

// Functions
void SV_ExtendedRecordInit(void);
void SV_ExtendedRecordUpdate(void);
void SV_ExtendedRecordShutdown(void);
void SV_ExtendedRecordClientConnect(int client);
void SV_ExtendedRecordDropClient(int client);
void SV_ExtendedRecordWriteStruct(void);
void SV_ExtendedRecordWriteValues(int client);
void SV_ExtendedRecordWritePlayersTable(int client);
void SV_ExtendedRecordInterframeInit(int client);
void SV_ExtendedRecordInterframeInitValues(int client);
double SV_ExtendedRecordInterframeInitValue(int client, int feature);
void SV_ExtendedRecordInterframeUpdate(int client);
void SV_ExtendedRecordInterframeUpdateValues(int client);
void SV_ExtendedRecordInterframeUpdateValuesAttacker(int client);
void SV_ExtendedRecordPlayersTableInit(int client);
//feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe);
//cJSON *SV_ExtendedRecordFeaturesToJson(feature_t *interframe, qboolean savetypes, qboolean savevalues, int client);
//void SV_ExtendedRecordWriteStructJson(void);
//void SV_ExtendedRecordWriteValuesJson(int client);
char *SV_ExtendedRecordFeaturesToCSV(char *csv_string, int max_string_size, feature_t *interframe, int savewhat, int client);
char *SV_ExtendedRecordPlayersTableToCSV(char *csv_string, int max_string_size, playerstable_t playerstable);
void SV_ExtendedRecordSetFeatureValue(interframeIndex_t feature, double value, int client);
//char* strncat_lin( char* dest, char* src, size_t n );
qboolean FS_IsFileEmpty(char* filename);
qboolean SV_IsBot(int client);
qboolean SV_IsSpectator(int client);
qboolean SV_IsWeaponInstantHit(int weapon);
int SV_CountPlayers(void);
int rand_range(int min, int max);
void SV_ExtendedRecordSetCheater( int client, int label );
