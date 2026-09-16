#include <common.h>

#if defined(CTR_NATIVE)
static void MainFrame_RegisterGpuLinkRanges(struct GameTracker *gGT)
{
	static const char *const primLabels[2] = {"db0 prim", "db1 prim"};
	static const char *const otLabels[2] = {"db0 OT", "db1 OT"};
	static const char *const swapchainLabels[2] = {"swapchain OT0", "swapchain OT1"};

	NativeGpuLinks_Reset();

	// NOTE(aalhendi): Retail links PS1 RAM addresses directly in 24-bit GPU
	// tags. Native keeps the same packet shape, but maps the double-buffered
	// host draw arenas to stable 24-bit tokens before any OT/tag writer runs.
	for (int i = 0; i < 2; i++)
	{
		struct DB *db = &gGT->db[i];
		NativeGpuLinks_RegisterRangeChecked(primLabels[i], db->primMem.start, db->primMem.capacityBytes);
		NativeGpuLinks_RegisterRangeChecked(otLabels[i], db->otMem.start, db->otMem.capacityBytes);
	}

	u32 swapchainOTBytes = ((u32)gGT->numPlyrCurrGame << 12) | 0x18u;
	for (int i = 0; i < 2; i++)
	{
		NativeGpuLinks_RegisterRangeChecked(swapchainLabels[i], gGT->otSwapchainDB[i], swapchainOTBytes);
	}
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034b48-0x80034bbc.
void MainFrame_TogglePauseAudio(b32 bool_pause)
{
	if (bool_pause == 0)
	{
		if (sdata->boolSoundPaused)
		{
			howl_StopAudio(0, 0, 1);
			howl_UnPauseAudio();
			sdata->boolSoundPaused = 0;
		}
	}
	else if (sdata->boolSoundPaused == 0)
	{
		OtherFX_Stop2(1);
		howl_PauseAudio();
		sdata->boolSoundPaused = 1;
	}
	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034bbc-0x80034d54 for the retail path.
void MainFrame_ResetDB(struct GameTracker *gGT)
{
	uint32_t *puVar3;
	int iVar4;
	struct DB *db;
	uint32_t *otSwapchainDB;

	// check if new adv hub should be loaded,
	// this was a random place for ND to put it
	LOAD_Hub_Main(sdata->ptrBigfile1);

	gGT->swapchainIndex = 1 - gGT->swapchainIndex;

	gGT->backBuffer = &gGT->db[gGT->swapchainIndex];
	gGT->frameTimer_MainFrame_ResetDB++;

	otSwapchainDB = gGT->otSwapchainDB[gGT->swapchainIndex];

	db = gGT->backBuffer;
	db->blurCameraMask = 0;
	db->primMem.cursor = db->primMem.start;
	db->primMem.primitiveCount = 0;
	db->otMem.cursor = db->otMem.start;

#if defined(CTR_NATIVE)
	MainFrame_RegisterGpuLinkRanges(gGT);
#endif

	CTR_EmptyFunc_MainFrame_ResetDB();
	DecalGlobal_EmptyFunc_MainFrame_ResetDB();

	ClearOTagR(otSwapchainDB, sdata->gGT->numPlyrCurrGame << 10 | 6);

	for (iVar4 = 0; iVar4 < sdata->gGT->numPlyrCurrGame; iVar4++)
	{
		gGT->pushBuffer[iVar4].ptrOT =
		    (uint32_t *)((u8 *)otSwapchainDB + (sdata->gGT->numPlyrCurrGame - iVar4 - 1) * 0x1000 + 0x18);
	}

	for (; iVar4 < 4; iVar4++)
	{
		// but why?
		gGT->pushBuffer[iVar4].ptrOT = (uint32_t *)((u8 *)otSwapchainDB + 3 * 0x1000 + 0x18);
	}

	puVar3 = (uint32_t *)((u8 *)otSwapchainDB + 4);
	gGT->pushBuffer_UI.ptrOT = puVar3;
	db->otMem.uiOT = puVar3;

#if defined(CTR_NATIVE)
	if (sdata->ptrPushBufferUI != 0)
	{
		struct PushBuffer *wumpaPushBuffer = sdata->ptrPushBufferUI;

		// NOTE(aalhendi): Retail stores PS1 RAM OT addresses here. Native stores
		// host pointers, so reset the fake UI pushbuffer to the current backbuffer
		// before RenderBucket can publish this frame's range metadata.
		wumpaPushBuffer->ptrOT = gGT->pushBuffer_UI.ptrOT;
		wumpaPushBuffer->renderBucketOTRangeEnd = NULL;
		wumpaPushBuffer->renderBucketOTByteOffset = 0;
	}
#endif

	return;
}

#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
#include <platform/native_replay_scheduler.h>
#endif

void MainFrame_GameLogic(struct GameTracker *gGT, struct GamepadSystem *gGamepads)
{
	b32 wasPausedAtFrameStart;
	s16 sVar2;
	u32 uVar3;
	int iVar4;
	DriverFunc pcVar5;
	u32 uVar5;
	struct Driver *psVar8;
	struct Driver *psVar9;
	struct PushBuffer *pushBuffer;
	int iVar11;
	struct Thread *psVar12;

	// Record transitions and a bounded heartbeat, never every particle/frame.
	static int previousLevel = -1;
	static u32 previousMode;
	static u32 diagnosticFrames;
	if (previousLevel != gGT->levelID || previousMode != gGT->gameMode1 || ++diagnosticFrames >= 300)
	{
		Platform_Log("[CTR Game] frame=%u level=%d mode=0x%x players=%d particles=%d particle_free=%d oscillator_free=%d\n",
		             (unsigned)sdata->frameCounter, gGT->levelID, (unsigned)gGT->gameMode1,
		             gGT->numPlyrCurrGame, gGT->numParticles,
		             gGT->JitPools.particle.free.count, gGT->JitPools.oscillator.free.count);
		previousLevel = gGT->levelID;
		previousMode = gGT->gameMode1;
		diagnosticFrames = 0;
	}

	wasPausedAtFrameStart = true;
	if ((gGT->gameMode1 & PAUSE_ALL) == 0)
	{
		wasPausedAtFrameStart = false;
		pushBuffer = gGT->pushBuffer;
		for (psVar12 = gGT->threadBuckets[0].thread; psVar12 != 0; psVar12 = psVar12->siblingThread)
		{
			psVar9 = (struct Driver *)psVar12->object;

			if (psVar9->clockSend)
			{
				psVar9->clockSend--;
			}
			uVar3 = psVar9->clockFlash;
			if (uVar3 == 0)
			{
				if (psVar9->clockReceive == 0)
				{
					uVar3 = (u32)psVar9->clockSend;
					if (uVar3 == 0)
					{
						if ((gGT->clockEffectEnabled & 1) == 0)
						{
							goto LAB_80034e74;
						}
						uVar3 = 10000;
					}
				}
				else
				{
					if ((psVar9->actionsFlagSet & ACTION_RACE_FINISHED) != 0)
					{
						psVar9->clockReceive = 0;
					}
					uVar3 = (u32)psVar9->clockReceive;
				}

#if defined(CTR_NATIVE)
				DISPLAY_Blur_Main(pushBuffer, uVar3);
#endif
			}
			else
			{
#if defined(CTR_NATIVE)
				DISPLAY_Blur_Main(pushBuffer, -uVar3);
#endif
				psVar9->clockFlash--;
			}
		LAB_80034e74:
			pushBuffer = pushBuffer + 1;
		}
		gGT->timer = gGT->timer + 1;
		gGT->framesInThisLEV = gGT->framesInThisLEV + 1;
		gGT->unk1cc4[4] = 0;

		iVar4 = Timer_GetTime_Elapsed(gGT->clockFrameStart, &gGT->clockFrameStart);
		iVar4 = (iVar4 << 5) / 100;

		gGT->elapsedTimeMS = iVar4;
		if (iVar4 < 0)
		{
			gGT->elapsedTimeMS = 0x20;
		}
		if (0x40 < gGT->elapsedTimeMS)
		{
			gGT->elapsedTimeMS = 0x40;
		}
		if ((gGT->gameMode1_prevFrame & PAUSE_ALL) != 0)
		{
			gGT->elapsedTimeMS = 0x20;
		}
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
		// NOTE(aalhendi): Replay playback must not let host RCNT timing decide
		// cutscene/gameplay advancement. Use the recorded PS1-shaped frame delta
		// before msInThisLEV and elapsedEventTime consume it.
		NativeReplayScheduler_ConsumeFrameElapsedTimeMS(&gGT->elapsedTimeMS);
#endif
		gGT->msInThisLEV += gGT->elapsedTimeMS;
		if (gGT->trafficLightsTimer < 1)
		{
			if ((gGT->gameMode1 & DEBUG_MENU) == 0)
			{
				if (gGT->frozenTimeRemaining < 1)
				{
					if ((gGT->gameMode1 & END_OF_RACE) == 0)
					{
						gGT->elapsedEventTime += gGT->elapsedTimeMS;
					}
				}
				else
				{
					iVar4 = gGT->frozenTimeRemaining - gGT->elapsedTimeMS;
					gGT->frozenTimeRemaining = iVar4;
					if (iVar4 < 0)
					{
						gGT->frozenTimeRemaining = 0;
					}
					else
					{
						uVar3 = gGT->timer;
						// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034f84-0x80034fec for frozen-time tick SFX.
						if (uVar3 == (uVar3 / 6) * 6)
						{
							if (uVar3 == (uVar3 / 0xc) * 0xc)
							{
								OtherFX_Play_LowLevel(0x40, '\0', 0x8c9080);
							}
							else
							{
								OtherFX_Play_LowLevel(0x40, '\0', 0x8c8880);
							}
						}
					}
				}
			}
		}
		else
		{
			gGT->elapsedEventTime = 0;
		}

		CTR_CycleTex_AllModels(UINT32_MAX, sdata->PLYROBJECTLIST, gGT->timer);
		CTR_CycleTex_AllModels(gGT->level1->numModels, Level_GetModelRefs(gGT->level1, "MainFrame model references"), gGT->timer);

		psVar8 = 0;
		psVar9 = 0;

#if defined(CTR_NATIVE)
		for (psVar12 = gGT->threadBuckets[0].thread; psVar12 != 0; psVar12 = psVar12->siblingThread)
		{
			struct Driver *currentDriver = psVar12->object;

			if (currentDriver->driverID == 0)
			{
				psVar8 = currentDriver;
				continue;
			}

			if (currentDriver->driverID == 1)
			{
				psVar9 = currentDriver;
			}

			// NOTE(aalhendi): Retail may read PSX low memory before driver 0 appears.
			if (psVar8 == NULL)
			{
				continue;
			}

			if (currentDriver->numTimesAttacking < psVar8->numTimesAttacking)
			{
				psVar9 = psVar8;
				psVar8 = currentDriver;
			}
		}
#endif

		if (((psVar8 != 0) && (psVar9 != 0)) && (iVar4 = (u32)psVar9->numTimesAttacking - (u32)psVar8->numTimesAttacking, psVar8->quip2 < iVar4))
		{
			psVar8->quip2 = (s16)iVar4;
		}

		for (iVar4 = 0; iVar4 < PAUSE; iVar4++)
		{
			if ((((gGT->gameMode1 & DEBUG_MENU) == 0) || ((gGT->threadBuckets[iVar4].boolCantPause & 1) != 0)) &&

			    // if threads exist
			    (gGT->threadBuckets[iVar4].thread != 0))
			{
				if (iVar4 == 0)
				{
					for (psVar12 = gGT->threadBuckets[iVar4].thread; psVar12 != 0; psVar12 = psVar12->siblingThread)
					{
						VehPickupItem_ShootOnCirclePress((struct Driver *)psVar12->object);
					}

					// run all driver funcPtrs,
					// all drivers must run the same DRIVER_FUNC_* stage
					// at the same time, that's why the stages exist
					for (iVar11 = 0; iVar11 < DRIVER_FUNC_COUNT; iVar11++)
					{
						for (psVar12 = gGT->threadBuckets[iVar4].thread; psVar12 != 0; psVar12 = psVar12->siblingThread)
						{
							// if PLYR converted to robotcar at end of race,
							// dont run funcPtrs from inside driver struct
							if (psVar12->funcThTick != 0)
							{
								continue;
							}

							psVar9 = (struct Driver *)psVar12->object;

							pcVar5 = psVar9->funcPtrs[iVar11];

							if (pcVar5 != 0)
							{
								pcVar5(psVar12, psVar9);
							}
						}
					}
				}

				ThTick_RunBucket(gGT->threadBuckets[iVar4].thread);
			}
		}

#if defined(CTR_NATIVE)
		BOTS_UpdateGlobals();
#endif
		GhostTape_WriteMoves(0);
		gGT->unk1cc4[4] = (u32)(gGT->unk1cc4[4] * 10000) / 0x147e;

#if defined(CTR_NATIVE)


		Particle_UpdateAllParticles();

#endif
	}
	else
	{
		psVar12 = gGT->threadBuckets[AKUAKU].thread;
		if (psVar12 != 0)
		{
			ThTick_RunBucket(psVar12);
		}
	}

	uVar5 = LOAD_IsOpen_RacingOrBattle();
	if (uVar5 != 0)
	{
#if defined(CTR_NATIVE)
		if ((gGT->gameMode1 & PAUSE_ALL) == 0)
		{
			RB_Bubbles_RoosTubes();
		}
#endif
		if (gGT->threadBuckets[BURST].thread != 0)
		{
			RB_Burst_DrawAll(gGT);
		}
	}

	PROC_CheckAllForDead();

	if ((gGT->gameMode1 & PAUSE_ALL) == 0)
	{
		Audio_Update1();
	}

	gGT->gameMode1_prevFrame = gGT->gameMode1;
	uVar5 = GAMEPAD_GetNumConnected(gGamepads);
	uVar3 = gGT->gameMode1;

	if ((uVar3 & END_OF_RACE) == 0)
	{
		if (wasPausedAtFrameStart || ((uVar3 & PAUSE_ALL) != 0))
		{
			if (gGT->cooldownfromPauseUntilUnpause == 0)
			{
				if (((sdata->ptrActiveMenu != &data.menuRacingWheelConfig) && (sdata->ptrActiveMenu != &D232.menuHintMenu) // in 232
				     ) &&
				    ((sdata->AnyPlayerTap & BTN_START) != 0))
				{
					RECTMENU_ClearInput();
					gGT->gameMode1 &= ~PAUSE_1;

					// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800354dc-0x80035508 for unpause audio side effects.
					MainFrame_TogglePauseAudio(0);
					OtherFX_Play(1, 1);

					MainFreeze_SafeAdvDestroy();
					ElimBG_Deactivate(gGT);

					RECTMENU_Hide(sdata->ptrActiveMenu);
					gGT->cooldownFromUnpauseUntilPause = 5;
				}
			}
			else
			{
				gGT->cooldownfromPauseUntilUnpause--;
			}
		}
		else if (gGT->cooldownFromUnpauseUntilPause == 0)
		{
			if ((uVar3 & (GAME_CUTSCENE | END_OF_RACE | MAIN_MENU)) == 0)
			{
				if (sdata->ptrActiveMenu == 0)
				{
					if (sdata->AkuAkuHintState == 0)
					{
						if (!RaceFlag_IsFullyOnScreen())
						{
							for (iVar4 = 0; iVar4 < gGT->numPlyrCurrGame; iVar4++)
							{
								if ((((uVar5 != 0) && ((
#if defined(CTR_NATIVE)
								                          !MainFrame_HaveAllPads((u16)gGT->numPlyrNextGame) &&
#endif
								                          ((gGT->gameMode1 & PAUSE_ALL) == 0)))) ||
								     ((gGamepads->gamepad[iVar4].buttonsTapped & BTN_START) != 0)) &&
								    (gGT->overlayIndex_Threads != OVERLAY_INDEX_NONE))
								{
									// NOTE(aalhendi): Retail writes this before freezing the game for pause.
									gGT->gameModeEnd = (gGT->gameMode1 & GAME_MODE_END_RETAINED_MODE_MASK) | PAUSE_1;

									MainFreeze_IfPressStart();

									gGT->cooldownfromPauseUntilUnpause = 5;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			gGT->cooldownFromUnpauseUntilPause--;
		}
	}
	else if (gGT->timerEndOfRaceVS == 0)
	{
		uVar3 = gGT->gameModeEnd;
		if ((uVar3 & AKU_SONG) == 0)
		{
			if ((uVar3 & CRYSTAL_CHALLENGE) == 0)
			{
				if (gGT->unk_timerCooldown_similarTo_1d36 == 0)
				{
					return;
				}
			}
			else if (gGT->unk_timerCooldown_similarTo_1d36 == 0)
			{
				if ((uVar3 & PAUSE_2) == 0)
				{
					return;
				}

				sVar2 = SubmitName_DrawMenu(0x140);

				// if not done yet
				if (sVar2 == 0)
				{
					return;
				}


#if defined(CTR_NATIVE)

				// if SAVE
				if (sVar2 == 1)
				{
					sdata->boolSaveCupProgress = 0;

					SelectProfile_ToggleMode(SELECT_PROFILE_MODE_SLOT_SAVE);

					RECTMENU_Show(&data.menuWarning2);
					gGT->gameModeEnd |= NEW_NAME;

					return;
				}
#endif

				// if -1 (cancel)
				gGT->newHighScoreIndex = -1;
				gGT->gameModeEnd &= ~(NEW_BEST_LAP | NEW_HIGH_SCORE);
				return;
			}
			gGT->unk_timerCooldown_similarTo_1d36--;
		}
	}
	else if ((uVar3 & ARCADE_MODE) == 0)
	{
		if (gGT->timerEndOfRaceVS < 0x96)
		{
#if defined(CTR_NATIVE)
			UI_VsQuipDrawAll();
			UI_VsWaitForPressX();
#endif
		}
		if (0x1e < gGT->timerEndOfRaceVS)
		{
			gGT->timerEndOfRaceVS--;
		}
	}
	else
	{
		gGT->timerEndOfRaceVS = 0;
	}
	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 PSX path 0x80035d30-0x80035d70.
void MainFrame_InitVideoSTR(u32 boolPlayVideoStr, RECT *r, s16 posX, s16 posY)
{
#ifdef CTR_NATIVE
	if (r == NULL)
	{
		// NOTE(aalhendi): Native overlay 230 uses NULL to disable STR copy;
		// retail blindly copies the RECT fields in the PSX path below.
		sdata->videoSTR_src_vramRect.x = 0;
		sdata->videoSTR_src_vramRect.y = 0;
		sdata->videoSTR_src_vramRect.w = 0;
		sdata->videoSTR_src_vramRect.h = 0;
	}
	else
#endif
	{
		sdata->videoSTR_src_vramRect.x = r->x;
		sdata->videoSTR_src_vramRect.y = r->y;
		sdata->videoSTR_src_vramRect.w = r->w;
		sdata->videoSTR_src_vramRect.h = r->h;
	}

	sdata->boolPlayVideoSTR = boolPlayVideoStr;
	sdata->videoSTR_dst_vramX = posX;
	sdata->videoSTR_dst_vramY = posY;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80035d70-0x80035e20.
b32 MainFrame_HaveAllPads(s16 numPlyrNextGame)
{
	// if game is not loading
	if (sdata->Loading.stage == LOAD_IDLE)
	{
		struct GamepadBuffer *gb = &sdata->gGamepads->gamepad[0];

		if (numPlyrNextGame == 0)
		{
			return false;
		}

		for (int i = 0; i < numPlyrNextGame; i++)
		{
			struct ControllerPacket *packet = gb->ptrControllerPacket;

			if (packet == NULL)
			{
				return false;
			}
			if (packet->plugged != PLUGGED)
			{
				return false;
			}

			gb++;
		}
	}

	return true;
}

static void MainFrame_ReplacePackedVisList(int *dst, void *src, int byteCount)
{
	uintptr_t srcWord = (uintptr_t)src;

	if ((srcWord & 1) == 0)
	{
		memcpy(dst, src, byteCount);
		return;
	}

	CTR_unknownMaybeThunk1(dst, (void *)(srcWord & ~(uintptr_t)3));
}

static void MainFrame_OrPackedVisList(int *dst, void *src, int byteCount)
{
	uintptr_t srcWord = (uintptr_t)src;

	if ((srcWord & 1) == 0)
	{
		CTR_unknownMaybeThunk3(dst, src, byteCount);
		return;
	}

	CTR_unknownMaybeThunk2(dst, (void *)(srcWord & ~(uintptr_t)3));
}

static int MainFrame_VisMemHasQuad(const int *visFaceList, const struct QuadBlock *quad, const struct mesh_info *mesh)
{
	struct QuadBlock *quadBlocks = MeshInfo_GetQuadBlocks(mesh, "MainFrame visible quad blocks");
	if ((visFaceList == NULL) || (quadBlocks == NULL))
	{
		return 0;
	}
	int quadIndex = (int)(quad - quadBlocks);

	return (visFaceList[quadIndex >> 5] & (1 << (quadIndex & 0x1f))) != 0;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80035684-0x800357b8, unnamed in syms926.
static void MainFrame_VisMemAddDriverPVS(struct GameTracker *gGT, int playerIndex)
{
	struct Driver *driver = gGT->drivers[playerIndex];
	struct mesh_info *mesh = Level_GetMeshInfo(gGT->level1, "MainFrame driver PVS mesh");
	struct QuadBlock *quad = driver->underDriver;
	struct PVS *pvs;
	int *leafSource;
	int *faceSource;

	if ((quad == NULL) || (mesh == NULL))
	{
		return;
	}

	pvs = QuadBlock_GetPVS(quad, "MainFrame driver PVS");
	if (pvs == NULL)
	{
		return;
	}

	leafSource = PVS_GetLeafSrc(pvs, ((size_t)mesh->numBspNodes + 31u) >> 5u, "MainFrame driver leaf visibility");
	if (leafSource != NULL)
	{
		MainFrame_OrPackedVisList(gGT->visMem1->visLeafList[playerIndex], leafSource, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
	}

	faceSource = PVS_GetFaceSrc(pvs, ((size_t)mesh->numQuadBlock + 31u) >> 5u, "MainFrame driver face visibility");
	if (faceSource != NULL)
	{
		MainFrame_OrPackedVisList(gGT->visMem1->visFaceList[playerIndex], faceSource, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800357b8-0x80035d30.
void MainFrame_VisMemFullFrame(struct GameTracker *gGT, struct Level *level)
{
	struct VisMem *visMem;
	struct mesh_info *mesh;
	int playerIndex;

	visMem = gGT->visMem1;
	if (visMem == NULL)
	{
		return;
	}

	if (level == NULL)
	{
		return;
	}

	if (gGT->numPlyrCurrGame == 0)
	{
		return;
	}

	mesh = Level_GetMeshInfo(level, "MainFrame visibility mesh");
	if (mesh == NULL)
	{
		return;
	}

	for (playerIndex = 0; playerIndex < gGT->numPlyrCurrGame; playerIndex++)
	{
		struct CameraDC *camDC = &gGT->cameraDC[playerIndex];
		struct Driver *driver = gGT->drivers[playerIndex];
		struct QuadBlock *driverQuad = driver->underDriver;
		struct PVS *driverPVS = NULL;
		int *driverLeafSrc = NULL;
		int *driverFaceSrc = NULL;
		struct CtrAssetRef32 *driverInstSrc = NULL;

		if (driverQuad != NULL)
		{
			driverPVS = QuadBlock_GetPVS(driverQuad, "MainFrame driver PVS");
			driverLeafSrc = PVS_GetLeafSrc(driverPVS, ((size_t)mesh->numBspNodes + 31u) >> 5u, "MainFrame driver leaf visibility");
			driverFaceSrc = PVS_GetFaceSrc(driverPVS, ((size_t)mesh->numQuadBlock + 31u) >> 5u, "MainFrame driver face visibility");
			driverInstSrc = PVS_GetInstanceRefs(driverPVS, "MainFrame driver instance visibility");
		}

		camDC->flags &= ~0x4000;

		if (camDC->visLeafSrc == NULL)
		{
			if (driverLeafSrc != NULL)
			{
				visMem->visLeafSrc[playerIndex] = driverLeafSrc;
				MainFrame_ReplacePackedVisList(visMem->visLeafList[playerIndex], driverLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
			}
		}
		else if (visMem->visLeafSrc[playerIndex] != camDC->visLeafSrc)
		{
			visMem->visLeafSrc[playerIndex] = camDC->visLeafSrc;
			MainFrame_ReplacePackedVisList(visMem->visLeafList[playerIndex], camDC->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
		}

		if (camDC->visFaceSrc == NULL)
		{
			if (driverFaceSrc != NULL)
			{
				visMem->visFaceSrc[playerIndex] = driverFaceSrc;
				MainFrame_ReplacePackedVisList(visMem->visFaceList[playerIndex], driverFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
			}
		}
		else if (visMem->visFaceSrc[playerIndex] != camDC->visFaceSrc)
		{
			visMem->visFaceSrc[playerIndex] = camDC->visFaceSrc;
			MainFrame_ReplacePackedVisList(visMem->visFaceList[playerIndex], camDC->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);

			if ((driverPVS == NULL) || (driverLeafSrc == NULL) || (driverFaceSrc == NULL) || (driverInstSrc == NULL) ||
			    MainFrame_VisMemHasQuad(visMem->visFaceList[playerIndex], driverQuad, mesh))
			{
				camDC->flags &= ~0x2000;
			}
			else
			{
				camDC->flags |= 0x2000;
			}

			if ((camDC->flags & 0x2000) != 0)
			{
				MainFrame_VisMemAddDriverPVS(gGT, playerIndex);
				camDC->flags |= 0x4000;
			}
		}

		if ((camDC->flags & 0x5000) == 0x1000)
		{
			MainFrame_VisMemAddDriverPVS(gGT, playerIndex);
		}

		if ((camDC->cameraMode == 0) && ((camDC->flags & 0x2000) != 0) && (driverInstSrc != NULL))
		{
			camDC->visInstSrc = driverInstSrc;
		}

		if ((level->configFlags & 4) == 0)
		{
			if (visMem->visOVertSrc[playerIndex] != camDC->visOVertSrc)
			{
				visMem->visOVertSrc[playerIndex] = camDC->visOVertSrc;
				MainFrame_ReplacePackedVisList(visMem->visOVertList[playerIndex], camDC->visOVertSrc, ((level->numWaterVertices + 0x1f) >> 5) << 2);
			}
			else if (visMem->visOVertSrc[playerIndex] == NULL)
			{
				int *defaultSource = Level_GetVisOVertSrc(level, "MainFrame default ocean visibility");
				if (defaultSource != NULL)
				{
					memcpy(visMem->visOVertList[playerIndex], (void *)((uintptr_t)defaultSource & ~(uintptr_t)3),
					       ((level->numWaterVertices + 0x1f) >> 5) << 2);
				}
			}
		}
		else
		{
			if (visMem->visSCVertSrc[playerIndex] != camDC->visSCVertSrc)
			{
				visMem->visSCVertSrc[playerIndex] = camDC->visSCVertSrc;
				MainFrame_ReplacePackedVisList(visMem->visSCVertList[playerIndex], camDC->visSCVertSrc, ((level->numSCVert + 0x1f) >> 5) << 2);
			}
			else if (visMem->visSCVertSrc[playerIndex] == NULL)
			{
				int *defaultSource = Level_GetVisSCVertSrc(level, "MainFrame default scenery visibility");
				if (defaultSource != NULL)
				{
					memcpy(visMem->visSCVertList[playerIndex], (void *)((uintptr_t)defaultSource & ~(uintptr_t)3),
					       ((level->numSCVert + 0x1f) >> 5) << 2);
				}
			}
		}
	}
}

// Request Aku Hint, doesn't start till FUN_800b3dd8
// hintId:
//	0x00 - Welcome to Adventure Arena
//	0x01 - using a warp pad (part of welcome)
// 	0x02 - Need more trophies
//	0x03 - Need 4 trophies for Boss
//	0x04 - Need 4 keys for oxide
//	0x05 - Must have 1 Boss Key
//	0x06 - This is the load/save screen
// 	0x07 - Congrats on opening new area
//	0x12 - Must have 2 Boss Key
//	0x19 - Collect every crystal in arena
//	0x1a - CTR Token
//	0x1b - Gem Cups
//	0x1c - Must get 10 relics
//	0x1d - Relic
// param2:
//	0x00 - not interrupting a warppad load screen
// 	0x01 - interrupting (CTR, Relic, or Crystal hints)
// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80035e20-0x80035e70.
void MainFrame_RequestMaskHint(s16 hintId, s16 interruptWarpPad)
{
	struct GameTracker *gGT = sdata->gGT;

	if (((gGT->gameMode1 & PAUSE_ALL) == 0) && (sdata->AkuHint_RequestedHint == -1))
	{
		sdata->AkuAkuHintState = 1;

		gGT->drivers[0]->funcPtrs[DRIVER_FUNC_INIT] = VehPhysProc_FreezeEndEvent_Init;

		sdata->AkuHint_RequestedHint = hintId;
		sdata->AkuHint_boolInterruptWarppad = interruptWarpPad;
	}
	return;
}
