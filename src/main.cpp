/* SilverFallsFreezeFix - failsafe reboot for Silver Falls soft freezes where HaltFix (PPCHalt) does not trigger.
   Based on HaltFix by shutterbug2000.
*/

#include <wups.h>
#include <coreinit/screen.h>
#include <coreinit/title.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/ios.h>
#include <gx2/swap.h>
#include <mocha/mocha.h>
#include <stdlib.h>
#include <string.h>

WUPS_PLUGIN_NAME("SilverFallsFreezeFix");
WUPS_PLUGIN_DESCRIPTION("Reboots on soft freeze in Silver Falls games (HaltFix failsafe)");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Kaito-san + research");
WUPS_PLUGIN_LICENSE("ISC");

#define IOCTL_SVC 0x02
#define ALIGN_0x40 __attribute__((aligned(0x40)))

static const uint64_t TARGET_TIDS[] = {
    0x000500001021D000ULL, // Undertakers
    0x000500001021D300ULL, // White Inside Its Umbra
    0x000500001021DD00ULL  // Ghoul Busters
};
static const size_t NUM_TARGETS = sizeof(TARGET_TIDS) / sizeof(TARGET_TIDS[0]);

static int haxHandle = -1;
static volatile uint64_t lastSwapTicks = 0;
static volatile bool monitoring = false;
static OSThread *watchdogThread = nullptr;
static uint8_t watchdogStack[0x2000] __attribute__((aligned(8)));

static bool isTargetTitle(uint64_t tid) {
    for (size_t i = 0; i < NUM_TARGETS; i++) {
        if (tid == TARGET_TIDS[i]) return true;
    }
    return false;
}

static void doReboot() {
    // Same path as HaltFix
    OSScreenInit();
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    uint32_t tvSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    OSScreenSetBufferEx(SCREEN_TV, (void *)0xF4000000);
    OSScreenSetBufferEx(SCREEN_DRC, (void *)(0xF4000000 + tvSize));
    OSScreenClearBufferEx(SCREEN_TV, 0x0000FFFF);
    OSScreenClearBufferEx(SCREEN_DRC, 0x0000FFFF);
    OSScreenPutFontEx(SCREEN_TV, 0, 0, "Silver Falls freeze detected.\nRebooting console...");
    OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Silver Falls freeze detected.\nRebooting console...");
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSSleepTicks(OSMillisecondsToTicks(3000));

    ALIGN_0x40 uint32_t arguments[1] = {0x74};
    ALIGN_0x40 int result[1];
    if (haxHandle >= 0) {
        IOS_Ioctl(haxHandle, IOCTL_SVC, arguments, sizeof(arguments), result, sizeof(result));
    }
    // Fallback if ioctl fails
    while (1) OSSleepTicks(OSSecondsToTicks(1));
}

static int watchdogThreadFunc(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    const uint64_t timeoutTicks = OSSecondsToTicks(10); // 10s with no frame swap = freeze
    const uint64_t graceTicks = OSSecondsToTicks(30);   // allow load time

    uint64_t start = OSGetSystemTick();
    while (monitoring) {
        OSSleepTicks(OSMillisecondsToTicks(500));
        if (!monitoring) break;

        uint64_t now = OSGetSystemTick();
        if (now - start < graceTicks) continue; // still in grace period

        uint64_t last = lastSwapTicks;
        if (last != 0 && (now - last) > timeoutTicks) {
            monitoring = false;
            doReboot();
            break;
        }
    }
    return 0;
}

// Hook every frame present
DECL_FUNCTION(void, GX2SwapScanBuffers) {
    lastSwapTicks = OSGetSystemTick();
    real_GX2SwapScanBuffers();
}

ON_APPLICATION_START() {
    uint64_t tid = OSGetTitleID();

    if (!isTargetTitle(tid)) {
        monitoring = false;
        return;
    }

    haxHandle = IOS_Open((char *)"/dev/iosuhax", (IOSOpenMode)0);

    Mocha_InitLibrary(); // optional, for consistency with HaltFix

    lastSwapTicks = OSGetSystemTick();
    monitoring = true;

    // High-priority watchdog
    watchdogThread = (OSThread *)malloc(sizeof(OSThread));
    if (watchdogThread) {
        BOOL created = OSCreateThread(watchdogThread,
                                      (OSThreadEntryPointFn)watchdogThreadFunc,
                                      0, nullptr,
                                      watchdogStack + sizeof(watchdogStack),
                                      sizeof(watchdogStack),
                                      0, // highest practical priority
                                      OS_THREAD_ATTRIB_AFFINITY_CPU0 | OS_THREAD_ATTRIB_DETACHED);
        if (created) {
            OSResumeThread(watchdogThread);
        }
    }
}

ON_APPLICATION_ENDS() {
    monitoring = false;
    if (watchdogThread) {
        free(watchdogThread);
        watchdogThread = nullptr;
    }
    if (haxHandle >= 0) {
        IOS_Close(haxHandle);
        haxHandle = -1;
    }
    Mocha_DeInitLibrary();
}

WUPS_MUST_REPLACE_FOR_PROCESS(GX2SwapScanBuffers, WUPS_LOADER_LIBRARY_GX2, GX2SwapScanBuffers, WUPS_FP_TARGET_PROCESS_GAME);