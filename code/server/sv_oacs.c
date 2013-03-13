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

// sv_oacs.c -- Extended data recording facility, to be used as an interface with Open Anti Cheat System

#include "server.h"
//#include "sv_oacs.h"
//#include "../libjson/cJSON.h"

//feature_t interframe[FEATURES_COUNT];

cvar_t  *sv_oacsTypesFile;
cvar_t  *sv_oacsDataFile;


void SV_ExtendedRecordInit(void) {
    // Initialize the features
    SV_ExtendedRecordInterframeInit();
    
    // Write the types
    SV_ExtendedRecordWriteStruct();
}

void SV_ExtendedRecordUpdate(void) {
    // Update the features values
    //SV_ExtendedRecordInterframeUpdate();
    
    //SV_ExtendedRecordWriteValues();
}

// Write the types of the features in a text file, in JSON format
// Note: this function only needs to be called once, preferably at engine startup (Com_Init or SV_Init)
void SV_ExtendedRecordWriteStruct(void) {
    //char* name = "oacs/type.txt";
    fileHandle_t	file;

    Com_Printf("Saving the record struct in file %s", sv_oacsTypesFile->string);
    
    /* Test
    file = FS_FOpenFileWrite( sv_oacsTypesFile->string );
    
    cJSON *root,*fmt;char *out;

	root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, "name", cJSON_CreateString("Jack (\"Bee\") Nimble"));
	cJSON_AddItemToObject(root, "format", fmt=cJSON_CreateObject());
	cJSON_AddStringToObject(fmt,"type",		"rect");
	cJSON_AddNumberToObject(fmt,"width",		1920);
	cJSON_AddNumberToObject(fmt,"height",		1080);
	cJSON_AddFalseToObject (fmt,"interlace");
	cJSON_AddNumberToObject(fmt,"frame rate",	24);
	
	out=cJSON_PrintUnformatted(root);	cJSON_Delete(root); FS_Write(out, strlen(out), file); free(out);
    */
    
    
    
    // Prepare the interframe (convert to an array to allow for walking through it)
    // feature_t* interframe_array;
    // interframe_array = SV_ExtendedRecordInterframeToArray(sv.interframe);

    // Convert the interframe_array into a JSON tree
    cJSON* root;
    root = SV_ExtendedRecordFeaturesToJson(sv.interframe, qtrue, qfalse);
    
    // Convert the json tree into a text format (unformatted = no line returns)
    char *out;
    out=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    
    // Output into the text file
    file = FS_FOpenFileWrite( sv_oacsTypesFile->string ); // open in write mode
    FS_Write(out, strlen(out), file); free(out); // write the text and free it
    FS_FCloseFile( file ); // close the file
}

// Write the values of the current interframe's features
void SV_ExtendedRecordWriteValues(void) {

    // Convert the interframe_array into a JSON tree
    // cJSON* root;
    // root = SV_ExtendedRecordFeaturesToJson(interframe_array, qfalse, qtrue);
    
    // file = FS_FOpenFileAppend( sv_oacsDataFile->string );
}

// Will init the interframe types and values
void SV_ExtendedRecordInterframeInit(void) {
    int i;

    // PlayerID
    sv.interframe[FEATURE_PLAYERID].key = "playerid";
    sv.interframe[FEATURE_PLAYERID].type = FEATURE_ID;
    
    // Timestamp
    sv.interframe[FEATURE_TIMESTAMP].key = "timestamp";
    sv.interframe[FEATURE_TIMESTAMP].type = FEATURE_ID;
    
    // Framenumber
    sv.interframe[FEATURE_FRAMENUMBER].key = "framenumber";
    sv.interframe[FEATURE_FRAMENUMBER].type = FEATURE_ID;
    
    // Frags in a row (accumulator)
    sv.interframe[FEATURE_FRAGSINAROW].key = "fragsinarow";
    sv.interframe[FEATURE_FRAGSINAROW].type = FEATURE_HUMAN;
    
    // Armor
    sv.interframe[FEATURE_ARMOR].key = "armor";
    sv.interframe[FEATURE_ARMOR].type = FEATURE_GAMESPECIFIC;
    
    // Now we will initialize the value for every feature and every client (else we may get a weird random value from an old memory zone that was not cleaned up)
    for (i=0;i<MAX_CLIENTS;i++) {
        sv.interframe[FEATURE_PLAYERID].value[i] = 0;
        sv.interframe[FEATURE_TIMESTAMP].value[i] = 0;
        sv.interframe[FEATURE_FRAMENUMBER].value[i] = 0;
        sv.interframe[FEATURE_FRAGSINAROW].value[i] = 0;
        sv.interframe[FEATURE_ARMOR].value[i] = 0;
    }
}

void SV_ExtendedRecordInterframeUpdate(void) {

}

/*
// Will return a feature_t* array from an interframe_t (this is necessary because in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.)
feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe) {

}
*/

// Loop through an array of feature_t and convert to JSON
cJSON *SV_ExtendedRecordFeaturesToJson(feature_t *interframe, qboolean savetypes, qboolean savevalues) {
    int i;
    cJSON *root, *feature;
    
    //root=cJSON_CreateArray();
    root=cJSON_CreateObject();
	for (i=0;i<FEATURES_COUNT;i++)
	{
        cJSON_AddItemToObject(root, interframe[i].key, feature=cJSON_CreateObject());
		//cJSON_AddItemToArray(root,feature=cJSON_CreateObject());
		cJSON_AddStringToObject(feature, "key", interframe[i].key);
        cJSON_AddNumberToObject(feature, "type", interframe[i].type);
	}
    
    return root;
}


