#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>

#define INNER_HEAP_SIZE 0x80000
#define MAX_PADS 8

static HidsysUniquePadId connectedPads[MAX_PADS];
static int numConnectedPads = 0;
static HidsysNotificationLedPattern Pattern;
static bool sysmoduleRunning = true;

static bool chargeSelected = false;
static bool currentlyCharging = false;

static bool batterySelected = false;
static int batteryStatus = -1; // 0: 100%-16% | 1: 15%-6% | 2: 5%-1%

static bool smartSelected = false;
static int smartBatterySegment = -1;
static int smartFadeCountdown = 0;
static int smartLastState = -1; // 0: off, 1: fade, 2: solid, 3: blink
static int smartFullToggleCounter = 0;
static bool smartFullBlinkPhase = false;

static void applySolidPattern(void) {
    memset(&Pattern, 0, sizeof(Pattern));
    Pattern.baseMiniCycleDuration = 0x0F;
    Pattern.startIntensity = 0xF;
    Pattern.miniCycles[0].ledIntensity = 0xF;
    Pattern.miniCycles[0].transitionSteps = 0x0F;
    Pattern.miniCycles[0].finalStepDuration = 0x0F;
}

static void applyDimPattern(void) {
    memset(&Pattern, 0, sizeof(Pattern));
    Pattern.baseMiniCycleDuration = 0x0F;
    Pattern.startIntensity = 0x5;
    Pattern.miniCycles[0].ledIntensity = 0x5;
    Pattern.miniCycles[0].transitionSteps = 0x0F;
    Pattern.miniCycles[0].finalStepDuration = 0x0F;
}

static void applyFadePattern(void) {
    memset(&Pattern, 0, sizeof(Pattern));
    Pattern.baseMiniCycleDuration = 0x8;
    Pattern.totalMiniCycles = 0x2;
    Pattern.startIntensity = 0x2;
    Pattern.miniCycles[0].ledIntensity = 0xF;
    Pattern.miniCycles[0].transitionSteps = 0xF;
    Pattern.miniCycles[1].ledIntensity = 0x2;
    Pattern.miniCycles[1].transitionSteps = 0xF;
}

static void applyOffPattern(void) {
    memset(&Pattern, 0, sizeof(Pattern));
}

static void applyBlinkPattern(void) {
    memset(&Pattern, 0, sizeof(Pattern));
    Pattern.baseMiniCycleDuration = 0x4;
    Pattern.totalMiniCycles = 0x4;
    Pattern.startIntensity = 0x2;
    Pattern.miniCycles[0].ledIntensity = 0xF;
    Pattern.miniCycles[0].transitionSteps = 0x2;
    Pattern.miniCycles[1].ledIntensity = 0x2;
    Pattern.miniCycles[1].transitionSteps = 0x2;
}


#ifdef __cplusplus
extern "C" {
#endif

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc;
    rc = smInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    }
    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }
    rc = hidInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));
    }
    rc = hidsysInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));
    }   
    rc = fsInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
    }
    psmInitialize();
    fsdevMountSdmc();
    smExit();
}

void __appExit(void) {
    sysmoduleRunning = false;
    fsdevUnmountAll();
    hidsysExit();
    hidExit();
    fsExit();
    psmExit();
}

#ifdef __cplusplus
}
#endif

void setPattern(char* buffer) {
    if (strcmp(buffer, "solid") == 0) {
        applySolidPattern();
    } if (strcmp(buffer, "dim") == 0) {
        applyDimPattern();
    } else if (strcmp(buffer, "fade") == 0) {
        applyFadePattern();
    } else if (strcmp(buffer, "off") == 0) {
        applyOffPattern();
    } else if (strcmp(buffer, "charge") == 0) {
        applyOffPattern();
        chargeSelected = true;
    } else if (strcmp(buffer, "battery") == 0) {
        applyOffPattern();
        batterySelected = true;
    } else if (strcmp(buffer, "smart") == 0) {
        applyOffPattern();
        smartSelected = true;
        smartBatterySegment = -1;
        smartFadeCountdown = 0;
        smartLastState = -1;
        smartFullToggleCounter = 0;
        smartFullBlinkPhase = false;
    }
    if (strcmp(buffer, "charge") != 0) {
        chargeSelected = false;
        currentlyCharging = false;
    }
    if (strcmp(buffer, "battery") != 0) {
        batterySelected = false;
        batteryStatus = -1;
    }
    if (strcmp(buffer, "smart") != 0) {
        smartSelected = false;
        smartBatterySegment = -1;
        smartFadeCountdown = 0;
        smartLastState = -1;
        smartFullToggleCounter = 0;
        smartFullBlinkPhase = false;
    }
}

bool isControllerConnected(HidsysUniquePadId* padId) {
    for (int i = 0; i < numConnectedPads; i++) {
        if (memcmp(&connectedPads[i], padId, sizeof(HidsysUniquePadId)) == 0) {
            return true;
        }
    }
    return false;
}

void removeController(HidsysUniquePadId* padId) {
    for (int i = 0; i < numConnectedPads; i++) {
        if (memcmp(&connectedPads[i], padId, sizeof(HidsysUniquePadId)) == 0) {
            for (int j = i; j < numConnectedPads - 1; j++) {
                connectedPads[j] = connectedPads[j + 1];
            }
            numConnectedPads--;
            break;
        }
    }
}

void setLed(HidsysUniquePadId* padId) {
    Result rc = hidsysSetNotificationLedPattern(&Pattern, *padId);
    if (R_FAILED(rc)) {
        removeController(padId);
    }
}

void changeLed() {
    for (int i = 0; i < numConnectedPads; i++) {
        setLed(&connectedPads[i]);
    }
}

void scanForNewControllers() {
    HidNpadIdType controllerTypes[MAX_PADS] = {
        HidNpadIdType_Handheld,
        HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_No3, HidNpadIdType_No4,
        HidNpadIdType_No5, HidNpadIdType_No6, HidNpadIdType_No7
    };
    for (int i = 0; i < MAX_PADS; i++) {
        HidsysUniquePadId padIds[MAX_PADS];
        s32 total_entries = 0;

        Result rc = hidsysGetUniquePadsFromNpad(controllerTypes[i], padIds, MAX_PADS, &total_entries);

        if (R_SUCCEEDED(rc) && total_entries > 0) {
            for (int j = 0; j < total_entries; j++) {
                if (!isControllerConnected(&padIds[j])) {
                    if (numConnectedPads < MAX_PADS) {
                        connectedPads[numConnectedPads++] = padIds[j];
                        setLed(&padIds[j]);
                    }
                }
            }
        }
    }
}

void verifyConnectedControllers() {
    for (int i = 0; i < numConnectedPads; i++) {
        Result rc = hidsysSetNotificationLedPattern(&Pattern, connectedPads[i]);
        if (R_FAILED(rc)) {
            removeController(&connectedPads[i]);
            i--;
        }
    }
}

int main(int argc, char* argv[]) {
    DIR* dir = opendir("sdmc:/config/sys-notif-LED");
    if (dir) {
        closedir(dir);
    } else {
        mkdir("sdmc:/config/sys-notif-LED", 0777);
    }
    FILE* file = fopen("sdmc:/config/sys-notif-LED/type", "r");
    if (!file) {
        fclose(file);
        FILE* f = fopen("sdmc:/config/sys-notif-LED/type", "w");
        if (f) {
            fprintf(f, "dim");
            fclose(f);
        }
    }
    fclose(file);
    file = fopen("sdmc:/config/sys-notif-LED/type", "r");
    if (file) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), file) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            setPattern(buffer);
        }
        fclose(file);
    }
    scanForNewControllers();
    
    while (sysmoduleRunning) {
        scanForNewControllers();
        static int verifyCounter = 0;
        if (verifyCounter++ >= 5) {
            verifyConnectedControllers();
            verifyCounter = 0;
        }
        FILE *file = fopen("sdmc:/config/sys-notif-LED/reset", "r");
        if (file) {
            fclose(file);
            remove("sdmc:/config/sys-notif-LED/reset");
            FILE* file = fopen("sdmc:/config/sys-notif-LED/type", "r");
        if (file) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), file) != NULL) {
                    buffer[strcspn(buffer, "\n")] = 0;
                    setPattern(buffer);
                }
                fclose(file);
                changeLed();
            }
        }
        if (smartSelected) {
            PsmChargerType chargerType;
            psmGetChargerType(&chargerType);
            u32 batteryCharge;
            psmGetBatteryChargePercentage(&batteryCharge);

            int currentSegment = batteryCharge / 10;

            if (chargerType == PsmChargerType_Unconnected) {
                if (smartBatterySegment == -1) {
                    smartBatterySegment = currentSegment;
                }

                if (currentSegment < smartBatterySegment && batteryCharge >= 15) {
                    smartBatterySegment = currentSegment;
                    smartFadeCountdown = 20;
                }

                if (batteryCharge < 15) {
                    smartFadeCountdown = 0;
                    if (smartLastState != 3) {
                        applyBlinkPattern();
                        smartLastState = 3;
                        changeLed();
                    }
                } else if (smartFadeCountdown > 0) {
                    smartFadeCountdown--;
                    if (smartLastState != 1) {
                        applyFadePattern();
                        smartLastState = 1;
                        changeLed();
                    }
                } else {
                    if (smartLastState != 0) {
                        applyOffPattern();
                        smartLastState = 0;
                        changeLed();
                    }
                }
            } else {
                smartBatterySegment = -1;
                smartFadeCountdown = 0;

                if (batteryCharge >= 100) {
                    smartFullToggleCounter++;
                    if (smartFullToggleCounter >= 10) {
                        smartFullToggleCounter = 0;
                        smartFullBlinkPhase = !smartFullBlinkPhase;
                    }

                    if (smartFullBlinkPhase) {
                        if (smartLastState != 3) {
                            applyBlinkPattern();
                            smartLastState = 3;
                            changeLed();
                        }
                    } else {
                        if (smartLastState != 2) {
                            applySolidPattern();
                            smartLastState = 2;
                            changeLed();
                        }
                    }
                } else {
                    smartFullToggleCounter = 0;
                    smartFullBlinkPhase = false;
                    if (smartLastState != 1) {
                        applyFadePattern();
                        smartLastState = 1;
                        changeLed();
                    }
                }
            }
        }
        if (chargeSelected) {
            PsmChargerType chargerType;
            psmGetChargerType(&chargerType);
            if (!currentlyCharging) {
                if (chargerType != PsmChargerType_Unconnected) {
                    currentlyCharging = true;
                    memset(&Pattern, 0, sizeof(Pattern));
                    Pattern.baseMiniCycleDuration = 0x0F;
                    Pattern.startIntensity = 0x5;
                    Pattern.miniCycles[0].ledIntensity = 0x5;
                    Pattern.miniCycles[0].transitionSteps = 0x0F;
                    Pattern.miniCycles[0].finalStepDuration = 0x0F;
                    changeLed();
                }
            } else {
                if (chargerType == PsmChargerType_Unconnected) {
                    currentlyCharging = false;
                    memset(&Pattern, 0, sizeof(Pattern));
                    changeLed();
                }
            }
        }
        if (batterySelected) {
            PsmChargerType chargerType;
            psmGetChargerType(&chargerType);
            if (chargerType == PsmChargerType_Unconnected) {
                u32 batteryCharge;
                psmGetBatteryChargePercentage(&batteryCharge);
                int lastStatus = batteryStatus;
                if (batteryCharge <= 5) {
                    batteryStatus = 2;
                } else if (batteryCharge <= 15) {
                    batteryStatus = 1;
                } else {
                    batteryStatus = 0;
                }
                if (lastStatus != batteryStatus) {
                    if (batteryStatus == 0) {
                        memset(&Pattern, 0, sizeof(Pattern));
                        changeLed();
                    } else if (batteryStatus == 1) {
                        memset(&Pattern, 0, sizeof(Pattern));
                        Pattern.baseMiniCycleDuration = 0x0F;
                        Pattern.startIntensity = 0x5;
                        Pattern.miniCycles[0].ledIntensity = 0x5;
                        Pattern.miniCycles[0].transitionSteps = 0x0F;
                        Pattern.miniCycles[0].finalStepDuration = 0x0F;
                        changeLed();
                    } else if (batteryStatus == 2) {
                        memset(&Pattern, 0, sizeof(Pattern));
                        memset(&Pattern, 0, sizeof(Pattern));
                        Pattern.baseMiniCycleDuration = 0x4;
                        Pattern.totalMiniCycles = 0x4;
                        Pattern.startIntensity = 0x2;
                        Pattern.miniCycles[0].ledIntensity = 0xF;
                        Pattern.miniCycles[0].transitionSteps = 0x2;
                        Pattern.miniCycles[1].ledIntensity = 0x2;
                        Pattern.miniCycles[1].transitionSteps = 0x2;
                        changeLed();
                    }
                }
            } else {
                memset(&Pattern, 0, sizeof(Pattern));
                batteryStatus = -1;
                changeLed();
            }
        }
        svcSleepThread(500000000ULL);
    }
    return 0;
}
