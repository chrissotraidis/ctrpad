/* Included by the native test build: exercise real allocation and cleanup,
 * using private pools and no retail assets or user's runtime state. */
#define MAINT_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "maintenance check failed at %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static void Maintenance_InitPool(struct JitPool *pool, void *storage, int count, int stride)
{
    pool->ptrPoolData = storage;
    pool->maxItems = count;
    pool->itemSize = stride;
    pool->poolSize = count * stride;
    JitPool_Clear(pool);
}

static int NativeMaintenance_WeaponTest(void)
{
    static struct GameTracker gt;
    static struct sData savedData;
    static struct Driver driver;
    struct Instance driverInst;
    struct Thread driverThread, existingChild;
    struct Thread threads[2];
    struct Instance instances[3];
    union { max_align_t alignment; unsigned char bytes[4096]; } stack;
    struct Model model = {0};
    struct GamepadSystem pads = {0};
    const int weapons[] = {WEAPON_ID_BOMB_MISSILE, WEAPON_ID_MINE,
        WEAPON_ID_BEAKER, WEAPON_ID_SHIELD, WEAPON_ID_WARPBALL};
    int cases = 0;
    savedData = *sdata;
    sdata->gGamepads = &pads;

    /* Missing model, empty object pool, empty thread pool, and empty instance
     * pool all fail through the production INSTANCE_BirthWithThread path. */
    for (unsigned w = 0; w < sizeof(weapons) / sizeof(weapons[0]); ++w)
    for (int juiced = 0; juiced < 2; ++juiced)
    for (int failure = 0; failure < 5; ++failure)
    {
        if (failure == 4 && weapons[w] != WEAPON_ID_BOMB_MISSILE && weapons[w] != WEAPON_ID_WARPBALL) continue;
        memset(&gt, 0, sizeof(gt));
        memset(&driver, 0, sizeof(driver));
        memset(&driverInst, 0, sizeof(driverInst));
        memset(&driverThread, 0, sizeof(driverThread));
        memset(&existingChild, 0, sizeof(existingChild));
        memset(threads, 0, sizeof(threads));
        memset(instances, 0, sizeof(instances));
        sdata->gGT = &gt;
        sdata->boolAudioEnabled = 0;
        driver.instSelf = &driverInst;
        driverInst.thread = &driverThread;
        driver.actionsFlagSet = ACTION_BOT;
        driver.numWumpas = juiced ? DRIVER_WUMPA_JUICED_COUNT : 0;
        driverThread.childThread = &existingChild;
        driver.heldItemID = juiced ? HELD_ITEM_BOMB_1X : 0;
        gt.numMissiles = 3;
        driver.numTimesMissileLaunched = 5;
        for (unsigned i = 0; i < sizeof(gt.modelPtr) / sizeof(gt.modelPtr[0]); ++i)
            gt.modelPtr[i] = failure == 0 ? NULL : &model;
        Maintenance_InitPool(&gt.JitPools.thread, threads, failure == 2 ? 0 : 2, sizeof(threads[0]));
        Maintenance_InitPool(&gt.JitPools.instance, instances, failure == 3 ? 0 : 3, sizeof(instances[0]));
        Maintenance_InitPool(&gt.JitPools.mediumStack, stack.bytes, failure == 1 ? 0 : 1, sizeof(stack));
        /* Only one stack class is used in each call. */
        gt.JitPools.smallStack = gt.JitPools.mediumStack;
        VehPickupItem_ShootNow(&driver, weapons[w], 0);
        if (failure == 4)
        {
            struct Thread *born = (weapons[w] == WEAPON_ID_BOMB_MISSILE && juiced)
                ? driverThread.childThread : gt.threadBuckets[TRACKING].thread;
            MAINT_CHECK(born && born != &existingChild && born->inst && born->object);
            MAINT_CHECK(gt.JitPools.instance.taken.count == 1 && gt.JitPools.mediumStack.free.count == 0);
            MAINT_CHECK(gt.numMissiles == (weapons[w] == WEAPON_ID_BOMB_MISSILE ? 4 : 3));
            MAINT_CHECK(driver.numTimesMissileLaunched == (weapons[w] == WEAPON_ID_BOMB_MISSILE ? 6 : 5));
            if (born == driverThread.childThread) driverThread.childThread = born->siblingThread;
            else gt.threadBuckets[TRACKING].thread = born->siblingThread;
            PROC_DestroySelf(born);
            MAINT_CHECK(driverThread.childThread == &existingChild && existingChild.timesDestroyed == 0);
            MAINT_CHECK(gt.JitPools.instance.free.count == 3 && gt.JitPools.instance.taken.count == 0);
            MAINT_CHECK(gt.JitPools.mediumStack.free.count == 1 && gt.JitPools.thread.free.count == 2);
            MAINT_CHECK(gt.numMissiles == 3);
            ++cases;
            continue;
        }
        MAINT_CHECK(gt.numMissiles == 3 && driver.numTimesMissileLaunched == 5);
        MAINT_CHECK(driverThread.childThread == &existingChild);
        MAINT_CHECK(existingChild.siblingThread == NULL && existingChild.timesDestroyed == 0);
        MAINT_CHECK(driver.instBubbleHold == NULL && driver.instBombThrow == NULL && driver.instTntSend == NULL);
        MAINT_CHECK(gt.JitPools.thread.free.count == (failure == 2 ? 0 : 2));
        MAINT_CHECK(gt.JitPools.instance.taken.count == 0);
        MAINT_CHECK(gt.JitPools.instance.free.count == (failure == 3 ? 0 : 3));
        struct JitPool *usedStack = (weapons[w] == WEAPON_ID_MINE || weapons[w] == WEAPON_ID_BEAKER)
            ? &gt.JitPools.smallStack : &gt.JitPools.mediumStack;
        MAINT_CHECK(usedStack->free.count == (failure == 1 ? 0 : 1));
        for (unsigned i = 0; i < sizeof(gt.threadBuckets) / sizeof(gt.threadBuckets[0]); ++i)
            MAINT_CHECK(gt.threadBuckets[i].thread == NULL);
        ++cases;
    }

    /* One/two slots exhaust the shield's secondary births; three slots must
     * succeed. In every case retain and never destroy an existing child. */
    for (int slots = 1; slots <= 3; ++slots)
    for (int juiced = 0; juiced < 2; ++juiced)
    {
        memset(&gt, 0, sizeof(gt));
        memset(&driver, 0, sizeof(driver));
        memset(&driverInst, 0, sizeof(driverInst));
        memset(&driverThread, 0, sizeof(driverThread));
        memset(&existingChild, 0, sizeof(existingChild));
        memset(threads, 0, sizeof(threads));
        memset(instances, 0, sizeof(instances));
        sdata->gGT = &gt;
        driver.instSelf = &driverInst;
        driverInst.thread = &driverThread;
        driverThread.childThread = &existingChild;
        driver.numWumpas = juiced ? DRIVER_WUMPA_JUICED_COUNT : 0;
        for (unsigned i = 0; i < sizeof(gt.modelPtr) / sizeof(gt.modelPtr[0]); ++i) gt.modelPtr[i] = &model;
        Maintenance_InitPool(&gt.JitPools.thread, threads, 2, sizeof(threads[0]));
        Maintenance_InitPool(&gt.JitPools.instance, instances, slots, sizeof(instances[0]));
        Maintenance_InitPool(&gt.JitPools.mediumStack, stack.bytes, 1, sizeof(stack));
        VehPickupItem_ShootNow(&driver, WEAPON_ID_SHIELD, 0);
        if (slots == 3)
        {
            MAINT_CHECK(driver.instBubbleHold != NULL);
            struct Thread *shieldThread = driver.instBubbleHold->thread;
            struct Shield *shield = shieldThread->object;
            MAINT_CHECK(driverThread.childThread == shieldThread && shieldThread->siblingThread == &existingChild);
            MAINT_CHECK(gt.JitPools.instance.taken.count == 3);
            MAINT_CHECK(shield->instColor && shield->instHighlight && shield->instColor != shield->instHighlight);
            MAINT_CHECK((shield->flags & SHIELD_FLAG_BLUE) == (juiced ? SHIELD_FLAG_BLUE : 0));
            INSTANCE_Death(shield->instColor);
            INSTANCE_Death(shield->instHighlight);
            driverThread.childThread = shieldThread->siblingThread;
            PROC_DestroySelf(shieldThread);
        }
        else MAINT_CHECK(driver.instBubbleHold == NULL);
        MAINT_CHECK(driverThread.childThread == &existingChild && existingChild.timesDestroyed == 0);
        MAINT_CHECK(gt.JitPools.instance.free.count == slots && gt.JitPools.instance.taken.count == 0);
        MAINT_CHECK(gt.JitPools.thread.free.count == 2 && gt.JitPools.mediumStack.free.count == 1);
        ++cases;
    }
    *sdata = savedData;
    printf("[CTR Maintenance] weapon allocation passed: %d cases, real pools, rollback, preserved children, shield success\n", cases);
    return 0;
}
static int NativeMaintenance_FullDynamicTest(void)
{
    Platform_InitScratchpad();
    struct DrawLevelOvr1PScratchVertex *projected = CTR_SCRATCHPAD_PTR(
        struct DrawLevelOvr1PScratchVertex, DRAW_LEVEL_OVR1P_DEEPEST_PROJECTED_FRAME_OFFSET);
    POLY_GT4 packets[3] = {0};
    u32 ot[1] = {NATIVE_GPU_LINK_TERMINATOR};
    struct PushBuffer pb = {0};
    struct QuadBlock block = {0};
    struct PrimMem prim = {0};
    /* Expected shared-edge topology: both mixed faces meet at vertex 3,
     * followed by the upper-left face. These are packet outputs, not mocks. */
    const int expected[3][4] = {{4, 1, 6, 3}, {5, 6, 2, 3}, {0, 4, 5, 6}};
    memset(DrawLevelOvr1P_Scratch(), 0, sizeof(*DrawLevelOvr1P_Scratch()));
    memset(projected, 0, 9 * sizeof(*projected));
    for (int i = 0; i < 9; ++i)
    {
        projected[i].posScreen[0] = 10 + i;
        projected[i].posScreen[1] = 20 + i;
        projected[i].depth = 100;
    }
    DrawLevelOvr1P_Scratch()->clipWindowPacked = (240u << 16) | 320u;
    DrawLevelOvr1P_Scratch()->drawOrderOrHeader = 0x80000000u;
    *CTR_SCRATCHPAD_PTR(u32, DRAW_LEVEL_OVR1P_DIRECT_HANDLER_TABLE_OFFSET + DRAW_LEVEL_OVR1P_DIRECT_QUAD)
        = DRAW_LEVEL_OVR_RETAIL_LABEL_FULL_DYNAMIC_DIRECT_GT4_226;
    sDrawLevelOvr1P_FullDynamicInheritedOtIndex = 0;
    pb.ptrOT = ot;
    prim.cursor = prim.start = packets;
    prim.end = prim.guardEnd = packets + 3;
    NativeGpuLinks_Reset();
    MAINT_CHECK(NativeGpuLinks_RegisterRange(packets, sizeof(packets), NULL));
    MAINT_CHECK(Ovr226_800a15d4_FullDynamicHelperSlot0(&pb, &prim, &block, projected, NULL, 0));
    MAINT_CHECK(prim.cursor == packets + 3 && prim.primitiveCount == 3);
    for (int i = 0; i < 3; ++i)
    {
        MAINT_CHECK(packets[i].x0 == 10 + expected[i][0] && packets[i].y0 == 20 + expected[i][0]);
        MAINT_CHECK(packets[i].x1 == 10 + expected[i][1] && packets[i].y1 == 20 + expected[i][1]);
        MAINT_CHECK(packets[i].x2 == 10 + expected[i][2] && packets[i].y2 == 20 + expected[i][2]);
        MAINT_CHECK(packets[i].x3 == 10 + expected[i][3] && packets[i].y3 == 20 + expected[i][3]);
    }
    printf("[CTR Maintenance] full-dynamic topology passed: three GT4 packets, mixed-face shared edges\n");
    return 0;
}
#undef MAINT_CHECK
