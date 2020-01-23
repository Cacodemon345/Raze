//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors
Modified by Raze developers and contributors

This file was part of EDuke32.

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

#include "osdcmds.h"

#include "ns.h"

#include "build.h"

#include "keys.h"
#include "names2.h"
#include "panel.h"
#include "game.h"
#include "mytypes.h"
#include "text.h"

#include "gamecontrol.h"
#include "gstrings.h"

#include "common.h"
#include "common/2d/v_text.h"

#include "cheats.h"
#include "demo.h"  // g_firstDemoFile[]
#include "menus.h"
#include "mapinfo.h"

BEGIN_SW_NS

char boardfilename[BMAX_PATH] = {0};

struct osdcmd_cheatsinfo osdcmd_cheatsinfo_stat = { -1, 0, 0 };

#if 0
static int osdcmd_map(osdcmdptr_t parm)
{
    char filename[BMAX_PATH];

    const int32_t wildcardp = parm->numparms==1 &&
        (Bstrchr(parm->parms[0], '*') != NULL);

    if (parm->numparms != 1 || wildcardp)
    {
        return OSDCMD_SHOWHELP;
    }

    maybe_append_ext(filename, sizeof(filename), parm->parms[0], ".map");

    if (!fileSystem.FileExists(filename))
    {
        OSD_Printf(OSD_ERROR "map: file \"%s\" not found.\n", filename);
        return OSDCMD_OK;
    }

    boardfilename[0] = '/';
    boardfilename[1] = 0;
    strcat(boardfilename, filename);

    if (numplayers > 1)
    {
        return OSDCMD_OK;
    }

    osdcmd_cheatsinfo_stat.cheatnum = -1;
    //ud.m_volume_number = 0;
    m_level_number = 7;

    //ud.m_monsters_off = ud.monsters_off = 0;

    //ud.m_respawn_items = 0;
    //ud.m_respawn_inventory = 0;

    //ud.multimode = 1;

    if (g_player[myconnectindex].ps->gm & MODE_GAME)
    {
        //G_NewGame(ud.m_volume_number, m_level_number, ud.m_player_skill);
        g_player[myconnectindex].ps->gm = MODE_RESTART;
    }
    else G_NewGame_EnterLevel();

    return OSDCMD_OK;
}

// demo <demonum or demofn> [<prof>]
//
// To profile a demo ("timedemo mode"), <prof> can be given in the range 0-8,
// which will start to replay it as fast as possible, rendering <prof> frames
// for each gametic.
//
// Notes:
//  * The demos should be recorded with demorec_diffs set to 0, so that the
//    game state updates are actually computed.
//  * Currently, the profiling can only be aborted on SDL 1.2 builds by
//    pressing any key.
//  * With <prof> greater than 1, interpolation should be calculated properly,
//    though this has not been verified by looking at the frames.
//  * When testing whether a change in the source has an effect on performance,
//    the variance of the run times MUST be taken into account (that is, the
//    replaying must be performed multiple times for the old and new versions,
//    etc.)
static int osdcmd_demo(osdcmdptr_t parm)
{
    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    if (g_player[myconnectindex].ps->gm & MODE_GAME)
    {
        OSD_Printf("demo: Must not be in a game.\n");
        return OSDCMD_OK;
    }

    if (parm->numparms != 1 && parm->numparms != 2)
        return OSDCMD_SHOWHELP;

    {
        int32_t prof = parm->numparms==2 ? Batoi(parm->parms[1]) : -1;

        Demo_SetFirst(parm->parms[0]);
        Demo_PlayFirst(clamp(prof, -1, 8)+1, 0);
    }

    return OSDCMD_OK;
}
#endif

static int osdcmd_activatecheat(osdcmdptr_t parm)
{
    if (parm->numparms != 1)
        return OSDCMD_SHOWHELP;

    memset(MessageInputString, '\0', sizeof(MessageInputString));
    strcpy(MessageInputString, parm->parms[0]);
    CheatInput();

    return OSDCMD_OK;
}

static int osdcmd_god(osdcmdptr_t UNUSED(parm))
{
    UNREFERENCED_CONST_PARAMETER(parm);

    GodCheat(Player, "swgod");

    return OSDCMD_OK;
}

static int osdcmd_noclip(osdcmdptr_t UNUSED(parm))
{
    UNREFERENCED_CONST_PARAMETER(parm);

    ClipCheat(Player, "swghost");

    return OSDCMD_OK;
}

int osdcmd_restartmap(osdcmdptr_t UNUSED(parm))
{
    UNREFERENCED_CONST_PARAMETER(parm);

    RestartCheat(Player, "swstart");

    return OSDCMD_OK;
}

#if 0
static int osdcmd_spawn(osdcmdptr_t parm)
{
    int32_t picnum = 0;
    uint16_t cstat=0;
    char pal=0;
    int16_t ang=0;
    int16_t set=0, idx;
    vec3_t vect;

    if (numplayers > 1 || !(g_player[myconnectindex].ps->gm & MODE_GAME))
    {
        OSD_Printf("spawn: Can't spawn sprites in multiplayer games or demos\n");
        return OSDCMD_OK;
    }

    switch (parm->numparms)
    {
    case 7: // x,y,z
        vect.x = Batol(parm->parms[4]);
        vect.y = Batol(parm->parms[5]);
        vect.z = Batol(parm->parms[6]);
        set |= 8;
        fallthrough__;
    case 4: // ang
        ang = Batol(parm->parms[3]) & 2047;
        set |= 4;
        fallthrough__;
    case 3: // cstat
        cstat = (uint16_t)Batol(parm->parms[2]);
        set |= 2;
        fallthrough__;
    case 2: // pal
        pal = (uint8_t)Batol(parm->parms[1]);
        set |= 1;
        fallthrough__;
    case 1: // tile number
        if (isdigit(parm->parms[0][0]))
        {
            picnum = Batol(parm->parms[0]);
        }
        else
        {
            int32_t i;
            int32_t j;

            for (j=0; j<2; j++)
            {
                for (i=0; i<g_labelCnt; i++)
                {
                    if ((j == 0 && !Bstrcmp(label+(i<<6),     parm->parms[0])) ||
                        (j == 1 && !Bstrcasecmp(label+(i<<6), parm->parms[0])))
                    {
                        picnum = labelcode[i];
                        break;
                    }
                }

                if (i < g_labelCnt)
                    break;
            }
            if (i==g_labelCnt)
            {
                OSD_Printf("spawn: Invalid tile label given\n");
                return OSDCMD_OK;
            }
        }

        if ((uint32_t)picnum >= MAXUSERTILES)
        {
            OSD_Printf("spawn: Invalid tile number\n");
            return OSDCMD_OK;
        }
        break;

    default:
        return OSDCMD_SHOWHELP;
    }

    idx = A_Spawn(g_player[myconnectindex].ps->i, picnum);
    if (set & 1) sprite[idx].pal = (uint8_t)pal;
    if (set & 2) sprite[idx].cstat = (int16_t)cstat;
    if (set & 4) sprite[idx].ang = ang;
    if (set & 8)
    {
        if (setsprite(idx, &vect) < 0)
        {
            OSD_Printf("spawn: Sprite can't be spawned into null space\n");
            A_DeleteSprite(idx);
        }
    }

    return OSDCMD_OK;
}

static int osdcmd_setvar(osdcmdptr_t parm)
{
    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    if (parm->numparms != 2)
        return OSDCMD_SHOWHELP;
    
    int i = hash_find(&h_gamevars, parm->parms[1]);
    int const newValue = (i == -1) ? Batol(parm->parms[1]) : Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex);

    if ((i = hash_find(&h_gamevars, parm->parms[0])) >= 0)
    {
        Gv_SetVar(i, newValue, g_player[myconnectindex].ps->i, myconnectindex);

        OSD_Printf("Variable \"%s\" now has value %d (input: %d)\n", aGameVars[i].szLabel,
                   Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex), newValue);
    }
    else
    {
        OSD_Printf("setvar: \"%s\" is not a game variable!\n", parm->parms[0]);
        return OSDCMD_SHOWHELP;
    }

    return OSDCMD_OK;
}

static int osdcmd_addlogvar(osdcmdptr_t parm)
{
    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    if (parm->numparms != 1)
        return OSDCMD_SHOWHELP;

    int const i = hash_find(&h_gamevars, parm->parms[0]);

    if (i >= 0)
        OSD_Printf("Variable \"%s\" has value %d, default %d\n", parm->parms[0], Gv_GetVar(i, g_player[screenpeek].ps->i, screenpeek), (int)aGameVars[i].defaultValue);
    else
    {
        OSD_Printf("addlogvar: %s is not a game variable!\n", parm->parms[0]);
        return OSDCMD_SHOWHELP;
    }

    return OSDCMD_OK;
}

static int osdcmd_setactorvar(osdcmdptr_t parm)
{
    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    if (parm->numparms != 3)
        return OSDCMD_SHOWHELP;

    int16_t const spriteNum = Batol(parm->parms[0]);

    if ((unsigned)spriteNum >= MAXSPRITES)
    {
        OSD_Printf("setactorvar: Invalid sprite number!\n");
        return OSDCMD_OK;
    }

    // get value to set
    int i = hash_find(&h_gamevars, parm->parms[2]);
    int const newValue = (i >= 0) ? Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex) : Batol(parm->parms[2]);

    if ((i = hash_find(&h_gamevars, parm->parms[1])) >= 0)
    {
        Gv_SetVar(i, newValue, spriteNum, myconnectindex);

        OSD_Printf("Variable \"%s\" for sprite %d value is now %d (input: %d)\n", aGameVars[i].szLabel, spriteNum,
                   Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex), newValue);
    }
    else
    {
        OSD_Printf("setactorvar: %s is not a game variable!\n", parm->parms[1]);
        return OSDCMD_SHOWHELP;
    }

    return OSDCMD_OK;
}
#endif

static int osdcmd_give(osdcmdptr_t parm)
{
    int32_t i;

//    if (numplayers != 1 || (g_player[myconnectindex].ps->gm & MODE_GAME) == 0 ||
//            g_player[myconnectindex].ps->dead_flag != 0)
//    {
//        OSD_Printf("give: Cannot give while dead or not in a single-player game.\n");
//        return OSDCMD_OK;
//    }

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if (!Bstrcasecmp(parm->parms[0], "all"))
    {
        ItemCheat(Player, "swgimme");
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "health"))
    {
        HealCheat(Player, "swmedic");
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "weapons"))
    {
	WeaponCheat(Player, "swguns");
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "ammo"))
    {
//        for (i=MAX_WEAPONS-(VOLUMEONE?6:1)-1; i>=PISTOL_WEAPON; i--)
//            P_AddAmmo(g_player[myconnectindex].ps,i,g_player[myconnectindex].ps->max_ammo_amount[i]);
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "armor"))
    {
//        g_player[myconnectindex].ps->inv_amount[GET_SHIELD] = 100;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "keys"))
    {
        KeysCheat(Player,"swkeys");
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "inventory"))
    {
        ItemCheat(Player, "swgimme");
        return OSDCMD_OK;
    }
    return OSDCMD_SHOWHELP;
}

#if 0
void onvideomodechange(int32_t newmode)
{
    uint8_t palid;

    // XXX?
//    if (!newmode || g_player[screenpeek].ps->palette < BASEPALCOUNT)
//        palid = g_player[screenpeek].ps->palette;
//    else
//        palid = BASEPAL;

#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
    {
        int32_t i = 0;

        while (i < MAXSPRITES)
        {
            if (actor[i].lightptr)
            {
                polymer_deletelight(actor[i].lightId);
                actor[i].lightptr = NULL;
                actor[i].lightId = -1;
            }
            i++;
        }
    }
#endif

    videoSetPalette(0, palid, 0);
    g_restorePalette = -1;
    g_crosshairSum = -1;
}
#endif

#if 0
static int osdcmd_dumpmapstate(osdfuncparm_t const * const)
{
    // this command takes no parameters

    DumpMapStateHistory();

    return OSDCMD_OK;
}

static int osdcmd_printtimes(osdcmdptr_t UNUSED(parm))
{
    UNREFERENCED_CONST_PARAMETER(parm);

    char buf[32];
    int32_t maxlen = 0;
    int32_t haveev=0, haveac=0;
    static char const s_event_[] = "EVENT_";
    int constexpr strlen_event_  = ARRAY_SIZE(s_event_) - 1;

    for (auto & EventName : EventNames)
    {
        int const len = Bstrlen(EventName+strlen_event_);
        Bassert(len < ARRAY_SSIZE(buf));
        maxlen = max(len, maxlen);
    }

    for (int i=0; i<MAXEVENTS; i++)
        if (g_eventCalls[i])
        {
            int32_t n=Bsprintf(buf, "%s", EventNames[i]+strlen_event_);

            if (!haveev)
            {
                haveev = 1;
                OSD_Printf("\nevent times: event, total calls, total time [ms], mean time/call [us]\n");
            }

            buf[n] = 0;

            OSD_Printf("%17s, %8d, %10.3f, %10.3f,\n",
                buf, g_eventCalls[i], g_eventTotalMs[i],
                1000*g_eventTotalMs[i]/g_eventCalls[i]);
        }

    for (int i=0; i<MAXTILES; i++)
        if (g_actorCalls[i])
        {
            if (!haveac)
            {
                haveac = 1;
                OSD_Printf("\nactor times: tile, total calls, total time [ms], {min,mean,max} time/call [us]\n");
            }

            buf[0] = 0;

            for (int ii=0; ii<g_labelCnt; ii++)
            {
                if (labelcode[ii] == i && labeltype[ii] & LABEL_ACTOR)
                {
                    Bstrcpy(buf, label+(ii<<6));
                    break;
                }
            }

            if (!buf[0]) Bsprintf(buf, "%d", i);

            OSD_Printf("%17s, %8d, %9.3f, %9.3f, %9.3f, %9.3f,\n",
                buf, g_actorCalls[i], g_actorTotalMs[i],
                1000*g_actorMinMs[i],
                1000*g_actorTotalMs[i]/g_actorCalls[i],
                1000*g_actorMaxMs[i]);
        }

    return OSDCMD_OK;
}
#endif

int32_t registerosdcommands(void)
{
    OSD_RegisterFunction("give","give <all|health|weapons|ammo|armor|keys|inventory>: gives requested item", osdcmd_give);
    OSD_RegisterFunction("god","god: toggles god mode", osdcmd_god);
    OSD_RegisterFunction("activatecheat","activatecheat <string>: activates a classic cheat code", osdcmd_activatecheat);

    OSD_RegisterFunction("noclip","noclip: toggles clipping mode", osdcmd_noclip);


//    OSD_RegisterFunction("printtimes", "printtimes: prints VM timing statistics", osdcmd_printtimes);

    OSD_RegisterFunction("restartmap", "restartmap: restarts the current map", osdcmd_restartmap);
//	OSD_RegisterFunction("addlogvar","addlogvar <gamevar>: prints the value of a gamevar", osdcmd_addlogvar);
//    OSD_RegisterFunction("setvar","setvar <gamevar> <value>: sets the value of a gamevar", osdcmd_setvar);
//    OSD_RegisterFunction("setvarvar","setvarvar <gamevar1> <gamevar2>: sets the value of <gamevar1> to <gamevar2>", osdcmd_setvar);
//    OSD_RegisterFunction("setactorvar","setactorvar <actor#> <gamevar> <value>: sets the value of <actor#>'s <gamevar> to <value>", osdcmd_setactorvar);

//    OSD_RegisterFunction("spawn","spawn <picnum> [palnum] [cstat] [ang] [x y z]: spawns a sprite with the given properties",osdcmd_spawn);

    return 0;
}

END_SW_NS
