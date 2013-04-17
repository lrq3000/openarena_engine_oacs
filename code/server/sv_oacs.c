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

/* How to add a new feature:
- Edit sv_oacs.h and add your new feature in the interframeIndex_t enum,
- Edit the SV_ExtendedRecordInterframeInit() to add the initialization settings (key, type, modifier),
- Edit the SV_ExtendedRecordInterframeInitValues() function OR add your code anywhere else inside the ioquake3 code to update the feature's value on every frame, but then use the function SV_ExtendedRecordSetFeatureValue() to update the value of a feature!
Note: ALWAYS use SV_ExtendedRecordSetFeatureValue() to change the value of a feature (_don't_ do it directly!), because this function manage if the interframe needs to be written and flushed into a file on change or not (if a feature is non-modifier).

*/

#include "server.h"
//#include "sv_oacs.h"
//#include "../libjson/cJSON.h"
#include <time.h> // for timestamp and to init random seed
#include <stdlib.h> // for random

//feature_t interframe[FEATURES_COUNT];

// Cvars to configure OACS behavior
cvar_t  *sv_oacsEnable;
cvar_t  *sv_oacsPlayersTableEnable;
cvar_t  *sv_oacsTypesFile;
cvar_t  *sv_oacsDataFile;
cvar_t  *sv_oacsPlayersTable;
cvar_t  *sv_oacsMinPlayers;

char *sv_playerstable_keys = "playerid,playerip,playerguid,timestamp,datetime,playername"; // key names, edit this if you want to add more infos in the playerstable

// Initialize the interframe structure at the start of the server
// Called only once, at the launching of the server
void SV_ExtendedRecordInit(void) {
    //Initializing the random seed for the random values generator
    srand(time(NULL));

    // Initialize the features
    SV_ExtendedRecordInterframeInit(-1);
    
    // Write down the types in the typesfile
    SV_ExtendedRecordWriteStruct();
}

// Update interframe values for all connected clients
// Called for each server frame
void SV_ExtendedRecordUpdate(void) {
    // Update the features values
    SV_ExtendedRecordInterframeUpdate(-1);
    
    // Write down the values in the datafile
    //SV_ExtendedRecordWriteValues(-1); // this will be automatically called when an interframe needs to be committed
}

// Write down the last interframe values at shutdown
// Called once at engine shutdown
void SV_ExtendedRecordShutdown(void) {
    // Write down all the not yet committed values into the datafile
    SV_ExtendedRecordWriteValues(-1);
}

// When a client connects to the server
void SV_ExtendedRecordClientConnect(int client) {
    // Proceed only if the player is not a bot
    if ( !SV_IsBot(client) ) {
        // Recompute the number of connected human players
        sv_oacshumanplayers = SV_CountPlayers();
    
        // Init the values for this client (like playerid, etc.)
        SV_ExtendedRecordInterframeInitValues(client);

        // If the admin is willing to save extended identification informations
        if (sv_oacsPlayersTableEnable->integer == 1) {
            // Init the playerstable entry of this client (extended player identification informations)
            SV_ExtendedRecordPlayersTableInit(client);
            
            // Write down the entry into the players table file
            SV_ExtendedRecordWritePlayersTable(client);
        }
    }
}

// When a client gets disconnected (either willingly or unwillingly)
void SV_ExtendedRecordDropClient(int client) {
    // Drop the client only if he isn't already dropped (so that the reset won't be called multiple times while the client goes into CS_ZOMBIE)
    if ( !( (sv_interframe[FEATURE_PLAYERID].value[client] == featureDefaultValue) | isnan(sv_interframe[FEATURE_PLAYERID].value[client]) | SV_IsBot(client) ) ) {
        // Recompute the number of connected human players
        sv_oacshumanplayers = SV_CountPlayers();
    
        // Commit the last interframe for that client before he gets totally disconnected
        SV_ExtendedRecordWriteValues(client);
        // Reinit the values for that client
        SV_ExtendedRecordInterframeInit(client);
    }
}

// Write the types of the features in a text file, in CSV format into a file
// Note: this function only needs to be called once, preferably at engine startup (Com_Init or SV_Init)
void SV_ExtendedRecordWriteStruct(void) {
    fileHandle_t	file;
    char outheader[MAX_STRING_CSV];
    char out[MAX_STRING_CSV];

    Com_DPrintf("OACS: Saving the oacs types in file %s\n", sv_oacsTypesFile->string);
    
    SV_ExtendedRecordFeaturesToCSV(outheader, MAX_STRING_CSV, sv_interframe, 0, -1);
    SV_ExtendedRecordFeaturesToCSV(out, MAX_STRING_CSV, sv_interframe, 1, -1);
    
    // Output into the text file
    file = FS_FOpenFileWrite( sv_oacsTypesFile->string ); // open in write mode
    FS_Write(va("%s\n%s", outheader, out), strlen(outheader)+strlen("\n")+strlen(out), file); //free(out); // write the text and free it
    FS_Flush( file ); // update the content of the file
    FS_FCloseFile( file ); // close the file
}

// Write the values of the current interframe's features in CSV format into a file
void SV_ExtendedRecordWriteValues(int client) {
    fileHandle_t	file;
    char out[MAX_STRING_CSV];
    int i, startclient, endclient;
    
    if ( sv_oacshumanplayers < sv_oacsMinPlayers->integer ) // if we are below the minimum number of human players, we just break here
        return;

    // If a client id is supplied, we will only write the JSON interframe for this client
    if (client >= 0) {
        startclient = client;
        endclient = client + 1;
    // Else for every client
    } else {
        startclient = 0;
        endclient = sv_maxclients->integer;
    }

    // Open the data file
    file = FS_FOpenFileAppend( sv_oacsDataFile->string ); // open in append mode

    // If there is no data file or it is empty, we first output the headers (features keys/names)
    if ( FS_FileExists( sv_oacsDataFile->string ) == qfalse || (FS_IsFileEmpty( sv_oacsDataFile->string ) == qtrue) ) {
        char outheader[MAX_STRING_CSV];
        
        // Get the CSV string from the features keys
        SV_ExtendedRecordFeaturesToCSV(outheader, MAX_STRING_CSV, sv_interframe, 0, -1);

        // Output the headers into the text file (only if there's at least one client connected!)
        FS_Write(va("%s\n", outheader), strlen(outheader)+strlen("\n"), file); // write the text (with a line return)
        FS_Flush( file ); // update the content of the file
    }

    for (i=startclient;i<endclient;i++) {
		if ( (svs.clients[i].state < CS_ACTIVE) )
			continue;
        else if ( SV_IsBot(i) | SV_IsSpectator(i) ) // avoid saving empty interframes of not yet fully connected players, bots nor spectators
            continue;

        SV_ExtendedRecordFeaturesToCSV(out, MAX_STRING_CSV, sv_interframe, 2, i);

        // Output into the text file (only if there's at least one client connected!)
        Com_DPrintf("OACS: Saving the oacs values for client %i in file %s\n", client, sv_oacsDataFile->string);
        FS_Write(va("%s\n", out), strlen(out)+strlen("\n"), file); //free(out); // write the text (with a line return) and free it
        FS_Flush( file ); // update the content of the file
	}

    // Close the data file and free variables
    FS_FCloseFile( file ); // close the file
    // No variable to free, there's no malloc'd variable
}

// Write the values of the current players table entry (extended identification informations for one player) in CSV format into a file
void SV_ExtendedRecordWritePlayersTable(int client) {
    fileHandle_t	file;
    char out[MAX_STRING_CSV];

    if ( SV_IsBot(client) ) // avoid saving empty players table entry of not yet fully connected players and bots
        return;

    // Open the data file
    Com_DPrintf("OACS: Saving the oacs players table entry for client %i in file %s\n", client, sv_oacsPlayersTable->string);
    file = FS_FOpenFileAppend( sv_oacsPlayersTable->string ); // open in append mode

    // If there is no data file or it is empty, we first output the headers (features keys/names)
    if ( FS_FileExists( sv_oacsPlayersTable->string ) == qfalse || (FS_IsFileEmpty( sv_oacsPlayersTable->string ) == qtrue) ) {
        // Output the headers into the text file (only if there's at least one client connected!)
        FS_Write(va("%s\n", sv_playerstable_keys), strlen(sv_playerstable_keys)+strlen("\n"), file); // write the text (with a line return)
        FS_Flush( file ); // update the content of the file
    }

    SV_ExtendedRecordPlayersTableToCSV(out, MAX_STRING_CSV, sv_playerstable);

    // Output into the text file (only if there's at least one client connected!)
    FS_Write(va("%s\n", out), strlen(out)+strlen("\n"), file); //free(out); // write the text (with a line return) and free it
    FS_Flush( file ); // update the content of the file

    // Close the data file and free variables
    FS_FCloseFile( file ); // close the file
    // No variable to free, there's no malloc'd variable
}

/*
// Write the types of the features in a text file, in JSON format
// Note: this function only needs to be called once, preferably at engine startup (Com_Init or SV_Init)
void SV_ExtendedRecordWriteStructJson(void) {
    //char* name = "oacs/type.txt";
    fileHandle_t	file;

    Com_DPrintf("OACS: Saving the oacs types in file %s\n", sv_oacsTypesFile->string);

    // Test
    //file = FS_FOpenFileWrite( sv_oacsTypesFile->string );
     //cJSON *root,*fmt;char *out;
	//root=cJSON_CreateObject();	
	//cJSON_AddItemToObject(root, "name", cJSON_CreateString("Jack (\"Bee\") Nimble"));
	//cJSON_AddItemToObject(root, "format", fmt=cJSON_CreateObject());
	//cJSON_AddStringToObject(fmt,"type",		"rect");
	//cJSON_AddNumberToObject(fmt,"width",		1920);
	//cJSON_AddNumberToObject(fmt,"height",		1080);
	//cJSON_AddFalseToObject (fmt,"interlace");
	//cJSON_AddNumberToObject(fmt,"frame rate",	24);
	//out=cJSON_PrintUnformatted(root);	cJSON_Delete(root); FS_Write(out, strlen(out), file); free(out);
    // End Test



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

// Write the values of the current interframe's features in JSON
void SV_ExtendedRecordWriteValuesJson(int client) {
    fileHandle_t	file;
    cJSON *root;
    char *out;
    int i, startclient, endclient;

    // If a client id is supplied, we will only write the JSON interframe for this client
    if (client >= 0) {
        startclient = client;
        endclient = client + 1;
    // Else for every client
    } else {
        startclient = 0;
        endclient = sv_maxclients->integer;
    }

    for (i=startclient;i<endclient;i++) {
		if (svs.clients[i].state < CS_ACTIVE)
			continue;
		
        // Convert the interframe array into a JSON tree
        root = SV_ExtendedRecordFeaturesToJson(sv_interframe, qfalse, qtrue, i);

        // Convert the json tree into a text format (unformatted = no line returns)
        out=cJSON_PrintUnformatted(root); cJSON_Delete(root);

        // Output into the text file (only if there's at least one client connected!)
        Com_DPrintf("OACS: Saving the oacs values in file %s\n", sv_oacsDataFile->string);
        file = FS_FOpenFileAppend( sv_oacsDataFile->string ); // open in append mode
        FS_Write(va("%s\n", out), strlen(out)+strlen("\n"), file); free(out); // write the text (with a line return) and free it
        FS_Flush( file ); // update the content of the file
        FS_FCloseFile( file ); // close the file
	}
}
*/

// Will init the interframe types and values
// Note: this is also used to reset the values for one specific client at disconnection
// Add here the initialization settings for your own features
void SV_ExtendedRecordInterframeInit(int client) {
    int i, j, startclient, endclient;
    
    Com_Printf("OACS: Initializing the features for client %i\n", client);

    if (client < 0) {
        // PlayerID
        sv_interframe[FEATURE_PLAYERID].key = "playerid";
        sv_interframe[FEATURE_PLAYERID].type = FEATURE_ID;
        sv_interframe[FEATURE_PLAYERID].modifier = qtrue;
        
        // Timestamp
        sv_interframe[FEATURE_TIMESTAMP].key = "timestamp";
        sv_interframe[FEATURE_TIMESTAMP].type = FEATURE_ID;
        sv_interframe[FEATURE_TIMESTAMP].modifier = qfalse;
        
        // SVTime (server time, serves as frame number)
        sv_interframe[FEATURE_SVTIME].key = "svtime";
        sv_interframe[FEATURE_SVTIME].type = FEATURE_ID;
        sv_interframe[FEATURE_SVTIME].modifier = qfalse;
        
        // Reaction time (sv_time delta, used to compute the time elapsed between the last action and the current action)
        sv_interframe[FEATURE_REACTIONTIME].key = "reactiontime";
        sv_interframe[FEATURE_REACTIONTIME].type = FEATURE_HUMAN;
        sv_interframe[FEATURE_REACTIONTIME].modifier = qfalse;
        
        // Frags in a row (accumulator)
        sv_interframe[FEATURE_FRAGSINAROW].key = "fragsinarow";
        sv_interframe[FEATURE_FRAGSINAROW].type = FEATURE_HUMAN;
        sv_interframe[FEATURE_FRAGSINAROW].modifier = qtrue;
        
        // Armor
        sv_interframe[FEATURE_ARMOR].key = "armor";
        sv_interframe[FEATURE_ARMOR].type = FEATURE_GAMESPECIFIC;
        sv_interframe[FEATURE_ARMOR].modifier = qtrue;
        
        // Frame Repeat
        sv_interframe[FEATURE_FRAMEREPEAT].key = "framerepeat";
        sv_interframe[FEATURE_FRAMEREPEAT].type = FEATURE_METADATA;
        sv_interframe[FEATURE_FRAMEREPEAT].modifier = qfalse;
        
        // Label
        sv_interframe[LABEL_CHEATER].key = "cheater";
        sv_interframe[LABEL_CHEATER].type = FEATURE_LABEL;
        sv_interframe[LABEL_CHEATER].modifier = qtrue;
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
        // Generic features reset, setting default value (you can set a specific default value for a specific feature in the function SV_ExtendedRecordInterframeInitValues() )
        for (j=0;j<FEATURES_COUNT;j++) {
            sv_interframe[j].value[i] = featureDefaultValue; // set the default value for features (preferable NAN - Not A Number)
        }
        
        // Hack to avoid the first interframe (which is null) from being committed (we don't want the first interframe to be saved, since it contains only null value - the update func will take care of fetching correct values)
        sv_interframeModified[i] = qtrue;
    }
}

// Set the initial values for some features
// set here the default values you want for a feature if you want it to be different than 0 or NaN
// Note: do not use SV_ExtendedRecordSetFeatureValue() here, just access directly sv_interframe
void SV_ExtendedRecordInterframeInitValues(int client) {
    // Proceed only if the client is not a bot
    if ( !SV_IsBot(client) ) {
        // Set unique player id (we want this id to be completely generated serverside and without any means to tamper it clientside) - we don't care that the id change for the same player when he reconnects, since anyway the id will always link to the player's ip and guid using the playerstable
        //char tmp[MAX_STRING_CHARS] = ""; snprintf(tmp, MAX_STRING_CHARS, "%i%lu", rand_range(1, 99999), (unsigned long int)time(NULL));
        sv_interframe[FEATURE_PLAYERID].value[client] = atof( va("%i%lu", rand_range(1, 99999), (unsigned long int)time(NULL)) ); // TODO: use a real UUID/GUID here (for the moment we simply use the timestamp in seconds + a random number, this should be enough for now to ensure the uniqueness of all the players) - do NOT use ioquake3 GUID since it can be spoofed (there's no centralized authorization system!)
        // Server time
        sv_interframe[FEATURE_SVTIME].value[client] = sv.time;
        // FrameRepeat: number of times a frame was repeated (1 = one frame, it was not repeated)
        sv_interframe[FEATURE_FRAMEREPEAT].value[client] = 1;
    }
}

// Update features for each server frame
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

    // Now we will initialize the value for every feature and every client or just one client if a clientid is supplied (else we may get a weird random value from an old memory zone that was not cleaned up)
    for (i=startclient;i<endclient;i++) {
        // If the client was disconnected, and not already reinitialized, we commit the last interframe and reset
        if ( (svs.clients[i].state == CS_ZOMBIE) && !( (sv_interframe[FEATURE_PLAYERID].value[i] == featureDefaultValue) | isnan(sv_interframe[FEATURE_PLAYERID].value[i]) ) ) {
            SV_ExtendedRecordDropClient(i);
        // Else if the client is not already fully connected in the game, we just skip this client
        } else if ( svs.clients[i].state < CS_ACTIVE ) {
			continue;
        // Avoid updating bots
        } else if ( SV_IsBot(i) ) {
            continue;
        } // Note: we do update spectators and players even when below the number of required human players, but we just don't save them (see SV_ExtendedRecordWriteValues()). This allows to always have fresh values (eg: reactiontime won't get too huge).

        // Updating values: you can add here your code to update a feature at the end of every frame and for every player
        //SV_ExtendedRecordSetFeatureValue(FEATURE_PLAYERID, i, i);
        SV_ExtendedRecordSetFeatureValue(FEATURE_TIMESTAMP, time(NULL), i);
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, 0, i);
        SV_ExtendedRecordSetFeatureValue(FEATURE_ARMOR, 0, i);
        SV_ExtendedRecordSetFeatureValue(LABEL_CHEATER, rand_range(0,1), i);
        
        // Update delta features (values that need to be computed only in difference with the previous interframe when there's a change)
        // Note: you should only update those features at the end, because you WANT to make sure that any change to any modifier feature already happened, so that we know for sure that the interframe will be committed or not.

        if (sv_interframeModified[i] == qtrue) {
            // Update reaction time (sv.time delta) only if we have committed the last frame, because we want the difference (the delta) between the last move and the new one
            SV_ExtendedRecordSetFeatureValue(FEATURE_REACTIONTIME, sv.time - sv_interframe[FEATURE_SVTIME].value[i], i);
        }
        
        // We have to update SVTIME only after reaction time, and we also update it if it doesn't have any value (because we don't want that the first interframe of a player is set to the start of the server/map!)
        if (sv_interframeModified[i] == qtrue || sv_interframe[FEATURE_SVTIME].value[i] == 0)
            SV_ExtendedRecordSetFeatureValue(FEATURE_SVTIME, sv.time, i);
            
        
        // Check if the interframe is repeated or if it has changed since the previous one
        if (sv_interframeModified[i] == qtrue) { // it changed, and the previous one was already committed, so we just have to reset FRAMEREPEAT and the modified flag
            SV_ExtendedRecordSetFeatureValue(FEATURE_FRAMEREPEAT, 1, i);
            sv_interframeModified[i] = qfalse;
        } else { // Else the interframe is repeated, not changed, thus we increment the FRAMEREPEAT
            sv_interframe[FEATURE_FRAMEREPEAT].value[i]++;
        }

        /*
        sv_interframe[FEATURE_PLAYERID].value[i] = rand() % 100;
        sv_interframe[FEATURE_TIMESTAMP].value[i] = rand() % 100;
        sv_interframe[FEATURE_FRAMENUMBER].value[i] = rand() % 100;
        sv_interframe[FEATURE_FRAGSINAROW].value[i] = rand() % 100;
        sv_interframe[FEATURE_ARMOR].value[i] = rand() % 100;
        sv_interframe[FEATURE_FRAMEREPEAT].value[i]++;
        sv_interframe[LABEL_CHEATER].value[i] = rand() % 2;
        */
    }

}

// Setup the values for the players table entry of one client (these are extended identification informations, useful at prediction to do a post action like kick or ban, or just to report with the proper name and ip)
// Edit here if you want to add more identification informations
void SV_ExtendedRecordPlayersTableInit(int client) {
    client_t	*cl;

    // Get the client object
    cl = &svs.clients[client];

    // Set playerid (the random server-side uuid we set for each player, it should be computed prior calling this function, when initializing the sv_interframe)
    sv_playerstable.playerid = sv_interframe[FEATURE_PLAYERID].value[client];

    // Set IP
    //sv_playerstable.ip = Info_ValueForKey( cl->userinfo, "ip" ); // alternative way to get the ip, from the userinfo string
    Q_strncpyz(sv_playerstable.ip, NET_AdrToString(cl->netchan.remoteAddress), MAX_STRING_CHARS); // reliable way to get the client's ip adress
    //snprintf(sv_playerstable.ip, MAX_STRING_CHARS, "%i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3]);

    // Set GUID
    sv_playerstable.guid = Info_ValueForKey( cl->userinfo, "cl_guid" );

    // Set timestamp
    sv_playerstable.timestamp = time(NULL);

    // Human readable date time (only for human operators when reviewing the playerstable, else it's totally useless for the cheat detection)
    // Note: this is UTC time
	time_t  utcnow = time(NULL);
	struct tm tnow = *gmtime(&utcnow);
	strftime( sv_playerstable.datetime, MAX_STRING_CSV, "%Y-%m-%d %H:%M:%S", &tnow );

    // Set nickname
    sv_playerstable.nickname = cl->name;
}

/*
// Will return a feature_t* array from an interframe_t (this is necessary because in C there's no safe way to walk/iterate through a struct, and a struct for interframe is necessary to directly access a feature. An alternative could be a hash dictionary.)
feature_t* SV_ExtendedRecordInterframeToArray(interframe_t interframe) {

}
*/

/*
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
*/

// Loop through an array of feature_t and convert to a CSV row
char *SV_ExtendedRecordFeaturesToCSV(char *csv_string, int max_string_size, feature_t *interframe, int savewhat, int client) {
    int i;
    int length = 0; // willl store the current cursor position at the end of the string, so that we can quickly append without doing a strlen() everytime which would be O(n^2) instead of O(n)

    //root=cJSON_CreateObject();
	for (i=0;i<FEATURES_COUNT;i++)
	{
        // Add CSV separator between fields
        if (i > 0) {
            length += snprintf(csv_string+length, max_string_size, "%s", ",");
        }
        
        // Save the fields
        if (savewhat == 0) { // type 0: save the key (name strings)
            length += snprintf(csv_string+length, max_string_size, "%s", interframe[i].key);
        } else if (savewhat == 1) { // type 1: save the type (int coming from an enum)
            //s = strncat_lin( s, va("%i", interframe[i].type), (sizeof(csv_string) - sizeof(s)) );
            length += snprintf(csv_string+length, max_string_size, "%i", interframe[i].type);
        } else { // type 2: save the values (the data type depends on the content of the field)
            // If possible, print the value as a long int, else as a float
            if (round(interframe[i].value[client]) == interframe[i].value[client]) {
                length += snprintf(csv_string+length, max_string_size, "%.0f", interframe[i].value[client]);
            } else {
                length += snprintf(csv_string+length, max_string_size, "%f", interframe[i].value[client]);
            }
        }

	}
    
    return csv_string;
}

// Convert a playerstable_t entry into a CSV string
char *SV_ExtendedRecordPlayersTableToCSV(char *csv_string, int max_string_size, playerstable_t playerstable) {
    int length = 0; // willl store the current cursor position at the end of the string, so that we can quickly append without doing a strlen() everytime which would be O(n^2) instead of O(n)

    // append playerid
    length += snprintf(csv_string+length, max_string_size, "%.0f", playerstable.playerid);

    // append ip
    length += snprintf(csv_string+length, max_string_size, "%s", ","); // append a comma
    length += snprintf(csv_string+length, max_string_size, "\"%s\"", playerstable.ip);

    // append guid
    length += snprintf(csv_string+length, max_string_size, "%s", ",");
    length += snprintf(csv_string+length, max_string_size, "\"%s\"", playerstable.guid);

    // append timestamp
    length += snprintf(csv_string+length, max_string_size, "%s", ",");
    length += snprintf(csv_string+length, max_string_size, "%0.f", playerstable.timestamp);

    // append human-readable date time
    length += snprintf(csv_string+length, max_string_size, "%s", ",");
    length += snprintf(csv_string+length, max_string_size, "\"%s\"", playerstable.datetime);

    // append nick name
    length += snprintf(csv_string+length, max_string_size, "%s", ",");
    length += snprintf(csv_string+length, max_string_size, "\"%s\"", playerstable.nickname); // wrap inside quotes to avoid breaking if there are spaces or special characters in the nickname (quotes are filtered by the engine anyway, since the engine also use quotes to wrap variables content)
    
    return csv_string;
}

// Set the value of a feature in the current interframe for a given player, and commit the previous interframe for this client if the feature is a modifier
// Note: you should use this function whenever you want to modify the value of a feature for a client, because it will take care of writing down the interframes values whenever needed (else you may lose the data of your interframes!), because the function to commit the features values is only called from here.
void SV_ExtendedRecordSetFeatureValue(interframeIndex_t feature, double value, int client) {
    // If the value has changed (or the old one is NaN), we do something, else we just keep it like that
    // TODO: maybe compare the delta (diff) below a certain threshold for floats, eg: if (fabs(a - b) < SOME_DELTA)
    if ( (sv_interframe[feature].value[client] != value) | isnan(sv_interframe[feature].value[client]) ) {
        // If this feature is a modifier, and the interframe wasn't already modified in the current frame, we switch the modified flag and commit the previous interframe (else if it was already modified, we wait until all features modifications take place and we'll see at the next frame)
        if ( sv_interframe[feature].modifier == qtrue && sv_interframeModified[client] != qtrue ) {
            // Set the modified flag to true
            sv_interframeModified[client] = qtrue;
            
            // Flush/Commit/Write down the previous interframe since it will be modified
            SV_ExtendedRecordWriteValues(client);
        }
        // Modify the value (after having committed the previous interframe if this feature is a modifier)
        sv_interframe[feature].value[client] = value;
    }
}
// TODO: try to understand why the following (when set in the beginning of SV_ExtendedRecordSetFeatureValue):
/*
    if (feature == LABEL_CHEATER) {
        Com_DPrintf("OACS TEST3 old:%.0f new:%.0f\n", sv_interframe[feature].value[client], value);
        Com_DPrintf("OACS TEST4 r1:%i r2:%i, r3:%i r4:%i\n", value == sv_interframe[feature].value[client], !(value == sv_interframe[feature].value[client]), (sv_interframe[feature].value[client] == sv_interframe[feature].value[client]), isnan(sv_interframe[feature].value[client]));
        double t1 = sv_interframe[feature].value[client];
        double t2 = 1.0;
        Com_DPrintf("OACS TEST5 t1:%f t2:%f\n", t1, t2);
        Com_DPrintf("OACS TEST6 u1:%i u2:%i\n", t1 == t2, t1 != t2);
    }
Produces the following:
OACS TEST3 old:nan new:1
OACS TEST4 r1:1 r2:0, r3:1 r4:1
OACS TEST5 t1:nan t2:1.000000
OACS TEST6 u1:1 u2:0
But if we do:
double t1 = NAN;
or:
double t1 = featureDefaultValue;
It works OK!
*/

/* Append a string to another in linear time and safely with a limit n of maximum size - DOESN'T WORK
char* strncat_lin( char* dest, char* src, size_t n ) {
    Com_DPrintf("OACS Test: dest: %s src: %s n: %i\n", dest, src, n);
     while (*dest != '\0') {
        dest++; n--;
     }
     // Overwrite the null byte if present
     if( dest-1 == '\0') {
        dest--; n++;
     }
     while ( n-- > 0 && ((*dest++ = *src++) != '\0' ) );
     if (*dest != '\0') *dest = '\0';
     Com_DPrintf("OACS Test2: dest: %s src: %s\n", dest, src);
     return --dest;
}
*/

// Check if a file is empty
qboolean FS_IsFileEmpty(char* filename) {
    qboolean result;
    fileHandle_t file;
    char buf[1];

    // Open the file (we need to use ioquake3 functions because it will build the full path relative to homepath and gamedir, see FS_BuildOSPath)
    FS_FOpenFileRead(filename, &file, qfalse); // the length returned by FS_FOpenFileRead is equal to 1 when the file has no content or when it has 1 character, so this length can't be used to know if the file is empty or not

    // Get the first character from the file, but what we are interested in is the number of characters read, which we store in the var c
    int c = FS_Read(buf, 1, file);

    // If the number of characters that were read is 0, then the file is empty
    if (c == 0) {
        result = qtrue;
    // Else if we have read 1 character, then the file is not empty
    } else {
        result = qfalse;
    }
    // Close the file
    FS_FCloseFile(file);
    // Return the result
    return result;
}

// Check if a client is a bot
qboolean SV_IsBot(int client) {
    sharedEntity_t *entity;
    entity = SV_GentityNum(client); // Get entity (of this player) object
    
    // Proceed only if the client is not a bot
    if ( (entity->r.svFlags & SVF_BOT) | (svs.clients[client].netchan.remoteAddress.type == NA_BOT) ) {
        return qtrue;
    } else {
        return qfalse;
    }
}

// Check if a player is spectating
qboolean SV_IsSpectator(int client) {
    // WARNING: sv.configstrings[CS_PLAYERS + client] is NOT the same as cl->userinfo, they don't contain the same infos!

    int team;
    char team_s[MAX_STRING_CHARS];
    client_t	*cl;
    cl = &svs.clients[client]; // Get client object

    team = atoi( Info_ValueForKey( sv.configstrings[CS_PLAYERS + client], "t" ) ); // Get the team
    Q_strncpyz(team_s, Info_ValueForKey( cl->userinfo, "team" ), MAX_STRING_CHARS); // Get the team from another cvar which is formatted differently

    if ( (team == TEAM_SPECTATOR) | !Q_stricmp(team_s, "spectator") | !Q_stricmp(team_s, "s") ) {
        return qtrue;
    } else {
        return qfalse;
    }
}

// Count the number of connected players
int SV_CountPlayers(void) {
    int i, count;

    count = 0;
    for (i=0;i<sv_maxclients->integer;i++) {
        // If the slot is empty, we skip
        if ( svs.clients[i].state < CS_CONNECTED ) {
			continue;
        // or if it's a bot, we also skip
        } else if ( SV_IsBot(i) ) {
            continue;
        }
        count++; // else, it's a human player, we add him to the count
    }
    
    return count;
}

// Return a random number between min and max, with more variability than a simple (rand() % (max-min)) + min
int rand_range(int min, int max) {

    int retval;
    
    retval = ( (double) rand() / (double)RAND_MAX * (max-min+1) ) + min;

    return retval;
}
