/*
** p_acs.cpp
** General BEHAVIOR management and ACS execution environment
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "templates.h"
#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sound.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "doomstat.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "s_sndseq.h"
#include "i_system.h"
#include "i_movie.h"
#include "sbar.h"
#include "vectors.h"
#include "m_swap.h"
#include "a_sharedglobal.h"
#include "v_video.h"
#include "w_wad.h"

#define STACK_SIZE 4096

#define CLAMPCOLOR(c)	(EColorRange)((unsigned)(c)>CR_UNTRANSLATED?CR_UNTRANSLATED:(c))
#define LANGREGIONMASK	MAKE_ID(0,0,0xff,0xff)	

struct CallReturn
{
	int ReturnAddress;
	ScriptFunction *ReturnFunction;
	BYTE bDiscardResult;
	BYTE Pad[3];
};

static int Stack[STACK_SIZE];

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay);

struct FBehavior::ArrayInfo
{
	int ArraySize;
	SDWORD *Elements;
};

//---- Temporary inventory functions --------------------------------------//
// to be simplified dramatically once I have a real inventory system
//
#include "gi.h"

static void DoClearInv (player_t *player)
{
	int i;

	memset (player->inventory, 0, sizeof(player->inventory));
	memset (player->weaponowned, 0, sizeof(player->weaponowned));
	memset (player->keys, 0, sizeof(player->keys));
	memset (player->ammo, 0, sizeof(player->ammo));
	if (player->backpack)
	{
		player->backpack = false;
		for (i = 0; i < NUMAMMO; i++)
		{
			player->maxammo[i] /= 2;
		}
	}
	player->pendingweapon = NUMWEAPONS;
}

static void ClearInventory (AActor *activator)
{
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoClearInv (&players[i]);
		}
	}
	else if (activator->player != NULL)
	{
		DoClearInv (activator->player);
	}
}

static const char *DoomAmmoNames[4] =
{
	"Clip", "Shell", "Cell", "RocketAmmo"
};

static const char *DoomWeaponNames[9] =
{
	"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher",
	"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun"
};

static const char *HereticAmmoNames[6] =
{
	"GoldWandAmmo", "CrossbowAmmo", "BlasterAmmo",
	"SkullRodAmmo", "PhoenixRodAmmo", "MaceAmmo"
};

static const char *HereticWeaponNames[9] =
{
	"Staff", "GoldWand", "Crossbow", "Blaster", "SkullRod",
	"PhoenixRod", "Mace", "Gauntlets", "Beak"
};

static const char *HereticArtifactNames[11] =
{
	"ArtiInvulnerability", "ArtiInvisibility", "ArtiHealth",
	"ArtiSuperHealth", "ArtiTomeOfPower", NULL, NULL, "ArtiTorch",
	"ArtiTimeBomb", "ArtiEgg", "ArtiFly"
};

static const char *HereticKeyNames[3] =
{
	"KeyBlue", "KeyYellow", "KeyGreen",
};
static const char *DoomKeyNames[6] =
{
	"BlueCard", "YellowCard", "RedCard",
	"BlueSkull", "YellowSkull", "RedSkull"
};

static void DoGiveInv (player_t *player, const char *type, int amount)
{
	int i;
	weapontype_t savedpendingweap = player->pendingweapon;

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 4; i++)
		{
			if (strcmp (DoomAmmoNames[i], type) == 0)
			{
				player->ammo[i] = MIN(player->ammo[i]+amount, player->maxammo[i]);
				return;
			}
		}
	}
	else
	{
		for (i = 0; i < 6; i++)
		{
			if (strcmp (HereticAmmoNames[i], type) == 0)
			{
				player->ammo[i] = MIN(player->ammo[i+am_goldwand]+amount, player->maxammo[i]);
				return;
			}
		}
		for (i = 0; i < 11; i++)
		{
			if (strcmp (HereticArtifactNames[i], type) == 0)
			{
				player->inventory[i] = MIN(player->inventory[i]+amount, 16);
				return;
			}
		}
	}
	const TypeInfo *info = TypeInfo::FindType (type);
	if (info == NULL)
	{
		Printf ("I don't know what %s is\n", type);
	}
	else if (!info->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		Printf ("%s is not an inventory item\n", type);
	}
	else
	{
		do
		{
			AInventory *item = static_cast<AInventory *>(Spawn
				(info, player->mo->x, player->mo->y, player->mo->z));
			item->TryPickup (player->mo);
			if (!(item->ObjectFlags & OF_MassDestruction))
			{
				item->Destroy ();
			}
		} while (--amount > 0);
	}

	// If the item was a weapon, don't bring it up automatically
	player->pendingweapon = savedpendingweap;
}

static void GiveInventory (AActor *activator, const char *type, int amount)
{
	if (amount <= 0)
	{
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoGiveInv (&players[i], type, amount);
		}
	}
	else if (activator->player != NULL)
	{
		DoGiveInv (activator->player, type, amount);
	}
}

static void TakeWeapon (player_t *player, int weapon)
{
	player->weaponowned[weapon] = false;
	if (player->readyweapon == weapon || player->pendingweapon == weapon)
	{
		P_PickNewWeapon (player);
	}
}

static void TakeAmmo (player_t *player, int ammo, int amount)
{
	if (amount == 0)
	{
		player->ammo[ammo] = 0;
	}
	else
	{
		player->ammo[ammo] = MAX(player->ammo[ammo]-amount, 0);
	}
	if (player->pendingweapon != wp_nochange)
	{ // Make sure we have the ammo for the weapon being switched to
		weapontype_t readynow = player->readyweapon;
		player->readyweapon = player->pendingweapon;
		player->pendingweapon = wp_nochange;
		if (P_CheckAmmo (player))
		{ // There was enough ammo for the pending weapon, so keep switching
			player->pendingweapon = player->readyweapon;
			player->readyweapon = readynow;
		}
		else
		{
			player->pendingweapon = player->readyweapon = readynow;
			P_CheckAmmo (player);
		}
	}
	else
	{ // Make sure we still have enough ammo for the current weapon
		P_CheckAmmo (player);
	}
}

static void TakeBackpack (player_t *player)
{
	if (!player->backpack)
		return;

	player->backpack = false;
	for (int i = 0; i < NUMAMMO; ++i)
	{
		player->maxammo[i] /= 2;
		if (player->ammo[i] > player->maxammo[i])
		{
			player->ammo[i] = player->maxammo[i];
		}
	}
}

static void DoTakeInv (player_t *player, const char *type, int amount)
{
	int i;

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 4; ++i)
		{
			if (strcmp (DoomAmmoNames[i], type) == 0)
			{
				TakeAmmo (player, i, amount);
				return;
			}
		}
		for (i = 0; i < 9; ++i)
		{
			if (strcmp (DoomWeaponNames[i], type) == 0)
			{
				TakeWeapon (player, i);
				return;
			}
		}
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (DoomKeyNames[i], type) == 0)
			{
				player->keys[i] = 0;
			}
		}
		if (strcmp ("Backpack", type) == 0)
		{
			TakeBackpack (player);
		}
	}
	else
	{
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (HereticAmmoNames[i], type) == 0)
			{
				TakeAmmo (player, i+am_goldwand, amount);
				return;
			}
		}
		for (i = 0; i < 9; ++i)
		{
			if (strcmp (HereticWeaponNames[i], type) == 0)
			{
				TakeWeapon (player, i+wp_staff);
				return;
			}
		}
		for (i = 0; i < 3; ++i)
		{
			if (strcmp (HereticKeyNames[i], type) == 0)
			{
				player->keys[i] = 0;
			}
		}
		for (i = 0; i < 11; ++i)
		{
			if (strcmp (HereticArtifactNames[i], type) == 0)
			{
				if (amount == 0)
				{
					player->inventory[i] = 0;
				}
				else
				{
					player->inventory[i] = MAX(player->inventory[i]-amount, 0);
				}
				return;
			}
		}
		if (strcmp ("BagOfHolding", type) == 0)
		{
			TakeBackpack (player);
		}
	}
}

static void TakeInventory (AActor *activator, const char *type, int amount)
{
	if (amount < 0)
	{
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoTakeInv (&players[i], type, amount);
		}
	}
	else if (activator->player != NULL)
	{
		DoTakeInv (activator->player, type, amount);
	}
}

static int CheckInventory (AActor *activator, const char *type)
{
	if (activator == NULL || activator->player == NULL)
		return 0;

	player_t *player = activator->player;
	int i;

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 4; ++i)
		{
			if (strcmp (DoomAmmoNames[i], type) == 0)
			{
				return player->ammo[i];
			}
		}
		for (i = 0; i < 9; ++i)
		{
			if (strcmp (DoomWeaponNames[i], type) == 0)
			{
				return player->weaponowned[i] ? 1 : 0;
			}
		}
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (DoomKeyNames[i], type) == 0)
			{
				return player->keys[i] ? 1 : 0;
			}
		}
		if (strcmp ("Backpack", type) == 0)
		{
			return player->backpack ? 1 : 0;
		}
	}
	else
	{
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (HereticAmmoNames[i], type) == 0)
			{
				return player->ammo[i+am_goldwand];
			}
		}
		for (i = 0; i < 9; ++i)
		{
			if (strcmp (HereticWeaponNames[i], type) == 0)
			{
				return player->weaponowned[i+wp_staff] ? 1 : 0;
			}
		}
		for (i = 0; i < 3; ++i)
		{
			if (strcmp (HereticKeyNames[i], type) == 3)
			{
				return player->keys[i] ? 1 : 0;
			}
		}
		for (i = 0; i < 11; ++i)
		{
			if (strcmp (HereticArtifactNames[i], type) == 0)
			{
				return player->inventory[i];
			}
		}
		if (strcmp ("BagOfHolding", type) == 0)
		{
			return player->backpack ? 1 : 0;
		}
	}
	return 0;
}

//---- Plane watchers ----//

class DPlaneWatcher : public DThinker
{
	DECLARE_CLASS (DPlaneWatcher, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
		int tag, int height, int special,
		int arg0, int arg1, int arg2, int arg3, int arg4);
	void Tick ();
	void Serialize (FArchive &arc);
private:
	sector_t *Sector;
	fixed_t WatchD, LastD;
	int Special, Arg0, Arg1, Arg2, Arg3, Arg4;
	AActor *Activator;
	line_t *Line;
	int LineSide;
	bool bCeiling;

	DPlaneWatcher() {}
};

IMPLEMENT_POINTY_CLASS (DPlaneWatcher)
 DECLARE_POINTER (Activator)
END_POINTERS

DPlaneWatcher::DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
	int tag, int height, int special,
	int arg0, int arg1, int arg2, int arg3, int arg4)
	: Special (special), Arg0 (arg0), Arg1 (arg1), Arg2 (arg2), Arg3 (arg3), Arg4 (arg4),
	  Activator (it), Line (line), LineSide (lineSide), bCeiling (ceiling)
{
	int secnum;

	secnum = P_FindSectorFromTag (tag, -1);
	if (secnum >= 0)
	{
		secplane_t plane;

		Sector = &sectors[secnum];
		if (bCeiling)
		{
			plane = Sector->ceilingplane;
		}
		else
		{
			plane = Sector->floorplane;
		}
		LastD = plane.d;
		plane.ChangeHeight (height << FRACBITS);
		WatchD = plane.d;
	}
	else
	{
		Sector = NULL;
		WatchD = LastD = 0;
	}
}

void DPlaneWatcher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << Special << Arg0 << Arg1 << Arg2 << Arg3 << Arg4
		<< Sector << bCeiling << WatchD << LastD << Activator
		<< Line << LineSide << bCeiling;
}

void DPlaneWatcher::Tick ()
{
	if (Sector == NULL)
	{
		Destroy ();
		return;
	}

	fixed_t newd;
	
	if (bCeiling)
	{
		newd = Sector->ceilingplane.d;
	}
	else
	{
		newd = Sector->floorplane.d;
	}

	if ((LastD < WatchD && newd >= WatchD) ||
		(LastD > WatchD && newd <= WatchD))
	{
		TeleportSide = LineSide;
		LineSpecials[Special] (Line, Activator, Arg0, Arg1, Arg2, Arg3, Arg4);
		Destroy ();
	}

}

//---- ACS lump manager ----//

FBehavior::FBehavior (BYTE *object, int len)
{
	int i;

	NumScripts = 0;
	NumFunctions = 0;
	NumArrays = 0;
	Scripts = NULL;
	Functions = NULL;
	Arrays = NULL;
	Chunks = NULL;

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		Format = ACS_Unknown;
		return;
	}

	switch (object[3])
	{
	case 0:
		Format = ACS_Old;
		break;
	case 'E':
		Format = ACS_Enhanced;
		break;
	case 'e':
		Format = ACS_LittleEnhanced;
		break;
	default:
		Format = ACS_Unknown;
		return;
	}

	Data = object;
	DataSize = len;
	
	if (Format == ACS_Old)
	{
		Chunks = object + len;
		Scripts = object + ((DWORD *)object)[1];
		NumScripts = ((DWORD *)Scripts)[0];
		if (NumScripts == 0 && ((DWORD *)Scripts)[1] == 0 && ((DWORD *)object)[1] >= 16)
		{ // No scripts and no strings, so check for redesigned ACSE/ACSe
			if (((DWORD *)Scripts)[-1] == MAKE_ID('A','C','S','e') ||
				((DWORD *)Scripts)[-1] == MAKE_ID('A','C','S','E'))
			{
				Format = (((BYTE *)Scripts)[-1] == 'e') ? ACS_LittleEnhanced : ACS_Enhanced;
				Chunks = object + ((DWORD *)Scripts)[-2];
				// Forget about the compatibility cruft at the end of the lump
				DataSize -= 16;
			}
		}
		else
		{
			Scripts += 4;
			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr2 ptr1 = *(ScriptPtr2 *)(Scripts + 12*i);
				ScriptPtr *ptr2 =  (ScriptPtr  *)(Scripts +  8*i);
				ptr2->Number = ptr1.Number % 1000;
				ptr2->Type = ptr1.Number / 1000;
				ptr2->ArgCount = ptr1.ArgCount;
				ptr2->Address = ptr1.Address;
			}
		}
	}
	else
	{
		Chunks = object + ((DWORD *)object)[1];
	}
	if (Format != ACS_Old)
	{
		Scripts = FindChunk (MAKE_ID('S','P','T','R'));
		if (object[3] != 0)
		{
			NumScripts = ((DWORD *)Scripts)[1] / 12;
			Scripts += 8;
			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr1 ptr1 = *(ScriptPtr1 *)(Scripts + 12*i);
				ScriptPtr *ptr2 =  (ScriptPtr  *)(Scripts +  8*i);
				ptr2->Number = ptr1.Number;
				ptr2->Type = ptr1.Type;
				ptr2->ArgCount = ptr1.ArgCount;
				ptr2->Address = ptr1.Address;
			}
		}
		else
		{
			NumScripts = ((DWORD *)Scripts)[1] / 8;
			Scripts += 8;
		}
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 0)
	{
		qsort (Scripts, NumScripts, 8, SortScripts);
	}

	if (Format == ACS_Old)
	{
		LanguageNeutral = ((DWORD *)Data)[1];
		LanguageNeutral += ((DWORD *)(Data + LanguageNeutral))[0] * 12 + 4;
	}
	else
	{
		LanguageNeutral = FindLanguage (0, false);
		PrepLocale (LanguageIDs[0], LanguageIDs[1], LanguageIDs[2], LanguageIDs[3]);
	}

	if (Format != ACS_Old)
	{
		DWORD *chunk;

		Functions = FindChunk(MAKE_ID('F','U','N','C'));
		if (Functions != NULL)
		{
			NumFunctions = LONG(((DWORD *)Functions)[1]);
			Functions += 8;
		}

		chunk = (DWORD *)FindChunk(MAKE_ID('M','I','N','I'));
		if (chunk != NULL)
		{
			int numvars = LONG(chunk[1])/4;
			int firstvar = LONG(chunk[2]);
			for (i = 0; i < numvars; ++i)
			{
				level.vars[i+firstvar] = LONG(chunk[3+i]);
			}
		}

		chunk = (DWORD *)FindChunk(MAKE_ID('A','R','A','Y'));
		if (chunk != NULL)
		{
			NumArrays = LONG(chunk[1])/8;
			Arrays = new ArrayInfo[NumArrays];
			memset (Arrays, 0, sizeof(*Arrays)*NumArrays);
			for (i = 0; i < NumArrays; ++i)
			{
				level.vars[LONG(chunk[2+i*2])] = i;
				Arrays[i].ArraySize = LONG(chunk[3+i*2]);
				Arrays[i].Elements = new SDWORD[Arrays[i].ArraySize];
				memset(Arrays[i].Elements, 0, Arrays[i].ArraySize*sizeof(DWORD));
			}
		}

		chunk = (DWORD *)FindChunk(MAKE_ID('A','I','N','I'));
		while (chunk != NULL)
		{
			int arraynum = level.vars[LONG(chunk[2])];
			if ((unsigned)arraynum < (unsigned)NumArrays)
			{
				int initsize = MIN<int> (Arrays[arraynum].ArraySize, (LONG(chunk[1])-4)/4);
				SDWORD *elems = Arrays[arraynum].Elements;
				for (i = 0; i < initsize; ++i)
				{
					elems[i] = LONG(chunk[3+i]);
				}
			}
			chunk = (DWORD *)NextChunk((BYTE *)chunk);
		}
	}

	DPrintf ("Loaded %d scripts, %d Functions\n", NumScripts, NumFunctions);
}

FBehavior::~FBehavior ()
{
	// Object file is freed by the zone heap
	if(Arrays != NULL)
	{
		for (int i = 0; i < NumArrays; ++i)
		{
			if (Arrays[i].Elements != NULL)
			{
				delete[] Arrays[i].Elements;
				Arrays[i].Elements = NULL;
			}
		}
		delete[] Arrays;
		Arrays = NULL;
	}
}

int STACK_ARGS FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

bool FBehavior::IsGood ()
{
	return Format != ACS_Unknown;
}

int *FBehavior::FindScript (int script) const
{
	const ScriptPtr *ptr = BinarySearch<ScriptPtr, WORD>
		((ScriptPtr *)Scripts, NumScripts, &ScriptPtr::Number, (WORD)script);

	return ptr ? (int *)(ptr->Address + Data) : NULL;
}

ScriptFunction *FBehavior::GetFunction (int funcnum) const
{
	if ((unsigned)funcnum >= (unsigned)NumFunctions)
	{
		return NULL;
	}
	return (ScriptFunction *)Functions + funcnum;
}

int FBehavior::GetArrayVal (int arraynum, int index) const
{
	if ((unsigned)arraynum >= (unsigned)NumArrays)
		return 0;
	const ArrayInfo *array = &Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return 0;
	return array->Elements[index];
}

void FBehavior::SetArrayVal (int arraynum, int index, int value)
{
	if ((unsigned)arraynum >= (unsigned)NumArrays)
		return;
	const ArrayInfo *array = &Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return;
	array->Elements[index] = value;
}

BYTE *FBehavior::FindChunk (DWORD id) const
{
	BYTE *chunk = Chunks;

	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

BYTE *FBehavior::NextChunk (BYTE *chunk) const
{
	DWORD id = *(DWORD *)chunk;
	chunk += ((DWORD *)chunk)[1] + 8;
	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

const char *FBehavior::LookupString (DWORD index, DWORD ofs) const
{
	if (Format == ACS_Old)
	{
		DWORD *list = (DWORD *)(Data + LanguageNeutral);

		if (index >= list[0])
			return NULL;	// Out of range for this list;
		return (const char *)(Data + list[1+index]);
	}
	else
	{
		if (ofs == 0)
		{
			ofs = LanguageNeutral;
			if (ofs == 0)
			{
				return NULL;
			}
		}
		DWORD *list = (DWORD *)(Data + ofs);

		if (index >= list[1])
			return NULL;	// Out of range for this list
		if (list[3+index] == 0)
			return NULL;	// Not defined in this list
		return (const char *)(Data + ofs + list[3+index]);
	}
}

const char *FBehavior::LocalizeString (DWORD index) const
{
	if (Format != ACS_Old)
	{
		DWORD ofs = Localized;
		const char *str = NULL;

		while (ofs != 0 && (str = LookupString (index, ofs)) == NULL)
		{
			ofs = ((DWORD *)(Data + ofs))[2];
		}
		return str;
	}
	else
	{
		return LookupString (index);
	}
}

void FBehavior::PrepLocale (DWORD userpref, DWORD userdef, DWORD syspref, DWORD sysdef)
{
	BYTE *chunk;
	DWORD *list;

	// Clear away any existing links
	for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
	{
		list = (DWORD *)chunk;
		if (list[0] == MAKE_ID('S','T','R','L'))
		{
			list[4] = 0;
		}
	}
	Localized = 0;

	if (userpref)
		AddLanguage (userpref);
	if (userpref & LANGREGIONMASK)
		AddLanguage (userpref & ~LANGREGIONMASK);
	if (userdef)
		AddLanguage (userdef);
	if (userdef & LANGREGIONMASK)
		AddLanguage (userdef & ~LANGREGIONMASK);
	if (syspref)
		AddLanguage (syspref);
	if (syspref & LANGREGIONMASK)
		AddLanguage (syspref & ~LANGREGIONMASK);
	if (sysdef)
		AddLanguage (sysdef);
	if (sysdef & LANGREGIONMASK)
		AddLanguage (sysdef & ~LANGREGIONMASK);
	AddLanguage (MAKE_ID('e','n',0,0));		// Use English as a fallback
	AddLanguage (0);			// Failing that, use language independent strings
}

void FBehavior::AddLanguage (DWORD langid)
{
	DWORD ofs, *ofsput;
	DWORD *list;
	BYTE *chunk;

	// First, make sure language is not already inserted
	ofsput = CheckIfInList (langid);
	if (ofsput == NULL)
	{ // Already in list
		return;
	}

	// Try to find an exact match first
	ofs = FindLanguage (langid, false);
	if (ofs != 0)
	{
		*ofsput = ofs;
		return;
	}

	// If langid has no sublanguage, add all languages that match the major
	// type, if not in list already
	if ((langid & LANGREGIONMASK) == 0)
	{
		for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
		{
			list = (DWORD *)chunk;
			if (list[0] != MAKE_ID('S','T','R','L'))
				continue;	// not a string list
			if ((list[2] & ~LANGREGIONMASK) != langid)
				continue;	// wrong language
			if (list[4] != 0)
				continue;	// definitely in language list
			ofsput = CheckIfInList (list[2]);
			if (ofsput != NULL)
				*ofsput = chunk - Data + 8;	// add to language list
		}
	}
}

DWORD *FBehavior::CheckIfInList (DWORD langid)
{
	DWORD ofs, *ofsput;
	DWORD *list;

	ofs = Localized;
	ofsput = &Localized;
	while (ofs != 0)
	{
		list = (DWORD *)(Data + ofs);
		if (list[0] == langid)
			return NULL;
		ofsput = &list[2];
		ofs = list[2];
	}
	return ofsput;
}

DWORD FBehavior::FindLanguage (DWORD langid, bool ignoreregion) const
{
	BYTE *chunk;
	DWORD *list;
	DWORD langmask;

	langmask = ignoreregion ? ~LANGREGIONMASK : ~0;

	for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
	{
		list = (DWORD *)chunk;
		if (list[0] == MAKE_ID('S','T','R','L') && (list[2] & langmask) == langid)
		{
			return chunk - Data + 8;
		}
	}
	return 0;
}

void FBehavior::StartTypedScripts (WORD type, AActor *activator) const
{
	ScriptPtr *ptr;
	int i;

	for (i = 0; i < NumScripts; ++i)
	{
		ptr = (ScriptPtr *)(Scripts + 8*i);
		if (ptr->Type == type)
		{
			P_GetScriptGoing (activator, NULL, ptr->Number,
				(int *)(ptr->Address + Data), 0, 0, 0, 0, 0, true);
		}
	}
}

//---- The ACS Interpreter ----//

void strbin (char *str);

IMPLEMENT_CLASS (DACSThinker)

DACSThinker *DACSThinker::ActiveThinker = NULL;

DACSThinker::DACSThinker ()
{
	if (ActiveThinker)
	{
		I_Error ("Only one ACSThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		Scripts = NULL;
		LastScript = NULL;
		for (int i = 0; i < 1000; i++)
			RunningScripts[i] = NULL;
	}
}

DACSThinker::~DACSThinker ()
{
	DLevelScript *script = Scripts;
	while (script)
	{
		DLevelScript *next = script->next;
		script->Destroy ();
		script = next;
	}
	Scripts = NULL;
	ActiveThinker = NULL;
}

void DACSThinker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Scripts << LastScript;
	if (arc.IsStoring ())
	{
		WORD i;
		for (i = 0; i < 1000; i++)
		{
			if (RunningScripts[i])
				arc << RunningScripts[i] << i;
		}
		DLevelScript *nil = NULL;
		arc << nil;
	}
	else
	{
		WORD scriptnum;
		DLevelScript *script = NULL;
		arc << script;
		while (script)
		{
			arc << scriptnum;
			RunningScripts[scriptnum] = script;
			arc << script;
		}
	}
}

void DACSThinker::Tick ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}
}

IMPLEMENT_CLASS (DLevelScript)

void *DLevelScript::operator new (size_t size)
{
	return Z_Malloc (sizeof(DLevelScript), PU_LEVACS, 0);
}

void DLevelScript::operator delete (void *block)
{
	Z_Free (block);
}

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);
	arc << next << prev
		<< script
		<< sp
		<< state
		<< statedata
		<< activator
		<< activationline
		<< lineSide;
	for (i = 0; i < LOCAL_SIZE; i++)
		arc << localvars[i];

	if (arc.IsStoring ())
	{
		i = level.behavior->PC2Ofs (pc);
		arc << i;
	}
	else
	{
		arc << i;
		pc = level.behavior->Ofs2PC (i);
	}

	arc << activefont;
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
	activefont = SmallFont;
}

void DLevelScript::Unlink ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		controller->LastScript = prev;
	if (controller->Scripts == this)
		controller->Scripts = next;
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
}

void DLevelScript::Link ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	next = controller->Scripts;
	if (controller->Scripts)
		controller->Scripts->prev = this;
	prev = NULL;
	controller->Scripts = this;
	if (controller->LastScript == NULL)
		controller->LastScript = this;
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	int num1, num2, num3, num4;
	unsigned int num;

	if (max - min > 255)
	{
		num1 = P_Random (pr_acs);
		num2 = P_Random (pr_acs);
		num3 = P_Random (pr_acs);
		num4 = P_Random (pr_acs);

		num = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
	}
	else
	{
		num = P_Random (pr_acs);
	}
	num %= (max - min + 1);
	num += min;
	return (int)num;
}

int DLevelScript::ThingCount (int type, int tid)
{
	AActor *actor;
	const TypeInfo *kind;
	int count = 0;

	if (type >= MAX_SPAWNABLES)
	{
		return 0;
	}
	else if (type > 0)
	{
		kind = SpawnableThings[type];
		if (kind == NULL)
			return 0;
	}
	else
	{
		kind = NULL;
	}
	
	if (tid)
	{
		FActorIterator iterator (tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				count++;
			}
		}
	}
	else
	{
		TThinkerIterator<AActor> iterator;
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				count++;
			}
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	int flat, secnum = -1;
	const char *flatname = level.behavior->LookupString (name);

	if (flatname == NULL)
		return;

	flat = R_FlatNumForName (flatname);

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		if (floorOrCeiling == false)
			sectors[secnum].floorpic = flat;
		else
			sectors[secnum].ceilingpic = flat;
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			count++;
	
	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	int texture, linenum = -1;
	const char *texname = level.behavior->LookupString (name);

	if (texname == NULL)
		return;

	side = !!side;

	texture = R_TextureNumForName (texname);

	while ((linenum = P_FindLineFromID (lineid, linenum)) >= 0)
	{
		side_t *sidedef;

		if (lines[linenum].sidenum[side] == -1)
			continue;
		sidedef = sides + lines[linenum].sidenum[side];

		switch (position)
		{
		case TEXTURE_TOP:
			sidedef->toptexture = texture;
			break;
		case TEXTURE_MIDDLE:
			sidedef->midtexture = texture;
			break;
		case TEXTURE_BOTTOM:
			sidedef->bottomtexture = texture;
			break;
		default:
			break;
		}

	}
}

int DLevelScript::DoSpawn (int type, fixed_t x, fixed_t y, fixed_t z, int tid, int angle)
{
	const char *typestr = level.behavior->LookupString (type);
	if (typestr == NULL)
		return 0;
	char name[64];
	name[0] = 'A';
	name[63] = 0;
	strncpy (name+1, typestr, 62);

	const TypeInfo *info = TypeInfo::FindType (name);
	AActor *actor = NULL;

	if (info != NULL)
	{
		actor = Spawn (info, x, y, z);
		if (actor != NULL)
		{
			if (P_TestMobjLocation (actor))
			{
				actor->angle = angle << 24;
				actor->tid = tid;
				actor->AddToHash ();
				actor->flags |= MF_DROPPED;  // Don't respawn
			}
			else
			{
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return (int)actor;
}

int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle)
{
	FActorIterator iterator (tid);
	AActor *aspot;
	int spawned = 0;

	while ( (aspot = iterator.Next ()) )
	{
		spawned = DoSpawn (type, aspot->x, aspot->y, aspot->z, tid, angle);
	}
	return spawned;
}

void DLevelScript::DoFadeTo (int r, int g, int b, int a, fixed_t time)
{
	DoFadeRange (0, 0, 0, -1, r, g, b, a, time);
}

void DLevelScript::DoFadeRange (int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2, fixed_t time)
{
	player_t *viewer;
	float ftime = (float)time / 65536.f;
	bool fadingFrom = a1 >= 0;
	float fr1, fg1, fb1, fa1;
	float fr2, fg2, fb2, fa2;
	int i;

	fr2 = (float)r2 / 255.f;
	fg2 = (float)g2 / 255.f;
	fb2 = (float)b2 / 255.f;
	fa2 = (float)a2 / 65536.f;

	if (fadingFrom)
	{
		fr1 = (float)r1 / 255.f;
		fg1 = (float)g1 / 255.f;
		fb1 = (float)b1 / 255.f;
		fa1 = (float)a1 / 65536.f;
	}

	if (activator != NULL)
	{
		viewer = activator->player;
		if (viewer == NULL)
			return;
		i = MAXPLAYERS;
		goto showme;
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				viewer = &players[i];
showme:
				if (ftime <= 0.f)
				{
					viewer->BlendR = fr2;
					viewer->BlendG = fg2;
					viewer->BlendB = fb2;
					viewer->BlendA = fa2;
				}
				else
				{
					if (!fadingFrom)
					{
						if (viewer->BlendA <= 0.f)
						{
							fr1 = fr2;
							fg1 = fg2;
							fb1 = fb2;
							fa1 = 0.f;
						}
						else
						{
							fr1 = viewer->BlendR;
							fg1 = viewer->BlendG;
							fb1 = viewer->BlendB;
							fa1 = viewer->BlendA;
						}
					}
					new DFlashFader (fr1, fg1, fb1, fa1, fr2, fg2, fb2, fa2, ftime, viewer->mo);
				}
			}
		}
	}
}

void DLevelScript::DoSetFont (int fontnum)
{
	const char *fontname = level.behavior->LookupString (fontnum);
	activefont = FFont::FindFont (fontname);
	if (activefont == NULL)
	{
		int lump = W_CheckNumForName (fontname);
		if (lump != -1)
		{
			activefont = new FSingleLumpFont (fontname, lump);
		}
		else
		{
			activefont = SmallFont;
		}
	}
	if (screen != NULL)
	{
		screen->SetFont (activefont);
	}
}

#define NEXTWORD	(LONG(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define STACK(a)	(Stack[sp - (a)])
#define PushToStack(a)	(Stack[sp++] = (a))

inline int getbyte (int *&pc)
{
	int res = *(BYTE *)pc;
	pc = (int *)((BYTE *)pc+1);
	return res;
}

void DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	TeleportSide = lineSide;
	int *locals = localvars;
	ScriptFunction *activeFunction = NULL;
	BYTE *translation = 0;

	switch (state)
	{
	case SCRIPT_Delayed:
		// Decrement the delay counter and enter state running
		// if it hits 0
		if (--statedata == 0)
			state = SCRIPT_Running;
		break;

	case SCRIPT_TagWait:
		// Wait for tagged sector(s) to go inactive, then enter
		// state running
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (statedata, secnum)) >= 0)
			if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
				return;
		
		// If we got here, none of the tagged sectors were busy
		state = SCRIPT_Running;
	}
	break;

	case SCRIPT_PolyWait:
		// Wait for polyobj(s) to stop moving, then enter state running
		if (!PO_Busy (statedata))
		{
			state = SCRIPT_Running;
		}
		break;

	case SCRIPT_ScriptWaitPre:
		// Wait for a script to start running, then enter state scriptwait
		if (controller->RunningScripts[statedata])
			state = SCRIPT_ScriptWait;
		break;

	case SCRIPT_ScriptWait:
		// Wait for a script to stop running, then enter state running
		if (controller->RunningScripts[statedata])
			return;

		state = SCRIPT_Running;
		PutFirst ();
		break;

	default:
		break;
	}

	int *pc = this->pc;
	int sp = this->sp;
	const ACSFormat fmt = level.behavior->GetFormat();
	int runaway = 0;	// used to prevent infinite loops
	int pcd;
	char work[4096], *workwhere = work;
	const char *lookup;
	int optstart = -1;
	int temp;

	if (screen != NULL)
	{
		screen->SetFont (activefont);
	}

	while (state == SCRIPT_Running)
	{
		if (++runaway > 500000)
		{
			Printf ("Runaway script %d terminated\n", script);
			state = SCRIPT_PleaseRemove;
			break;
		}

		pcd = NEXTBYTE;
		switch (pcd)
		{
		default:
			Printf ("Unknown P-Code %d in script %d\n", pcd, script);
			// fall through
		case PCD_TERMINATE:
			state = SCRIPT_PleaseRemove;
			break;

		case PCD_NOP:
			break;

		case PCD_SUSPEND:
			state = SCRIPT_Suspended;
			break;

		case PCD_PUSHNUMBER:
			PushToStack (NEXTWORD);
			break;

		case PCD_PUSHBYTE:
			PushToStack (*(BYTE *)pc);
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_PUSH2BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			sp += 2;
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_PUSH3BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			sp += 3;
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_PUSH4BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			sp += 4;
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_PUSH5BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			Stack[sp+4] = ((BYTE *)pc)[4];
			sp += 5;
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_PUSHBYTES:
			temp = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + temp + 1);
			for (temp = -temp; temp; temp++)
			{
				PushToStack (*((BYTE *)pc + temp));
			}
			break;

		case PCD_DUP:
			Stack[sp] = Stack[sp-1];
			sp++;
			break;

		case PCD_SWAP:
			swap(Stack[sp-2], Stack[sp-1]);
			break;

		case PCD_LSPEC1:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(1), 0, 0, 0, 0);
			sp -= 1;
			break;

		case PCD_LSPEC2:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(2), STACK(1), 0, 0, 0);
			sp -= 2;
			break;

		case PCD_LSPEC3:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(3), STACK(2), STACK(1), 0, 0);
			sp -= 3;
			break;

		case PCD_LSPEC4:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(4), STACK(3), STACK(2),
									STACK(1), 0);
			sp -= 4;
			break;

		case PCD_LSPEC5:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(5), STACK(4), STACK(3),
									STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_LSPEC1DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], 0, 0, 0, 0);
			pc += 1;
			break;

		case PCD_LSPEC2DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], 0, 0, 0);
			pc += 2;
			break;

		case PCD_LSPEC3DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], 0, 0);
			pc += 3;
			break;

		case PCD_LSPEC4DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], pc[3], 0);
			pc += 4;
			break;

		case PCD_LSPEC5DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], pc[3], pc[4]);
			pc += 5;
			break;

		case PCD_LSPEC1DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], 0, 0, 0, 0);
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_LSPEC2DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], 0, 0, 0);
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_LSPEC3DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3], 0, 0);
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_LSPEC4DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], 0);
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_LSPEC5DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], ((BYTE *)pc)[5]);
			pc = (int *)((BYTE *)pc + 6);
			break;

		case PCD_CALL:
		case PCD_CALLDISCARD:
			{
				int funcnum;
				int i;
				ScriptFunction *func;

				funcnum = NEXTBYTE;
				func = level.behavior->GetFunction (funcnum);
				if (func == NULL)
				{
					Printf ("Function %d in script %d out of range\n", funcnum, script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				if (sp + func->LocalCount + 32 > STACK_SIZE)
				{ // 32 is the margin for the function's working space
					Printf ("Out of stack space in script %d\n", script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				// The function's first argument is also its first local variable.
				locals = &Stack[sp - func->ArgCount];
				// Make space on the stack for any other variables the function uses.
				for (i = 0; i < func->LocalCount; ++i)
				{
					Stack[sp+i] = 0;
				}
				sp += i;
				((CallReturn *)&Stack[sp])->ReturnAddress = level.behavior->PC2Ofs (pc);
				((CallReturn *)&Stack[sp])->ReturnFunction = activeFunction;
				((CallReturn *)&Stack[sp])->bDiscardResult = (pcd == PCD_CALLDISCARD);
				sp += sizeof(CallReturn)/sizeof(int);
				pc = level.behavior->Ofs2PC (func->Address);
				activeFunction = func;
			}
			break;

		case PCD_RETURNVOID:
		case PCD_RETURNVAL:
			{
				int value;
				CallReturn *retState;

				if (pcd == PCD_RETURNVAL)
				{
					value = Stack[--sp];
				}
				else
				{
					value = 0;
				}
				sp -= sizeof(CallReturn)/sizeof(int);
				retState = (CallReturn *)&Stack[sp];
				pc = level.behavior->Ofs2PC (retState->ReturnAddress);
				sp -= activeFunction->ArgCount + activeFunction->LocalCount;
				activeFunction = retState->ReturnFunction;
				if (activeFunction == NULL)
				{
					locals = localvars;
				}
				else
				{
					locals = &Stack[sp - activeFunction->ArgCount - activeFunction->LocalCount];
				}
				if (!retState->bDiscardResult)
				{
					Stack[sp++] = value;
				}
			}
			break;

		case PCD_ADD:
			STACK(2) = STACK(2) + STACK(1);
			sp--;
			break;

		case PCD_SUBTRACT:
			STACK(2) = STACK(2) - STACK(1);
			sp--;
			break;

		case PCD_MULTIPLY:
			STACK(2) = STACK(2) * STACK(1);
			sp--;
			break;

		case PCD_DIVIDE:
			STACK(2) = STACK(2) / STACK(1);
			sp--;
			break;

		case PCD_MODULUS:
			STACK(2) = STACK(2) % STACK(1);
			sp--;
			break;

		case PCD_EQ:
			STACK(2) = (STACK(2) == STACK(1));
			sp--;
			break;

		case PCD_NE:
			STACK(2) = (STACK(2) != STACK(1));
			sp--;
			break;

		case PCD_LT:
			STACK(2) = (STACK(2) < STACK(1));
			sp--;
			break;

		case PCD_GT:
			STACK(2) = (STACK(2) > STACK(1));
			sp--;
			break;

		case PCD_LE:
			STACK(2) = (STACK(2) <= STACK(1));
			sp--;
			break;

		case PCD_GE:
			STACK(2) = (STACK(2) >= STACK(1));
			sp--;
			break;

		case PCD_ASSIGNSCRIPTVAR:
			locals[NEXTBYTE] = STACK(1);
			sp--;
			break;


		case PCD_ASSIGNMAPVAR:
			level.vars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNWORLDVAR:
			ACS_WorldVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNMAPARRAY:
			level.behavior->SetArrayVal (ACS_WorldVars[NEXTBYTE], STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_PUSHSCRIPTVAR:
			PushToStack (locals[NEXTBYTE]);
			break;

		case PCD_PUSHMAPVAR:
			PushToStack (level.vars[NEXTBYTE]);
			break;

		case PCD_PUSHWORLDVAR:
			PushToStack (ACS_WorldVars[NEXTBYTE]);
			break;

		case PCD_PUSHGLOBALVAR:
			PushToStack (ACS_GlobalVars[NEXTBYTE]);
			break;

		case PCD_PUSHMAPARRAY:
			STACK(1) = level.behavior->GetArrayVal (level.vars[NEXTBYTE], STACK(1));
			break;

		case PCD_ADDSCRIPTVAR:
			locals[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPVAR:
			level.vars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBSCRIPTVAR:
			locals[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPVAR:
			level.vars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBWORLDVAR:
			ACS_WorldVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULSCRIPTVAR:
			locals[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPVAR:
			level.vars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULWORLDVAR:
			ACS_WorldVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVSCRIPTVAR:
			locals[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVMAPVAR:
			level.vars[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVWORLDVAR:
			ACS_WorldVars[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODSCRIPTVAR:
			locals[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODMAPVAR:
			level.vars[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODWORLDVAR:
			ACS_WorldVars[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_INCSCRIPTVAR:
			++locals[NEXTBYTE];
			break;

		case PCD_INCMAPVAR:
			++level.vars[NEXTBYTE];
			break;

		case PCD_INCWORLDVAR:
			++ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_INCGLOBALVAR:
			++ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_INCMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) + 1);
				sp--;
			}
			break;

		case PCD_DECSCRIPTVAR:
			--locals[NEXTBYTE];
			break;

		case PCD_DECMAPVAR:
			--level.vars[NEXTBYTE];
			break;

		case PCD_DECWORLDVAR:
			--ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_DECGLOBALVAR:
			--ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_DECMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				level.behavior->SetArrayVal (a, i,
					level.behavior->GetArrayVal (a, i) - 1);
				sp--;
			}
			break;

		case PCD_GOTO:
			pc = level.behavior->Ofs2PC (*pc);
			break;

		case PCD_IFGOTO:
			if (STACK(1))
				pc = level.behavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_DROP:
			sp--;
			break;

		case PCD_DELAY:
			state = SCRIPT_Delayed;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_DELAYDIRECT:
			state = SCRIPT_Delayed;
			statedata = NEXTWORD;
			break;

		case PCD_DELAYDIRECTB:
			state = SCRIPT_Delayed;
			statedata = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_RANDOM:
			STACK(2) = Random (STACK(2), STACK(1));
			sp--;
			break;
			
		case PCD_RANDOMDIRECT:
			PushToStack (Random (pc[0], pc[1]));
			pc += 2;
			break;

		case PCD_RANDOMDIRECTB:
			PushToStack (Random (((BYTE *)pc)[0], ((BYTE *)pc)[1]));
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_THINGCOUNT:
			STACK(2) = ThingCount (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_THINGCOUNTDIRECT:
			PushToStack (ThingCount (pc[0], pc[1]));
			pc += 2;
			break;

		case PCD_TAGWAIT:
			state = SCRIPT_TagWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_TAGWAITDIRECT:
			state = SCRIPT_TagWait;
			statedata = NEXTWORD;
			break;

		case PCD_POLYWAIT:
			state = SCRIPT_PolyWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_POLYWAITDIRECT:
			state = SCRIPT_PolyWait;
			statedata = NEXTWORD;
			break;

		case PCD_CHANGEFLOOR:
			ChangeFlat (STACK(2), STACK(1), 0);
			sp -= 2;
			break;

		case PCD_CHANGEFLOORDIRECT:
			ChangeFlat (pc[0], pc[1], 0);
			pc += 2;
			break;

		case PCD_CHANGECEILING:
			ChangeFlat (STACK(2), STACK(1), 1);
			sp -= 2;
			break;

		case PCD_CHANGECEILINGDIRECT:
			ChangeFlat (pc[0], pc[1], 1);
			pc += 2;
			break;

		case PCD_RESTART:
			pc = level.behavior->FindScript (script);
			break;

		case PCD_ANDLOGICAL:
			STACK(2) = (STACK(2) && STACK(1));
			sp--;
			break;

		case PCD_ORLOGICAL:
			STACK(2) = (STACK(2) || STACK(1));
			sp--;
			break;

		case PCD_ANDBITWISE:
			STACK(2) = (STACK(2) & STACK(1));
			sp--;
			break;

		case PCD_ORBITWISE:
			STACK(2) = (STACK(2) | STACK(1));
			sp--;
			break;

		case PCD_EORBITWISE:
			STACK(2) = (STACK(2) ^ STACK(1));
			sp--;
			break;

		case PCD_NEGATELOGICAL:
			STACK(1) = !STACK(1);
			break;

		case PCD_LSHIFT:
			STACK(2) = (STACK(2) << STACK(1));
			sp--;
			break;

		case PCD_RSHIFT:
			STACK(2) = (STACK(2) >> STACK(1));
			sp--;
			break;

		case PCD_UNARYMINUS:
			STACK(1) = -STACK(1);
			break;

		case PCD_IFNOTGOTO:
			if (!STACK(1))
				pc = level.behavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_LINESIDE:
			PushToStack (lineSide);
			break;

		case PCD_SCRIPTWAIT:
			statedata = STACK(1);
			if (controller->RunningScripts[statedata])
				state = SCRIPT_ScriptWait;
			else
				state = SCRIPT_ScriptWaitPre;
			sp--;
			PutLast ();
			break;

		case PCD_SCRIPTWAITDIRECT:
			state = SCRIPT_ScriptWait;
			statedata = NEXTWORD;
			PutLast ();
			break;

		case PCD_CLEARLINESPECIAL:
			if (activationline)
				activationline->special = 0;
			break;

		case PCD_CASEGOTO:
			if (STACK(1) == NEXTWORD)
			{
				pc = level.behavior->Ofs2PC (*pc);
				sp--;
			}
			else
			{
				pc++;
			}
			break;

		case PCD_BEGINPRINT:
			workwhere = work;
			work[0] = 0;
			break;

		case PCD_PRINTSTRING:
		case PCD_PRINTLOCALIZED:
			lookup = (pcd == PCD_PRINTSTRING ?
				level.behavior->LookupString (STACK(1)) :
				level.behavior->LocalizeString (STACK(1)));
			if (lookup != NULL)
			{
				workwhere += sprintf (workwhere, "%s", lookup);
			}
			--sp;
			break;

		case PCD_PRINTNUMBER:
			workwhere += sprintf (workwhere, "%d", STACK(1));
			--sp;
			break;

		case PCD_PRINTCHARACTER:
			workwhere[0] = STACK(1);
			workwhere[1] = 0;
			workwhere++;
			--sp;
			break;

		case PCD_PRINTFIXED:
			workwhere += sprintf (workwhere, "%g", FIXED2FLOAT(STACK(1)));
			--sp;
			break;

		// [BC] Print activator's name
		// [RH] Fancied up a bit
		case PCD_PRINTNAME:
			{
				player_t *player = NULL;

				if (STACK(1) == 0 || (unsigned)STACK(1) > MAXPLAYERS)
				{
					if (activator)
					{
						player = activator->player;
					}
				}
				else if (playeringame[STACK(1)])
				{
					player = &players[STACK(1)];
				}
				else
				{
					workwhere += sprintf (workwhere, "Player %d\n",
						STACK(1));
					sp--;
					break;
				}
				if (player)
				{
					workwhere += sprintf (workwhere, "%s",
						activator->player->userinfo.netname);
				}
				else if (activator)
				{
					workwhere += sprintf (workwhere, "%s",
						RUNTIME_TYPE(activator)->Name+1);
				}
				else
				{
					workwhere += sprintf (workwhere, " ");
				}
				sp--;
			}
			break;

		case PCD_ENDPRINT:
		case PCD_ENDPRINTBOLD:
		case PCD_MOREHUDMESSAGE:
			strbin (work);
			if (pcd != PCD_MOREHUDMESSAGE)
			{
				if (pcd == PCD_ENDPRINTBOLD || activator == NULL ||
					(activator->player - players == consoleplayer))
				{
					strbin (work);
					C_MidPrint (work);
				}
			}
			else
			{
				optstart = -1;
			}
			break;

		case PCD_OPTHUDMESSAGE:
			optstart = sp;
			break;

		case PCD_ENDHUDMESSAGE:
		case PCD_ENDHUDMESSAGEBOLD:
			if (optstart == -1)
			{
				optstart = sp;
			}
			if (pcd == PCD_ENDHUDMESSAGEBOLD || activator == NULL ||
				(activator->player - players == consoleplayer))
			{
				int type = Stack[optstart-6];
				int id = Stack[optstart-5];
				EColorRange color = CLAMPCOLOR(Stack[optstart-4]);
				float x = FIXED2FLOAT(Stack[optstart-3]);
				float y = FIXED2FLOAT(Stack[optstart-2]);
				float holdTime = FIXED2FLOAT(Stack[optstart-1]);
				DHUDMessage *msg;

				switch (type)
				{
				default:	// normal
					msg = new DHUDMessage (work, x, y, color, holdTime);
					break;
				case 1:		// fade out
					{
						float fadeTime = (optstart < sp) ?
							FIXED2FLOAT(Stack[optstart]) : 0.5f;
						msg = new DHUDMessageFadeOut (work, x, y, color, holdTime, fadeTime);
					}
					break;
				case 2:		// type on, then fade out
					{
						float typeTime = (optstart < sp) ?
							FIXED2FLOAT(Stack[optstart]) : 0.05f;
						float fadeTime = (optstart < sp-1) ?
							FIXED2FLOAT(Stack[optstart+1]) : 0.5f;
						msg = new DHUDMessageTypeOnFadeOut (work, x, y, color, typeTime, holdTime, fadeTime);
					}
					break;
				}
				StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0);
			}
			sp = optstart-6;
			break;

		case PCD_SETFONT:
			DoSetFont (STACK(1));
			sp--;
			break;

		case PCD_SETFONTDIRECT:
			DoSetFont (pc[0]);
			pc++;
			break;

		case PCD_PLAYERCOUNT:
			PushToStack (CountPlayers ());
			break;

		case PCD_GAMETYPE:
			if (deathmatch)
				PushToStack (GAME_NET_DEATHMATCH);
			else if (multiplayer)
				PushToStack (GAME_NET_COOPERATIVE);
			else
				PushToStack (GAME_SINGLE_PLAYER);
			break;

		case PCD_GAMESKILL:
			PushToStack (gameskill);
			break;

// [BC] Start ST PCD's
		case PCD_PLAYERHEALTH:
			if (activator)
				PushToStack (activator->health);
			break;

		case PCD_PLAYERARMORPOINTS:
			if (activator && activator->player)
				PushToStack (activator->player->armorpoints[0]);
			break;

		case PCD_PLAYERFRAGS:
			if (activator && activator->player)
				PushToStack (activator->player->fragcount);
			break;

		case PCD_MUSICCHANGE:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL)
			{
				S_ChangeMusic (lookup, STACK(1));
			}
			sp -= 2;
			break;

		case PCD_SINGLEPLAYER:
			PushToStack (!netgame);
			break;
// [BC] End ST PCD's

		case PCD_TIMER:
			PushToStack (level.time);
			break;

		case PCD_SECTORSOUND:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activationline)
				{
					S_Sound (
						activationline->frontsector->soundorg,
						CHAN_BODY,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
				else
				{
					S_Sound (
						CHAN_BODY,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
			}
			sp -= 2;
			break;

		case PCD_AMBIENTSOUND:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_LOCALAMBIENTSOUND:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL && players[consoleplayer].camera == activator)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_ACTIVATORSOUND:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (activator, CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NORM);
			}
			sp -= 2;
			break;

		case PCD_SOUNDSEQUENCE:
			lookup = level.behavior->LookupString (STACK(1));
			if (lookup != NULL)
			{
				if (activationline)
				{
					SN_StartSequence (
						activationline->frontsector,
						lookup);
				}
			}
			sp--;
			break;

		case PCD_SETLINETEXTURE:
			SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_SETLINEBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					switch (STACK(1))
					{
					case BLOCK_NOTHING:
						lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
						break;
					case BLOCK_CREATURES:
					default:
						lines[line].flags &= ~ML_BLOCKEVERYTHING;
						lines[line].flags |= ML_BLOCKING;
						break;
					case BLOCK_EVERYTHING:
						lines[line].flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
						break;
					}
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINEMONSTERBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					if (STACK(1))
						lines[line].flags |= ML_BLOCKMONSTERS;
					else
						lines[line].flags &= ~ML_BLOCKMONSTERS;
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINESPECIAL:
			{
				int linenum = -1;

				while ((linenum = P_FindLineFromID (STACK(7), linenum)) >= 0) {
					line_t *line = &lines[linenum];

					line->special = STACK(6);
					line->args[0] = STACK(5);
					line->args[1] = STACK(4);
					line->args[2] = STACK(3);
					line->args[3] = STACK(2);
					line->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_SETTHINGSPECIAL:
			{
				FActorIterator iterator (STACK(7));
				AActor *actor;

				while ( (actor = iterator.Next ()) )
				{
					actor->special = STACK(6);
					actor->args[0] = STACK(5);
					actor->args[1] = STACK(4);
					actor->args[2] = STACK(3);
					actor->args[3] = STACK(2);
					actor->args[4] = STACK(1);
				}
				sp -= 7;
			}

		case PCD_THINGSOUND:
			lookup = level.behavior->LookupString (STACK(2));
			if (lookup != NULL)
			{
				FActorIterator iterator (STACK(3));
				AActor *spot;

				while ( (spot = iterator.Next ()) )
				{
					S_Sound (spot, CHAN_BODY,
							 lookup,
							 (float)(STACK(1))/127.f, ATTN_NORM);
				}
			}
			sp -= 3;
			break;

		case PCD_FIXEDMUL:
			STACK(2) = FixedMul (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_FIXEDDIV:
			STACK(2) = FixedDiv (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITY:
			level.gravity = (float)STACK(1) / 65536.f;
			sp--;
			break;

		case PCD_SETGRAVITYDIRECT:
			level.gravity = (float)pc[0] / 65536.f;
			pc++;
			break;

		case PCD_SETAIRCONTROL:
			level.aircontrol = STACK(1);
			sp--;
			G_AirControlChanged ();
			break;

		case PCD_SETAIRCONTROLDIRECT:
			level.aircontrol = pc[0];
			pc++;
			G_AirControlChanged ();
			break;

		case PCD_SPAWN:
			STACK(6) = DoSpawn (STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_SPAWNDIRECT:
			PushToStack (DoSpawn (pc[0], pc[1], pc[2], pc[3], pc[4], pc[5]));
			pc += 6;
			break;

		case PCD_SPAWNSPOT:
			STACK(4) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_SPAWNSPOTDIRECT:
			PushToStack (DoSpawnSpot (pc[0], pc[1], pc[2], pc[3]));
			pc += 4;
			break;

		case PCD_CLEARINVENTORY:
			ClearInventory (activator);
			break;

		case PCD_GIVEINVENTORY:
			GiveInventory (activator, level.behavior->LookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_GIVEINVENTORYDIRECT:
			GiveInventory (activator, level.behavior->LookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_TAKEINVENTORY:
			TakeInventory (activator, level.behavior->LookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_TAKEINVENTORYDIRECT:
			TakeInventory (activator, level.behavior->LookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_CHECKINVENTORY:
			STACK(1) = CheckInventory (activator, level.behavior->LookupString (STACK(1)));
			break;

		case PCD_CHECKINVENTORYDIRECT:
			PushToStack (CheckInventory (activator, level.behavior->LookupString (pc[0])));
			pc += 1;
			break;

		case PCD_SETMUSIC:
			S_ChangeMusic (level.behavior->LookupString (STACK(3)), STACK(2));
			sp -= 3;
			break;

		case PCD_SETMUSICDIRECT:
			S_ChangeMusic (level.behavior->LookupString (pc[0]), pc[1]);
			pc += 3;
			break;

		case PCD_LOCALSETMUSIC:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (level.behavior->LookupString (STACK(3)), STACK(2));
			}
			sp -= 3;
			break;

		case PCD_LOCALSETMUSICDIRECT:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (level.behavior->LookupString (pc[0]), pc[1]);
			}
			pc += 3;
			break;

		case PCD_FADETO:
			DoFadeTo (STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_FADERANGE:
			DoFadeRange (STACK(9), STACK(8), STACK(7), STACK(6),
						 STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 9;
			break;

		case PCD_CANCELFADE:
			{
				TThinkerIterator<DFlashFader> iterator;
				DFlashFader *fader;

				while ( (fader = iterator.Next()) )
				{
					if (activator == NULL || fader->WhoFor() == activator)
					{
						fader->Cancel ();
					}
				}
			}
			break;

		case PCD_PLAYMOVIE:
			STACK(1) = I_PlayMovie (level.behavior->LookupString (STACK(1)));
			break;

		case PCD_GETACTORX:
		case PCD_GETACTORY:
		case PCD_GETACTORZ:
			{
				AActor *actor;

				if (STACK(1) == 0)
				{
					actor = activator;
				}
				else
				{
					FActorIterator iterator (STACK(1));
					actor = iterator.Next ();
				}
				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = (&actor->x)[pcd - PCD_GETACTORX];
				}
			}
			break;

		case PCD_SETFLOORTRIGGER:
			new DPlaneWatcher (activator, activationline, lineSide, false, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_SETCEILINGTRIGGER:
			new DPlaneWatcher (activator, activationline, lineSide, true, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_STARTTRANSLATION:
			{
				int i = STACK(1);
				sp--;
				if (i >= 1 && i <= MAX_ACS_TRANSLATIONS)
				{
					translation = &translationtables[TRANSLATION_LevelScripted][i*256-256];
					for (i = 0; i < 256; ++i)
					{
						translation[i] = i;
					}
				}
			}
			break;

		case PCD_TRANSLATIONRANGE1:
			{ // translation using palette shifting
				int start = STACK(4);
				int end = STACK(3);
				int pal1 = STACK(2);
				int pal2 = STACK(1);
				fixed_t palcol, palstep;
				sp -= 4;

				if (translation == NULL)
				{
					break;
				}
				if (start > end)
				{
					swap (start, end);
					swap (pal1, pal2);
				}
				else if (start == end)
				{
					translation[start] = pal1;
					break;
				}
				palcol = pal1 << FRACBITS;
				palstep = ((pal2 << FRACBITS) - palcol) / (end - start);
				for (int i = start; i <= end; palcol += palstep, ++i)
				{
					translation[i] = palcol >> FRACBITS;
				}
			}
			break;

		case PCD_TRANSLATIONRANGE2:
			{ // translation using RGB values
			  // (would HSV be a good idea too?)
				int start = STACK(8);
				int end = STACK(7);
				fixed_t r1 = STACK(6) << FRACBITS;
				fixed_t g1 = STACK(5) << FRACBITS;
				fixed_t b1 = STACK(4) << FRACBITS;
				fixed_t r2 = STACK(3) << FRACBITS;
				fixed_t g2 = STACK(2) << FRACBITS;
				fixed_t b2 = STACK(1) << FRACBITS;
				fixed_t r, g, b;
				fixed_t rs, gs, bs;
				sp -= 8;

				if (translation == NULL)
				{
					break;
				}
				if (start > end)
				{
					swap (start, end);
					r = r2;
					g = g2;
					b = b2;
					rs = r1 - r2;
					gs = g1 - g2;
					bs = b1 - b2;
				}
				else
				{
					r = r1;
					g = g1;
					b = b1;
					rs = r2 - r1;
					gs = g2 - g1;
					bs = b2 - b1;
				}
				if (start == end)
				{
					translation[start] = ColorMatcher.Pick
						(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
					break;
				}
				rs /= (end - start);
				gs /= (end - start);
				bs /= (end - start);
				for (int i = start; i <= end; ++i)
				{
					translation[i] = ColorMatcher.Pick
						(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
					r += rs;
					g += gs;
					b += bs;
				}
			}
			break;

		case PCD_ENDTRANSLATION:
			// This might be useful for hardware rendering, but
			// for software it is superfluous.
			translation = NULL;
			break;

		case PCD_SIN:
			STACK(1) = finesine[(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_COS:
			STACK(1) = finecosine[(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_VECTORANGLE:
			STACK(2) = R_PointToAngle2 (0, 0, STACK(2), STACK(1)) >> 16;
			sp--;
			break;

		case PCD_CHECKWEAPON:
			if (activator == NULL || activator->player == NULL)
			{ // Non-players do not have ready weapons
				STACK(1) = 0;
			}
			else
			{
				STACK(1) = 0 == strcmp (level.behavior->LookupString (STACK(1)),
					wpnlev1info[activator->player->readyweapon]->type->Name+1);
			}
			break;

		case PCD_SETWEAPON:
			if (activator == NULL || activator->player == NULL)
			{
				STACK(1) = 0;
			}
			else
			{
				int i;

				for (i = 0; i < NUMWEAPONS; ++i)
				{
					if (0 == strcmp (level.behavior->LookupString (STACK(1)),
						wpnlev1info[i]->type->Name+1))
					{
						break;
					}
				}
				if (i >= NUMWEAPONS || !activator->player->weaponowned[i])
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = 1;
					if (activator->player->readyweapon != i)
					{
						activator->player->pendingweapon = (weapontype_t)i;
					}
				}
			}
			break;
		}
	}

	this->pc = pc;
	this->sp = sp;

	if (screen != NULL)
	{
		screen->SetFont (SmallFont);
	}

	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		if (controller->RunningScripts[script] == this)
			controller->RunningScripts[script] = NULL;
		this->Destroy ();
	}
}

#undef PushtoStack

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller && !always && controller->RunningScripts[num])
	{
		if (controller->RunningScripts[num]->GetState () == DLevelScript::SCRIPT_Suspended)
		{
			controller->RunningScripts[num]->SetState (DLevelScript::SCRIPT_Running);
			return true;
		}
		return false;
	}

	new DLevelScript (who, where, num, code, lineSide, arg0, arg1, arg2, always, delay);

	return true;
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, int *code, int lineside,
							int arg0, int arg1, int arg2, int always, bool delay)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	sp = 0;
	localvars[0] = arg0;
	localvars[1] = arg1;
	localvars[2] = arg2;
	memset (localvars+3, 0, sizeof(localvars)-3*sizeof(int));
	pc = code;
	activator = who;
	activationline = where;
	lineSide = lineside;
	activefont = SmallFont;
	if (delay) {
		// From Hexen: Give the world some time to set itself up before
		// running open scripts.
		//script->state = SCRIPT_Delayed;
		//script->statedata = TICRATE;
		state = SCRIPT_Running;
	} else {
		state = SCRIPT_Running;
	}

	if (!always)
		DACSThinker::ActiveThinker->RunningScripts[num] = this;

	Link ();

	DPrintf ("Script %d started.\n", num);
}

static void SetScriptState (int script, DLevelScript::EScriptState state)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->RunningScripts[script])
		controller->RunningScripts[script]->SetState (state);
}

void P_DoDeferedScripts (void)
{
	acsdefered_t *def;
	int *scriptdata;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = level.behavior->FindScript (def->script);
			if (scriptdata)
			{
				P_GetScriptGoing ((unsigned)def->playernum < MAXPLAYERS &&
					playeringame[def->playernum] ? players[def->playernum].mo : NULL,
					NULL, def->script,
					scriptdata,
					0, def->arg0, def->arg1, def->arg2,
					def->type == acsdefered_t::defexealways, true);
			} else
				Printf ("P_DoDeferredScripts: Unknown script %d\n", def->script);
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf ("Defered suspend of script %d\n", def->script);
			break;

		case acsdefered_t::defterminate:
			SetScriptState (def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf ("Defered terminate of script %d\n", def->script);
			break;
		}
		delete def;
		def = next;
	}
	level.info->defered = NULL;
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, int arg0, int arg1, int arg2, AActor *who)
{
	if (i)
	{
		acsdefered_t *def = new acsdefered_s;

		def->next = i->defered;
		def->type = type;
		def->script = script;
		def->arg0 = arg0;
		def->arg1 = arg1;
		def->arg2 = arg2;
		if (who != NULL && who->player != NULL)
		{
			def->playernum = who->player - players;
		}
		else
		{
			def->playernum = -1;
		}
		i->defered = def;
		DPrintf ("Script %d on map %s defered\n", script, i->mapname);
	}
}

bool P_StartScript (AActor *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always)
{
	if (!strnicmp (level.mapname, map, 8))
	{
		int *scriptdata;

		if (level.behavior != NULL &&
			(scriptdata = level.behavior->FindScript (script)) != NULL)
		{
			return P_GetScriptGoing (who, where, script,
									 scriptdata,
									 lineSide, arg0, arg1, arg2, always, false);
		}
		else
		{
			Printf ("P_StartScript: Unknown script %d\n", script);
		}
	}
	else
	{
		addDefered (FindLevelInfo (map),
					always ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, arg0, arg1, arg2, who);
	}
	return false;
}

void P_SuspendScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_PleaseRemove);
}

void strbin (char *str)
{
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'c':
					*str++ = TEXTCOLOR_ESCAPE;
					break;
				case 'n':
					*str++ = '\n';
					break;
				case 't':
					*str++ = '\t';
					break;
				case 'r':
					*str++ = '\r';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
}

FArchive &operator<< (FArchive &arc, acsdefered_s *&defertop)
{
	BYTE more;

	if (arc.IsStoring ())
	{
		acsdefered_s *defer = defertop;
		more = 1;
		while (defer)
		{
			BYTE type;
			arc << more;
			type = (BYTE)defer->type;
			arc << type << defer->script << defer->playernum
				<< defer->arg0 << defer->arg1 << defer->arg2;
			defer = defer->next;
		}
		more = 0;
		arc << more;
	}
	else
	{
		acsdefered_s **defer = &defertop;

		arc << more;
		while (more)
		{
			*defer = new acsdefered_s;
			arc << more;
			(*defer)->type = (acsdefered_s::EType)more;
			arc << (*defer)->script << (*defer)->playernum
				<< (*defer)->arg0 << (*defer)->arg1 << (*defer)->arg2;
			defer = &((*defer)->next);
			arc << more;
		}
		*defer = NULL;
	}
	return arc;
}

CCMD (scriptstat)
{
	if (DACSThinker::ActiveThinker == NULL)
	{
		Printf ("No scripts are running.\n");
	}
	else
	{
		DACSThinker::ActiveThinker->DumpScriptStatus ();
	}
}

void DACSThinker::DumpScriptStatus ()
{
	static const char *stateNames[] =
	{
		"Running",
		"Suspended",
		"Delayed",
		"TagWait",
		"PolyWait",
		"ScriptWaitPre",
		"ScriptWait",
		"PleaseRemove"
	};
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		Printf ("%d: %s\n", script->script, stateNames[script->state]);
		script = script->next;
	}
}
