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

void SV_ExtendedRecordWriteStruct(void);

typedef enum {
	FEATURE_ID,			// no map loaded
	FEATURE_HUMAN,			// spawning level entities
	FEATURE_GAMESPECIFIC,				// actively running
    FEATURE_PHYSICS
} featureType_t;

typedef struct
{
    char* key;
	featureType_t type;
    double value;
} feature_t;

