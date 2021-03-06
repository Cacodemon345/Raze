//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include "ns.h"	// Must come before everything else!

#include "duke3d.h"
#include "premap.h"
#include "prlights.h"
#include "savegame.h"
#include "i_specialpaths.h"
#include "gamecontrol.h"
#include "version.h"
#include "z_music.h"
#include "mapinfo.h"

#include "savegamehelp.h"
BEGIN_RR_NS


// For storing pointers in files.
//  back_p==0: ptr -> "small int"
//  back_p==1: "small int" -> ptr
//
//  mode: see enum in savegame.h
void G_Util_PtrToIdx(void *ptr, int32_t const count, const void *base, int32_t const mode)
{
    intptr_t *iptr = (intptr_t *)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode&P2I_ONLYNON0_BIT;

    // TODO: convert to proper offsets/indices for (a step towards) cross-
    //       compatibility between 32- and 64-bit systems in the netplay.
    //       REMEMBER to bump BYTEVERSION then.

    // WARNING: C std doesn't say that bit pattern of NULL is necessarily 0!
    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] -= ibase;
    }
    else
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] += ibase;
    }
}

void G_Util_PtrToIdx2(void *ptr, int32_t const count, size_t const stride, const void *base, int32_t const mode)
{
    uint8_t *iptr = (uint8_t *)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode&P2I_ONLYNON0_BIT;

    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t *)iptr)
                *(intptr_t *)iptr -= ibase;

            iptr += stride;
        }
    }
    else
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t *)iptr)
                *(intptr_t *)iptr += ibase;

            iptr += stride;
        }
    }
}

// TODO: sync with TROR special interpolations? (e.g. upper floor of subway)
void G_ResetInterpolations(void)
{
    int32_t k, i;

    g_interpolationCnt = 0;

    k = headspritestat[STAT_EFFECTOR];
    while (k >= 0)
    {
        switch (sprite[k].lotag)
        {
        case SE_31_FLOOR_RISE_FALL:
            G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
            break;
        case SE_32_CEILING_RISE_FALL:
            G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case SE_17_WARP_ELEVATOR:
        case SE_25_PISTON:
            G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
            G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case SE_0_ROTATING_SECTOR:
        case SE_5:
        case SE_6_SUBWAY:
        case SE_11_SWINGING_DOOR:
        case SE_14_SUBWAY_CAR:
        case SE_15_SLIDING_DOOR:
        case SE_16_REACTOR:
        case SE_26:
        case SE_30_TWO_WAY_TRAIN:
            Sect_SetInterpolation(sprite[k].sectnum);
            break;
        }

        k = nextspritestat[k];
    }

    for (i=g_interpolationCnt-1; i>=0; i--) bakipos[i] = *curipos[i];
    for (i = g_animateCnt-1; i>=0; i--)
        G_SetInterpolation(g_animatePtr[i]);
}


static FileReader *OpenSavegame()
{
	auto file = ReadSavegameChunk("snapshot.dat");
	if (!file.isOpen())
	{
		FinishSavegameRead();
		return nullptr;
	}
	return new FileReader(std::move(file));
}


static void sv_postudload();


// XXX: keyboard input 'blocked' after load fail? (at least ESC?)
int32_t G_LoadPlayer(const char *path)
{
    auto fil = OpenSavegame();

    if (!fil)
        return -1;

    ready2send = 0;

    savehead_t h;
    int status = sv_loadheader(*fil, 0, &h);

    if (status < 0 || h.numplayers != ud.multimode)
    {
        if (status == -4 || status == -3 || status == 1)
            P_DoQuote(QUOTE_SAVE_BAD_VERSION, g_player[myconnectindex].ps);
        else if (h.numplayers != ud.multimode)
            P_DoQuote(QUOTE_SAVE_BAD_PLAYERS, g_player[myconnectindex].ps);

        ototalclock = totalclock;
        ready2send = 1;

		delete fil;
		FinishSavegameRead();
		return 1;
    }

    // some setup first
    ud.multimode = h.numplayers;
	S_PauseSounds(true);

    if (numplayers > 1)
    {
        pub = NUMPAGES;
        pus = NUMPAGES;
        G_UpdateScreenArea();
        G_DrawBackground();
        menutext_center(100, "Loading...");
        videoNextPage();
    }

    Net_WaitForEverybody();

    FX_StopAllSounds();
    S_ClearSoundLocks();

    // non-"m_" fields will be loaded from svgm_udnetw
    ud.m_volume_number = h.volnum;
    m_level_number = h.levnum;
    ud.m_player_skill = h.skill;

    // NOTE: Bmemcpy needed for SAVEGAME_MUSIC.
    strcpy(boardfilename, currentLevel->fileName);

	char workbuffer[BMAX_PATH];
	Bstrcpy(workbuffer, currentLevel->fileName);

    if (workbuffer[0])
    {
        artSetupMapArt(workbuffer);
        append_ext_UNSAFE(workbuffer, ".mhk");
        engineLoadMHK(workbuffer);
    }

    if (status == 2)
        G_NewGame_EnterLevel();
    else if ((status = sv_loadsnapshot(*fil, 0, &h)))  // read the rest...
    {
        // in theory, we could load into an initial dump first and trivially
        // recover if things go wrong...
        Bsprintf(tempbuf, "Loading save game file \"%s\" failed (code %d), cannot recover.", path, status);
        G_GameExit(tempbuf);
    }
	
    sv_postudload();  // ud.m_XXX = ud.XXX

	delete fil;
	FinishSavegameRead();
	return 0;
}

////////// TIMER SAVING/RESTORING //////////

static struct {
    int32_t totalclock, totalclocklock;  // engine
    int32_t ototalclock, lockclock;  // game
} g_timers;

static void G_SaveTimers(void)
{
    g_timers.totalclock     = (int32_t) totalclock;
    g_timers.totalclocklock = (int32_t) totalclocklock;
    g_timers.ototalclock    = (int32_t) ototalclock;
    g_timers.lockclock      = (int32_t) lockclock;
}

static void G_RestoreTimers(void)
{
    totalclock     = g_timers.totalclock;
    totalclocklock = g_timers.totalclocklock;
    ototalclock    = g_timers.ototalclock;
    lockclock      = g_timers.lockclock;
}

bool G_SavePlayer(FSaveGameNode *sv)
{
    G_SaveTimers();

    Net_WaitForEverybody();
    ready2send = 0;

	FString fn;

	errno = 0;
	FileWriter *fil;

	fil = WriteSavegameChunk("snapshot.dat");
	// The above call cannot fail.
	{
		auto& fw = *fil;

		// temporary hack
		ud.user_map = G_HaveUserMap();


        // SAVE!
        sv_saveandmakesnapshot(fw, 0, 0);


		fw.Close();
		bool res = FinishSavegameWrite();

		if (!g_netServer && ud.multimode < 2)
		{
			OSD_Printf("Saved: %s\n", fn.GetChars());
			quoteMgr.InitializeQuote(QUOTE_RESERVED4, "Game Saved");
			P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
		}
		
		ready2send = 1;
		Net_WaitForEverybody();
		
		G_RestoreTimers();
		ototalclock = totalclock;
		
		return res;
	}
}

bool GameInterface::LoadGame(FSaveGameNode* sv)
{
    if (g_netServer || ud.multimode > 1)
    {
		quoteMgr.InitializeQuote(QUOTE_RESERVED4, "Multiplayer Loading Not Yet Supported");
        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);

//        g_player[myconnectindex].ps->gm = MODE_GAME;
        return false;
    }
    else
    {
        int32_t c = G_LoadPlayer(sv->Filename);
        if (c == 0)
            g_player[myconnectindex].ps->gm = MODE_GAME;
        return !c;
    }
}

bool GameInterface::SaveGame(FSaveGameNode* sv)
{
    if (g_netServer || ud.multimode > 1)
    {
		quoteMgr.InitializeQuote(QUOTE_RESERVED4, "Multiplayer Saving Not Yet Supported");
        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
		return false;
    }
    else
    {

		videoNextPage();	// no idea if this is needed here.
        return G_SavePlayer(sv);
    }
}

////////// GENERIC SAVING/LOADING SYSTEM //////////

typedef struct dataspec_
{
    uint32_t flags;
    void * const ptr;
    uint32_t size;
    intptr_t cnt;
} dataspec_t;

#define DS_DYNAMIC 1  // dereference .ptr one more time
#define DS_STRING 2
#define DS_CMP 4
// 8
#define DS_CNT(x) ((sizeof(x))<<3)  // .cnt is pointer to...
#define DS_CNT16 16
#define DS_CNT32 32
#define DS_CNTMASK (8|DS_CNT16|DS_CNT32|64)
// 64
#define DS_LOADFN 128  // .ptr is function that is run when loading
#define DS_SAVEFN 256  // .ptr is function that is run when saving
#define DS_NOCHK 1024  // don't check for diffs (and don't write out in dump) since assumed constant throughout demo
#define DS_PROTECTFN 512
#define DS_END (0x70000000)

static int32_t ds_getcnt(const dataspec_t *spec)
{
    int cnt = -1;

    switch (spec->flags & DS_CNTMASK)
    {
        case 0: cnt = spec->cnt; break;
        case DS_CNT16: cnt = *((int16_t *)spec->cnt); break;
        case DS_CNT32: cnt = *((int32_t *)spec->cnt); break;
    }

    return cnt;
}

static inline void ds_get(const dataspec_t *spec, void **ptr, int32_t *cnt)
{
    *cnt = ds_getcnt(spec);
    *ptr = (spec->flags & DS_DYNAMIC) ? *((void **)spec->ptr) : spec->ptr;
}

// write state to file and/or to dump
static uint8_t *writespecdata(const dataspec_t *spec, FileWriter *fil, uint8_t *dump)
{
    for (; spec->flags != DS_END; spec++)
    {
        if (spec->flags & (DS_SAVEFN|DS_LOADFN))
        {
            if (spec->flags & DS_SAVEFN)
                (*(void (*)(void))spec->ptr)();
            continue;
        }

        if (!fil && (spec->flags & (DS_NOCHK|DS_CMP|DS_STRING)))
            continue;
        else if (spec->flags & DS_STRING)
        {
            fil->Write(spec->ptr, Bstrlen((const char *)spec->ptr));  // not null-terminated!
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("wsd: cnt=%d, f=0x%x.\n", cnt, spec->flags);
            continue;
        }

        if (!ptr || !cnt)
            continue;

        if (fil)
        {
			fil->Write(ptr, spec->size * cnt);
        }

        if (dump && (spec->flags & (DS_NOCHK|DS_CMP)) == 0)
        {
            Bmemcpy(dump, ptr, spec->size * cnt);
            dump += spec->size * cnt;
        }
    }
    return dump;
}

// let havedump=dumpvar&&*dumpvar
// (fil>=0 && havedump): first restore dump from file, then restore state from dump
// (fil<0 && havedump): only restore state from dump
// (fil>=0 && !havedump): only restore state from file
static int32_t readspecdata(const dataspec_t *spec, FileReader *fil, uint8_t **dumpvar)
{
    uint8_t *  dump = dumpvar ? *dumpvar : NULL;
    auto const sptr = spec;

    for (; spec->flags != DS_END; spec++)
    {
        if (fil == nullptr && spec->flags & (DS_NOCHK|DS_STRING|DS_CMP))  // we're updating
            continue;

        if (spec->flags & (DS_LOADFN|DS_SAVEFN))
        {
            if (spec->flags & DS_LOADFN)
                (*(void (*)())spec->ptr)();
            continue;
        }

        if (spec->flags & (DS_STRING|DS_CMP))  // DS_STRING and DS_CMP is for static data only
        {
            static char cmpstrbuf[32];
            int const siz  = (spec->flags & DS_STRING) ? Bstrlen((const char *)spec->ptr) : spec->size * spec->cnt;
            int const ksiz = fil->Read(cmpstrbuf, siz);

            if (ksiz != siz || Bmemcmp(spec->ptr, cmpstrbuf, siz))
            {
                OSD_Printf("rsd: spec=%s, idx=%d:\n", (char *)sptr->ptr, (int32_t)(spec-sptr));

                if (ksiz!=siz)
                    OSD_Printf("    file read returned %d, expected %d.\n", ksiz, siz);
                else
                    OSD_Printf("    sp->ptr and cmpstrbuf not identical!\n");

                return -1;
            }
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("rsd: cnt<0... wtf?\n");
            return -1;
        }

        if (!ptr || !cnt)
            continue;

        if (fil != nullptr && fil->isOpen())
        {
            auto const mem  = (dump && (spec->flags & DS_NOCHK) == 0) ? dump : (uint8_t *)ptr;
            int const  siz  = cnt * spec->size;
            int const  ksiz = fil->Read(mem, siz);

            if (ksiz != siz)
            {
                OSD_Printf("rsd: spec=%s, idx=%d, mem=%p\n", (char *)sptr->ptr, (int32_t)(spec - sptr), mem);
                OSD_Printf("     : read %d, expected %d!\n",
                           ksiz, siz);

                if (ksiz == -1)
                    OSD_Printf("     read: %s\n", strerror(errno));

                return -1;
            }
        }

        if (dump && (spec->flags & DS_NOCHK) == 0)
        {
            Bmemcpy(ptr, dump, spec->size * cnt);
            dump += spec->size * cnt;
        }
    }

    if (dumpvar)
        *dumpvar = dump;

    return 0;
}

#define UINT(bits) uint##bits##_t
#define BYTES(bits) (bits>>3)
#define VAL(bits,p) (*(UINT(bits) const *)(p))
#define WVAL(bits,p) (*(UINT(bits) *)(p))

static void docmpsd(const void *ptr, void *dump, uint32_t size, uint32_t cnt, uint8_t **diffvar)
{
    uint8_t *retdiff = *diffvar;

    // Hail to the C preprocessor, baby!
#define CPSINGLEVAL(Datbits)                                              \
    if (VAL(Datbits, ptr) != VAL(Datbits, dump))                          \
    {                                                                     \
        WVAL(Datbits, retdiff) = WVAL(Datbits, dump) = VAL(Datbits, ptr); \
        *diffvar = retdiff + BYTES(Datbits);                              \
    }

    if (cnt == 1)
        switch (size)
        {
        case 8: CPSINGLEVAL(64); return;
        case 4: CPSINGLEVAL(32); return;
        case 2: CPSINGLEVAL(16); return;
        case 1: CPSINGLEVAL(8); return;
        }

#define CPELTS(Idxbits, Datbits)             \
    do                                       \
    {                                        \
        for (int i = 0; i < nelts; i++)      \
        {                                    \
            if (*p != *op)                   \
            {                                \
                *op = *p;                    \
                WVAL(Idxbits, retdiff) = i;  \
                retdiff += BYTES(Idxbits);   \
                WVAL(Datbits, retdiff) = *p; \
                retdiff += BYTES(Datbits);   \
            }                                \
            p++;                             \
            op++;                            \
        }                                    \
        WVAL(Idxbits, retdiff) = -1;         \
        retdiff += BYTES(Idxbits);           \
    } while (0)

#define CPDATA(Datbits)                                                  \
    do                                                                   \
    {                                                                    \
        auto p     = (UINT(Datbits) const *)ptr;                         \
        auto op    = (UINT(Datbits) *)dump;                              \
        int  nelts = tabledivide32_noinline(size * cnt, BYTES(Datbits)); \
        if (nelts > 65536)                                               \
            CPELTS(32, Datbits);                                         \
        else if (nelts > 256)                                            \
            CPELTS(16, Datbits);                                         \
        else                                                             \
            CPELTS(8, Datbits);                                          \
    } while (0)

    if (size == 8)
        CPDATA(64);
    else if ((size & 3) == 0)
        CPDATA(32);
    else if ((size & 1) == 0)
        CPDATA(16);
    else
        CPDATA(8);

    *diffvar = retdiff;

#undef CPELTS
#undef CPSINGLEVAL
#undef CPDATA
}

// get the number of elements to be monitored for changes
static int32_t getnumvar(const dataspec_t *spec)
{
    int n = 0;
    for (; spec->flags != DS_END; spec++)
        if (spec->flags & (DS_STRING|DS_CMP|DS_NOCHK|DS_SAVEFN|DS_LOADFN))
            ++n;
    return n;
}

// update dump at *dumpvar with new state and write diff to *diffvar
static void cmpspecdata(const dataspec_t *spec, uint8_t **dumpvar, uint8_t **diffvar)
{
    uint8_t * dump   = *dumpvar;
    uint8_t * diff   = *diffvar;
    int       nbytes = (getnumvar(spec) + 7) >> 3;
    int const slen   = Bstrlen((const char *)spec->ptr);

    Bmemcpy(diff, spec->ptr, slen);
    diff += slen;

    while (nbytes--)
        *(diff++) = 0;  // the bitmap of indices which elements of spec have changed go here

    int eltnum = 0;

    for (spec++; spec->flags!=DS_END; spec++)
    {
        if ((spec->flags&(DS_NOCHK|DS_STRING|DS_CMP)))
            continue;

        if (spec->flags&(DS_LOADFN|DS_SAVEFN))
        {
            if ((spec->flags&(DS_PROTECTFN))==0)
                (*(void (*)())spec->ptr)();
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("csd: cnt=%d, f=0x%x\n", cnt, spec->flags);
            continue;
        }

        uint8_t * const tmptr = diff;

        docmpsd(ptr, dump, spec->size, cnt, &diff);

        if (diff != tmptr)
            (*diffvar + slen)[eltnum>>3] |= 1<<(eltnum&7);

        dump += spec->size*cnt;
        eltnum++;
    }

    *diffvar = diff;
    *dumpvar = dump;
}

#define VALOFS(bits,p,ofs) (*(((UINT(bits) *)(p)) + (ofs)))

// apply diff to dump, not to state! state is restored from dump afterwards.
static int32_t applydiff(const dataspec_t *spec, uint8_t **dumpvar, uint8_t **diffvar)
{
    uint8_t * dump   = *dumpvar;
    uint8_t * diff   = *diffvar;
    int const nbytes = (getnumvar(spec)+7)>>3;
    int const slen   = Bstrlen((const char *)spec->ptr);

    if (Bmemcmp(diff, spec->ptr, slen))  // check STRING magic (sync check)
        return 1;

    diff += slen+nbytes;

    int eltnum = -1;
    for (spec++; spec->flags != DS_END; spec++)
    {
        if ((spec->flags & (DS_NOCHK|DS_STRING|DS_CMP|DS_LOADFN|DS_SAVEFN)))
            continue;

        int const cnt = ds_getcnt(spec);
        if (cnt < 0) return 1;

        eltnum++;
        if (((*diffvar+slen)[eltnum>>3] & (1<<(eltnum&7))) == 0)
        {
            dump += spec->size * cnt;
            continue;
        }

// ----------
#define CPSINGLEVAL(Datbits)                  \
    WVAL(Datbits, dump) = VAL(Datbits, diff); \
    diff += BYTES(Datbits);                   \
    dump += BYTES(Datbits)

        if (cnt == 1)
        {
            switch (spec->size)
            {
                case 8: CPSINGLEVAL(64); continue;
                case 4: CPSINGLEVAL(32); continue;
                case 2: CPSINGLEVAL(16); continue;
                case 1: CPSINGLEVAL(8); continue;
            }
        }

#define CPELTS(Idxbits, Datbits)                             \
    do                                                       \
    {                                                        \
        UINT(Idxbits) idx;                                   \
        goto readidx_##Idxbits##_##Datbits;                  \
        do                                                   \
        {                                                    \
            VALOFS(Datbits, dump, idx) = VAL(Datbits, diff); \
            diff += BYTES(Datbits);                          \
readidx_##Idxbits##_##Datbits:                               \
            idx = VAL(Idxbits, diff);                        \
            diff += BYTES(Idxbits);                          \
        } while ((int##Idxbits##_t)idx != -1);               \
    } while (0)

#define CPDATA(Datbits)                                                            \
    do                                                                             \
    {                                                                              \
        int const elts = tabledivide32_noinline(spec->size * cnt, BYTES(Datbits)); \
        if (elts > 65536)                                                          \
            CPELTS(32, Datbits);                                                   \
        else if (elts > 256)                                                       \
            CPELTS(16, Datbits);                                                   \
        else                                                                       \
            CPELTS(8, Datbits);                                                    \
    } while (0)

        if (spec->size == 8)
            CPDATA(64);
        else if ((spec->size & 3) == 0)
            CPDATA(32);
        else if ((spec->size & 1) == 0)
            CPDATA(16);
        else
            CPDATA(8);
        dump += spec->size * cnt;
// ----------

#undef CPELTS
#undef CPSINGLEVAL
#undef CPDATA
    }

    *diffvar = diff;
    *dumpvar = dump;
    return 0;
}

#undef VAL
#undef VALOFS
#undef BYTES
#undef UINT

// calculate size needed for dump
static uint32_t calcsz(const dataspec_t *spec)
{
    uint32_t dasiz = 0;

    for (; spec->flags != DS_END; spec++)
    {
        // DS_STRINGs are used as sync checks in the diffs but not in the dump
        if ((spec->flags & (DS_CMP|DS_NOCHK|DS_SAVEFN|DS_LOADFN|DS_STRING)))
            continue;

        int const cnt = ds_getcnt(spec);

        if (cnt <= 0)
            continue;

        dasiz += cnt * spec->size;
    }

    return dasiz;
}

#ifdef USE_OPENGL
static void sv_prespriteextsave();
static void sv_postspriteext();
#endif
static void sv_postactordata();
static void sv_preanimateptrsave();
static void sv_postanimateptr();
static void sv_restsave();
static void sv_restload();
static void sv_rrrafog();

#define SVARDATALEN \
    ((sizeof(g_player[0].user_name)+sizeof(g_player[0].pcolor)+sizeof(g_player[0].pteam) \
      +sizeof(g_player[0].frags)+sizeof(DukePlayer_t))*MAXPLAYERS)

static uint8_t savegame_restdata[SVARDATALEN];

static char svgm_udnetw_string [] = "blK:udnt";
static const dataspec_t svgm_udnetw[] =
{
    { DS_STRING, (void *)svgm_udnetw_string, 0, 1 },
    { 0, &ud.multimode, sizeof(ud.multimode), 1 },
    { 0, &g_playerSpawnCnt, sizeof(g_playerSpawnCnt), 1 },
    { 0, &g_playerSpawnPoints, sizeof(g_playerSpawnPoints), 1 },

    { DS_NOCHK, &ud.volume_number, sizeof(ud.volume_number), 1 },
    { DS_NOCHK, &ud.level_number, sizeof(ud.level_number), 1 },
    { DS_NOCHK, &ud.user_map, sizeof(ud.user_map), 1 },
    { DS_NOCHK, &ud.player_skill, sizeof(ud.player_skill), 1 },
    { DS_NOCHK, &ud.music_episode, sizeof(ud.music_episode), 1 },
    { DS_NOCHK, &ud.music_level, sizeof(ud.music_level), 1 },

    { DS_NOCHK, &ud.from_bonus, sizeof(ud.from_bonus), 1 },
    { DS_NOCHK, &ud.secretlevel, sizeof(ud.secretlevel), 1 },
    { DS_NOCHK, &ud.respawn_monsters, sizeof(ud.respawn_monsters), 1 },
    { DS_NOCHK, &ud.respawn_items, sizeof(ud.respawn_items), 1 },
    { DS_NOCHK, &ud.respawn_inventory, sizeof(ud.respawn_inventory), 1 },
    { 0, &ud.god, sizeof(ud.god), 1 },
    { DS_NOCHK, &ud.monsters_off, sizeof(ud.monsters_off), 1 },
    { DS_NOCHK, &ud.last_level, sizeof(ud.last_level), 1 },
    { 0, &ud.eog, sizeof(ud.eog), 1 },
    { DS_NOCHK, &ud.coop, sizeof(ud.coop), 1 },
    { DS_NOCHK, &ud.marker, sizeof(ud.marker), 1 },
    { DS_NOCHK, &ud.ffire, sizeof(ud.ffire), 1 },
    { DS_NOCHK, &ud.noexits, sizeof(ud.noexits), 1 },
    { DS_NOCHK, &ud.playerai, sizeof(ud.playerai), 1 },
    { 0, &ud.pause_on, sizeof(ud.pause_on), 1 },
    { 0, connectpoint2, sizeof(connectpoint2), 1 },
    { 0, &randomseed, sizeof(randomseed), 1 },
    { 0, &g_globalRandom, sizeof(g_globalRandom), 1 },
//    { 0, &lockclock_dummy, sizeof(lockclock), 1 },
    { DS_END, 0, 0, 0 }
};

#if !defined DEBUG_MAIN_ARRAYS
# define DS_MAINAR DS_DYNAMIC
#else
# define DS_MAINAR 0
#endif

static char svgm_secwsp_string [] = "blK:swsp";
static const dataspec_t svgm_secwsp[] =
{
    { DS_STRING, (void *)svgm_secwsp_string, 0, 1 },
    { DS_NOCHK, &g_cyclerCnt, sizeof(g_cyclerCnt), 1 },
    { DS_CNT(g_cyclerCnt), &g_cyclers[0][0], sizeof(g_cyclers[0]), (intptr_t)&g_cyclerCnt },
    { DS_NOCHK, &g_animWallCnt, sizeof(g_animWallCnt), 1 },
    { DS_CNT(g_animWallCnt), &animwall, sizeof(animwall[0]), (intptr_t)&g_animWallCnt },
    { DS_NOCHK, &g_mirrorCount, sizeof(g_mirrorCount), 1 },
    { DS_NOCHK, &g_mirrorWall[0], sizeof(g_mirrorWall[0]), ARRAY_SIZE(g_mirrorWall) },
    { DS_NOCHK, &g_mirrorSector[0], sizeof(g_mirrorSector[0]), ARRAY_SIZE(g_mirrorSector) },
    { 0, &everyothertime, sizeof(everyothertime), 1 },
    { DS_END, 0, 0, 0 }
};

static char svgm_script_string [] = "blK:scri";
static const dataspec_t svgm_script[] =
{
    { DS_STRING, (void *)svgm_script_string, 0, 1 },
    { 0, &actor[0], sizeof(actor_t), MAXSPRITES },
    { DS_SAVEFN|DS_LOADFN, (void *)&sv_postactordata, 0, 1 },
    { DS_END, 0, 0, 0 }
};

static char svgm_anmisc_string [] = "blK:anms";
static char svgm_end_string [] = "savegame_end";

static const dataspec_t svgm_anmisc[] =
{
    { DS_STRING, (void *)svgm_anmisc_string, 0, 1 },
    { 0, &g_animateCnt, sizeof(g_animateCnt), 1 },
    { 0, &g_animateSect[0], sizeof(g_animateSect[0]), MAXANIMATES },
    { 0, &g_animateGoal[0], sizeof(g_animateGoal[0]), MAXANIMATES },
    { 0, &g_animateVel[0], sizeof(g_animateVel[0]), MAXANIMATES },
    { DS_SAVEFN, (void *)&sv_preanimateptrsave, 0, 1 },
    { 0, &g_animatePtr[0], sizeof(g_animatePtr[0]), MAXANIMATES },
    { DS_SAVEFN|DS_LOADFN , (void *)&sv_postanimateptr, 0, 1 },
    { 0, &g_curViewscreen, sizeof(g_curViewscreen), 1 },
    { 0, &g_origins[0], sizeof(g_origins[0]), ARRAY_SIZE(g_origins) },
    { 0, &g_spriteDeleteQueuePos, sizeof(g_spriteDeleteQueuePos), 1 },
    { DS_NOCHK, &g_deleteQueueSize, sizeof(g_deleteQueueSize), 1 },
    { DS_CNT(g_deleteQueueSize), &SpriteDeletionQueue[0], sizeof(int16_t), (intptr_t)&g_deleteQueueSize },
    { 0, &show2dsector[0], sizeof(uint8_t), MAXSECTORS>>3 },
    { DS_NOCHK, &g_cloudCnt, sizeof(g_cloudCnt), 1 },
    { 0, &g_cloudSect[0], sizeof(g_cloudSect), 1 },
    { 0, &g_cloudX, sizeof(g_cloudX), 1 },
    { 0, &g_cloudY, sizeof(g_cloudY), 1 },
    { 0, &g_pskyidx, sizeof(g_pskyidx), 1 },  // DS_NOCHK?
    { 0, &g_earthquakeTime, sizeof(g_earthquakeTime), 1 },

    // RR stuff
    { 0, &g_spriteExtra[0], sizeof(g_spriteExtra[0]), MAXSPRITES },
    { 0, &g_sectorExtra[0], sizeof(g_sectorExtra[0]), MAXSECTORS },

    { 0, &g_jailDoorSecHitag[0], sizeof(g_jailDoorSecHitag[0]), ARRAY_SIZE(g_jailDoorSecHitag) },
    { 0, &g_jailDoorSect[0], sizeof(g_jailDoorSect[0]), ARRAY_SIZE(g_jailDoorSect) },
    { 0, &g_jailDoorOpen[0], sizeof(g_jailDoorOpen[0]), ARRAY_SIZE(g_jailDoorOpen) },
    { 0, &g_jailDoorDir[0], sizeof(g_jailDoorDir[0]), ARRAY_SIZE(g_jailDoorDir) },
    { 0, &g_jailDoorDrag[0], sizeof(g_jailDoorDrag[0]), ARRAY_SIZE(g_jailDoorDrag) },
    { 0, &g_jailDoorDist[0], sizeof(g_jailDoorDist[0]), ARRAY_SIZE(g_jailDoorDist) },
    { 0, &g_jailDoorSpeed[0], sizeof(g_jailDoorSpeed[0]), ARRAY_SIZE(g_jailDoorSpeed) },
    { 0, &g_jailDoorSound[0], sizeof(g_jailDoorSound[0]), ARRAY_SIZE(g_jailDoorSound) },
    { 0, &g_jailDoorCnt, sizeof(g_jailDoorCnt), 1 },

    { 0, &g_shadedSector[0], sizeof(g_shadedSector[0]), MAXSECTORS },

    { 0, &g_mineCartSect[0], sizeof(g_mineCartSect[0]), ARRAY_SIZE(g_mineCartSect) },
    { 0, &g_mineCartChildSect[0], sizeof(g_mineCartChildSect[0]), ARRAY_SIZE(g_mineCartChildSect) },
    { 0, &g_mineCartOpen[0], sizeof(g_mineCartOpen[0]), ARRAY_SIZE(g_mineCartOpen) },
    { 0, &g_mineCartDir[0], sizeof(g_mineCartDir[0]), ARRAY_SIZE(g_mineCartDir) },
    { 0, &g_mineCartDrag[0], sizeof(g_mineCartDrag[0]), ARRAY_SIZE(g_mineCartDrag) },
    { 0, &g_mineCartDist[0], sizeof(g_mineCartDist[0]), ARRAY_SIZE(g_mineCartDist) },
    { 0, &g_mineCartSpeed[0], sizeof(g_mineCartSpeed[0]), ARRAY_SIZE(g_mineCartSpeed) },
    { 0, &g_mineCartSound[0], sizeof(g_mineCartSound[0]), ARRAY_SIZE(g_mineCartSound) },
    { 0, &g_mineCartCnt, sizeof(g_mineCartCnt), 1 },

    { 0, &g_ambientCnt, sizeof(g_ambientCnt), 1 },
    { 0, &g_ambientHitag[0], sizeof(g_ambientHitag[0]), ARRAY_SIZE(g_ambientHitag) },
    { 0, &g_ambientLotag[0], sizeof(g_ambientLotag[0]), ARRAY_SIZE(g_ambientLotag) },
    
    { 0, &g_ufoSpawn, sizeof(g_ufoSpawn), 1 },
    { 0, &g_ufoCnt, sizeof(g_ufoCnt), 1 },
    { 0, &g_hulkSpawn, sizeof(g_hulkSpawn), 1 },
    { 0, &g_lastLevel, sizeof(g_lastLevel), 1 },

    { 0, &g_torchSector[0], sizeof(g_torchSector[0]), ARRAY_SIZE(g_torchSector) },
    { 0, &g_torchSectorShade[0], sizeof(g_torchSectorShade[0]), ARRAY_SIZE(g_torchSectorShade) },
    { 0, &g_torchType[0], sizeof(g_torchType[0]), ARRAY_SIZE(g_torchType) },
    { 0, &g_torchCnt, sizeof(g_torchCnt), 1 },

    { 0, &g_lightninSector[0], sizeof(g_lightninSector[0]), ARRAY_SIZE(g_lightninSector) },
    { 0, &g_lightninSectorShade[0], sizeof(g_lightninSectorShade[0]), ARRAY_SIZE(g_lightninSectorShade) },
    { 0, &g_lightninCnt, sizeof(g_lightninCnt), 1 },

    { 0, &g_geoSector[0], sizeof(g_geoSector[0]), ARRAY_SIZE(g_geoSector) },
    { 0, &g_geoSectorWarp[0], sizeof(g_geoSectorWarp[0]), ARRAY_SIZE(g_geoSectorWarp) },
    { 0, &g_geoSectorX[0], sizeof(g_geoSectorX[0]), ARRAY_SIZE(g_geoSectorX) },
    { 0, &g_geoSectorY[0], sizeof(g_geoSectorY[0]), ARRAY_SIZE(g_geoSectorY) },
    { 0, &g_geoSectorWarp2[0], sizeof(g_geoSectorWarp2[0]), ARRAY_SIZE(g_geoSectorWarp2) },
    { 0, &g_geoSectorX2[0], sizeof(g_geoSectorX2[0]), ARRAY_SIZE(g_geoSectorX2) },
    { 0, &g_geoSectorY2[0], sizeof(g_geoSectorY2[0]), ARRAY_SIZE(g_geoSectorY2) },
    { 0, &g_geoSectorCnt, sizeof(g_geoSectorCnt), 1 },

    { 0, &g_windTime, sizeof(g_windTime), 1 },
    { 0, &g_windDir, sizeof(g_windDir), 1 },
    { 0, &g_fakeBubbaCnt, sizeof(g_fakeBubbaCnt), 1 },
    { 0, &g_mamaSpawnCnt, sizeof(g_mamaSpawnCnt), 1 },
    { 0, &g_banjoSong, sizeof(g_banjoSong), 1 },
    { 0, &g_bellTime, sizeof(g_bellTime), 1 },
    { 0, &g_bellSprite, sizeof(g_bellSprite), 1 },

    { 0, &g_changeEnemySize, sizeof(g_changeEnemySize), 1 },
    { 0, &g_slotWin, sizeof(g_slotWin), 1 },
    { 0, &g_ufoSpawnMinion, sizeof(g_ufoSpawnMinion), 1 },
    { 0, &g_pistonSound, sizeof(g_pistonSound), 1 },
    { 0, &g_chickenWeaponTimer, sizeof(g_chickenWeaponTimer), 1 },
    { 0, &g_RAendLevel, sizeof(g_RAendLevel), 1 },
    { 0, &g_RAendEpisode, sizeof(g_RAendEpisode), 1 },
    { 0, &g_fogType, sizeof(g_fogType), 1 },
    { DS_LOADFN, (void *)sv_rrrafog, 0, 1 },

    { DS_SAVEFN, (void *)&sv_restsave, 0, 1 },
    { 0, savegame_restdata, 1, sizeof(savegame_restdata) },  // sz/cnt swapped for kdfread
    { DS_LOADFN, (void *)&sv_restload, 0, 1 },

    { DS_STRING, (void *)svgm_end_string, 0, 1 },
    { DS_END, 0, 0, 0 }
};

static uint8_t *dosaveplayer2(FileWriter *fil, uint8_t *mem);
static int32_t doloadplayer2(FileReader &fil, uint8_t **memptr);
static void postloadplayer(int32_t savegamep);

// SVGM snapshot system
static uint32_t svsnapsiz;
static uint8_t *svsnapshot;
static uint8_t *svinitsnap;
static uint32_t svdiffsiz;
static uint8_t *svdiff;

#include "gamedef.h"

void sv_freemem()
{
    DO_FREE_AND_NULL(svsnapshot);
    DO_FREE_AND_NULL(svinitsnap);
    DO_FREE_AND_NULL(svdiff);
}

static void SV_AllocSnap(int32_t allocinit)
{
    sv_freemem();

    svsnapshot = (uint8_t *)Xmalloc(svsnapsiz);
    if (allocinit)
        svinitsnap = (uint8_t *)Xmalloc(svsnapsiz);
    svdiffsiz = svsnapsiz;  // theoretically it's less than could be needed in the worst case, but practically it's overkill
    svdiff = (uint8_t *)Xmalloc(svdiffsiz);
}

// make snapshot only if spot < 0 (demo)
int32_t sv_saveandmakesnapshot(FileWriter &fil, int8_t spot, bool isAutoSave)
{
    savehead_t h;

    // calculate total snapshot size
    svsnapsiz = calcsz(svgm_udnetw) + calcsz(svgm_secwsp) + calcsz(svgm_script) + calcsz(svgm_anmisc);


    // create header
    Bmemcpy(h.headerstr, "DERSAVEGAME", 11);
    h.majorver = SV_MAJOR_VER;
    h.minorver = SV_MINOR_VER;
    h.ptrsize  = sizeof(intptr_t);

    if (isAutoSave)
        h.ptrsize |= 1u << 7u;

    h.bytever      = BYTEVERSION;
    h.userbytever  = ud.userbytever;
    h.scriptcrc    = g_scriptcrc;

    h.reccnt  = 0;
    h.snapsiz = svsnapsiz;

    // the following is kinda redundant, but we save it here to be able to quickly fetch
    // it in a savegame header read

    h.numplayers = ud.multimode;
    h.volnum     = ud.volume_number;
    h.levnum     = ud.level_number;
    h.skill      = ud.player_skill;

    if (spot >= 0)
    {
        // savegame
		auto fw = WriteSavegameChunk("header.dat");
		fw->Write(&h, sizeof(savehead_t));
	}
    else
    {
        // demo
        // demo (currently broken, needs a new format.)
        const time_t t = time(NULL);
		struct tm *  st = localtime(&t);
        FStringf demoname("Demo %04d%02d%02d %s", st->tm_year+1900, st->tm_mon+1, st->tm_mday, GetGitDescription());
		fil.Write(&h, sizeof(savehead_t));

	}


    // write header

#if 0 // not usable anymore
    if (spot >= 0 && tileData(TILE_SAVESHOT))
    {
		auto fw = WriteSavegameChunk("screenshot.dat");
        fw->Write(tileData(TILE_SAVESHOT), 320*200);

    }
#endif


    if (spot >= 0)
    {
        // savegame
        dosaveplayer2(&fil, NULL);
    }
    else
    {
        // demo
        SV_AllocSnap(0);

        uint8_t * const p = dosaveplayer2(&fil, svsnapshot);

        if (p != svsnapshot+svsnapsiz)
        {
            OSD_Printf("sv_saveandmakesnapshot: ptr-(snapshot end)=%d!\n", (int32_t)(p - (svsnapshot + svsnapsiz)));
            return 1;
        }
    }

    return 0;
}

// if file is not an EDuke32 savegame/demo, h->headerstr will be all zeros
int32_t sv_loadheader(FileReader &fill, int32_t spot, savehead_t *h)
{
	FileReader filc;
	FileReader* filp = &fill;
    int32_t havedemo = (spot < 0);
	if (!havedemo)
	{
		filc = ReadSavegameChunk("header.dat");
		filp = &filc;
	}

    if (filp->Read(h, sizeof(savehead_t)) != sizeof(savehead_t))
    {
        OSD_Printf("%s %d header corrupt.\n", havedemo ? "Demo":"Savegame", havedemo ? -spot : spot);
        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -1;
    }

    if (Bmemcmp(h->headerstr, "DERSAVEGAME", 11)
       )
    {
        char headerCstr[sizeof(h->headerstr) + 1];
        Bmemcpy(headerCstr, h->headerstr, sizeof(h->headerstr));
        headerCstr[sizeof(h->headerstr)] = '\0';
        OSD_Printf("%s %d header reads \"%s\", expected \"DERSAVEGAME\".\n",
                   havedemo ? "Demo":"Savegame", havedemo ? -spot : spot, headerCstr);
        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -2;
    }

    if (h->majorver != SV_MAJOR_VER || h->minorver != SV_MINOR_VER || h->bytever != BYTEVERSION || h->userbytever != ud.userbytever || (apScript != NULL && h->scriptcrc != g_scriptcrc))
    {
#ifndef DEBUGGINGAIDS
        if (havedemo)
#endif
            OSD_Printf("Incompatible savegame. Expected version %d.%d.%d.%d.%0x, found %d.%d.%d.%d.%0x\n", SV_MAJOR_VER, SV_MINOR_VER, BYTEVERSION,
                       ud.userbytever, g_scriptcrc, h->majorver, h->minorver, h->bytever, h->userbytever, h->scriptcrc);

        if (h->majorver == SV_MAJOR_VER && h->minorver == SV_MINOR_VER)
        {
            return 1;
        }
        else
        {
            Bmemset(h->headerstr, 0, sizeof(h->headerstr));
            return -3;
        }
    }

    if (h->getPtrSize() != sizeof(intptr_t))
    {
#ifndef DEBUGGINGAIDS
        if (havedemo)
#endif
            OSD_Printf("File incompatible. Expected pointer size %d, found %d\n",
                       (int32_t)sizeof(intptr_t), h->getPtrSize());

        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -4;
    }

    return 0;
}

int32_t sv_loadsnapshot(FileReader &fil, int32_t spot, savehead_t *h)
{
    uint8_t *p;
    int32_t i;

    if (spot < 0)
    {
        // demo
        i = sv_loadheader(fil, spot, h);
        if (i)
            return i;

        // Used to be in doloadplayer2(), now redundant for savegames since
        // we checked before. Multiplayer demos need still to be taken care of.
        if (h->numplayers != numplayers)
            return 9;
    }
    // else (if savegame), we just read the header and are now at offset sizeof(savehead_t)

#ifdef DEBUGGINGAIDS
    OSD_Printf("sv_loadsnapshot: snapshot size: %d bytes.\n", h->snapsiz);
#endif

    if (spot >= 0)
    {
        // savegame
        i = doloadplayer2(fil, NULL);
        if (i)
        {
            OSD_Printf("sv_loadsnapshot: doloadplayer2() returned %d.\n", i);
            return 5;
        }
    }
    else
    {
        svsnapsiz = h->snapsiz;

        SV_AllocSnap(1);

        p = svsnapshot;
        i = doloadplayer2(fil, &p);
        if (i)
        {
            OSD_Printf("sv_loadsnapshot: doloadplayer2() returned %d.\n", i);
            sv_freemem();
            return 5;
        }

        if (p != svsnapshot+svsnapsiz)
        {
            OSD_Printf("sv_loadsnapshot: internal error: p-(snapshot end)=%d!\n",
                       (int32_t)(p-(svsnapshot+svsnapsiz)));
            sv_freemem();
            return 6;
        }

        Bmemcpy(svinitsnap, svsnapshot, svsnapsiz);
    }

    postloadplayer((spot >= 0));

    return 0;
}


uint32_t sv_writediff(FileWriter *fil)
{
    uint8_t *p = svsnapshot;
    uint8_t *d = svdiff;

    cmpspecdata(svgm_udnetw, &p, &d);
    cmpspecdata(svgm_secwsp, &p, &d);
    cmpspecdata(svgm_script, &p, &d);
    cmpspecdata(svgm_anmisc, &p, &d);

    if (p != svsnapshot+svsnapsiz)
        OSD_Printf("sv_writediff: dump+siz=%p, p=%p!\n", svsnapshot+svsnapsiz, p);
    
    uint32_t const diffsiz = d - svdiff;

    fil->Write("dIfF",4);
	fil->Write(&diffsiz, sizeof(diffsiz));

	fil->Write(svdiff,  diffsiz);

    return diffsiz;
}

int32_t sv_readdiff(FileReader &fil)
{
    int32_t diffsiz;

    if (fil.Read(&diffsiz, sizeof(uint32_t)) != sizeof(uint32_t))
        return -1;

	if (fil.Read(svdiff, diffsiz) != diffsiz)
            return -2;

    uint8_t *p = svsnapshot;
    uint8_t *d = svdiff;

    if (applydiff(svgm_udnetw, &p, &d)) return -3;
    if (applydiff(svgm_secwsp, &p, &d)) return -4;
    if (applydiff(svgm_script, &p, &d)) return -5;
    if (applydiff(svgm_anmisc, &p, &d)) return -6;

    int i = 0;

    if (p!=svsnapshot+svsnapsiz)
        i|=1;
    if (d!=svdiff+diffsiz)
        i|=2;
    if (i)
        OSD_Printf("sv_readdiff: p=%p, svsnapshot+svsnapsiz=%p; d=%p, svdiff+diffsiz=%p",
                   p, svsnapshot+svsnapsiz, d, svdiff+diffsiz);
    return i;
}

// SVGM data description
static void sv_postudload()
{
#if 1
    m_level_number      = ud.level_number;
    ud.m_volume_number     = ud.volume_number;
    ud.m_player_skill      = ud.player_skill;
    ud.m_respawn_monsters  = ud.respawn_monsters;
    ud.m_respawn_items     = ud.respawn_items;
    ud.m_respawn_inventory = ud.respawn_inventory;
    ud.m_monsters_off      = ud.monsters_off;
    m_coop              = ud.coop;
    m_marker            = ud.marker;
    m_ffire             = ud.ffire;
    m_noexits           = ud.noexits;
#endif
}
//static int32_t lockclock_dummy;


static void sv_postactordata()
{
#ifdef POLYMER
    for (auto & i : actor)
    {
        i.lightptr = NULL;
        i.lightId = -1;
    }
#endif
}

static void sv_preanimateptrsave()
{
    G_Util_PtrToIdx(g_animatePtr, g_animateCnt, sector, P2I_FWD);
}
static void sv_postanimateptr()
{
    G_Util_PtrToIdx(g_animatePtr, g_animateCnt, sector, P2I_BACK);
}
static void sv_restsave()
{
    uint8_t *    mem = savegame_restdata;
    DukePlayer_t dummy_ps;

    Bmemset(&dummy_ps, 0, sizeof(DukePlayer_t));

#define CPDAT(ptr,sz) do { Bmemcpy(mem, ptr, sz), mem+=sz ; } while (0)
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        CPDAT(g_player[i].user_name, 32);
        CPDAT(&g_player[i].pcolor, sizeof(g_player[0].pcolor));
        CPDAT(&g_player[i].pteam, sizeof(g_player[0].pteam));
        CPDAT(&g_player[i].frags[0], sizeof(g_player[0].frags));
        CPDAT(g_player[i].ps ? g_player[i].ps : &dummy_ps, sizeof(DukePlayer_t));
    }
    
    Bassert((savegame_restdata + SVARDATALEN) - mem == 0);
#undef CPDAT
}
static void sv_restload()
{
    uint8_t *    mem = savegame_restdata;
    DukePlayer_t dummy_ps;

#define CPDAT(ptr,sz) Bmemcpy(ptr, mem, sz), mem+=sz
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        CPDAT(g_player[i].user_name, 32);
        CPDAT(&g_player[i].pcolor, sizeof(g_player[0].pcolor));
        CPDAT(&g_player[i].pteam, sizeof(g_player[0].pteam));
        CPDAT(&g_player[i].frags[0], sizeof(g_player[0].frags));
        CPDAT(g_player[i].ps ? g_player[i].ps : &dummy_ps, sizeof(DukePlayer_t));
    }
#undef CPDAT

    if (g_player[myconnectindex].ps)
        g_player[myconnectindex].ps->auto_aim = cl_autoaim;
}

#ifdef DEBUGGINGAIDS
# define PRINTSIZE(name) do { if (mem) OSD_Printf(name ": %d\n", (int32_t)(mem-tmem)); \
        OSD_Printf(name ": %d ms\n", timerGetTicks()-t); t=timerGetTicks(); tmem=mem; } while (0)
#else
# define PRINTSIZE(name) do { } while (0)
#endif

static uint8_t *dosaveplayer2(FileWriter *fil, uint8_t *mem)
{
#ifdef DEBUGGINGAIDS
    uint8_t *tmem = mem;
    int32_t t=timerGetTicks();
#endif
    mem=writespecdata(svgm_udnetw, fil, mem);  // user settings, players & net
    PRINTSIZE("ud");
    mem=writespecdata(svgm_secwsp, fil, mem);  // sector, wall, sprite
    PRINTSIZE("sws");
    mem=writespecdata(svgm_script, fil, mem);  // script
    PRINTSIZE("script");
    mem=writespecdata(svgm_anmisc, fil, mem);  // animates, quotes & misc.
    PRINTSIZE("animisc");

    return mem;
}

static int32_t doloadplayer2(FileReader &fil, uint8_t **memptr)
{
    uint8_t *mem = memptr ? *memptr : NULL;
#ifdef DEBUGGINGAIDS
    uint8_t *tmem=mem;
    int32_t t=timerGetTicks();
#endif
    if (readspecdata(svgm_udnetw, &fil, &mem)) return -2;
    PRINTSIZE("ud");
    if (readspecdata(svgm_secwsp, &fil, &mem)) return -4;
    PRINTSIZE("sws");
    if (readspecdata(svgm_script, &fil, &mem)) return -5;
    PRINTSIZE("script");
    if (readspecdata(svgm_anmisc, &fil, &mem)) return -6;
    PRINTSIZE("animisc");

    if (memptr)
        *memptr = mem;
    return 0;
}

int32_t sv_updatestate(int32_t frominit)
{
    uint8_t *p = svsnapshot, *pbeg=p;

    if (frominit)
        Bmemcpy(svsnapshot, svinitsnap, svsnapsiz);

    if (readspecdata(svgm_udnetw, nullptr, &p)) return -2;
    if (readspecdata(svgm_secwsp, nullptr, &p)) return -4;
    if (readspecdata(svgm_script, nullptr, &p)) return -5;
    if (readspecdata(svgm_anmisc, nullptr, &p)) return -6;

    if (p != pbeg+svsnapsiz)
    {
        OSD_Printf("sv_updatestate: ptr-(snapshot end)=%d\n", (int32_t)(p-(pbeg+svsnapsiz)));
        return -9;
    }

    if (frominit)
        postloadplayer(0);
#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_resetlights();  // must do it after polymer_loadboard() !!!
#endif

    return 0;
}

static void sv_rrrafog()
{
    G_SetFog(g_fogType ? 2 : 0);
}

static void postloadplayer(int32_t savegamep)
{
    int32_t i;

    //1
    if (g_player[myconnectindex].ps->over_shoulder_on != 0)
    {
        CAMERADIST = 0;
        CAMERACLOCK = 0;
        g_player[myconnectindex].ps->over_shoulder_on = 1;
    }

    //2
    screenpeek = myconnectindex;

    //2.5
    if (savegamep)
    {
        Bmemset(gotpic, 0, sizeof(gotpic));
        S_ClearSoundLocks();
        G_CacheMapData();
		Mus_ResumeSaved();
        Mus_SetPaused(false);

        g_player[myconnectindex].ps->gm = MODE_GAME;
        ud.recstat = 0;

        if (g_player[myconnectindex].ps->jetpack_on)
            A_PlaySound(DUKE_JETPACK_IDLE, g_player[myconnectindex].ps->i);
    }

    //3
    P_UpdateScreenPal(g_player[myconnectindex].ps);
    g_restorePalette = -1;

    //3.5
    if (savegamep)
    {
        for (SPRITES_OF(STAT_FX, i))
            if (sprite[i].picnum == MUSICANDSFX)
            {
                T2(i) = SoundEnabled();
                T1(i) = 0;
            }

        G_UpdateScreenArea();
        FX_SetReverb(0);
    }

    //4
    if (savegamep)
    {
        if (adult_lockout)
        {
            for (i=0; i<g_animWallCnt; i++)
                switch (DYNAMICTILEMAP(wall[animwall[i].wallnum].picnum))
                {
                case FEMPIC1__STATIC:
                    wall[animwall[i].wallnum].picnum = BLANKSCREEN;
                    break;
                case FEMPIC2__STATIC:
                case FEMPIC3__STATIC:
                    wall[animwall[i].wallnum].picnum = SCREENBREAK6;
                    break;
                }
        }
#if 0
        else
        {
            for (i=0; i<g_numAnimWalls; i++)
                if (wall[animwall[i].wallnum].extra >= 0)
                    wall[animwall[i].wallnum].picnum = wall[animwall[i].wallnum].extra;
        }
#endif
    }

    //5
    G_ResetInterpolations();

    //6
    g_showShareware = 0;
    if (savegamep)
        everyothertime = 0;

    //7
    for (i=0; i<MAXPLAYERS; i++)
        g_player[i].playerquitflag = 1;

    // ----------

    //7.5
    if (savegamep)
    {
        ready2send = 1;
        G_ClearFIFO();
        Net_WaitForEverybody();
    }

    //8
    // if (savegamep)  ?
    G_ResetTimers(0);

#ifdef POLYMER
    //9
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_loadboard();

    // this light pointer nulling needs to be outside the videoGetRenderMode check
    // because we might be loading the savegame using another renderer but
    // change to Polymer later
    for (i=0; i<MAXSPRITES; i++)
    {
        actor[i].lightptr = NULL;
        actor[i].lightId = -1;
    }
#endif
    for (i=0; i<MAXPLAYERS; i++)
        g_player[i].ps->drug_timer = 0;

    G_InitRRRASkies();
}

////////// END GENERIC SAVING/LOADING SYSTEM //////////


END_RR_NS
