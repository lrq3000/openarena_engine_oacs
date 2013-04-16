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
typedef enum {
    FEATURE_PLAYERID,
    FEATURE_TIMESTAMP,
    FEATURE_SVTIME,
    FEATURE_REACTIONTIME, // this is sv_time delta (difference between the last interframe and this one)
    FEATURE_FRAGSINAROW,
	FEATURE_ARMOR,

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
    FEATURE_METADATA,           // Feature containing meta data about other features (like the framerepeat, which should be used as a ponderation factor for all the others features)
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
typedef struct playertable_s
{
    double playerid;
    char *ip;
    char *guid;
    double timestamp;
    char datetime[MAX_STRING_CSV];
    char *nickname;
} playertable_t;

// Declare the sv.interframe global variable, which will contain the array of all features
//extern feature_t interframe[FEATURES_COUNT];

// Cvars to configure OACS behavior
extern cvar_t  *sv_oacsEnable;
extern cvar_t  *sv_oacsPlayersTableEnable;
extern cvar_t  *sv_oacsTypesFile;
extern cvar_t  *sv_oacsDataFile;
extern cvar_t  *sv_oacsPlayersTable;

// OACS extended recording variables (necessary for functionning)
feature_t sv_interframe[FEATURES_COUNT];
feature_t sv_previnterframe[FEATURES_COUNT]; // previous interframe
qboolean sv_interframeModified[MAX_CLIENTS]; // was the current interframe modified from the previous one?
playertable_t playertable; // extended player identification data (we only need to store one in memory at a time, since we only need it at client connection)
char *playerstable_keys; // key names, edit this (in sv_oacs.c) if you want to add more infos in the playerstable

// Functions
void SV_ExtendedRecordInit(void);
void SV_ExtendedRecordUpdate(void);
void SV_ExtendedRecordShutdown(void);
void SV_ExtendedRecordClientConnect(int client);
void SV_ExtendedRecordDropClient(int client);
void SV_ExtendedRecordWriteStruct(void);
void SV_ExtendedRecordWriteValues(int client);
void SV_ExtendedRecordWritePlayerTable(int client);
void SV_ExtendedRecordInterframeInit(int client);
void SV_ExtendedRecordInterframeInitValues(int client);
void SV_ExtendedRecordInterframeUpdate(int client);
void SV_ExtendedRecordPlayerTableInit(int client);
//feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe);
//cJSON *SV_ExtendedRecordFeaturesToJson(feature_t *interframe, qboolean savetypes, qboolean savevalues, int client);
//void SV_ExtendedRecordWriteStructJson(void);
//void SV_ExtendedRecordWriteValuesJson(int client);
char *SV_ExtendedRecordFeaturesToCSV(char *csv_string, int max_string_size, feature_t *interframe, int savewhat, int client);
char *SV_ExtendedRecordPlayerTableToCSV(char *csv_string, int max_string_size, playertable_t playertable);
void SV_ExtendedRecordSetFeatureValue(interframeIndex_t feature, double value, int client);
//char* strncat_lin( char* dest, char* src, size_t n );
qboolean FS_IsFileEmpty(char* filename);
int rand_range(int min, int max);
