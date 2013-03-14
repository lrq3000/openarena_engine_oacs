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
#include <time.h> // for random
#include <stdlib.h> // for random

//feature_t interframe[FEATURES_COUNT];

cvar_t  *sv_oacsTypesFile;
cvar_t  *sv_oacsDataFile;
cvar_t  *sv_oacsEnable;


void SV_ExtendedRecordInit(void) {
    // Initialize the features
    SV_ExtendedRecordInterframeInit(-1);
    
    // Write down the types in the typesfile
    SV_ExtendedRecordWriteStruct();
}

void SV_ExtendedRecordUpdate(void) {
    // Update the features values
    SV_ExtendedRecordInterframeUpdate(-1);
    
    // Write down the values in the datafile
    SV_ExtendedRecordWriteValues();
}

// Write the types of the features in a text file, in JSON format
// Note: this function only needs to be called once, preferably at engine startup (Com_Init or SV_Init)
void SV_ExtendedRecordWriteStruct(void) {
    //char* name = "oacs/type.txt";
    fileHandle_t	file;

    Com_DPrintf("Saving the oacs types in file %s\n", sv_oacsTypesFile->string);
    
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
    // interframe_array = SV_ExtendedRecordInterframeToArray(sv_interframe);

    // Convert the interframe array into a JSON tree
    cJSON *root;
    root = SV_ExtendedRecordFeaturesToJson(sv_interframe, qtrue, qfalse, -1);
    
    // Convert the json tree into a text format (unformatted = no line returns)
    char *out;
    out=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    
    // Output into the text file
    file = FS_FOpenFileWrite( sv_oacsTypesFile->string ); // open in write mode
    FS_Write(out, strlen(out), file); free(out); // write the text and free it
    FS_Flush( file ); // update the content of the file
    FS_FCloseFile( file ); // close the file
}

// Write the values of the current interframe's features
void SV_ExtendedRecordWriteValues(void) {
    fileHandle_t	file;
    cJSON *root;
    char *out;
    int i;

    for (i = 0; i < sv_maxclients->integer; i++)
	{
		if (svs.clients[i].state < CS_ACTIVE)
			continue;
		
        // Convert the interframe array into a JSON tree
        root = SV_ExtendedRecordFeaturesToJson(sv_interframe, qfalse, qtrue, i);

        // Convert the json tree into a text format (unformatted = no line returns)
        out=cJSON_PrintUnformatted(root); cJSON_Delete(root);

        // Output into the text file (only if there's at least one client connected!)
        Com_DPrintf("Saving the oacs values in file %s\n", sv_oacsDataFile->string);
        file = FS_FOpenFileAppend( sv_oacsDataFile->string ); // open in append mode
        FS_Write(va("%s\n", out), strlen(out)+strlen("\n"), file); free(out); // write the text (with a line return) and free it
        FS_Flush( file ); // update the content of the file
        FS_FCloseFile( file ); // close the file
	}
}

// Will init the interframe types and values
// Note: this is also used to reset the values for one specific client at disconnection
void SV_ExtendedRecordInterframeInit(int client) {
    int i, startclient, endclient;

    if (client < 0) {
        // PlayerID
        sv_interframe[FEATURE_PLAYERID].key = "playerid";
        sv_interframe[FEATURE_PLAYERID].type = FEATURE_ID;
        
        // Timestamp
        sv_interframe[FEATURE_TIMESTAMP].key = "timestamp";
        sv_interframe[FEATURE_TIMESTAMP].type = FEATURE_ID;
        
        // Framenumber
        sv_interframe[FEATURE_FRAMENUMBER].key = "framenumber";
        sv_interframe[FEATURE_FRAMENUMBER].type = FEATURE_ID;
        
        // Frags in a row (accumulator)
        sv_interframe[FEATURE_FRAGSINAROW].key = "fragsinarow";
        sv_interframe[FEATURE_FRAGSINAROW].type = FEATURE_HUMAN;
        
        // Armor
        sv_interframe[FEATURE_ARMOR].key = "armor";
        sv_interframe[FEATURE_ARMOR].type = FEATURE_GAMESPECIFIC;
        
        // Label
        sv_interframe[LABEL_CHEATER].key = "cheater";
        sv_interframe[LABEL_CHEATER].type = LABEL_CHEATER;
    }
    
    // If a client id is supplied, we will only reset values for this client
    if (client >= 0) {
        startclient = client;
        endclient = client + 1;
    // Else we reset for every client
    } else {
        startclient = 0;
        endclient = MAX_CLIENTS;
    }

    // Now we will initialize the value for every feature and every client or just one client if a clientid is supplied (else we may get a weird random value from an old memory zone that was not cleaned up)
    for (i=startclient;i<endclient;i++) {
        sv_interframe[FEATURE_PLAYERID].value[i] = 0;
        sv_interframe[FEATURE_TIMESTAMP].value[i] = 0;
        sv_interframe[FEATURE_FRAMENUMBER].value[i] = 0;
        sv_interframe[FEATURE_FRAGSINAROW].value[i] = 0;
        sv_interframe[FEATURE_ARMOR].value[i] = 0;
        sv_interframe[LABEL_CHEATER].value[i] = 0;
    }
}

void SV_ExtendedRecordInterframeUpdate(int client) {
    int i, startclient, endclient;

    // If a client id is supplied, we will only reset values for this client
    if (client >= 0) {
        startclient = client;
        endclient = client + 1;
    // Else we reset for every client
    } else {
        startclient = 0;
        endclient = sv_maxclients->integer;
    }
    
    // TEST: initializing random values
    srand(time(NULL));

    // Now we will initialize the value for every feature and every client or just one client if a clientid is supplied (else we may get a weird random value from an old memory zone that was not cleaned up)
    for (i=startclient;i<endclient;i++) {
        if (svs.clients[i].state < CS_ACTIVE)
			continue;

        sv_interframe[FEATURE_PLAYERID].value[i] = rand() % 100;
        sv_interframe[FEATURE_TIMESTAMP].value[i] = rand() % 100;
        sv_interframe[FEATURE_FRAMENUMBER].value[i] = rand() % 100;
        sv_interframe[FEATURE_FRAGSINAROW].value[i] = rand() % 100;
        sv_interframe[FEATURE_ARMOR].value[i] = rand() % 100;
        sv_interframe[LABEL_CHEATER].value[i] = rand() % 2;
    }

}

/*
// Will return a feature_t* array from an interframe_t (this is necessary because in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.)
feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe) {

}
*/

// Loop through an array of feature_t and convert to JSON
cJSON *SV_ExtendedRecordFeaturesToJson(feature_t *interframe, qboolean savetypes, qboolean savevalues, int client) {
    int i;
    cJSON *root, *feature;
    
    //root=cJSON_CreateArray();
    root=cJSON_CreateObject();
	for (i=0;i<FEATURES_COUNT;i++)
	{
        cJSON_AddItemToObject(root, interframe[i].key, feature=cJSON_CreateObject());
		//cJSON_AddItemToArray(root,feature=cJSON_CreateObject());
		cJSON_AddStringToObject(feature, "key", interframe[i].key);
        if (savetypes == qtrue)
            cJSON_AddNumberToObject(feature, "type", interframe[i].type);
        if (savevalues == qtrue && client >= 0)
            cJSON_AddNumberToObject(feature, "value", interframe[i].value[client]);
	}
    
    return root;
}


