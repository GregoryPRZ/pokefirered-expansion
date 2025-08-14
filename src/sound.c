#include "global.h"
#include "gba/m4a_internal.h"
#include "sound.h"
#include "battle.h"
#include "m4a.h"
#include "main.h"
#include "pokemon.h"
#include "quest_log.h"
#include "constants/cries.h"
#include "constants/songs.h"
#include "constants/global.h"
#include "task.h"
#include "test_runner.h"

struct Fanfare
{
    u16 songNum;
    u16 duration;
};

typedef u16 (*MusicHandler)(u16);

// TODO: what are these
extern u8 gDisableMapMusicChangeOnMapLoad;
extern u8 gDisableHelpSystemVolumeReduce;

EWRAM_DATA struct MusicPlayerInfo *gMPlay_PokemonCry = NULL;
EWRAM_DATA u8 gPokemonCryBGMDuckingCounter = 0;

static u16 sCurrentMapMusic;
static u16 sNextMapMusic;
static u8 sMapMusicState;
static u8 sMapMusicFadeInSpeed;
static u16 sFanfareCounter;

COMMON_DATA bool8 gDisableMusic = 0;

extern struct ToneData gCryTable[];
extern struct ToneData gCryTable_Reverse[];

static void Task_Fanfare(u8 taskId);
static void CreateFanfareTask(void);
static void RestoreBGMVolumeAfterPokemonCry(void);

static const struct Fanfare sFanfares[] = {
    [FANFARE_LEVEL_UP]      = { MUS_LEVEL_UP,         80 },
    [FANFARE_OBTAIN_ITEM]   = { MUS_OBTAIN_ITEM,     160 },
    [FANFARE_EVOLVED]       = { MUS_EVOLVED,         220 },
    [FANFARE_OBTAIN_TMHM]   = { MUS_OBTAIN_TMHM,     220 },
    [FANFARE_HEAL]          = { MUS_HEAL,            160 },
    [FANFARE_OBTAIN_BADGE]  = { MUS_OBTAIN_BADGE,    340 },
    [FANFARE_MOVE_DELETED]  = { MUS_MOVE_DELETED,    180 },
    [FANFARE_OBTAIN_BERRY]  = { MUS_OBTAIN_BERRY,    120 },
    [FANFARE_SLOTS_JACKPOT] = { MUS_SLOTS_JACKPOT,   250 },
    [FANFARE_SLOTS_WIN]     = { MUS_SLOTS_WIN,       150 },
    [FANFARE_TOO_BAD]       = { MUS_TOO_BAD,         160 },
    [FANFARE_POKE_FLUTE]    = { MUS_POKE_FLUTE,      450 },
    [FANFARE_KEY_ITEM]      = { MUS_OBTAIN_KEY_ITEM, 170 },
    [FANFARE_DEX_EVAL]      = { MUS_DEX_RATING,      196 },
    [FANFARE_HG_OBTAIN_KEY_ITEM]       = { MUS_HG_OBTAIN_KEY_ITEM      , 170 },
    [FANFARE_HG_LEVEL_UP]              = { MUS_HG_LEVEL_UP             ,  80 },
    [FANFARE_HG_HEAL]                  = { MUS_HG_HEAL                 , 160 },
    [FANFARE_HG_DEX_RATING_1]          = { MUS_HG_DEX_RATING_1         , 200 },
    [FANFARE_HG_DEX_RATING_2]          = { MUS_HG_DEX_RATING_2         , 180 },
    [FANFARE_HG_DEX_RATING_3]          = { MUS_HG_DEX_RATING_3         , 220 },
    [FANFARE_HG_DEX_RATING_4]          = { MUS_HG_DEX_RATING_4         , 210 },
    [FANFARE_HG_DEX_RATING_5]          = { MUS_HG_DEX_RATING_5         , 210 },
    [FANFARE_HG_DEX_RATING_6]          = { MUS_HG_DEX_RATING_6         , 370 },
    [FANFARE_HG_RECEIVE_EGG]           = { MUS_HG_OBTAIN_EGG           , 155 },
    [FANFARE_HG_OBTAIN_ITEM]           = { MUS_HG_OBTAIN_ITEM          , 160 },
    [FANFARE_HG_EVOLVED]               = { MUS_HG_EVOLVED              , 240 },
    [FANFARE_HG_OBTAIN_BADGE]          = { MUS_HG_OBTAIN_BADGE         , 340 },
    [FANFARE_HG_OBTAIN_TMHM]           = { MUS_HG_OBTAIN_TMHM          , 220 },
    [FANFARE_HG_VOLTORB_FLIP_1]        = { MUS_HG_CARD_FLIP            , 195 },
    [FANFARE_HG_VOLTORB_FLIP_2]        = { MUS_HG_CARD_FLIP_GAME_OVER  , 240 },
    [FANFARE_HG_ACCESSORY]             = { MUS_HG_OBTAIN_ACCESSORY     , 160 },
    [FANFARE_HG_REGISTER_POKEGEAR]     = { MUS_HG_POKEGEAR_REGISTERED  , 185 },
    [FANFARE_HG_OBTAIN_BERRY]          = { MUS_HG_OBTAIN_BERRY         , 120 },
    [FANFARE_HG_RECEIVE_POKEMON]       = { MUS_HG_RECEIVE_POKEMON      , 150 },
    [FANFARE_HG_MOVE_DELETED]          = { MUS_HG_MOVE_DELETED         , 180 },
    [FANFARE_HG_THIRD_PLACE]           = { MUS_HG_BUG_CONTEST_3RD_PLACE, 130 },
    [FANFARE_HG_SECOND_PLACE]          = { MUS_HG_BUG_CONTEST_2ND_PLACE, 225 },
    [FANFARE_HG_FIRST_PLACE]           = { MUS_HG_BUG_CONTEST_1ST_PLACE, 250 },
    [FANFARE_HG_POKEATHLON_NEW]        = { MUS_HG_POKEATHLON_READY     , 110 },
    [FANFARE_HG_WINNING_POKEATHLON]    = { MUS_HG_POKEATHLON_1ST_PLACE , 144 },
    [FANFARE_HG_OBTAIN_B_POINTS]       = { MUS_HG_OBTAIN_B_POINTS      , 264 },
    [FANFARE_HG_OBTAIN_ARCADE_POINTS]  = { MUS_HG_OBTAIN_ARCADE_POINTS , 175 },
    [FANFARE_HG_OBTAIN_CASTLE_POINTS]  = { MUS_HG_OBTAIN_CASTLE_POINTS , 200 },
    [FANFARE_HG_CLEAR_MINIGAME]        = { MUS_HG_WIN_MINIGAME         , 230 },
    [FANFARE_HG_PARTNER]               = { MUS_HG_LETS_GO_TOGETHER     , 180 },
};

u16 FireRedMusicHandler(u16 songNum) {
    return songNum;
}

u16 HGSSMusicHandler(u16 songNum) {
    switch(songNum) {
        case MUS_HEAL:
            return MUS_HG_HEAL;
        case MUS_LEVEL_UP:
            return MUS_HG_LEVEL_UP;
        case MUS_OBTAIN_ITEM:
            return MUS_HG_OBTAIN_ITEM;
        case MUS_EVOLVED:
            return MUS_HG_EVOLVED;
        case MUS_OBTAIN_BADGE:
            return MUS_HG_OBTAIN_BADGE;
        case MUS_OBTAIN_TMHM:
            return MUS_HG_OBTAIN_TMHM;
        case MUS_OBTAIN_BERRY:
            return MUS_HG_OBTAIN_BERRY;
        case MUS_EVOLUTION_INTRO:
            return MUS_HG_EVOLUTION_NO_INTRO;
        case MUS_EVOLUTION:
            return MUS_HG_EVOLUTION;
        case MUS_RS_VS_GYM_LEADER:
            return MUS_HG_VS_GYM_LEADER;
        case MUS_RS_VS_TRAINER:
            return MUS_HG_VS_TRAINER;
        case MUS_SCHOOL:
            return MUS_HG_LYRA;
        case MUS_SLOTS_JACKPOT:
            return MUS_HG_GAME_CORNER_WIN;
        case MUS_SLOTS_WIN:
            return MUS_HG_GAME_CORNER_WIN;
        case MUS_MOVE_DELETED:
            return MUS_HG_MOVE_DELETED;
        case MUS_TOO_BAD:
            return MUS_HG_RADIO_UNOWN;
        case MUS_FOLLOW_ME:
            return MUS_HG_FOLLOW_ME_1;
        case MUS_GAME_CORNER:
            return MUS_HG_GAME_CORNER;
        case MUS_ROCKET_HIDEOUT:
            return MUS_HG_TEAM_ROCKET_HQ;
        case MUS_GYM:
            return MUS_HG_GYM;
        case MUS_JIGGLYPUFF:
            return MUS_HG_RADIO_LULLABY;
        case MUS_INTRO_FIGHT:
            return MUS_HG_INTRO;
        case MUS_TITLE:
            return MUS_HG_TITLE;
        case MUS_CINNABAR:
            return MUS_HG_CINNABAR;
        case MUS_LAVENDER:
            return MUS_HG_LAVENDER;
        case MUS_HEAL_UNUSED:
            return MUS_HG_HEAL;
        case MUS_CYCLING:
            return MUS_HG_CYCLING;
        case MUS_ENCOUNTER_ROCKET:
            return MUS_HG_ENCOUNTER_ROCKET;
        case MUS_ENCOUNTER_GIRL:
            return MUS_HG_ENCOUNTER_GIRL_1;
        case MUS_ENCOUNTER_BOY:
            return MUS_HG_ENCOUNTER_BOY_1;
        case MUS_HALL_OF_FAME:
            return MUS_HG_E_DENDOURIRI;
        case MUS_VIRIDIAN_FOREST:
            return MUS_HG_VIRIDIAN_FOREST;
        case MUS_MT_MOON:
            return MUS_HG_ROCK_TUNNEL;
        case MUS_POKE_MANSION:
            return MUS_HG_RUINS_OF_ALPH;
        case MUS_CREDITS:
            return MUS_HG_CREDITS;
        case MUS_ROUTE1:
            return MUS_HG_ROUTE1;
        case MUS_ROUTE24:
            return MUS_HG_ROUTE24;
        case MUS_ROUTE3:
            return MUS_HG_ROUTE3;
        case MUS_ROUTE11:
            return MUS_HG_ROUTE11;
        case MUS_VICTORY_ROAD:
            return MUS_HG_VICTORY_ROAD;
        case MUS_VS_GYM_LEADER:
            return MUS_HG_VS_GYM_LEADER_KANTO;
        case MUS_VS_TRAINER:
            return MUS_HG_VS_TRAINER_KANTO;
        case MUS_VS_WILD:
            return MUS_HG_VS_WILD_KANTO;
        case MUS_VS_CHAMPION:
            return MUS_HG_VS_CHAMPION;
        case MUS_PALLET:
            return MUS_HG_PALLET;
        case MUS_OAK_LAB:
            return MUS_HG_ELM_LAB;
        case MUS_OAK:
            return MUS_HG_OAK;
        case MUS_POKE_CENTER:
            return MUS_HG_POKE_CENTER;
        case MUS_SS_ANNE:
            return MUS_HG_SS_AQUA;
        case MUS_SURF:
            return MUS_HG_SURF;
        case MUS_POKE_TOWER:
            return MUS_HG_BELL_TOWER;
        case MUS_SILPH:
            return MUS_HG_ROCKET_TAKEOVER;
        case MUS_FUCHSIA:
            return MUS_HG_CERULEAN;
        case MUS_CELADON:
            return MUS_HG_CELADON;
        case MUS_VICTORY_TRAINER:
            return MUS_HG_VICTORY_TRAINER;
        case MUS_VICTORY_WILD:
            return MUS_HG_VICTORY_TRAINER;
        case MUS_VICTORY_GYM_LEADER:
            return MUS_HG_VICTORY_TRAINER;
        case MUS_VERMILLION:
            return MUS_HG_VERMILION;
        case MUS_PEWTER:
            return MUS_HG_PEWTER;
        case MUS_ENCOUNTER_RIVAL:
            return MUS_HG_ENCOUNTER_RIVAL;
        case MUS_RIVAL_EXIT:
            return MUS_HG_RIVAL_EXIT;
        case MUS_DEX_RATING:
            return MUS_HG_DEX_RATING_1;
        case MUS_OBTAIN_KEY_ITEM:
            return MUS_HG_OBTAIN_KEY_ITEM;
        case MUS_CAUGHT_INTRO:
            return MUS_HG_CAUGHT;
        case MUS_PHOTO:
            return MUS_HG_CAUGHT;
        case MUS_GAME_FREAK:
            return MUS_HG_INTRO;
        case MUS_CAUGHT:
            return MUS_HG_CAUGHT;
        case MUS_NEW_GAME_INSTRUCT:
            return MUS_HG_NEW_GAME;
        case MUS_NEW_GAME_INTRO:
            return MUS_HG_NEW_GAME;
        case MUS_NEW_GAME_EXIT:
            return MUS_HG_NEW_GAME;
        case MUS_POKE_JUMP:
            return MUS_HG_BUG_CATCHING_CONTEST;
        case MUS_UNION_ROOM:
            return MUS_HG_UNION_CAVE;
        case MUS_NET_CENTER:
            return MUS_HG_POKE_CENTER;
        case MUS_MYSTERY_GIFT:
            return MUS_HG_MYSTERY_GIFT;
        case MUS_BERRY_PICK:
            return MUS_HG_OBTAIN_BERRY;
        case MUS_SEVII_CAVE:
            return MUS_HG_UNION_CAVE;
        case MUS_TEACHY_TV_SHOW:
            return MUS_HG_RADIO_OAK;
        case MUS_SEVII_ROUTE:
            return MUS_HG_ROUTE26;
        case MUS_SEVII_DUNGEON:
            return MUS_HG_VIRIDIAN_FOREST;
        case MUS_SEVII_123:
            return MUS_HG_CHERRYGROVE;
        case MUS_SEVII_45:
            return MUS_HG_VIOLET;
        case MUS_SEVII_67:
            return MUS_HG_AZALEA;
        case MUS_POKE_FLUTE:
            return MUS_HG_RADIO_POKE_FLUTE;
        case MUS_VS_DEOXYS:
            return MUS_HG_VS_SUICUNE;
        case MUS_VS_MEWTWO:
            return MUS_HG_VS_SUICUNE;
        case MUS_VS_LEGEND:
            return MUS_HG_VS_SUICUNE;
        case MUS_ENCOUNTER_GYM_LEADER:
            return MUS_HG_ENCOUNTER_KIMONO_GIRL;
        case MUS_ENCOUNTER_DEOXYS:
            return MUS_HG_ENCOUNTER_RIVAL;
        case MUS_TRAINER_TOWER:
            return MUS_HG_B_TOWER;
        case MUS_SLOW_PALLET:
            return MUS_HG_PALLET;
        case MUS_TEACHY_TV_MENU:
            return MUS_HG_RADIO_OAK;
        default:
            return songNum;
    }
}

u16 RegionalMusicHandler(u16 songNum) {
    MusicHandler handler;
    switch (gSaveBlock1Ptr->optionsMusicSet) {
        case OPTIONS_MUSIC_HGSS:
            handler = HGSSMusicHandler;
            break;
        case OPTIONS_MUSIC_FIRERED:
        default:
            handler = FireRedMusicHandler;
            break;
    }

    return handler(songNum);
}

void InitMapMusic(void)
{
    gDisableMusic = FALSE;
    ResetMapMusic();
}

void MapMusicMain(void)
{
    switch (sMapMusicState)
    {
    case 0:
        break;
    case 1:
        sMapMusicState = 2;
        PlayBGM(sCurrentMapMusic);
        break;
    case 2:
    case 3:
    case 4:
        break;
    case 5:
        if (IsBGMStopped())
        {
            sNextMapMusic = 0;
            sMapMusicState = 0;
        }
        break;
    case 6:
        if (IsBGMStopped() && IsFanfareTaskInactive())
        {
            sCurrentMapMusic = sNextMapMusic;
            sNextMapMusic = 0;
            sMapMusicState = 2;
            PlayBGM(sCurrentMapMusic);
        }
        break;
    case 7:
        if (IsBGMStopped() && IsFanfareTaskInactive())
        {
            FadeInNewBGM(sNextMapMusic, sMapMusicFadeInSpeed);
            sCurrentMapMusic = sNextMapMusic;
            sNextMapMusic = 0;
            sMapMusicState = 2;
            sMapMusicFadeInSpeed = 0;
        }
        break;
    }
}

void ResetMapMusic(void)
{
    sCurrentMapMusic = 0;
    sNextMapMusic = 0;
    sMapMusicState = 0;
    sMapMusicFadeInSpeed = 0;
}

u16 GetCurrentMapMusic(void)
{
    return sCurrentMapMusic;
}

void PlayNewMapMusic(u16 songNum)
{
    sCurrentMapMusic = songNum;
    sNextMapMusic = 0;
    sMapMusicState = 1;
}

void StopMapMusic(void)
{
    sCurrentMapMusic = 0;
    sNextMapMusic = 0;
    sMapMusicState = 1;
}

void FadeOutMapMusic(u8 speed)
{
    if (IsNotWaitingForBGMStop())
        FadeOutBGM(speed);
    sCurrentMapMusic = 0;
    sNextMapMusic = 0;
    sMapMusicState = 5;
}

void FadeOutAndPlayNewMapMusic(u16 songNum, u8 speed)
{
    FadeOutMapMusic(speed);
    sCurrentMapMusic = 0;
    sNextMapMusic = songNum;
    sMapMusicState = 6;
}

void FadeOutAndFadeInNewMapMusic(u16 songNum, u8 fadeOutSpeed, u8 fadeInSpeed)
{
    FadeOutMapMusic(fadeOutSpeed);
    sCurrentMapMusic = 0;
    sNextMapMusic = songNum;
    sMapMusicState = 7;
    sMapMusicFadeInSpeed = fadeInSpeed;
}

static void UNUSED FadeInNewMapMusic(u16 songNum, u8 speed)
{
    FadeInNewBGM(songNum, speed);
    sCurrentMapMusic = songNum;
    sNextMapMusic = 0;
    sMapMusicState = 2;
    sMapMusicFadeInSpeed = 0;
}

bool8 IsNotWaitingForBGMStop(void)
{
    if (sMapMusicState == 6)
        return FALSE;
    if (sMapMusicState == 5)
        return FALSE;
    if (sMapMusicState == 7)
        return FALSE;
    return TRUE;
}

void PlayFanfareByFanfareNum(u8 fanfareNum)
{
    u16 songNum;
    if(gQuestLogState == QL_STATE_PLAYBACK)
    {
        sFanfareCounter = 0xFF;
    }
    else
    {
        m4aMPlayStop(&gMPlayInfo_BGM);
        songNum = RegionalMusicHandler(sFanfares[fanfareNum].songNum);
        sFanfareCounter = RegionalMusicHandler(sFanfares[fanfareNum].duration);
        m4aSongNumStart(songNum);
    }
}

bool8 WaitFanfare(bool8 stop)
{
    if (sFanfareCounter)
    {
        sFanfareCounter--;
        return FALSE;
    }
    else
    {
        if (!stop)
            m4aMPlayContinue(&gMPlayInfo_BGM);
        else
            m4aSongNumStart(MUS_DUMMY);

        return TRUE;
    }
}

// Unused
void StopFanfareByFanfareNum(u8 fanfareNum)
{
    m4aSongNumStop(sFanfares[fanfareNum].songNum);
}

void PlayFanfare(u16 songNum)
{
    s32 i;
    for (i = 0; (u32)i < ARRAY_COUNT(sFanfares); i++)
    {
        if (sFanfares[i].songNum == songNum)
        {
            PlayFanfareByFanfareNum(i);
            CreateFanfareTask();
            return;
        }
    }

    // songNum is not in sFanfares
    // Play first fanfare in table instead
    PlayFanfareByFanfareNum(0);
    CreateFanfareTask();
}

bool8 IsFanfareTaskInactive(void)
{
    if (FuncIsActiveTask(Task_Fanfare) == TRUE)
        return FALSE;
    return TRUE;
}

static void Task_Fanfare(u8 taskId)
{
    if (gTestRunnerHeadless)
    {
        DestroyTask(taskId);
        sFanfareCounter = 0;
        return;
    }

    if (sFanfareCounter)
    {
        sFanfareCounter--;
    }
    else
    {
        m4aMPlayContinue(&gMPlayInfo_BGM);
        DestroyTask(taskId);
    }
}

static void CreateFanfareTask(void)
{
    if (FuncIsActiveTask(Task_Fanfare) != TRUE)
        CreateTask(Task_Fanfare, 80);
}

void FadeInNewBGM(u16 songNum, u8 speed)
{
    if (gDisableMusic)
        songNum = 0;
    if (songNum == MUS_NONE)
        songNum = 0;
    m4aSongNumStart(songNum);
    m4aMPlayImmInit(&gMPlayInfo_BGM);
    m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 0);
    m4aSongNumStop(songNum);
    m4aMPlayFadeIn(&gMPlayInfo_BGM, speed);
}

void FadeOutBGMTemporarily(u8 speed)
{
    m4aMPlayFadeOutTemporarily(&gMPlayInfo_BGM, speed);
}

bool8 IsBGMPausedOrStopped(void)
{
    if (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE)
        return TRUE;
    if (!(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_TRACK))
        return TRUE;
    return FALSE;
}

void FadeInBGM(u8 speed)
{
    m4aMPlayFadeIn(&gMPlayInfo_BGM, speed);
}

void FadeOutBGM(u8 speed)
{
    m4aMPlayFadeOut(&gMPlayInfo_BGM, speed);
}

bool8 IsBGMStopped(void)
{
    if (!(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_TRACK))
        return TRUE;
    return FALSE;
}

void PlayCry_Normal(u16 species, s8 pan)
{
    m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 85);
    PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, CRY_MODE_NORMAL);
    gPokemonCryBGMDuckingCounter = 2;
    RestoreBGMVolumeAfterPokemonCry();
}

void PlayCry_NormalNoDucking(u16 species, s8 pan, s8 volume, u8 priority)
{
    PlayCryInternal(species, pan, volume, priority, CRY_MODE_NORMAL);
}

// Assuming it's not CRY_MODE_DOUBLES, this is equivalent to PlayCry_Normal except it allows other modes.
void PlayCry_ByMode(u16 species, s8 pan, u8 mode)
{
    if (mode == CRY_MODE_DOUBLES)
    {
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
    }
    else
    {
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 85);
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
        gPokemonCryBGMDuckingCounter = 2;
        RestoreBGMVolumeAfterPokemonCry();
    }
}

// Used when releasing multiple Pokémon at once in battle.
void PlayCry_ReleaseDouble(u16 species, s8 pan, u8 mode)
{
    if (mode == CRY_MODE_DOUBLES)
    {
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
    }
    else
    {
        if (!(gBattleTypeFlags & BATTLE_TYPE_MULTI))
            m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 85);
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
    }
}

// Duck the BGM but don't restore it. Not present in R/S
void PlayCry_DuckNoRestore(u16 species, s8 pan, u8 mode)
{
    if (mode == CRY_MODE_DOUBLES)
    {
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
    }
    else
    {
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 85);
        PlayCryInternal(species, pan, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
        gPokemonCryBGMDuckingCounter = 2;
    }
}

void PlayCry_Script(u16 species, u8 mode)
{
    if (!QL_IS_PLAYBACK_STATE) // This check is exclusive to FR/LG
    {
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 85);
        PlayCryInternal(species, 0, CRY_VOLUME, CRY_PRIORITY_NORMAL, mode);
    }
    gPokemonCryBGMDuckingCounter = 2;
    RestoreBGMVolumeAfterPokemonCry();
}

void PlayCryInternal(u16 species, s8 pan, s8 volume, u8 priority, u8 mode)
{
    bool32 reverse;
    u32 release;
    u32 length;
    u32 pitch;
    u32 chorus;

    // Set default values
    // May be overridden depending on mode.
    length = 210;
    reverse = FALSE;
    release = 0;
    pitch = 15360;
    chorus = 0;

    switch (mode)
    {
    case CRY_MODE_NORMAL:
        break;
    case CRY_MODE_DOUBLES:
        length = 20;
        release = 225;
        break;
    case CRY_MODE_ENCOUNTER:
        release = 225;
        pitch = 15600;
        chorus = 20;
        volume = 90;
        break;
    case CRY_MODE_HIGH_PITCH:
        length = 50;
        release = 200;
        pitch = 15800;
        chorus = 20;
        volume = 90;
        break;
    case CRY_MODE_ECHO_START:
        length = 25;
        reverse = TRUE;
        release = 100;
        pitch = 15600;
        chorus = 192;
        volume = 90;
        break;
    case CRY_MODE_FAINT:
        release = 200;
        pitch = 14440;
        break;
    case CRY_MODE_ECHO_END:
        release = 220;
        pitch = 15555;
        chorus = 192;
        volume = 90; // FR/LG changed this from 70 to 90
        break;
    case CRY_MODE_ROAR_1:
        length = 10;
        release = 100;
        pitch = 14848;
        break;
    case CRY_MODE_ROAR_2:
        length = 60;
        release = 225;
        pitch = 15616;
        break;
    case CRY_MODE_GROWL_1:
        length = 15;
        reverse = TRUE;
        release = 125;
        pitch = 15200;
        break;
    case CRY_MODE_GROWL_2:
        length = 100;
        release = 225;
        pitch = 15200;
        break;
    case CRY_MODE_WEAK_DOUBLES:
        length = 20;
        release = 225;
        // fallthrough
    case CRY_MODE_WEAK:
        pitch = 15000;
        break;
    }

    SetPokemonCryVolume(volume);
    SetPokemonCryPanpot(pan);
    SetPokemonCryPitch(pitch);
    SetPokemonCryLength(length);
    SetPokemonCryProgress(0);
    SetPokemonCryRelease(release);
    SetPokemonCryChorus(chorus);
    SetPokemonCryPriority(priority);

    enum PokemonCry cryId = GetCryIdBySpecies(species);
    if (cryId != CRY_NONE)
    {
        cryId--;
        gMPlay_PokemonCry = SetPokemonCryTone(reverse ? &gCryTable_Reverse[cryId] : &gCryTable[cryId]);
    }
}

bool8 IsCryFinished(void)
{
    if (FuncIsActiveTask(Task_DuckBGMForPokemonCry) == TRUE)
    {
        return FALSE;
    }
    else
    {
        ClearPokemonCrySongs();
        return TRUE;
    }
}

void StopCryAndClearCrySongs(void)
{
    m4aMPlayStop(gMPlay_PokemonCry);
    ClearPokemonCrySongs();
}

void StopCry(void)
{
    m4aMPlayStop(gMPlay_PokemonCry);
}

bool8 IsCryPlayingOrClearCrySongs(void)
{
    if (IsPokemonCryPlaying(gMPlay_PokemonCry))
    {
        return TRUE;
    }
    else
    {
        ClearPokemonCrySongs();
        return FALSE;
    }
}

bool8 IsCryPlaying(void)
{
    if (IsPokemonCryPlaying(gMPlay_PokemonCry))
        return TRUE;
    else
        return FALSE;
}

void Task_DuckBGMForPokemonCry(u8 taskId)
{
    if (gPokemonCryBGMDuckingCounter)
    {
        gPokemonCryBGMDuckingCounter--;
        return;
    }

    if (!IsPokemonCryPlaying(gMPlay_PokemonCry))
    {
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 256);
        DestroyTask(taskId);
    }
}

static void RestoreBGMVolumeAfterPokemonCry(void)
{
    if (FuncIsActiveTask(Task_DuckBGMForPokemonCry) != TRUE)
        CreateTask(Task_DuckBGMForPokemonCry, 80);
}

void PlayBGM(u16 songNum)
{
    u16 song;
    song = RegionalMusicHandler(songNum);
    if (gDisableMusic)
        songNum = 0;
    if (songNum == MUS_NONE)
        songNum = 0;
    
    m4aSongNumStart(song);
}

void PlaySE(u16 songNum)
{
    if (gDisableMapMusicChangeOnMapLoad == 0 && gQuestLogState != QL_STATE_PLAYBACK)
            m4aSongNumStart(RegionalMusicHandler(songNum));
}

void PlaySE12WithPanning(u16 songNum, s8 pan)
{
    m4aSongNumStart(songNum);
    m4aMPlayImmInit(&gMPlayInfo_SE1);
    m4aMPlayImmInit(&gMPlayInfo_SE2);
    m4aMPlayPanpotControl(&gMPlayInfo_SE1, TRACKS_ALL, pan);
    m4aMPlayPanpotControl(&gMPlayInfo_SE2, TRACKS_ALL, pan);
}

void PlaySE1WithPanning(u16 songNum, s8 pan)
{
    m4aSongNumStart(songNum);
    m4aMPlayImmInit(&gMPlayInfo_SE1);
    m4aMPlayPanpotControl(&gMPlayInfo_SE1, TRACKS_ALL, pan);
}

void PlaySE2WithPanning(u16 songNum, s8 pan)
{
    m4aSongNumStart(songNum);
    m4aMPlayImmInit(&gMPlayInfo_SE2);
    m4aMPlayPanpotControl(&gMPlayInfo_SE2, TRACKS_ALL, pan);
}

void SE12PanpotControl(s8 pan)
{
    m4aMPlayPanpotControl(&gMPlayInfo_SE1, TRACKS_ALL, pan);
    m4aMPlayPanpotControl(&gMPlayInfo_SE2, TRACKS_ALL, pan);
}

bool8 IsSEPlaying(void)
{
    if ((gMPlayInfo_SE1.status & MUSICPLAYER_STATUS_PAUSE) && (gMPlayInfo_SE2.status & MUSICPLAYER_STATUS_PAUSE))
        return FALSE;
    if (!(gMPlayInfo_SE1.status & MUSICPLAYER_STATUS_TRACK) && !(gMPlayInfo_SE2.status & MUSICPLAYER_STATUS_TRACK))
        return FALSE;
    return TRUE;
}

bool8 IsBGMPlaying(void)
{
    if (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE)
        return FALSE;
    if (!(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_TRACK))
        return FALSE;
    return TRUE;
}

bool8 IsSpecialSEPlaying(void)
{
    if (gMPlayInfo_SE3.status & MUSICPLAYER_STATUS_PAUSE)
        return FALSE;
    if (!(gMPlayInfo_SE3.status & MUSICPLAYER_STATUS_TRACK))
        return FALSE;
    return TRUE;
}

void SetBGMVolume_SuppressHelpSystemReduction(u16 volume)
{
    gDisableHelpSystemVolumeReduce = TRUE;
    m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, volume);
}

void BGMVolumeMax_EnableHelpSystemReduction(void)
{
    gDisableHelpSystemVolumeReduce = FALSE;
    m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 256);
}
