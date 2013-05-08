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
- Edit sv_interframe_keys, sv_interframe_modifiers and sv_interframe_types and add your new feature in these arrays too (please specify the name of your feature in a comment!).
- Edit the SV_ExtendedRecordInterframeInitValue() (not InitValues()! Note the S) if you want to change the initial value (or you can set it in update, the default value is NAN),
- Edit the SV_ExtendedRecordInterframeUpdateValues() function OR add your code anywhere else inside the ioquake3 code to update the feature's value on every frame, but then use the function SV_ExtendedRecordSetFeatureValue() to update the value of a feature!
Note: ALWAYS use SV_ExtendedRecordSetFeatureValue() to update the value of a feature (_don't_ do it directly!), because this function manage if the interframe needs to be written and flushed into a file on change or not (if a feature is non-modifier). Exception is when you initialize the value of the feature, because you don't want to commit the initial interframe (which contains only default NaN values).
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
cvar_t  *sv_oacsLabelPassword;
cvar_t *sv_oacsMaxPing;
cvar_t *sv_oacsMaxLastPacketTime;

char *sv_playerstable_keys = "playerid,playerip,playerguid,connection_timestamp,connection_datetime,playername"; // key names, edit this if you want to add more infos in the playerstable

// names of the features, array of string keys to output in the typesfile and datafile
char *sv_interframe_keys[] = {
    "playerid",
    "timestamp",

    "svstime",
    "reactiontime",
    "svtime",
    "lastcommandtime",
    "commandtime_reactiontime",
    "angleinaframe",
    "lastmouseeventtime",
    "mouseeventtime_reactiontime",
    "movementdirection",


    "score",
    "scoreacc",
    "hits",
    "hitsacc",
    "death",
    "deathacc",
    "captures",
    "capturesacc",
    "impressivecount",
    "impressivecountacc",
    "excellentcount",
    "excellentcountacc",
	"defendcount",
    "defendcountacc",
	"assistcount",
    "assistcountacc",
	"gauntletfragcount",
    "gauntletfragcountacc",
    
    "frags",
    "fragsinarow",
    
    "selfdamageeventcount",
    "selfdamageeventcountacc",

    "ducked",
    "midair",

    "weapon",
    "weaponstate",
    "weaponinstanthit",

    "powerup_quad",
    "powerup_battlesuit",
    "powerup_haste",
    "powerup_invisibility",
    "powerup_regeneration",
    "powerup_flight",
#ifdef MISSIONPACK
    "powerup_scout",
    "powerup_guard",
    "powerup_doubler",
    "powerup_ammoregen",
    "powerup_invulnerability",

    "powerup_persistant_powerup",
#endif

    "hasflag",
    "holyshit",
    "rank",
    "enemyhadflag",

    "health",
    "max_health",
    "armor",
    "speed",
    "speedratio",
    "selfdamagecount",

    "framerepeat",
    "cheater"
};

// types of the features, will be outputted in the typesfile
int sv_interframe_types[] = {
    FEATURE_ID, // playerid
    FEATURE_ID, // timestamp

    FEATURE_ID, // svstime
    FEATURE_HUMAN, // reactiontime
    FEATURE_ID, // svtime
    FEATURE_METADATA, // lastcommandtime
    FEATURE_HUMAN, // commandtime_reactiontime
    FEATURE_HUMAN, // angleinaframe
    FEATURE_METADATA, // lastmouseeventtime
    FEATURE_HUMAN, // mouseeventtime_reactiontime
    FEATURE_HUMAN, // movementdirection

    FEATURE_PHYSICS, // score
    FEATURE_HUMAN, // scoreacc
    FEATURE_PHYSICS, // hits
    FEATURE_HUMAN, // hitsacc
    FEATURE_PHYSICS, // death
    FEATURE_HUMAN, // deathacc
    FEATURE_PHYSICS, // captures
    FEATURE_HUMAN, // capturesacc
    FEATURE_PHYSICS, // impressivecount
    FEATURE_HUMAN, // impressivecountacc
    FEATURE_PHYSICS, // excellentcount
    FEATURE_HUMAN, // excellentcountacc
	FEATURE_PHYSICS, // defendcount
    FEATURE_HUMAN, // defendcountacc
	FEATURE_PHYSICS, // assistcount
    FEATURE_HUMAN, // assistcountacc
	FEATURE_PHYSICS, // gauntletfragcount
    FEATURE_HUMAN, // gauntletfragcountacc
    
    FEATURE_PHYSICS, // frags
    FEATURE_HUMAN, // fragsinarow

    FEATURE_PHYSICS, // damagecount
    FEATURE_HUMAN, // damageeventcountacc

    FEATURE_HUMAN, // ducked
    FEATURE_HUMAN, // midair

    FEATURE_HUMAN, // weapon
    FEATURE_HUMAN, // weaponstate
    FEATURE_HUMAN, // weaponinstanthit

    FEATURE_HUMAN, // powerup_quad
    FEATURE_HUMAN, // powerup_battlesuit
    FEATURE_HUMAN, // powerup_haste
    FEATURE_HUMAN, // powerup_invisibility
    FEATURE_HUMAN, // powerup_regeneration
    FEATURE_HUMAN, // powerup_flight
#ifdef MISSIONPACK
    FEATURE_HUMAN, // powerup_scout
    FEATURE_HUMAN, // powerup_guard
    FEATURE_HUMAN, // powerup_doubler
    FEATURE_HUMAN, // powerup_ammoregen
    FEATURE_HUMAN, // powerup_invulnerability

    FEATURE_HUMAN, // powerup_persistant_powerup
#endif

    FEATURE_HUMAN, // hasflag
    FEATURE_HUMAN, // holyshit
    FEATURE_HUMAN, // rank
    FEATURE_HUMAN, // enemyhadflag

    FEATURE_PHYSICS, // health
    FEATURE_PHYSICS, // max_health
    FEATURE_GAMESPECIFIC, // armor
    FEATURE_PHYSICS, // speed
    FEATURE_GAMESPECIFIC, // speedratio
    FEATURE_GAMESPECIFIC, // damagecount

    FEATURE_METAINTERFRAME, // framerepeat
    FEATURE_LABEL // cheater
};

// modifiers for the features, array of boolean that specifies if a feature should commit the interframe on change or not
qboolean sv_interframe_modifiers[] = {
    qtrue, // playerid
    qfalse, // timestamp

    qfalse, // svstime
    qfalse, // reactiontime
    qfalse, // svtime
    qfalse, // lastcommandtime
    qfalse, // commandtime_reactiontime
    qtrue, // angleinaframe
    qtrue, // lastmouseeventtime
    qfalse, // mouseeventtime_reactiontime
    qfalse, // movementdirection

    qtrue, // score
    qtrue, // scoreacc
    qtrue, // hits
    qtrue, // hitsacc
    qtrue, // death
    qtrue, // deathacc
    qtrue, // captures
    qtrue, // capturesacc
    qtrue, // impressivecount
    qtrue, // impressivecountacc
    qtrue, // excellentcount
    qtrue, // excellentcountacc
	qtrue, // defendcount
    qtrue, // defendcountacc
	qtrue, // assistcount
    qtrue, // assistcountacc
	qtrue, // gauntletfragcount
    qtrue, // gauntletfragcountacc
    
    qtrue, // frags
    qtrue, // fragsinarow

    qtrue, // damagecount
    qtrue, // damageeventcountacc

    qtrue, // ducked
    qtrue, // midair

    qtrue, // weapon
    qtrue, // weaponstate
    qtrue, // weaponinstanthit

    qtrue, // powerup_quad
    qtrue, // powerup_battlesuit
    qtrue, // powerup_haste
    qtrue, // powerup_invisibility
    qtrue, // powerup_regeneration
    qtrue, // powerup_flight
#ifdef MISSIONPACK
    qtrue, // powerup_scout
    qtrue, // powerup_guard
    qtrue, // powerup_doubler
    qtrue, // powerup_ammoregen
    qtrue, // powerup_invulnerability

    qtrue, // powerup_persistant_powerup
#endif

    qtrue, // hasflag
    qtrue, // holyshit
    qtrue, // rank
    qtrue, // enemyhadflag

    qtrue, // health
    qfalse, // max_health
    qtrue, // armor
    qfalse, // speed
    qfalse, // speedratio
    qfalse, // damagecount

    qfalse, // framerepeat
    qtrue // cheater
};

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
        SV_ExtendedRecordInterframeInit(client);
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
    playerState_t	*ps;
    
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
        ps = SV_GameClientNum( i );
		if ( (svs.clients[i].state < CS_ACTIVE) ) {
			continue;
        } else if ( SV_IsBot(i) | SV_IsSpectator(i) ) { // avoid saving empty interframes of not yet fully connected players, bots nor spectators
            continue;
        } else if ( ( (svs.time - svs.clients[client].lastPacketTime) > sv_oacsMaxLastPacketTime->integer) || (svs.clients[client].ping > sv_oacsMaxPing->integer) || // drop the last interframe(s) if the player is lagging (we don't want to save extreme values for reaction time and such stuff just because the player is flying in the air, waiting for the connection to stop lagging)
                    ( (svs.clients[client].netchan.outgoingSequence - svs.clients[client].deltaMessage) >= (PACKET_BACKUP - 3) ) ) { // client hasn't gotten a good message through in a long time
            continue;
        } else if ( ps->pm_type != PM_NORMAL ) { // save only if the player is playing in a normal state, not in a special state where he can't play like dead or intermission FIXME: maybe add PM_DEAD too?
            continue;
        }

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

    // at startup (no client), initialize the key name, type and modifier of each variable
    if (client < 0) {
        // for each variable, we set its name (key), type and modifier flag
        for (i=0;i<FEATURES_COUNT;i++) {
            sv_interframe[i].key = sv_interframe_keys[i];
            sv_interframe[i].type = sv_interframe_types[i];
            sv_interframe[i].modifier = sv_interframe_modifiers[i];
        }
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

// Set the initial values for some features, this function will call another one in order to set the correct value (this ease later modifications)
// Note: do not use SV_ExtendedRecordSetFeatureValue() here, just access directly sv_interframe (you don't want to commit anything here)
void SV_ExtendedRecordInterframeInitValues(int client) {
    int feature;
    playerState_t *ps;

    // Loop through all features and set a default value
    for (feature=0;feature<FEATURES_COUNT;feature++) {
        // Proceed only if the client is not a bot
        if ( !SV_IsBot(client) ) {
            sv_interframe[feature].value[client] = SV_ExtendedRecordInterframeInitValue(client, feature);
        }
    }

    // Init the previous player's state values
    ps = SV_GameClientNum( client ); // get the player's state
    Com_Memset(&prev_ps, 0, sizeof(playerState_t));
    Com_Memcpy(&prev_ps[client], ps, sizeof(playerState_t));
}

// Set the initial values for some features
// This is a simple switch/case in order to ease modifications, just return the value you want for this feature
// set here the default values you want for a feature if you want it to be different than 0 or NaN
double SV_ExtendedRecordInterframeInitValue(int client, int feature) {
    playerState_t *ps;
    ps = SV_GameClientNum( client ); // get the player's state

    switch( feature ) {
        case FEATURE_PLAYERID:
            // Set unique player id (we want this id to be completely generated serverside and without any means to tamper it clientside) - we don't care that the id change for the same player when he reconnects, since anyway the id will always link to the player's ip and guid using the playerstable
            //char tmp[MAX_STRING_CHARS] = ""; snprintf(tmp, MAX_STRING_CHARS, "%i%lu", rand_range(1, 99999), (unsigned long int)time(NULL));
            return atof( va("%i%lu", rand_range(1, 99999), (unsigned long int)time(NULL)) ); // FIXME: use a real UUID/GUID here (for the moment we simply use the timestamp in seconds + a random number, this should be enough for now to ensure the uniqueness of all the players) - do NOT use ioquake3 GUID since it can be spoofed (there's no centralized authorization system!)
        case FEATURE_SVSTIME:
            // Server time (serverStatic_t time, which is always strictly increasing)
            return svs.time;
        case FEATURE_SVTIME:
            // Server time (non-persistant server time, can be used to check whether a new game is starting)
            return sv.time;
        case FEATURE_LASTCOMMANDTIME:
            return ps->commandTime;
        case FEATURE_MOVEMENTDIR:
            return ps->movementDir;

        case FEATURE_SCORE:
        case FEATURE_HITS:
        case FEATURE_DEATH:
        case FEATURE_CAPTURES:
        case FEATURE_IMPRESSIVE_COUNT:
        case FEATURE_EXCELLENT_COUNT:
        case FEATURE_DEFEND_COUNT:
        case FEATURE_ASSIST_COUNT:
        case FEATURE_GAUNTLET_FRAG_COUNT:
        case FEATURE_FRAGS:
        case FEATURE_DAMAGEEVENT_COUNT:
            return 0;

        case FEATURE_POWERUP_QUAD:
        case FEATURE_POWERUP_BATTLESUIT:
        case FEATURE_POWERUP_HASTE:
        case FEATURE_POWERUP_INVIS:
        case FEATURE_POWERUP_REGEN:
        case FEATURE_POWERUP_FLIGHT:
#ifdef MISSIONPACK
        case FEATURE_POWERUP_SCOUT:
        case FEATURE_POWERUP_GUARD:
        case FEATURE_POWERUP_DOUBLER:
        case FEATURE_POWERUP_AMMOREGEN:
        case FEATURE_POWERUP_INVULNERABILITY:
        case FEATURE_PERSISTANT_POWERUP:
            return 0;
#endif

        case FEATURE_HASFLAG:
        case FEATURE_HOLYSHIT:
        case FEATURE_RANK:
        case FEATURE_ENEMYHADFLAG:
            return 0;
        
        case FEATURE_HEALTH:
            return ps->stats[STAT_HEALTH];
        case FEATURE_MAX_HEALTH:
            return ps->stats[STAT_MAX_HEALTH];
        case FEATURE_ARMOR:
            return ps->stats[STAT_ARMOR];
        case FEATURE_SPEED:
            return 0; // initial speed of the player, not ps->speed which is the theoretical maximum player's speed
        case FEATURE_SPEEDRATIO:
            return 0;
        case FEATURE_DAMAGE_COUNT:
            return 0;

        case FEATURE_FRAMEREPEAT:
            // FrameRepeat: number of times a frame was repeated (1 = one frame, it was not repeated)
            return 1;
        case LABEL_CHEATER:
            // Label: by default, the player is honest. The player is labeled as a cheater only under supervision of the admin, to grow the data file with anomalous examples.
            return 0;
        default:
            return featureDefaultValue;
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
        } // Note: we do update spectators and players even when below the number of required human players, but we just don't save them (see SV_ExtendedRecordWriteValues()). This allows to always have fresh values (eg: reactiontime won't get too huge).
        // Ok this player is valid, we update the values
        
        // Update the features' values that are set from the victims
        // Note: this must be done BEFORE the bot check, because we want that victims bots set features of human attackers
        //SV_ExtendedRecordInterframeUpdateValuesAttacker(i);

        // Avoid updating bots
        if ( SV_IsBot(i) ) {
            continue;
        }

        // Update the features' values
        SV_ExtendedRecordInterframeUpdateValues(i);
        
        // Check if the interframe is repeated or if it has changed since the previous one
        if (sv_interframeModified[i] == qtrue) { // it changed, and the previous one was already committed, so we just have to reset FRAMEREPEAT and the modified flag
            SV_ExtendedRecordSetFeatureValue(FEATURE_FRAMEREPEAT, 1, i);
            sv_interframeModified[i] = qfalse;
        } else { // Else the interframe is repeated, not changed, thus we increment the FRAMEREPEAT
            sv_interframe[FEATURE_FRAMEREPEAT].value[i]++;
        }

    }

}

// Set the updated values for all features
// you can add here your code to update a feature at the end of every frame and for every player
// Note: You need to update all the features! Either here in this function, or elsewhere in the ioquake3 code!
// Note2: you should here use SV_ExtendedRecordSetFeatureValue() because you generally want to commit your variables when they are updated (if not, prefer to use a modifier = qfalse and still use the SetFeatureValue() function).
// Note3: the order matters here, so that you can compute a feature only after another feature was computed.
void SV_ExtendedRecordInterframeUpdateValues(int client) {
    int attacker;
    playerState_t	*ps;
    ps = SV_GameClientNum( client ); // get the player's state
    attacker = ps->persistant[PERS_ATTACKER];

    //== Updating normal features' values
    // TIMESTAMP
    SV_ExtendedRecordSetFeatureValue(FEATURE_TIMESTAMP, time(NULL), client);
    // COMMANDTIME_REACTIONTIME
    // must be set before setting the new lastcommandtime
    SV_ExtendedRecordSetFeatureValue(FEATURE_COMMANDTIME_REACTIONTIME, ps->commandTime - sv_interframe[FEATURE_LASTCOMMANDTIME].value[client], client);
    // LASTCOMMANDTIME
    SV_ExtendedRecordSetFeatureValue(FEATURE_LASTCOMMANDTIME, ps->commandTime, client);
    // ANGLEINAFRAME
    // Compute the total angle in all directions in one frame
    SV_ExtendedRecordSetFeatureValue(FEATURE_ANGLEINAFRAME, (abs(ps->viewangles[0] - prev_ps[client].viewangles[0]) + abs(ps->viewangles[1] - prev_ps[client].viewangles[1]) + abs(ps->viewangles[2] - prev_ps[client].viewangles[2])) , client);
    // LASTMOUSEEVENTTIME and MOUSEEVENTTIME_REACTIONTIME
    if ( sv_interframe[FEATURE_ANGLEINAFRAME].value[client] > 0 ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_MOUSEEVENTTIME_REACTIONTIME, svs.time - sv_interframe[FEATURE_LASTMOUSEEVENTTIME].value[client], client); // update the mouse reaction time before the last mouse event time
        SV_ExtendedRecordSetFeatureValue(FEATURE_LASTMOUSEEVENTTIME, svs.time, client);
    }
    // MOVEMENTDIR
    SV_ExtendedRecordSetFeatureValue(FEATURE_MOVEMENTDIR, ps->movementDir, client);


    // FRAGS and FRAGSINAROW
    // we try to heuristically tell when the current player killed someone when the number of hits and the score both increased (then probably the player killed another player and got his score incremented)
    // Note: this must be done before updating both FEATURE_HITS and FEATURE_SCORE
    // Note2: this also works with bots, but may not be very reliable.
    // FIXME: find a more reliable way to count frags, and which works with bots?
    if ( (ps->persistant[PERS_HITS] > sv_interframe[FEATURE_HITS].value[client]) && (ps->persistant[PERS_SCORE] > sv_interframe[FEATURE_SCORE].value[client]) ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGS, sv_interframe[FEATURE_FRAGS].value[client] + 1, client);
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, sv_interframe[FEATURE_FRAGSINAROW].value[client] + 1, client);
    } else { // reset the accumulator if no frag in this frame
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, 0, client);
    }

    // SCOREACC
    // increment when the value change
    // Note: must be done before updating FEATURE_HITS
    if ( ps->persistant[PERS_SCORE] > sv_interframe[FEATURE_SCORE].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_SCOREACC, sv_interframe[FEATURE_SCOREACC].value[client] + (ps->persistant[PERS_SCORE] - sv_interframe[FEATURE_SCORE].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_SCOREACC, 0, client);
    }

    // SCORE
    SV_ExtendedRecordSetFeatureValue(FEATURE_SCORE, ps->persistant[PERS_SCORE], client);

    // HITSACC
    // increment when the value change
    // Note: must be done before updating FEATURE_HITS
    if ( ps->persistant[PERS_HITS] > sv_interframe[FEATURE_HITS].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_HITSACC, sv_interframe[FEATURE_HITSACC].value[client] + (ps->persistant[PERS_HITS] - sv_interframe[FEATURE_HITS].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_HITSACC, 0, client);
    }

    // HITS
    SV_ExtendedRecordSetFeatureValue(FEATURE_HITS, ps->persistant[PERS_HITS], client);

    // DEATHACC
    // increment when the value change
    // Note: must be done before updating FEATURE_DEATH
    if ( ps->persistant[PERS_KILLED] > sv_interframe[FEATURE_DEATH].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_DEATHACC, sv_interframe[FEATURE_DEATHACC].value[client] + (ps->persistant[PERS_KILLED] - sv_interframe[FEATURE_DEATH].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_DEATHACC, 0, client);
    }

    // DEATH
    SV_ExtendedRecordSetFeatureValue(FEATURE_DEATH, ps->persistant[PERS_KILLED], client);

    // CAPTURESACC
    // increment when the value change
    // Note: must be done before updating FEATURE_CAPTURES
    if ( ps->persistant[PERS_CAPTURES] > sv_interframe[FEATURE_CAPTURES].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_CAPTURESACC, sv_interframe[FEATURE_CAPTURESACC].value[client] + (ps->persistant[PERS_CAPTURES] - sv_interframe[FEATURE_CAPTURES].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_CAPTURESACC, 0, client);
    }

    // CAPTURES
    SV_ExtendedRecordSetFeatureValue(FEATURE_CAPTURES, ps->persistant[PERS_CAPTURES], client);

    // IMPRESSIVE_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_IMPRESSIVE_COUNT
    if ( ps->persistant[PERS_IMPRESSIVE_COUNT] > sv_interframe[FEATURE_IMPRESSIVE_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_IMPRESSIVE_COUNTACC, sv_interframe[FEATURE_IMPRESSIVE_COUNTACC].value[client] + (ps->persistant[PERS_IMPRESSIVE_COUNT] - sv_interframe[FEATURE_IMPRESSIVE_COUNT].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_IMPRESSIVE_COUNTACC, 0, client);
    }

    // IMPRESSIVE_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_IMPRESSIVE_COUNT, ps->persistant[PERS_IMPRESSIVE_COUNT], client);

    // EXCELLENT_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_EXCELLENT_COUNT
    if ( ps->persistant[PERS_EXCELLENT_COUNT] > sv_interframe[FEATURE_EXCELLENT_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_EXCELLENT_COUNTACC, sv_interframe[FEATURE_EXCELLENT_COUNTACC].value[client] + (ps->persistant[PERS_EXCELLENT_COUNT] - sv_interframe[FEATURE_EXCELLENT_COUNT].value[client]), client);
        // Excellent = two successive kills in a row, thus we also update FRAGSINAROW and FRAGS
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, sv_interframe[FEATURE_FRAGS].value[client] + 2, client);
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, sv_interframe[FEATURE_FRAGSINAROW].value[client] + 2, client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_EXCELLENT_COUNTACC, 0, client);
    }

    // EXCELLENT_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_EXCELLENT_COUNT, ps->persistant[PERS_EXCELLENT_COUNT], client);

    // DEFEND_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_DEFEND_COUNT
    if ( ps->persistant[PERS_DEFEND_COUNT] > sv_interframe[FEATURE_DEFEND_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_DEFEND_COUNTACC, sv_interframe[FEATURE_DEFEND_COUNTACC].value[client] + (ps->persistant[PERS_DEFEND_COUNT] - sv_interframe[FEATURE_DEFEND_COUNT].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_DEFEND_COUNTACC, 0, client);
    }

    // DEFEND_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_DEFEND_COUNT, ps->persistant[PERS_DEFEND_COUNT], client);

    // ASSIST_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_ASSIST_COUNT
    if ( ps->persistant[PERS_ASSIST_COUNT] > sv_interframe[FEATURE_ASSIST_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_ASSIST_COUNTACC, sv_interframe[FEATURE_ASSIST_COUNTACC].value[client] + (ps->persistant[PERS_ASSIST_COUNT] - sv_interframe[FEATURE_ASSIST_COUNT].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_ASSIST_COUNTACC, 0, client);
    }

    // ASSIST_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_ASSIST_COUNT, ps->persistant[PERS_ASSIST_COUNT], client);

    // GAUNTLET_FRAG_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_GAUNTLET_FRAG_COUNT
    if ( ps->persistant[PERS_GAUNTLET_FRAG_COUNT] > sv_interframe[FEATURE_GAUNTLET_FRAG_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_GAUNTLET_FRAG_COUNTACC, sv_interframe[FEATURE_GAUNTLET_FRAG_COUNTACC].value[client] + (ps->persistant[PERS_GAUNTLET_FRAG_COUNT] - sv_interframe[FEATURE_GAUNTLET_FRAG_COUNT].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_GAUNTLET_FRAG_COUNTACC, 0, client);
    }

    // GAUNTLET_FRAG_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_GAUNTLET_FRAG_COUNT, ps->persistant[PERS_GAUNTLET_FRAG_COUNT], client);


    // DAMAGEEVENT_COUNTACC
    // increment when the value change
    // Note: must be done before updating FEATURE_DAMAGEEVENT_COUNT
    if ( ps->damageEvent > sv_interframe[FEATURE_DAMAGEEVENT_COUNT].value[client] ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_DAMAGEEVENT_COUNTACC, sv_interframe[FEATURE_DAMAGEEVENT_COUNTACC].value[client] + (ps->damageEvent - sv_interframe[FEATURE_DAMAGEEVENT_COUNT].value[client]), client);
    } else { // else we reset the accumulator to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_DAMAGEEVENT_COUNTACC, 0, client);
    }

    // DAMAGEEVENT_COUNT
    SV_ExtendedRecordSetFeatureValue(FEATURE_DAMAGEEVENT_COUNT, ps->damageEvent, client);

    // DUCKED
    SV_ExtendedRecordSetFeatureValue(FEATURE_DUCKED, (ps->pm_flags & PMF_DUCKED) ? 1 : 0, client);
    // MIDAIR
    SV_ExtendedRecordSetFeatureValue(FEATURE_MIDAIR, ps->groundEntityNum == ENTITYNUM_NONE, client);

    // WEAPON
    SV_ExtendedRecordSetFeatureValue(FEATURE_WEAPON, ps->weapon, client);
    // WEAPONSTATE
    SV_ExtendedRecordSetFeatureValue(FEATURE_WEAPONSTATE, ps->weaponstate, client);
    // WEAPONINSTANTHIT
    SV_ExtendedRecordSetFeatureValue(FEATURE_WEAPONINSTANTHIT, SV_IsWeaponInstantHit(ps->weapon) ? 1 : 0, client);

    // POWERUP_QUAD
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_QUAD, (ps->powerups[PW_QUAD] != 0) ? 1 : 0, client);
    // POWERUP_BATTLESUIT
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_BATTLESUIT, (ps->powerups[PW_BATTLESUIT] != 0) ? 1 : 0, client);
    // POWERUP_HASTE
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_HASTE, (ps->powerups[PW_HASTE] != 0) ? 1 : 0, client);
    // POWERUP_INVIS
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_INVIS, (ps->powerups[PW_INVIS] != 0) ? 1 : 0, client);
    // POWERUP_REGEN
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_REGEN, (ps->powerups[PW_REGEN] != 0) ? 1 : 0, client);
    // POWERUP_FLIGHT
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_FLIGHT, (ps->powerups[PW_FLIGHT] != 0) ? 1 : 0, client);
#ifdef MISSIONPACK
    // POWERUP_SCOUT
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_SCOUT, (ps->powerups[PW_SCOUT] != 0) ? 1 : 0, client);
    // POWERUP_GUARD
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_GUARD, (ps->powerups[PW_GUARD] != 0) ? 1 : 0, client);
    // POWERUP_DOUBLER
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_DOUBLER, (ps->powerups[PW_DOUBLER] != 0) ? 1 : 0, client);
    // POWERUP_AMMOREGEN
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_AMMOREGEN, (ps->powerups[PW_AMMOREGEN] != 0) ? 1 : 0, client);
    // POWERUP_INVULNERABILITY
    SV_ExtendedRecordSetFeatureValue(FEATURE_POWERUP_INVULNERABILITY, (ps->powerups[PW_INVULNERABILITY] != 0) ? 1 : 0, client);

    // PERSISTANT_POWERUP
    SV_ExtendedRecordSetFeatureValue(FEATURE_PERSISTANT_POWERUP, ps->stats[STAT_PERSISTANT_POWERUP], client);
#endif

    // HASFLAG
    // if the player owns any flag, we consider he has the flag (not necessary his team's flag, but any flag anyway makes this player a more likely target)
    if ( (ps->powerups[PW_REDFLAG] > 0) || (ps->powerups[PW_BLUEFLAG] > 0) || (ps->powerups[PW_NEUTRALFLAG] > 0) ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_HASFLAG, 1, client);
    } else {
        SV_ExtendedRecordSetFeatureValue(FEATURE_HASFLAG, 0, client);
    }
    // HOLYSHIT
    // Only the attacker gets HolyShit, here we choose to not set it to the victim (but in the engine the flag is set for both!). Plus, this is an accumulator.
    // PERS_PLAYEREVENT events get XORed everytime, so that we can't know when the event is happening or not unless we check the difference with the previous state (if we witness that it was xored, then the event just happened!)
    // Note: works only when a player kills another player. It won't work if the player killed a bot.
    if (ps->persistant[PERS_PLAYEREVENTS] != prev_ps[client].persistant[PERS_PLAYEREVENTS]) { // an event was changed
		if ((ps->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_HOLYSHIT) !=
				(prev_ps[client].persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_HOLYSHIT)) { // check that the HOLYSHIT event was XORed
			SV_ExtendedRecordSetFeatureValue(FEATURE_HOLYSHIT, sv_interframe[FEATURE_HOLYSHIT].value[attacker] + 1, attacker);
		}
    } // FIXME: reset the HOLYSHIT accumulator? When?
    // RANK
    SV_ExtendedRecordSetFeatureValue(FEATURE_RANK, ps->persistant[PERS_RANK], client);

    // HEALTH
    SV_ExtendedRecordSetFeatureValue(FEATURE_HEALTH, ps->stats[STAT_HEALTH], client);
    // MAX_HEALTH
    SV_ExtendedRecordSetFeatureValue(FEATURE_MAX_HEALTH, ps->stats[STAT_MAX_HEALTH], client);
    // ARMOR
    SV_ExtendedRecordSetFeatureValue(FEATURE_ARMOR, ps->stats[STAT_ARMOR], client);
    // SPEED
    // Total amount of speed in all directions
    SV_ExtendedRecordSetFeatureValue(FEATURE_SPEED, ( abs(ps->velocity[0]) + abs(ps->velocity[1]) + abs(ps->velocity[2]) ), client);
    // SPEEDRATIO
    if ( ps->speed > 0 ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_SPEEDRATIO, (double)( abs(ps->velocity[0]) + abs(ps->velocity[1]) + abs(ps->velocity[2]) ) / ps->speed, client);
    } else {
        SV_ExtendedRecordSetFeatureValue(FEATURE_SPEEDRATIO, 0, client);
    }
    // DAMAGE_COUNT 
    SV_ExtendedRecordSetFeatureValue(FEATURE_DAMAGE_COUNT, ps->damageCount, client);

    //== Update special delta features: values that need to be computed only in difference with the previous interframe when there's a change, or after other features
    // Note: you should only update those features at the end, because you WANT to make sure that any change to any modifier feature already happened, so that we know for sure that the interframe will be committed or not.

    // REACTIONTIME
    if (sv_interframeModified[client] == qtrue) {
        // Update reaction time (svs.time delta) only if we have committed the last frame, because we want the difference (the delta) between the last move and the new one
        SV_ExtendedRecordSetFeatureValue(FEATURE_REACTIONTIME, svs.time - sv_interframe[FEATURE_SVSTIME].value[client], client);

        // SVSTIME
        // We have to update SVSTIME only after reaction time, and we also update it if it doesn't have any value (because we don't want that the first interframe of a player is set to the start of the server/map!)
        SV_ExtendedRecordSetFeatureValue(FEATURE_SVSTIME, svs.time, client);
    
        // SVTIME
        SV_ExtendedRecordSetFeatureValue(FEATURE_SVTIME, sv.time, client);
    }
    
    //== Update the previous player's state with the current one (in preparation for the next iteration)
    Com_Memcpy(&prev_ps[client], ps, sizeof(playerState_t));
}

// Update the features of the attacker from the victim
// WARNING: only put here features that are set on the attacker, never the victim! Else you may cause a crash (because the victim can be a bot, but bots don't have interframes)
void SV_ExtendedRecordInterframeUpdateValuesAttacker(int client) {
    int attacker;
    playerState_t	*ps;
    ps = SV_GameClientNum( client ); // get the player's state
    attacker = ps->persistant[PERS_ATTACKER];

    // Check that the attacker is human, else we abort
    if (SV_IsBot(attacker)) {
        return;
    }

    // FRAGS and FRAGSINAROW alternative way of computing (more reliable but works only when a player kills a player, not a bot)
    // we check if the current player is dead, then the attacker gets a frag
    // Note: this must be done before updating FEATURE_DEATH
    // Note2: works only when a player kills another player. Won't work with bots (since they are not taken in account in interframes).
    /*
    if ( (ps->persistant[PERS_KILLED] > sv_interframe[FEATURE_DEATH].value[client]) ) {
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGS, attacker, attacker);
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, sv_interframe[FEATURE_FRAGSINAROW].value[attacker] + 1, attacker);
    } else { // reset the accumulator if no frag in this frame
        SV_ExtendedRecordSetFeatureValue(FEATURE_FRAGSINAROW, 0, attacker);
    }
    */

    // ENEMYHADFLAG
    // If the current client has a flag and is dead, the last attacker gets the ENEMYHADFLAG set to true
    // Note: works only when a player killed another player. It won't work if the player killed a bot which had the flag.
    // FIXME: this apparently produce a random crash when picking the enemy flag and killing the enemy while he has the flag (tested against bots).
    if ( sv_interframe[FEATURE_HASFLAG].value[client] && sv_interframe[FEATURE_DEATHACC].value[client] >= 0 ) { // ps->stats[STAT_HEALTH] <= 0 is not reliable for this
        SV_ExtendedRecordSetFeatureValue(FEATURE_ENEMYHADFLAG, 1, attacker);
    } else { // else the next person hit by the attacker will reset the flag to 0
        SV_ExtendedRecordSetFeatureValue(FEATURE_ENEMYHADFLAG, 0, attacker);
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
    // FIXME: maybe compare the delta (diff) below a certain threshold for floats, eg: if (fabs(a - b) < SOME_DELTA)
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
// FIXME: try to understand why the following (when set in the beginning of SV_ExtendedRecordSetFeatureValue):
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

// Check whether a given weapon (from weapon_t enum type) is an instant-hit long range weapon
qboolean SV_IsWeaponInstantHit(int weapon) {
    switch( weapon ) {
        case WP_NONE:
            return qfalse;
            break;
        case WP_GAUNTLET: // not long range
            return qfalse;
            break;
        case WP_LIGHTNING:
            return qtrue;
            break;
        case WP_SHOTGUN:
            return qtrue;
            break;
        case WP_MACHINEGUN:
            return qtrue;
            break;
        case WP_GRENADE_LAUNCHER:
            return qfalse;
            break;
        case WP_ROCKET_LAUNCHER:
            return qfalse;
            break;
        case WP_PLASMAGUN:
            return qfalse;
            break;
        case WP_RAILGUN:
            return qtrue;
            break;
        case WP_BFG:
            return qfalse;
            break;
        case WP_GRAPPLING_HOOK:
            return qfalse;
            break;
    #ifdef MISSIONPACK
        case WP_NAILGUN:
            return qfalse;
            break;
        case WP_PROX_LAUNCHER:
            return qfalse;
            break;
        case WP_CHAINGUN:
            return qtrue;
            break;
    #endif
        default:
            return qfalse;
            break;
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

//
// COMMANDS
//

// Set the cheater label for a client
void SV_ExtendedRecordSetCheater( int client, int label ) {
    // Checking for sane values
    if ( (client < 0) || (client > sv_maxclients->integer) ) {
        Com_Printf("OACS: SV_ExtendedRecordSetCheater() Invalid arguments\n");
        return;
    }

    if ( label < 0 ) {
        Com_Printf("Label for player %i: %.0f\n", client, sv_interframe[LABEL_CHEATER].value[client]);
    } else {
        // Set the label for this client
        SV_ExtendedRecordSetFeatureValue(LABEL_CHEATER, label, client);
    }
}

// Server-side command to set a client's cheater label
// cheater <client> <label> where label is 0 for honest players, and >= 1 for cheaters
void SV_ExtendedRecordSetCheater_f( void ) {
    // Need at least one argument to proceed (the first argument is the command, so min = 2)
    if ( Cmd_Argc() < 2) {
        Com_Printf("Invalid arguments. Usage: cheater <client> [label] where label is 0 for honest players, and >= 1 for cheaters. If no label, will show the current value for the player.\n");
        return;
    }

    // If only one argument is given
    if ( Cmd_Argc() == 2) {
        // Show the cheater label for the specified client
        SV_ExtendedRecordSetCheater( atoi(Cmd_Argv(1)), -1 );
    } else {
        // Set cheater label
        SV_ExtendedRecordSetCheater( atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)) );
    }
    
}

// Client-side command to declare being a cheater
// Note: client needs a password to use this command
// Note2: the client can optionally set the value of the label >= 1, this will then represent a kind of cheat
void SV_ExtendedRecordSetCheaterFromClient_f( client_t *cl ) {
    int client;

    // Get the client id
    client = cl - svs.clients;

    if ( Cmd_Argc() < 2) // need at least the password
        return;

    if ( !strlen(sv_oacsLabelPassword->string) || Q_stricmp(Cmd_Argv(1), sv_oacsLabelPassword->string) ) {
        Com_Printf("OACS: Client tried to declare being cheater: Bad label password from %s\n", NET_AdrToString(cl->netchan.remoteAddress));
        return;
    }

    if ( Cmd_Argc() >= 3) { // type of cheat was given
        if ( atoi(Cmd_Argv(2)) > 0 ) { // accept only if it's a cheat type (0 == honest, >= 1 == cheat)
            Com_Printf("OACS: Client %i declares being cheater with label %i\n", client, atoi(Cmd_Argv(2)));
            SV_ExtendedRecordSetCheater(client, atoi(Cmd_Argv(2)));
        } else {
            Com_Printf("OACS: Client tried to declare being cheater: Bad cheat label from %s: %i\n", NET_AdrToString(cl->netchan.remoteAddress), atoi(Cmd_Argv(2)));
        }
    } else {
        Com_Printf("OACS: Client %i declares being cheater with label 1\n", client);
        SV_ExtendedRecordSetCheater(client, 1);
    }
}

// Client-side command to declare being a honest player
// Note: client needs a password to use this command
void SV_ExtendedRecordSetHonestFromClient_f( client_t *cl ) {
    int client;

    // Get the client id
    client = cl - svs.clients;

    if ( Cmd_Argc() < 2) // need at least the password
        return;

    if ( !strlen(sv_oacsLabelPassword->string) || Q_stricmp(Cmd_Argv(1), sv_oacsLabelPassword->string) ) {
        Com_Printf("OACS: Client tried to declare being honest: Bad label password from %s\n", NET_AdrToString(cl->netchan.remoteAddress));
        return;
    }

    Com_Printf("OACS: Client %i declares being honest\n", client);
    SV_ExtendedRecordSetCheater(client, 0);
}
