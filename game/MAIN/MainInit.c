#include <common.h>

#if defined(CTR_NATIVE)
#include "platform/native_memory.h"
#include "platform/native_log.h"

enum
{
	NATIVE_RENDER_BUCKET_MAX_RETAIL_BYTES = 0x1000,
	NATIVE_RENDER_BUCKET_RETAIL_ENTRY_BYTES = 0x8,
	NATIVE_RENDER_BUCKET_MAX_ENTRIES =
	    NATIVE_RENDER_BUCKET_MAX_RETAIL_BYTES / NATIVE_RENDER_BUCKET_RETAIL_ENTRY_BYTES + 1,
};

struct NativeRenderBucketEntryStorage
{
	void *word[2];
};

static struct NativeRenderBucketEntryStorage
    s_nativeRenderBucketStorage[NATIVE_RENDER_BUCKET_MAX_ENTRIES];

#if UINTPTR_MAX > UINT32_MAX
// The retail stack-pool strides only accommodate retail-width objects. Keep
// the retail item counts, but widen each LP64 slot for the largest object that
// can be requested from that pool. PROC_BirthWithObject deliberately requires
// the request to be smaller than the usable slot, hence the extra byte before
// alignment.
#define NATIVE_STACK_POOL_ITEM_SIZE(maxObjectType) \
	JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Item) + sizeof(maxObjectType) + 1u)

enum
{
	NATIVE_SMALL_STACK_ITEM_SIZE = NATIVE_STACK_POOL_ITEM_SIZE(struct WoodDoor),
	NATIVE_MEDIUM_STACK_ITEM_SIZE = NATIVE_STACK_POOL_ITEM_SIZE(struct WarpPad),
	NATIVE_RETAIL_THREAD_ITEM_SIZE = 0x48,
	NATIVE_RETAIL_INSTANCE_ITEM_SIZE = 0x74,
	NATIVE_RETAIL_INSTANCE_DRAW_PLAYER_SIZE = 0x88,
	NATIVE_RETAIL_SMALL_STACK_ITEM_SIZE = 0x48,
	NATIVE_RETAIL_MEDIUM_STACK_ITEM_SIZE = 0x88,
	NATIVE_RETAIL_LARGE_STACK_ITEM_SIZE = 0x670,
	NATIVE_RETAIL_PARTICLE_ITEM_SIZE = 0x7c,
	NATIVE_RETAIL_RAIN_ITEM_SIZE = 0x28,
};

#define NATIVE_STACK_POOL_ASSERT_FITS(itemSize, objectType) \
	CTR_STATIC_ASSERT(sizeof(objectType) < (itemSize) - sizeof(struct Item))

NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct WoodDoor);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct MineWeapon);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct UiElement3D);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct Baron);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct Seal);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct Follower);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct SelectProfileLoadSaveObj);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct MaskHeadWeapon);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct Turbo);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct RainCloud);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct SaveObj);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct BossGarageDoor);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, struct CsPodiumCameraThreadObj);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_SMALL_STACK_ITEM_SIZE, void *[3]);

NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct WarpPad);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct CutsceneObj);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct TrackerWeapon);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct Title);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct Shield);
NATIVE_STACK_POOL_ASSERT_FITS(NATIVE_MEDIUM_STACK_ITEM_SIZE, struct Prize);

#undef NATIVE_STACK_POOL_ASSERT_FITS

#define NATIVE_POOL_MAX_OVERHEAD(count, nativeSize, retailSize) \
	((count) * ((nativeSize) - (retailSize)))

CTR_STATIC_ASSERT(
    NATIVE_POOL_MAX_OVERHEAD(0x60, JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Thread)), NATIVE_RETAIL_THREAD_ITEM_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(
            0x80, JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Instance) + 4u * sizeof(struct InstDrawPerPlayer)),
            NATIVE_RETAIL_INSTANCE_ITEM_SIZE + 4u * NATIVE_RETAIL_INSTANCE_DRAW_PLAYER_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(0x64, NATIVE_SMALL_STACK_ITEM_SIZE, NATIVE_RETAIL_SMALL_STACK_ITEM_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(0x20, NATIVE_MEDIUM_STACK_ITEM_SIZE, NATIVE_RETAIL_MEDIUM_STACK_ITEM_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(0x8, DRIVER_LARGE_STACK_ITEM_SIZE, NATIVE_RETAIL_LARGE_STACK_ITEM_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(0x80, JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Particle)), NATIVE_RETAIL_PARTICLE_ITEM_SIZE) +
        NATIVE_POOL_MAX_OVERHEAD(0x8, JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct RainLocal)), NATIVE_RETAIL_RAIN_ITEM_SIZE) ==
    CTR_NATIVE_MEMPACK_LP64_POOL_OVERHEAD_MAX);

#undef NATIVE_POOL_MAX_OVERHEAD

static u32 MainInit_NativeJitPoolOverhead(u32 gameMode, int renderBucketSize, int poolScale, int numPlayers)
{
	const int threadCount = (renderBucketSize * 3) >> 7;
	const int instanceCount = renderBucketSize >> 5;
	const int smallStackCount = (poolScale * 0x19) >> 10;
	const int mediumStackCount = poolScale >> 7;
	const int driverCount = ((gameMode & MAIN_MENU) != 0) ? 4 : poolScale >> 9;
	const int particleCount = poolScale >> 5;
	const int rainCount = poolScale >> 9;
	const int nativeInstanceSize =
	    JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Instance) + sizeof(struct InstDrawPerPlayer) * (u32)numPlayers);
	const int retailInstanceSize =
	    NATIVE_RETAIL_INSTANCE_ITEM_SIZE + NATIVE_RETAIL_INSTANCE_DRAW_PLAYER_SIZE * numPlayers;
	u32 overhead = 0;

	overhead += (u32)threadCount *
	            (JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Thread)) - NATIVE_RETAIL_THREAD_ITEM_SIZE);
	overhead += (u32)instanceCount * (u32)(nativeInstanceSize - retailInstanceSize);
	overhead += (u32)smallStackCount *
	            (NATIVE_SMALL_STACK_ITEM_SIZE - NATIVE_RETAIL_SMALL_STACK_ITEM_SIZE);
	overhead += (u32)mediumStackCount *
	            (NATIVE_MEDIUM_STACK_ITEM_SIZE - NATIVE_RETAIL_MEDIUM_STACK_ITEM_SIZE);
	overhead += (u32)driverCount *
	            (DRIVER_LARGE_STACK_ITEM_SIZE - NATIVE_RETAIL_LARGE_STACK_ITEM_SIZE);
	overhead += (u32)particleCount *
	            (JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct Particle)) - NATIVE_RETAIL_PARTICLE_ITEM_SIZE);
	overhead += (u32)rainCount *
	            (JITPOOL_ALIGN_ITEM_STRIDE(sizeof(struct RainLocal)) - NATIVE_RETAIL_RAIN_ITEM_SIZE);
	return overhead;
}

static void MainInit_ReserveUnusedNativeJitPoolOverhead(u32 gameMode, int renderBucketSize, int poolScale, int numPlayers)
{
	const u32 overhead = MainInit_NativeJitPoolOverhead(gameMode, renderBucketSize, poolScale, numPlayers);

	if (overhead > CTR_NATIVE_MEMPACK_LP64_POOL_OVERHEAD_MAX)
	{
		CTR_ErrorScreen(0xff, 0, 0);
		return;
	}

	if (overhead < CTR_NATIVE_MEMPACK_LP64_POOL_OVERHEAD_MAX)
	{
		MEMPACK_AllocMem((int)(CTR_NATIVE_MEMPACK_LP64_POOL_OVERHEAD_MAX - overhead));
	}
}
#endif
#endif

void MainInit_RebindNativeRuntimeStorage(struct GameTracker *gGT)
{
#if defined(CTR_NATIVE)
	if (gGT != NULL)
	{
		gGT->ptrRenderBucketInstance = s_nativeRenderBucketStorage;
	}
#else
	(void)gGT;
#endif
}

#ifdef CTR_NATIVE
static int MainInit_VisMemHasActivePlayerLists(const struct GameTracker *gGT, const struct VisMem *visMem,
					      const struct mesh_info *mesh)
{
	if ((gGT == NULL) || (gGT->level1 == NULL) || (visMem == NULL) || (mesh == NULL) ||
	    (gGT->numPlyrCurrGame > 4))
	{
		return 0;
	}

	for (int playerIndex = 0; playerIndex < gGT->numPlyrCurrGame; playerIndex++)
	{
		const char *missingList = NULL;

		if ((mesh->numBspNodes > 0) && (visMem->visLeafList[playerIndex] == NULL))
		{
			missingList = "leaf";
		}
		else if ((mesh->numQuadBlock > 0) && (visMem->visFaceList[playerIndex] == NULL))
		{
			missingList = "face";
		}
		else if (((gGT->level1->configFlags & 4) == 0) && (gGT->level1->numWaterVertices > 0) &&
			 (visMem->visOVertList[playerIndex] == NULL))
		{
			missingList = "ocean-vertex";
		}
		else if (((gGT->level1->configFlags & 4) != 0) && (gGT->level1->numSCVert > 0) &&
			 (visMem->visSCVertList[playerIndex] == NULL))
		{
			missingList = "scenery-vertex";
		}
		else if ((mesh->numBspNodes > 0) && (visMem->bspList[playerIndex] == NULL))
		{
			missingList = "BSP";
		}

		if (missingList != NULL)
		{
			Platform_LogError("[CTR AssetRef] active level visibility list missing: player=%d players=%d list=%s level=%p mesh=%p\n",
					  playerIndex, gGT->numPlyrCurrGame, missingList, (const void *)gGT->level1,
					  (const void *)mesh);
			return 0;
		}
	}

	return 1;
}

static void MainInit_InitVisMemBspListNodes(struct VisMem *visMem, struct mesh_info *mesh)
{
	struct BSP *bspRoot = MeshInfo_GetBspRoot(mesh, "MainInit visibility BSP root");

	if (bspRoot == NULL)
	{
		return;
	}

	for (int playerIndex = 0; playerIndex < 4; playerIndex++)
	{
		struct VisMemBspListNode *bspList = visMem->bspList[playerIndex];

		if (bspList == NULL)
		{
			continue;
		}

		for (int bspIndex = 0; bspIndex < mesh->numBspNodes; bspIndex++)
		{
			// NOTE(aalhendi): Native 226 reads the retained BSP pointer; RenderLists only rewrites the link word.
			bspList[bspIndex].next = NULL;
			bspList[bspIndex].bsp = &bspRoot[bspIndex];
		}
	}
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003af84-0x8003b008 for the retail path.
void MainInit_VisMem(struct GameTracker *gGT)
{
	struct VisMem *visMem = Level_GetVisMem(gGT->level1, "MainInit visibility memory");
#ifdef CTR_NATIVE
	struct mesh_info *mesh = Level_GetMeshInfo(gGT->level1, "MainInit visibility mesh");

	if (!MainInit_VisMemHasActivePlayerLists(gGT, visMem, mesh))
	{
		visMem = NULL;
	}
#endif
	gGT->visMem1 = visMem;

	if (visMem == NULL)
	{
		return;
	}

	for (int i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		visMem->visLeafSrc[i] = NULL;
		visMem->visFaceSrc[i] = NULL;
		visMem->visOVertSrc[i] = NULL;
		visMem->visSCVertSrc[i] = NULL;
	}

#ifdef CTR_NATIVE
	MainInit_InitVisMemBspListNodes(visMem, mesh);
#endif
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b008-0x8003b0f0
void MainInit_RainBuffer(struct GameTracker *gGT)
{
	u8 numPlyr = gGT->numPlyrCurrGame;

	if (numPlyr == 0)
	{
		return;
	}

	for (int i = 0; i < numPlyr; i++)
	{
		struct RainBuffer *dst = &gGT->rainBuffer[i];
		const u32 *srcWords = (const u32 *)(const void *)&gGT->level1->rainBuffer;
		u32 *dstWords = (u32 *)(void *)dst;

		for (int word = 0; word < (int)(sizeof(struct RainBuffer) / sizeof(u32)); word += 4)
		{
			dstWords[word + 0] = srcWords[word + 0];
			dstWords[word + 1] = srcWords[word + 1];
			dstWords[word + 2] = srcWords[word + 2];
			dstWords[word + 3] = srcWords[word + 3];
		}

		dst->numParticles_curr /= numPlyr;
		dst->numParticles_max = (s16)((u16)dst->numParticles_max / numPlyr);
	}
}

static int MainInit_GetPrimMemSize(struct GameTracker *gGT)
{
	int levelID;

	// adv garage
	if (gGT->levelID == ADVENTURE_GARAGE)
	{
		return 0x1b800;
	}

	// main menu
	if ((gGT->gameMode1 & MAIN_MENU) != 0)
	{
		return 0x17c00;
	}

	levelID = gGT->levelID;

	switch (gGT->numPlyrCurrGame)
	{
	case 0:
		return 0x25800;

	case 1:
		if ((gGT->gameMode1 & ADVENTURE_ARENA) != 0)
		{
			return 0x1c000;
		}

		if ((u32)(levelID - INTRO_RACE_TODAY) < 9)
		{
			return 0x1e000;
		}

		if (levelID < GEM_STONE_VALLEY)
		{
			return data.primMem_SizePerLEV_1P[levelID] << 10;
		}

		return 0x17c00;

	case 2:
		if (levelID < GEM_STONE_VALLEY)
		{
			return data.primMem_SizePerLEV_2P[levelID] << 10;
		}

		return 0x1e000;

	case 3:
	case 4:
		if (levelID < GEM_STONE_VALLEY)
		{
			return data.primMem_SizePerLEV_4P[levelID] << 10;
		}

		return 0x25800;

	default:
		return 0;
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b0f0-0x8003b2d4.
void MainInit_PrimMem(struct GameTracker *gGT)
{
	int size = MainInit_GetPrimMemSize(gGT);

	if (size == 0)
	{
		return;
	}

	MainDB_PrimMem(&gGT->db[0].primMem, size);
	MainDB_PrimMem(&gGT->db[1].primMem, size);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b2d4-0x8003b334.
void MainInit_JitPoolsReset(struct GameTracker *gGT)
{
	JitPool_Clear(&gGT->JitPools.thread);
	JitPool_Clear(&gGT->JitPools.instance);
	JitPool_Clear(&gGT->JitPools.smallStack);
	JitPool_Clear(&gGT->JitPools.mediumStack);
	JitPool_Clear(&gGT->JitPools.largeStack);
	JitPool_Clear(&gGT->JitPools.particle);
	JitPool_Clear(&gGT->JitPools.oscillator);
	JitPool_Clear(&gGT->JitPools.rain);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b334-0x8003b43c.
void MainInit_OTMem(struct GameTracker *gGT)
{
	int size;
	u32 gameMode = gGT->gameMode1;

	if ((gameMode & MAIN_MENU) != 0)
	{
		size = 0x1800;
		goto EndFunc;
	}

	if ((gameMode & ADVENTURE_ARENA) != 0)
	{
		size = 0x2c00;
		goto EndFunc;
	}

	if ((gameMode & BATTLE_MODE) != 0)
	{
		size = 0x8000;
		goto EndFunc;
	}

	// 1P/2P mode
	if (gGT->numPlyrCurrGame < 3)
	{
		size = 0x2000;
		goto EndFunc;
	}

	// 3P/4P mode
	size = 0x3000;

EndFunc:

	MainDB_OTMem(&gGT->db[0].otMem, size);
	MainDB_OTMem(&gGT->db[1].otMem, size);

	// 0x1000 per player, plus 0x18 for linking
	size = ((gGT->numPlyrCurrGame) << 0xC) | 0x18;
	gGT->otSwapchainDB[0] = MEMPACK_AllocMem(size); // "ot1"
	gGT->otSwapchainDB[1] = MEMPACK_AllocMem(size); // "ot2"
}

void MainInit_JitPoolsNew(struct GameTracker *gGT)
{
	// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b43c-0x8003b6d0 for the retail path.
	u32 gameMode = gGT->gameMode1;
	int poolScale = 0x800;
	if ((gameMode & ADVENTURE_ARENA) == 0)
	{
		poolScale = 0x1000;
		if ((gameMode & MAIN_MENU) != 0)
		{
			poolScale = 0x400;
		}
	}

	int renderBucketSize = 0x800;
	if ((gameMode & ADVENTURE_ARENA) == 0)
	{
		renderBucketSize = 0x1000;
		if ((gameMode & MAIN_MENU) != 0)
		{
			renderBucketSize = 0x400;
			if (gGT->levelID == ADVENTURE_GARAGE)
			{
				renderBucketSize = 0x800;
			}
		}
	}

	MEMPACK_PushState();

#if defined(CTR_NATIVE) && UINTPTR_MAX > UINT32_MAX
	MainInit_ReserveUnusedNativeJitPoolOverhead(gameMode, renderBucketSize, poolScale, gGT->numPlyrCurrGame);
#endif

	JitPool_Init(&gGT->JitPools.thread, (renderBucketSize * 3) >> 7, sizeof(struct Thread), rdata.s_ThreadPool);
	JitPool_Init(&gGT->JitPools.instance, renderBucketSize >> 5, sizeof(struct Instance) + (sizeof(struct InstDrawPerPlayer) * gGT->numPlyrCurrGame),
	             rdata.s_InstancePool);
#if defined(CTR_NATIVE) && UINTPTR_MAX > UINT32_MAX
	JitPool_Init(&gGT->JitPools.smallStack, (poolScale * 0x19) >> 10, NATIVE_SMALL_STACK_ITEM_SIZE, rdata.s_SmallStackPool);
	JitPool_Init(&gGT->JitPools.mediumStack, poolScale >> 7, NATIVE_MEDIUM_STACK_ITEM_SIZE, rdata.s_MediumStackPool);
#else
	JitPool_Init(&gGT->JitPools.smallStack, (poolScale * 0x19) >> 10, PROC_STACK_ITEM_SIZE(0x48), rdata.s_SmallStackPool);
	JitPool_Init(&gGT->JitPools.mediumStack, poolScale >> 7, PROC_STACK_ITEM_SIZE(0x88), rdata.s_MediumStackPool);
#endif

	int numDriver = poolScale >> 9;
	if ((gameMode & MAIN_MENU) != 0)
	{
		numDriver = 4;
	}
	JitPool_Init(&gGT->JitPools.largeStack, numDriver, DRIVER_LARGE_STACK_ITEM_SIZE, rdata.s_LargeStackPool);

	int numParticle = poolScale >> 5;
	JitPool_Init(&gGT->JitPools.particle, numParticle, sizeof(struct Particle), rdata.s_ParticlePool);
	// Host pointers widen the oscillator from 24 to 32 bytes on 64-bit builds.
	JitPool_Init(&gGT->JitPools.oscillator, numParticle, sizeof(struct ParticleOscillator), rdata.s_OscillatorPool);
	JitPool_Init(&gGT->JitPools.rain, poolScale >> 9, sizeof(struct RainLocal), rdata.s_RainPool);

#ifndef CTR_NATIVE
	gGT->ptrRenderBucketInstance = MEMPACK_AllocMem(renderBucketSize);
#else
	// Retail budgets eight bytes per entry. Native entries contain two host
	// pointers, so keep the same entry capacity in aligned host-width storage
	// without consuming the retail-pressure mempack.
	gGT->ptrRenderBucketInstance = s_nativeRenderBucketStorage;
#endif

	for (int i = 0; i < 3; i++)
	{
		struct JitPool *pool = (struct JitPool *)((char *)&gGT->JitPools.smallStack + (sizeof(struct JitPool) * i));
		struct Item *item = pool->free.first;
		while (item != NULL)
		{
			void **objectSlot = (void **)((u8 *)item + sizeof(struct Item));
			*objectSlot = objectSlot;
			item = item->next;
		}
	}

	for (int i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		data.PtrClipBuffer[i] = MEMPACK_AllocMem(MainDB_GetClipSize(gGT->levelID, gGT->numPlyrCurrGame) << 2);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b6d0-0x8003b934; CTR_NATIVE gates TT ghost model publication.
void MainInit_Drivers(struct GameTracker *gGT)
{
	u8 numPlyrCurrGame = gGT->numPlyrCurrGame;
	u8 numDrivers;
	int gameMode = gGT->gameMode1;

	for (int i = 0; i < 8; i++)
	{
		gGT->drivers[i] = NULL;
	}

	gGT->numBotsNextGame = 0;

	if ((gameMode & (GAME_CUTSCENE | ADVENTURE_ARENA | MAIN_MENU)) == 0)
	{
		BOTS_Adv_AdjustDifficulty();
	}

	GhostReplay_Init1();

	if (LOAD_IsOpen_RacingOrBattle())
	{
		RB_MinePool_Init();
	}

	// Spawn all players,
	// This MUST be in reverse order,
	// because of threadBucket linked list order
	for (int i = numPlyrCurrGame - 1; i >= 0; i--)
	{
		gGT->drivers[i] = VehBirth_Player(i);
	}

	// spawn all AIs
	if ((
	        // exclude cutscene, relic, Time Trial,
	        // Adventure Hub, Main Menu, Battle
	        ((gameMode & 0x2c122020) == 0) &&

	        // numPlyrCurrGame requires AIs
	        (numPlyrCurrGame < 3)) &&
	    (
	        // in Arcade or Adventure
	        (gameMode & (ARCADE_MODE | ADVENTURE_MODE)) != 0))
	{
		// If you're in Boss Mode
		// 0x80000000
		if (gameMode < 0)
		{
			numDrivers = numPlyrCurrGame + 1;
		}

		// Purple Gem Cup
		else if (

		    // If you are in Adventure cup
		    ((gameMode & ADVENTURE_CUP) != 0) &&

		    // purple gem cup
		    (gGT->cup.cupID == 4))
		{
			numDrivers = numPlyrCurrGame + 4;
		}

		else if (numPlyrCurrGame == 1)
		{
			numDrivers = 8;
		}

		else // if (numPlyrCurrGame == 2)
		{
			numDrivers = 6;
		}

		// Spawn AIs
		for (int i = numPlyrCurrGame; i < numDrivers; i++)
		{
			// spawn an AI at this character index
			BOTS_Driver_Init(i);
		}
	}

	// If number of AIs is not zero
	// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b868-0x8003b894 for AI engine-audio init side effects.
	if (gGT->numBotsNextGame != 0)
	{
		// Init AI engine sounds
		EngineAudio_InitOnce(0x10, HOWL_SFX_CENTER_NO_DISTORTION);
		EngineAudio_InitOnce(0x11, HOWL_SFX_CENTER_NO_DISTORTION);
	}

	// if this is main menu
	if ((gameMode & MAIN_MENU) != 0)
	{
		// fill up 4 players
		for (int i = numPlyrCurrGame; i < 4; i++)
		{
			gGT->drivers[i] = VehBirth_Player(i);
		}
	}

	// if you're in time trial, not main menu, not cutscene.
	// basically, if you're in time trial gameplay
	if ((gameMode & GAME_MODE_TIME_TRIAL_GAMEPLAY_MASK) == TIME_TRIAL)
	{
		GhostReplay_Init2();

		GhostTape_Start();

#if defined(CTR_NATIVE)
		struct Model **humanPlyrDriverModel = &gGT->threadBuckets[PLAYER].thread->inst->model;

		// that's characterIDs[1] from the MPK
		// humanGhost = *humanPlyrDriverModel,

		// then replace with intended P1 model
		*humanPlyrDriverModel = data.driverModelExtras[0].model;
#endif
	}
}

void MainInit_FinalizeInit(struct GameTracker *gGT)
{
	int i;
	int numPlyr;
	struct Driver *d;
	struct Level *lev1;
	struct Instance *inst;

	// === Naughty Dog Bug ===
	// Quitting a race while heldItem is warpball,
	// never resets this flag, and then the game
	// can not give warpball again until you reboot
	gGT->gameMode1 &= ~(WARPBALL_HELD);

	// enable collisions with all temporary walls
	// (adv hub doors, tiger temple teeth, etc)
	sdata->doorAccessFlags = 0;

	// add a bookmark
	MEMPACK_PushState();

	gGT->pushBuffer[0].distanceToScreen_PREV = 0x100;
	gGT->pushBuffer[0].distanceToScreen_CURR = 0x100;

	memset(gGT->threadBuckets, 0, sizeof(gGT->threadBuckets));
	gGT->threadBuckets[STATIC].boolCantPause = 1;
	gGT->threadBuckets[WARPPAD].boolCantPause = 1;
	gGT->threadBuckets[CAMERA].boolCantPause = 1;
	gGT->threadBuckets[HUD].boolCantPause = 1;

	// particles
	gGT->particleList_ordinary = NULL;
	gGT->particleList_heatWarp = NULL;
	gGT->numParticles = 0;

	// deadc0ed, FUN_8006c684
	// RNG stuff
	gGT->deadcoed_struct.state0 = 0x30215400;
	gGT->deadcoed_struct.state1 = 0x493583fe;

	for (i = 0; i < 12; i++)
	{
		gGT->DecalMP[i].inst = NULL;
		*(s16 *)&gGT->DecalMP[i].data[0] = 1000;

		gGT->DecalMP[i].ptrOT1 = 0;
		gGT->DecalMP[i].ptrOT2 = 0;
	}

	MainInit_JitPoolsReset(gGT);

	lev1 = gGT->level1;

	struct CheckpointNode *restartPoints = Level_GetRestartPoints(lev1, "MainInit restart points");
	// NOTE(aalhendi): Native menu LEVs may publish no restart table.
	if (restartPoints != NULL)
	// 0x1d7c
	{
		gGT->trackLength_x_numLaps_x_8 = restartPoints[0].distToFinish * gGT->numLaps * 8;
	}

	MainInit_Drivers(gGT);

	// assume 1P fov
	numPlyr = 1;

	// if you are not in main menu
	if ((gGT->gameMode1 & MAIN_MENU) == 0)
	{
		numPlyr = gGT->numPlyrCurrGame;
	}

	// Initialize four PushBuffer, 4 main screens
	PushBuffer_Init(&gGT->pushBuffer[0], 0, numPlyr);
	PushBuffer_Init(&gGT->pushBuffer[1], 1, numPlyr);
	PushBuffer_Init(&gGT->pushBuffer[2], 2, numPlyr);
	PushBuffer_Init(&gGT->pushBuffer[3], 3, numPlyr);

	struct PushBuffer *pb;

	pb = &gGT->pushBuffer_UI;
	PushBuffer_Init(pb, 0, 1);

	pb->rot.x = 0x800;
	PushBuffer_SetPsyqGeom(pb);
	PushBuffer_SetMatrixVP(pb);

	if ((gGT->hudFlags & HUD_FLAG_INIT_UI_INSTANCES) != 0)
	{
		UI_INSTANCE_InitAll();
	}

	gGT->unk1cac[4] = 2;

	for (i = 0; i < 8; i++)
	{
		// get pointer to player structure of each driver
		d = gGT->drivers[i];

		// if pointer is not nullptr
		if (d == NULL)
		{
			continue;
		}

		inst = d->instSelf;
		if (inst != 0)
		{
			inst->scale = (SVec3){{0xccc, 0xccc, 0xccc}};
		}

		if (i < gGT->numPlyrCurrGame)
		{
			CAM_Init(&gGT->cameraDC[i], i, d, &gGT->pushBuffer[i]);

			// freeze camera of P1, only in main menu
			if (((gGT->gameMode1 & MAIN_MENU) == 0) || (i < 1))
			{
				// remove frozen camera flag
				gGT->cameraDC[i].flags &= ~CAMERA_FLAG_FROZEN;
			}
			else
			{
				gGT->cameraDC[i].flags |= CAMERA_FLAG_FROZEN;
			}
		}
	}

	if (gGT->levelID == MAIN_MENU_LEVEL)
	{
		// 30 seconds
		gGT->demoCountdownTimer = 900;
	}

	// copy InstDef to InstancePool
	INSTANCE_LevInitAll(Level_GetInstDefs(lev1, "MainInit instance definitions"), lev1->numInstances);

	// Debug_ToggleNormalSpawn == normal spawn
	if (gGT->Debug_ToggleNormalSpawn != 0)
	{
		MainGameStart_Initialize(gGT, 1);

		if (gGT->boolDemoMode != 0)
		{
			for (i = 0; i < gGT->numPlyrCurrGame; i++)
			{
				BOTS_Driver_Convert(gGT->drivers[i]);
			}
		}
	}

	// execute all camera thread update functions
	ThTick_RunBucket(gGT->threadBuckets[CAMERA].thread);

// dont write unused variables
#if 0
    // lev -> clearColor rgb
    sdata->LevClearColorRGB[0] = (u32)(char *)(lev1->clearColorRGBA)[0];
    sdata->LevClearColorRGB[1] = (u32)(char *)(lev1->clearColorRGBA)[1];
    sdata->LevClearColorRGB[2] = (u32)(char *)(lev1->clearColorRGBA)[2];
#endif

	// Used in Coco Park, encoded as Blue
	*(int *)&gGT->db[0].drawEnv.isbg = lev1->clearColorRGBA << 8;
	*(int *)&gGT->db[1].drawEnv.isbg = lev1->clearColorRGBA << 8;

	if ((gGT->numPlyrCurrGame == 1) && (lev1->clearColor[0].enable != 0) && (lev1->clearColor[1].enable != 0))
	{
		// set isbg of both DBs to false
		gGT->db[0].drawEnv.isbg = 0;
		gGT->db[1].drawEnv.isbg = 0;
	}
	else
	{
		// set isbg of both DBs to true
		gGT->db[0].drawEnv.isbg = 1;
		gGT->db[1].drawEnv.isbg = 1;
	}

	if (lev1 != NULL)
	{
		struct mesh_info *mesh = Level_GetMeshInfo(lev1, "MainInit level mesh");
		if (mesh != NULL)
		{
			LevInstDef_UnPack(mesh);
		}
	}

	MainInit_VisMem(gGT);

	MainInit_RainBuffer(gGT);

	// animates water, 1P mode
	AnimateWater1P(gGT->timer, lev1->numWaterVertices, Level_GetWater(lev1, "MainInit water"),
		       Level_GetWaterEnvMap(lev1, "MainInit water environment map"), Level_GetVisOVertSrc(lev1, "MainInit ocean visibility"));

	gGT->pushBuffer_UI.fadeFromBlack_desiredResult = 0x1000;
	gGT->pushBuffer_UI.fade_step = 0x200;

	numPlyr = gGT->numPlyrCurrGame;

	// stars
	gGT->stars.numStars = (s16)(lev1->stars.numStars / numPlyr);
	gGT->stars.spread = lev1->stars.spread;
	gGT->stars.seed = lev1->stars.seed;
	gGT->stars.distance = lev1->stars.distance;

	// confetti
	gGT->confetti.numParticles_currWord = 0;
	gGT->confetti.numParticles_max = 0;
	gGT->confetti.vanishRate = 0;
	gGT->confetti.velY = -10;

	for (i = 0; i < 4; i++)
	{
		gGT->winnerIndex[i] = 0;
	}

#if 0
    BOTS_EmptyFunc();
#endif

	if ((gGT->gameMode1 & GAME_CUTSCENE) != 0)
	{
		// freecam mode
		gGT->cameraDC[0].cameraMode = CAMERA_MODE_FREECAM;

		// disable all HUD flags
		gGT->hudFlags = 0;

		CS_Cutscene_Start();
	}

	if ((gGT->gameMode1 & ADVENTURE_ARENA) != 0)
	{
		// 0
		if (gGT->podiumRewardID != NOFUNC)
		{
			CS_Podium_FullScene_Init();
		}
	}

	PickupBots_Init();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c1d4-0x8003c248.
int MainInit_StringToLevID(char *str)
{
	for (int levelID = 0; levelID < 0x41; levelID++)
	{
		char *debugName = data.metaDataLEV[levelID].name_Debug;

		if (strncmp(debugName, str, strlen(debugName)) == 0)
		{
			return levelID;
		}
	}

	return 0;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c248-0x8003c310 for the retail path.
void MainInit_VRAMClear()
{
	DRAWENV drawEnv;

	struct
	{
		int a;
		s16 b1, b2, c, d, e, f;
	} commands;

	SetDefDrawEnv(&drawEnv, 0, 0, 0x400, 0x200);
	drawEnv.dfe = '\x01';
	PutDrawEnv(&drawEnv);

	commands.a = 0x3ffffff;
	commands.b1 = 0;
	commands.b2 = 0x200;
	commands.c = 0;
	commands.d = 0;
	commands.e = 0x3ff;
	commands.f = 0x1ff;
	DrawOTag(&commands);

	commands.d = 0x1ff;
	commands.f = 1;
	DrawOTag(&commands);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c310-0x8003c41c; native wraps
// the VRAM page moves in a platform frame for presentation.
void MainInit_VRAMDisplay()
{
	RECT r;
	DR_MOVE move;

	s16 x[2];
	s16 y[2];

	x[0] = 0;
	x[1] = 0x100;

	y[0] = 0;
	y[1] = 0x128;

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			r.x = x[i] + 0x200;
			r.y = 0x10c;
			r.w = 0x100;
			r.h = 0xd8;

			SetDrawMove(&move, &r, x[i], y[j]);

			move.tag |= 0xffffff;

			DrawOTag(&move);
			DrawSync(0);
		}
	}

#ifdef CTR_NATIVE
	Platform_PresentVRAMDisplay();
#endif
}
