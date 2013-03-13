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

feature_t interframe[FEATURES_COUNT];

cvar_t  *sv_oacsTypesFile;
cvar_t  *sv_oacsDataFile;


void SV_ExtendedRecordInit(void) {
    // Initialize the features
    //SV_ExtendedRecordInterframeInit();
    
    // Write the types
    //SV_ExtendedRecordWriteStruct();
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
    file = FS_FOpenFileWrite( sv_oacsTypesFile->string );
    
    cJSON *root,*fmt;char *out;	/* declare a few. */

	/* Here we construct some JSON standards, from the JSON site. */
	
	/* Our "Video" datatype: */
	root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, "name", cJSON_CreateString("Jack (\"Bee\") Nimble"));
	cJSON_AddItemToObject(root, "format", fmt=cJSON_CreateObject());
	cJSON_AddStringToObject(fmt,"type",		"rect");
	cJSON_AddNumberToObject(fmt,"width",		1920);
	cJSON_AddNumberToObject(fmt,"height",		1080);
	cJSON_AddFalseToObject (fmt,"interlace");
	cJSON_AddNumberToObject(fmt,"frame rate",	24);
	
	out=cJSON_PrintUnformatted(root);	cJSON_Delete(root); FS_Write(out, strlen(out), file); free(out);
    
    
    
    // Prepare the interframe (convert to an array to allow for walking through it)
    // feature_t* interframe_array;
    // interframe_array = SV_ExtendedRecordInterframeToArray(sv.interframe);

    // Convert the interframe_array into a JSON tree
    // cJSON* root;
    // root = SV_ExtendedRecordFeaturesToJson(interframe_array, qtrue, qfalse);
    
    // Convert the json tree into a text format (unformatted = no line returns)
    
    // char *out;
    // out=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    
    // file = FS_FOpenFileWrite( sv_oacsTypesFile->string );
    // FS_Write(out, strlen(out), file); free(out);

    FS_FCloseFile( file );
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

}

void SV_ExtendedRecordInterframeUpdate(void) {

}

/*
// Will return a feature_t* array from an interframe_t (this is necessary because in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.)
feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe) {

}
*/

// Loop through an array of feature_t and convert to JSON
cJSON* SV_ExtendedRecordFeaturesToJson(feature_t* interframe, qboolean savetypes, qboolean savevalues) {
    cJSON *root;
    root=cJSON_CreateObject();
    return root;
}


