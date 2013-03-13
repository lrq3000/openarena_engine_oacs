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

void SV_ExtendedRecordWriteStruct(void) {
    char* name = "oacs/type.txt";
    fileHandle_t	file;

    Com_Printf("Saving the record struct in file %s", name);
    file = FS_FOpenFileWrite( name );
    
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
    
    //FS_Write ("test", 4, file);
    FS_FCloseFile( file );
}

void SV_ExtendedRecordCreateTypes(void) {

}

void SV_ExtendedRecordTypesToJson(void) {

}
