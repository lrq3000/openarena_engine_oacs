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

#include "../libjson/cJSON.h"

// List of features types
typedef enum {
	FEATURE_ID,			// Identifier features
	FEATURE_HUMAN,			// Human-specific features
	FEATURE_GAMESPECIFIC,				// Game-specific features (game rules)
    FEATURE_PHYSICS                     // Physics limitation features (to avoid!!!)
} featureType_t;

// Structure of one feature
typedef struct feature_s
{
    char* key;
	featureType_t type;
    double value;
} feature_t;

// Struct containing the list of all features that will be collected
// Note: in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.
/*
typedef struct interframe_s
{
    feature_t armor;
} interframe_t;
*/

// List all indexes for any feature.
// This is necessary in order to allow for a quick access to a feature (for updating purposes)
typedef enum {
    FEATURE_PLAYERID,
    FEATURE_TIMESTAMP,
    FEATURE_FRAMENUMBER,
    FEATURE_FRAGSINAROW,
	FEATURE_ARMOR,
    FEATURES_COUNT // Important: always place this at the very end! This is used to count the total number of features
} interframeIndex_t;

// Declare the sv.interframe global variable, which will contain the array of all features
extern feature_t interframe[FEATURES_COUNT];

// Cvars
extern cvar_t  *sv_oacsTypesFile;
extern cvar_t  *sv_oacsDataFile;

// Functions
void SV_ExtendedRecordInit(void);
void SV_ExtendedRecordUpdate(void);
void SV_ExtendedRecordWriteStruct(void);
void SV_ExtendedRecordWriteValues(void);
void SV_ExtendedRecordInterframeInit(void);
void SV_ExtendedRecordInterframeUpdate(void);
//feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe);
cJSON* SV_ExtendedRecordFeaturesToJson(feature_t* interframe, qboolean savetypes, qboolean savevalues);

