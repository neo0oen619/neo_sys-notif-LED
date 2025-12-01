#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <switch.h>
#include <stdio.h>

static bool g_restore_on_resume = false;
static int g_lastFullRemaining = -1;

static void requestSystemSleep(void) {
    // Re-enable the backlight/auto-sleep so the OS can take over cleanly.
    appletSetLcdBacklightOffEnabled(false);
    appletSetAutoSleepDisabled(false);

    Result rc = appletRequestToSleep();
    if (R_FAILED(rc)) {
        // Fall back to the raw SVC in case the request API isn't available.
        svcSleepSystem();
    }
}

static void appletHookFunc(AppletHookType hook, void* param) {
    (void)param;
    if (hook == AppletHookType_OnResume) {
        g_restore_on_resume = true;
    }
}

static void loadSleepConfig(bool* outHasSmartSleep) {
    *outHasSmartSleep = false;

    fsdevMountSdmc();

    // Check type = smart.
    char typeBuf[32] = {0};
    FILE* tf = fopen("sdmc:/config/sys-notif-LED/type", "r");
    if (tf) {
        if (fgets(typeBuf, sizeof(typeBuf), tf)) {
            typeBuf[strcspn(typeBuf, "\r\n")] = 0;
        }
        fclose(tf);
    }

    if (strcmp(typeBuf, "smart") != 0) {
        fsdevUnmountDevice("sdmc");
        return;
    }

    int fullTimeoutMinutes = 0;
    int sleepOnFull = 0;

    FILE* f = fopen("sdmc:/config/sys-notif-LED/settings.cfg", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char key[64];
            char value[64];
            if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
                int v = atoi(value);
                if (strcmp(key, "full_timeout_minutes") == 0) {
                    if (v >= 1 && v <= 300) fullTimeoutMinutes = v;
                } else if (strcmp(key, "sleep_on_full_timeout") == 0) {
                    if (v == 0 || v == 1) sleepOnFull = v;
                }
            }
        }
        fclose(f);
    }

    fsdevUnmountDevice("sdmc");

    if (fullTimeoutMinutes > 0 && sleepOnFull != 0) {
        *outHasSmartSleep = true;
    }
}

static void loadStatus(int* outFullRemaining, bool* outHaveStatus) {
    *outFullRemaining = -1;
    *outHaveStatus = false;

    fsdevMountSdmc();
    FILE* f = fopen("sdmc:/config/sys-notif-LED/status.txt", "r");
    if (!f) {
        fsdevUnmountDevice("sdmc");
        return;
    }

    *outHaveStatus = true;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "full_remaining_seconds") == 0) {
                *outFullRemaining = atoi(value);
            }
        }
    }

    fclose(f);
    fsdevUnmountDevice("sdmc");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);

    bool hasSmartSleep = false;
    loadSleepConfig(&hasSmartSleep);

    printf("sys-notif-LED screen-off helper\n");
    printf("Backlight will be turned OFF and\n");
    printf("auto-sleep disabled while this app runs.\n");
    printf("\n");
    if (hasSmartSleep) {
        printf("Smart full-timeout + Sleep is enabled.\n");
        printf("The LED sysmodule will decide when to\n");
        printf("call real sleep based on its timeout.\n");
    } else {
        printf("No Smart full-timeout sleep configured.\n");
        printf("Console will auto-sleep after ~10 minutes\n");
        printf("of screen-off helper running.\n");
    }

    // Disable auto-sleep while running.
    appletSetAutoSleepDisabled(true);

    // Turn backlight off.
    appletSetLcdBacklightOffEnabled(true);

    // Install resume hook so we can restore backlight after sleep/resume.
    AppletHookCookie cookie;
    appletHook(&cookie, appletHookFunc, NULL);

    const u64 fallbackTicks = (10ULL * 60ULL * 1000000000ULL); // 10 minutes in ns
    u64 startTicks = armGetSystemTick();

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        // Allow manual exit with +.
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            appletSetLcdBacklightOffEnabled(false);
            appletSetAutoSleepDisabled(false);
            break;
        }

        // If Smart full-timeout+Sleep is configured, watch the sysmodule's timer.
        if (hasSmartSleep) {
            int fullRemaining = -1;
            bool haveStatus = false;
            loadStatus(&fullRemaining, &haveStatus);

            if (haveStatus) {
                if (fullRemaining == 0 && g_lastFullRemaining != 0 && fullRemaining != -1) {
                    // Mark that we want the Smart full-timeout cycle to restart on wake.
                    fsdevMountSdmc();
                    FILE* rf = fopen("sdmc:/config/sys-notif-LED/full_timeout_reset", "w");
                    if (rf) fclose(rf);
                    fsdevUnmountDevice("sdmc");

                    requestSystemSleep();
                }
                g_lastFullRemaining = fullRemaining;
            } else {
                // Status file unavailable: fall back to fixed timeout so we don't strand the system.
                u64 nowTicks = armGetSystemTick();
                u64 elapsedNs = armTicksToNs(nowTicks - startTicks);
                if (elapsedNs >= fallbackTicks) {
                    requestSystemSleep();
                }
            }
        } else {
            // No Smart full-timeout sleep configured: use fixed 10 minute timeout.
            u64 nowTicks = armGetSystemTick();
            u64 elapsedNs = armTicksToNs(nowTicks - startTicks);
            if (elapsedNs >= fallbackTicks) {
                requestSystemSleep();
            }
        }

        // If system has just resumed from sleep, restore backlight and exit.
        if (g_restore_on_resume) {
            appletSetLcdBacklightOffEnabled(false);
            appletSetAutoSleepDisabled(false);
            break;
        }

        consoleUpdate(NULL);

        // Purple credit line at bottom-right.
        PrintConsole* con = consoleGetDefault();
        if (con) {
            int rows = con->consoleHeight;
            int cols = con->consoleWidth;
            const char* credit = "made with <3 by neo0oen619";
            int len = (int)strlen(credit);
            int col = cols - len;
            if (col < 0) col = 0;
            int row = rows - 1;
            if (row < 1) row = 1;
            printf("\x1b[%d;%dH\x1b[35m%s\x1b[0m", row, col + 1, credit);
        }
    }

    // Ensure everything is restored if we exit for any reason.
    appletSetLcdBacklightOffEnabled(false);
    appletSetAutoSleepDisabled(false);

    consoleExit(NULL);
    return 0;
}
