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
static int smartLastState = -1; // 0: off, 1: solid, 2: dim, 3: fade, 4: blink
static int smartFullToggleCounter = 0;
static bool smartFullBlinkPhase = false;
static int smartFull100Counter = 0;
static bool smartFullTimeout = false;

static int smartLowBatteryThreshold = 15;
static int smartFullTimeoutMinutes = 30;
static int smartFullBlinkToggleSeconds = 5;
static int smartFullTimeoutLoops = 3600;       // derived from minutes
static int smartFullToggleThreshold = 10;      // derived from seconds (loops)
static int smartFullBlinkActiveLoops = 0;      // how long to stay blinking in a burst
static int smartFullPattern = 4;               // 0: off, 1: solid, 2: dim, 3: fade, 4: blink

static int smartChargeIntervalSeconds = 0;
static int smartChargeIntervalLoops = 0;
static int smartChargeIntervalCounter = 0;
static int smartChargeBurstLoops = 0;
static int smartChargePattern = 3;             // default fade while charging <100%

static int smartLowIntervalSeconds = 0;
static int smartLowIntervalLoops = 0;
static int smartLowIntervalCounter = 0;
static int smartLowBurstLoops = 0;
static int smartLowPattern = 4;                // 0: off, 1: solid, 2: dim, 3: fade, 4: blink

static int smartDischargeIntervalSeconds = 0;
static int smartDischargeIntervalLoops = 0;
static int smartDischargeIntervalCounter = 0;
static int smartDischargeBurstLoops = 0;
static int smartDischargeBurstLoopsMax = 4;
static int smartDischargePattern = 0;          // 0: off, 1: solid, 2: dim, 3: fade, 4: blink

static int smartDropNotificationSeconds = 10;
static int smartDropNotificationLoops = 20;
static int smartDropNotificationPattern = 3;   // 0: off, 1: solid, 2: dim, 3: fade, 4: blink

static int smartDropStepPercent = 10;

static void applySolidPattern(void);
static void applyDimPattern(void);
static void applyFadePattern(void);
static void applyOffPattern(void);
static void applyBlinkPattern(void);
static void changeLed(void);
static void handleFullBlinkNotification(void);

static void applyPatternCode(int code) {
    switch (code) {
        case 1:
            applySolidPattern();
            break;
        case 2:
            applyDimPattern();
            break;
        case 3:
            applyFadePattern();
            break;
        case 4:
            applyBlinkPattern();
            break;
        default:
            applyOffPattern();
            break;
    }
}

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

static void handleFullBlinkNotification(void) {
    if (smartFullBlinkActiveLoops > 0) {
        smartFullBlinkActiveLoops--;
        if (smartFullPattern == 0) {
            if (smartLastState != 0) {
                applyOffPattern();
                smartLastState = 0;
                changeLed();
            }
        } else if (smartLastState != smartFullPattern) {
            applyPatternCode(smartFullPattern);
            smartLastState = smartFullPattern;
            changeLed();
        }
    } else {
        smartFullToggleCounter++;
        if (smartFullToggleCounter >= smartFullToggleThreshold) {
            smartFullToggleCounter = 0;
            smartFullBlinkActiveLoops = 4; // ~2 seconds of notification
            if (smartFullPattern == 0) {
                if (smartLastState != 0) {
                    applyOffPattern();
                    smartLastState = 0;
                    changeLed();
                }
            } else if (smartLastState != smartFullPattern) {
                applyPatternCode(smartFullPattern);
                smartLastState = smartFullPattern;
                changeLed();
            }
        } else {
            if (smartLastState != 0) {
                applyOffPattern();
                smartLastState = 0;
                changeLed();
            }
        }
    }
}

static void loadSmartConfig(void) {
    int low = 15;
    int timeoutMin = 30;
    int toggleSec = 5;
    int chargeIntervalSec = 0;
    int lowIntervalSec = 0;
    int dischargeIntervalSec = 0;
    int dischargeDurationSec = 2;
    int dischargePattern = 0;
    int dropSec = 10;
    int dropPat = 3;
    int fullPat = 4;
    int lowPat = 4;
    int chargePat = 3;

    FILE* f = fopen("sdmc:/config/sys-notif-LED/settings.cfg", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char key[64];
            char value[64];
            if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
                int v = atoi(value);
                if (strcmp(key, "low_battery_threshold") == 0) {
                    if (v >= 1 && v <= 100) low = v;
                } else if (strcmp(key, "full_timeout_minutes") == 0) {
                    if (v >= 1 && v <= 300) timeoutMin = v;
                } else if (strcmp(key, "full_blink_toggle_seconds") == 0) {
                    if (v >= 0 && v <= 600) toggleSec = v;
                } else if (strcmp(key, "charge_interval_seconds") == 0) {
                    if (v >= 0 && v <= 600) chargeIntervalSec = v;
                } else if (strcmp(key, "low_interval_seconds") == 0) {
                    if (v >= 0 && v <= 600) lowIntervalSec = v;
                } else if (strcmp(key, "discharge_interval_seconds") == 0) {
                    if (v >= 0 && v <= 600) dischargeIntervalSec = v;
                } else if (strcmp(key, "discharge_duration_seconds") == 0) {
                    if (v >= 0 && v <= 600) dischargeDurationSec = v;
                } else if (strcmp(key, "discharge_pattern") == 0) {
                    if (strcmp(value, "solid") == 0) {
                        dischargePattern = 1;
                    } else if (strcmp(value, "dim") == 0) {
                        dischargePattern = 2;
                    } else if (strcmp(value, "fade") == 0) {
                        dischargePattern = 3;
                    } else if (strcmp(value, "blink") == 0) {
                        dischargePattern = 4;
                    } else {
                        dischargePattern = 0;
                    }
                } else if (strcmp(key, "charge_pattern") == 0) {
                    if (strcmp(value, "solid") == 0) {
                        chargePat = 1;
                    } else if (strcmp(value, "dim") == 0) {
                        chargePat = 2;
                    } else if (strcmp(value, "fade") == 0) {
                        chargePat = 3;
                    } else if (strcmp(value, "blink") == 0) {
                        chargePat = 4;
                    } else {
                        chargePat = 0;
                    }
                } else if (strcmp(key, "drop_notification_seconds") == 0) {
                    if (v >= 0 && v <= 600) dropSec = v;
                } else if (strcmp(key, "drop_notification_pattern") == 0) {
                    if (strcmp(value, "solid") == 0) {
                        dropPat = 1;
                    } else if (strcmp(value, "dim") == 0) {
                        dropPat = 2;
                    } else if (strcmp(value, "fade") == 0) {
                        dropPat = 3;
                    } else if (strcmp(value, "blink") == 0) {
                        dropPat = 4;
                    } else {
                        dropPat = 0;
                    }
                } else if (strcmp(key, "drop_step_percent") == 0) {
                    if (v >= 1 && v <= 100) smartDropStepPercent = v;
                } else if (strcmp(key, "full_pattern") == 0) {
                    if (strcmp(value, "solid") == 0) {
                        fullPat = 1;
                    } else if (strcmp(value, "dim") == 0) {
                        fullPat = 2;
                    } else if (strcmp(value, "fade") == 0) {
                        fullPat = 3;
                    } else if (strcmp(value, "blink") == 0) {
                        fullPat = 4;
                    } else {
                        fullPat = 0;
                    }
                } else if (strcmp(key, "low_pattern") == 0) {
                    if (strcmp(value, "solid") == 0) {
                        lowPat = 1;
                    } else if (strcmp(value, "dim") == 0) {
                        lowPat = 2;
                    } else if (strcmp(value, "fade") == 0) {
                        lowPat = 3;
                    } else if (strcmp(value, "blink") == 0) {
                        lowPat = 4;
                    } else {
                        lowPat = 0;
                    }
                }
            }
        }
        fclose(f);
    }

    smartLowBatteryThreshold = low;
    smartFullTimeoutMinutes = timeoutMin;
    smartFullBlinkToggleSeconds = toggleSec;

    smartFullTimeoutLoops = smartFullTimeoutMinutes * 60 * 2;
    smartFullToggleThreshold = (smartFullBlinkToggleSeconds > 0) ? smartFullBlinkToggleSeconds * 2 : 0;
    if (smartFullTimeoutLoops < 1) smartFullTimeoutLoops = 1;

    smartChargeIntervalSeconds = chargeIntervalSec;
    smartLowIntervalSeconds = lowIntervalSec;
    smartChargeIntervalLoops = (smartChargeIntervalSeconds > 0) ? smartChargeIntervalSeconds * 2 : 0;
    smartLowIntervalLoops = (smartLowIntervalSeconds > 0) ? smartLowIntervalSeconds * 2 : 0;
    smartDischargeIntervalSeconds = dischargeIntervalSec;
    smartDischargeIntervalLoops = (smartDischargeIntervalSeconds > 0) ? smartDischargeIntervalSeconds * 2 : 0;
    smartDischargeIntervalCounter = 0;
    smartDischargeBurstLoops = 0;
    smartDischargeBurstLoopsMax = (dischargeDurationSec > 0) ? dischargeDurationSec * 2 : 4;
    smartDischargePattern = dischargePattern;
    smartDropNotificationSeconds = dropSec;
    smartDropNotificationPattern = dropPat;
    smartDropNotificationLoops = (smartDropNotificationSeconds > 0) ? smartDropNotificationSeconds * 2 : 0;
    if (smartDropNotificationLoops < 0) smartDropNotificationLoops = 0;
    smartFullPattern = fullPat;
    smartLowPattern = lowPat;
    smartChargePattern = chargePat;
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
    } else if (strcmp(buffer, "dim") == 0) {
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
        smartFull100Counter = 0;
        smartFullTimeout = false;
        smartFullBlinkActiveLoops = 0;
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
        smartFull100Counter = 0;
        smartFullTimeout = false;
        smartFullBlinkActiveLoops = 0;
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

static void changeLed(void) {
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
    loadSmartConfig();
    FILE* file = fopen("sdmc:/config/sys-notif-LED/type", "r");
    if (!file) {
        FILE* f = fopen("sdmc:/config/sys-notif-LED/type", "w");
        if (f) {
            fprintf(f, "dim");
            fclose(f);
        }
    }
    else {
        fclose(file);
    }
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
        static int configReloadCounter = 0;
        scanForNewControllers();
        static int verifyCounter = 0;
        if (verifyCounter++ >= 5) {
            verifyConnectedControllers();
            verifyCounter = 0;
        }
        if (++configReloadCounter >= 120) { // ~60 seconds (loop sleeps 0.5s)
            loadSmartConfig();
            configReloadCounter = 0;
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

            int currentSegment = (smartDropStepPercent > 0) ? (batteryCharge / smartDropStepPercent) : (batteryCharge / 10);

            bool onBattery = (chargerType == PsmChargerType_Unconnected);

            if (onBattery) {
                smartChargeIntervalCounter = 0;
                smartChargeBurstLoops = 0;
            }

            if (!onBattery && batteryCharge >= 100) {
                smartBatterySegment = -1;
                smartFadeCountdown = 0;

                smartFull100Counter++;
                if (smartFull100Counter >= smartFullTimeoutLoops) {
                    smartFullTimeout = true;
                }

                if (smartFullTimeout) {
                    if (smartLastState != 0) {
                        applyOffPattern();
                        smartLastState = 0;
                        changeLed();
                    }
                } else {
                    handleFullBlinkNotification();
                }
            } else {
                smartFull100Counter = 0;
                smartFullTimeout = false;
                smartFullToggleCounter = 0;
                smartFullBlinkPhase = false;
                smartFullBlinkActiveLoops = 0;

                if (onBattery) {
                    if (smartBatterySegment == -1) {
                        smartBatterySegment = currentSegment;
                    }

                    if (currentSegment < smartBatterySegment && batteryCharge >= smartLowBatteryThreshold) {
                        smartBatterySegment = currentSegment;
                        smartFadeCountdown = smartDropNotificationLoops;
                    }

                    if (batteryCharge < smartLowBatteryThreshold) {
                        smartFadeCountdown = 0;
                        if (smartLowIntervalLoops <= 0) {
                            if (smartLowPattern == 0) {
                                if (smartLastState != 0) {
                                    applyOffPattern();
                                    smartLastState = 0;
                                    changeLed();
                                }
                            } else if (smartLastState != smartLowPattern) {
                                applyPatternCode(smartLowPattern);
                                smartLastState = smartLowPattern;
                                changeLed();
                            }
                        } else {
                            if (smartLowBurstLoops > 0) {
                                smartLowBurstLoops--;
                                if (smartLowPattern == 0) {
                                    if (smartLastState != 0) {
                                        applyOffPattern();
                                        smartLastState = 0;
                                        changeLed();
                                    }
                                } else if (smartLastState != smartLowPattern) {
                                    applyPatternCode(smartLowPattern);
                                    smartLastState = smartLowPattern;
                                    changeLed();
                                }
                            } else {
                                smartLowIntervalCounter++;
                                if (smartLowIntervalCounter >= smartLowIntervalLoops) {
                                    smartLowIntervalCounter = 0;
                                    smartLowBurstLoops = 4; // ~2 seconds of notification
                                    if (smartLowPattern == 0) {
                                        if (smartLastState != 0) {
                                            applyOffPattern();
                                            smartLastState = 0;
                                            changeLed();
                                        }
                                    } else if (smartLastState != smartLowPattern) {
                                        applyPatternCode(smartLowPattern);
                                        smartLastState = smartLowPattern;
                                        changeLed();
                                    }
                                } else {
                                    if (smartLastState != 0) {
                                        applyOffPattern();
                                        smartLastState = 0;
                                        changeLed();
                                    }
                                }
                            }
                        }
                    } else if (smartFadeCountdown > 0) {
                        smartFadeCountdown--;
                        if (smartDropNotificationPattern == 0) {
                            if (smartLastState != 0) {
                                applyOffPattern();
                                smartLastState = 0;
                                changeLed();
                            }
                        } else if (smartLastState != smartDropNotificationPattern) {
                            applyPatternCode(smartDropNotificationPattern);
                            smartLastState = smartDropNotificationPattern;
                            changeLed();
                        }
                    } else {
                        smartLowIntervalCounter = 0;
                        smartLowBurstLoops = 0;
                        if (smartDischargeIntervalLoops <= 0 || smartDischargePattern == 0) {
                            if (smartLastState != 0) {
                                applyOffPattern();
                                smartLastState = 0;
                                changeLed();
                            }
                        } else {
                            if (smartDischargeBurstLoops > 0) {
                                smartDischargeBurstLoops--;
                                if (smartLastState != smartDischargePattern) {
                                    applyPatternCode(smartDischargePattern);
                                    smartLastState = smartDischargePattern;
                                    changeLed();
                                }
                            } else {
                                smartDischargeIntervalCounter++;
                                if (smartDischargeIntervalCounter >= smartDischargeIntervalLoops) {
                                smartDischargeIntervalCounter = 0;
                                smartDischargeBurstLoops = smartDischargeBurstLoopsMax; // configurable burst duration
                                    if (smartDischargePattern == 0) {
                                        if (smartLastState != 0) {
                                            applyOffPattern();
                                            smartLastState = 0;
                                            changeLed();
                                        }
                                    } else {
                                        applyPatternCode(smartDischargePattern);
                                        smartLastState = smartDischargePattern;
                                        changeLed();
                                    }
                                } else {
                                    if (smartLastState != 0) {
                                        applyOffPattern();
                                        smartLastState = 0;
                                        changeLed();
                                    }
                                }
                            }
                        }
                    }
                } else {
                    smartBatterySegment = -1;
                    smartFadeCountdown = 0;

                    if (smartChargeIntervalLoops <= 0) {
                        if (smartChargePattern == 0) {
                            if (smartLastState != 0) {
                                applyOffPattern();
                                smartLastState = 0;
                                changeLed();
                            }
                        } else if (smartLastState != smartChargePattern) {
                            applyPatternCode(smartChargePattern);
                            smartLastState = smartChargePattern;
                            changeLed();
                        }
                    } else {
                        if (smartChargeBurstLoops > 0) {
                            smartChargeBurstLoops--;
                            if (smartChargePattern == 0) {
                                if (smartLastState != 0) {
                                    applyOffPattern();
                                    smartLastState = 0;
                                    changeLed();
                                }
                            } else if (smartLastState != smartChargePattern) {
                                applyPatternCode(smartChargePattern);
                                smartLastState = smartChargePattern;
                                changeLed();
                            }
                        } else {
                            smartChargeIntervalCounter++;
                            if (smartChargeIntervalCounter >= smartChargeIntervalLoops) {
                                smartChargeIntervalCounter = 0;
                                smartChargeBurstLoops = 4; // ~2 seconds notification
                                if (smartChargePattern == 0) {
                                    if (smartLastState != 0) {
                                        applyOffPattern();
                                        smartLastState = 0;
                                        changeLed();
                                    }
                                } else if (smartLastState != smartChargePattern) {
                                    applyPatternCode(smartChargePattern);
                                    smartLastState = smartChargePattern;
                                    changeLed();
                                }
                            } else {
                                if (smartLastState != 0) {
                                    applyOffPattern();
                                    smartLastState = 0;
                                    changeLed();
                                }
                            }
                        }
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
