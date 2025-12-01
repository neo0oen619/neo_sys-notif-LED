#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdarg.h>

typedef struct {
    int low_battery_threshold;
    int full_timeout_minutes;
    int full_blink_toggle_seconds;
    int full_pattern;
    int charge_interval_seconds;
    int charge_pattern;
    int low_interval_seconds;
    int low_pattern;
    int discharge_interval_seconds;
    int discharge_duration_seconds;
    int discharge_pattern; // 0: off, 1: solid, 2: dim, 3: fade, 4: blink
    int drop_step_percent;
    int drop_notification_seconds;
    int drop_notification_pattern;
    int log_enabled;
    int sleep_keep_led;
    int sleep_on_full_timeout;
} SmartConfig;

static char gCurrentMode[32] = "unknown";
static int gLogEnabled = 1;
static int gFullRemainingSeconds = -1;
static int gIdleRemainingSeconds = -1;

static void logLine(const char* fmt, ...) {
    if (!gLogEnabled)
        return;

    DIR* dir = opendir("sdmc:/config/sys-notif-LED");
    if (dir) {
        closedir(dir);
    } else {
        mkdir("sdmc:/config/sys-notif-LED", 0777);
    }

    FILE* f = fopen("sdmc:/config/sys-notif-LED/config_app_log.txt", "a");
    if (!f)
        return;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fputc('\n', f);
    fclose(f);
}

static void loadStatus(void) {
    FILE* f = fopen("sdmc:/config/sys-notif-LED/status.txt", "r");
    if (!f) {
        gFullRemainingSeconds = -1;
        return;
    }

    char line[128];
    int remainingFull = -1;
    int remainingIdle = -1;
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "full_remaining_seconds") == 0) {
                remainingFull = atoi(value);
            } else if (strcmp(key, "idle_remaining_seconds") == 0) {
                remainingIdle = atoi(value);
            }
        }
    }

    fclose(f);
    gFullRemainingSeconds = remainingFull;
    gIdleRemainingSeconds = remainingIdle;
}

static void loadCurrentMode(void) {
    FILE* f = fopen("sdmc:/config/sys-notif-LED/type", "r");
    if (!f) {
        strcpy(gCurrentMode, "unknown");
        logLine("[loadCurrentMode] type not found");
        return;
    }

    if (fgets(gCurrentMode, sizeof(gCurrentMode), f)) {
        gCurrentMode[strcspn(gCurrentMode, "\r\n")] = 0;
    } else {
        strcpy(gCurrentMode, "unknown");
    }

    logLine("[loadCurrentMode] mode='%s'", gCurrentMode);
    fclose(f);
}

static void setModeSmart(void) {
    DIR* dir = opendir("sdmc:/config/sys-notif-LED");
    if (dir) {
        closedir(dir);
    } else {
        mkdir("sdmc:/config/sys-notif-LED", 0777);
    }

    FILE* f = fopen("sdmc:/config/sys-notif-LED/reset", "w");
    if (f)
        fclose(f);

    f = fopen("sdmc:/config/sys-notif-LED/type", "w");
    if (f) {
        fputs("smart\n", f);
        fclose(f);
    }

    strcpy(gCurrentMode, "smart");
    logLine("[setModeSmart] mode set to smart");
}

static void loadConfig(SmartConfig* cfg) {
    cfg->low_battery_threshold = 15;
    cfg->full_timeout_minutes = 30;
    cfg->full_blink_toggle_seconds = 5;
    cfg->full_pattern = 4; // blink
    cfg->charge_interval_seconds = 0;
    cfg->charge_pattern = 3; // fade by default
    cfg->low_interval_seconds = 0;
    cfg->low_pattern = 4; // blink
    cfg->discharge_interval_seconds = 0;
    cfg->discharge_duration_seconds = 2;
    cfg->discharge_pattern = 0;
    cfg->drop_step_percent = 10;
    cfg->drop_notification_seconds = 10;
    cfg->drop_notification_pattern = 3;
    cfg->log_enabled = 1;
    cfg->sleep_keep_led = 0;
    cfg->sleep_on_full_timeout = 0;

    FILE* f = fopen("sdmc:/config/sys-notif-LED/settings.cfg", "r");
    if (!f) {
        logLine("[loadConfig] settings.cfg not found, using defaults");
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            int v = atoi(value);
            if (strcmp(key, "low_battery_threshold") == 0) {
                if (v >= 1 && v <= 100) cfg->low_battery_threshold = v;
            } else if (strcmp(key, "full_timeout_minutes") == 0) {
                if (v >= 1 && v <= 300) cfg->full_timeout_minutes = v;
            } else if (strcmp(key, "full_blink_toggle_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->full_blink_toggle_seconds = v;
            } else if (strcmp(key, "charge_interval_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->charge_interval_seconds = v;
            } else if (strcmp(key, "low_interval_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->low_interval_seconds = v;
            } else if (strcmp(key, "full_pattern") == 0) {
                if (strcmp(value, "solid") == 0) {
                    cfg->full_pattern = 1;
                } else if (strcmp(value, "dim") == 0) {
                    cfg->full_pattern = 2;
                } else if (strcmp(value, "fade") == 0) {
                    cfg->full_pattern = 3;
                } else if (strcmp(value, "blink") == 0) {
                    cfg->full_pattern = 4;
                } else {
                    cfg->full_pattern = 0;
                }
            } else if (strcmp(key, "low_pattern") == 0) {
                if (strcmp(value, "solid") == 0) {
                    cfg->low_pattern = 1;
                } else if (strcmp(value, "dim") == 0) {
                    cfg->low_pattern = 2;
                } else if (strcmp(value, "fade") == 0) {
                    cfg->low_pattern = 3;
                } else if (strcmp(value, "blink") == 0) {
                    cfg->low_pattern = 4;
                } else {
                    cfg->low_pattern = 0;
                }
            } else if (strcmp(key, "discharge_interval_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->discharge_interval_seconds = v;
            } else if (strcmp(key, "discharge_duration_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->discharge_duration_seconds = v;
            } else if (strcmp(key, "discharge_pattern") == 0) {
                if (strcmp(value, "solid") == 0) {
                    cfg->discharge_pattern = 1;
                } else if (strcmp(value, "dim") == 0) {
                    cfg->discharge_pattern = 2;
                } else if (strcmp(value, "fade") == 0) {
                    cfg->discharge_pattern = 3;
                } else if (strcmp(value, "blink") == 0) {
                    cfg->discharge_pattern = 4;
                } else {
                    cfg->discharge_pattern = 0;
                }
            } else if (strcmp(key, "drop_notification_seconds") == 0) {
                if (v >= 0 && v <= 600) cfg->drop_notification_seconds = v;
            } else if (strcmp(key, "drop_notification_pattern") == 0) {
                if (strcmp(value, "solid") == 0) {
                    cfg->drop_notification_pattern = 1;
                } else if (strcmp(value, "dim") == 0) {
                    cfg->drop_notification_pattern = 2;
                } else if (strcmp(value, "fade") == 0) {
                    cfg->drop_notification_pattern = 3;
                } else if (strcmp(value, "blink") == 0) {
                    cfg->drop_notification_pattern = 4;
                } else {
                    cfg->drop_notification_pattern = 0;
                }
            } else if (strcmp(key, "drop_step_percent") == 0) {
                if (v >= 1 && v <= 100) cfg->drop_step_percent = v;
            } else if (strcmp(key, "charge_pattern") == 0) {
                if (strcmp(value, "solid") == 0) {
                    cfg->charge_pattern = 1;
                } else if (strcmp(value, "dim") == 0) {
                    cfg->charge_pattern = 2;
                } else if (strcmp(value, "fade") == 0) {
                    cfg->charge_pattern = 3;
                } else if (strcmp(value, "blink") == 0) {
                    cfg->charge_pattern = 4;
                } else {
                    cfg->charge_pattern = 0;
                }
            } else if (strcmp(key, "log_enabled") == 0) {
                if (v == 0 || v == 1) cfg->log_enabled = v;
            } else if (strcmp(key, "sleep_keep_led") == 0) {
                if (v == 0 || v == 1) cfg->sleep_keep_led = v;
            } else if (strcmp(key, "sleep_on_full_timeout") == 0) {
                if (v == 0 || v == 1) cfg->sleep_on_full_timeout = v;
            }
        }
    }
    gLogEnabled = cfg->log_enabled;
    logLine("[loadConfig] loaded settings: low=%d full_timeout=%d full_interval=%d",
            cfg->low_battery_threshold, cfg->full_timeout_minutes, cfg->full_blink_toggle_seconds);
    fclose(f);
}

static void saveConfig(const SmartConfig* cfg) {
    DIR* dir = opendir("sdmc:/config/sys-notif-LED");
    if (dir) {
        closedir(dir);
    } else {
        mkdir("sdmc:/config/sys-notif-LED", 0777);
    }

    FILE* f = fopen("sdmc:/config/sys-notif-LED/settings.cfg", "w");
    if (!f) {
        logLine("[saveConfig] failed to open settings.cfg for write");
        return;
    }

    fprintf(f, "low_battery_threshold=%d\n", cfg->low_battery_threshold);
    fprintf(f, "full_timeout_minutes=%d\n", cfg->full_timeout_minutes);
    fprintf(f, "full_blink_toggle_seconds=%d\n", cfg->full_blink_toggle_seconds);
    fprintf(f, "charge_interval_seconds=%d\n", cfg->charge_interval_seconds);
    fprintf(f, "low_interval_seconds=%d\n", cfg->low_interval_seconds);
    const char* chargeName = "off";
    if (cfg->charge_pattern == 1) chargeName = "solid";
    else if (cfg->charge_pattern == 2) chargeName = "dim";
    else if (cfg->charge_pattern == 3) chargeName = "fade";
    else if (cfg->charge_pattern == 4) chargeName = "blink";
    fprintf(f, "charge_pattern=%s\n", chargeName);

    fprintf(f, "discharge_interval_seconds=%d\n", cfg->discharge_interval_seconds);
    fprintf(f, "discharge_duration_seconds=%d\n", cfg->discharge_duration_seconds);
    const char* pat = "off";
    if (cfg->discharge_pattern == 1) pat = "solid";
    else if (cfg->discharge_pattern == 2) pat = "dim";
    else if (cfg->discharge_pattern == 3) pat = "fade";
    else if (cfg->discharge_pattern == 4) pat = "blink";
    fprintf(f, "discharge_pattern=%s\n", pat);
    const char* fullName = "off";
    if (cfg->full_pattern == 1) fullName = "solid";
    else if (cfg->full_pattern == 2) fullName = "dim";
    else if (cfg->full_pattern == 3) fullName = "fade";
    else if (cfg->full_pattern == 4) fullName = "blink";
    fprintf(f, "full_pattern=%s\n", fullName);

    const char* lowName = "off";
    if (cfg->low_pattern == 1) lowName = "solid";
    else if (cfg->low_pattern == 2) lowName = "dim";
    else if (cfg->low_pattern == 3) lowName = "fade";
    else if (cfg->low_pattern == 4) lowName = "blink";
    fprintf(f, "low_pattern=%s\n", lowName);

    fprintf(f, "drop_step_percent=%d\n", cfg->drop_step_percent);
    fprintf(f, "drop_notification_seconds=%d\n", cfg->drop_notification_seconds);
    const char* dropName = "off";
    if (cfg->drop_notification_pattern == 1) dropName = "solid";
    else if (cfg->drop_notification_pattern == 2) dropName = "dim";
    else if (cfg->drop_notification_pattern == 3) dropName = "fade";
    else if (cfg->drop_notification_pattern == 4) dropName = "blink";
    fprintf(f, "drop_notification_pattern=%s\n", dropName);
    fprintf(f, "log_enabled=%d\n", cfg->log_enabled);
    fprintf(f, "sleep_keep_led=%d\n", cfg->sleep_keep_led);
    fprintf(f, "sleep_on_full_timeout=%d\n", cfg->sleep_on_full_timeout);
    fclose(f);
    logLine("[saveConfig] saved settings");
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);

    // Mount SD once at startup for all FS operations
    fsdevMountSdmc();
    logLine("=== sys-notif-LED-config start ===");

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    SmartConfig cfg;
    loadConfig(&cfg);
    loadCurrentMode();

    int screen = 0;   // 0: main, 1: full, 2: not charging, 3: low, 4: system
    int selected = 0; // index inside current screen
    bool dirty = false;

    while (appletMainLoop()) {
        loadStatus();
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown) {
            logLine("[loop] screen=%d selected=%d kDown=%llx", screen, selected, (unsigned long long)kDown);
        }

        if (kDown & HidNpadButton_Plus) {
            if (dirty) {
                saveConfig(&cfg);
            }
            logLine("[loop] Plus pressed, exiting app");
            break;
        }

        if (screen != 0 && (kDown & HidNpadButton_B)) {
            screen = 0;
            selected = 0;
            continue;
        }

        int step = 1;
        if (kDown & HidNpadButton_X) {
            step = 5;
        }

        if (screen == 0) {
            // Main screen: choose category
            if (kDown & HidNpadButton_Down) {
                selected = (selected + 1) % 4;
            }
            if (kDown & HidNpadButton_Up) {
                selected = (selected + 3) % 4;
            }
            if (kDown & HidNpadButton_Y) {
                logLine("[loop] Y pressed on main screen, setting mode smart");
                setModeSmart();
            }
            if (kDown & HidNpadButton_A) {
                screen = selected + 1;
                selected = 0;
            }
        } else if (screen == 1) {
            // Full charge + charging (<100%) screen
            if (kDown & HidNpadButton_Down) {
                selected = (selected + 1) % 5;
            }
            if (kDown & HidNpadButton_Up) {
                selected = (selected + 4) % 5;
            }

            if (kDown & HidNpadButton_Left) {
                if (selected == 0 && cfg.full_timeout_minutes > 1) {
                    cfg.full_timeout_minutes -= step;
                    if (cfg.full_timeout_minutes < 1) cfg.full_timeout_minutes = 1;
                    dirty = true;
                } else if (selected == 1 && cfg.full_blink_toggle_seconds > 0) {
                    cfg.full_blink_toggle_seconds -= step;
                    if (cfg.full_blink_toggle_seconds < 0) cfg.full_blink_toggle_seconds = 0;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.full_pattern--;
                    if (cfg.full_pattern < 0) cfg.full_pattern = 4;
                    dirty = true;
                } else if (selected == 3 && cfg.charge_interval_seconds > 0) {
                    cfg.charge_interval_seconds -= step;
                    if (cfg.charge_interval_seconds < 0) cfg.charge_interval_seconds = 0;
                    dirty = true;
                } else if (selected == 4) {
                    cfg.charge_pattern--;
                    if (cfg.charge_pattern < 0) cfg.charge_pattern = 4;
                    dirty = true;
                }
            }
            if (kDown & HidNpadButton_Right) {
                if (selected == 0 && cfg.full_timeout_minutes < 300) {
                    cfg.full_timeout_minutes += step;
                    if (cfg.full_timeout_minutes > 300) cfg.full_timeout_minutes = 300;
                    dirty = true;
                } else if (selected == 1 && cfg.full_blink_toggle_seconds < 600) {
                    cfg.full_blink_toggle_seconds += step;
                    if (cfg.full_blink_toggle_seconds > 600) cfg.full_blink_toggle_seconds = 600;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.full_pattern++;
                    if (cfg.full_pattern > 4) cfg.full_pattern = 0;
                    dirty = true;
                } else if (selected == 3 && cfg.charge_interval_seconds < 600) {
                    cfg.charge_interval_seconds += step;
                    if (cfg.charge_interval_seconds > 600) cfg.charge_interval_seconds = 600;
                    dirty = true;
                } else if (selected == 4) {
                    cfg.charge_pattern++;
                    if (cfg.charge_pattern > 4) cfg.charge_pattern = 0;
                    dirty = true;
                }
            }
        } else if (screen == 2) {
            // Not charging (discharge + drop) screen
            if (kDown & HidNpadButton_Down) {
                selected = (selected + 1) % 6;
            }
            if (kDown & HidNpadButton_Up) {
                selected = (selected + 5) % 6;
            }

            if (kDown & HidNpadButton_Left) {
                if (selected == 0 && cfg.discharge_interval_seconds > 0) {
                    cfg.discharge_interval_seconds -= step;
                    if (cfg.discharge_interval_seconds < 0) cfg.discharge_interval_seconds = 0;
                    dirty = true;
                } else if (selected == 1 && cfg.discharge_duration_seconds > 0) {
                    cfg.discharge_duration_seconds -= step;
                    if (cfg.discharge_duration_seconds < 0) cfg.discharge_duration_seconds = 0;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.discharge_pattern--;
                    if (cfg.discharge_pattern < 0) cfg.discharge_pattern = 4;
                    dirty = true;
                } else if (selected == 3 && cfg.drop_step_percent > 1) {
                    cfg.drop_step_percent -= step;
                    if (cfg.drop_step_percent < 1) cfg.drop_step_percent = 1;
                    dirty = true;
                } else if (selected == 4 && cfg.drop_notification_seconds > 0) {
                    cfg.drop_notification_seconds -= step;
                    if (cfg.drop_notification_seconds < 0) cfg.drop_notification_seconds = 0;
                    dirty = true;
                } else if (selected == 5) {
                    cfg.drop_notification_pattern--;
                    if (cfg.drop_notification_pattern < 0) cfg.drop_notification_pattern = 4;
                    dirty = true;
                }
            }
            if (kDown & HidNpadButton_Right) {
                if (selected == 0 && cfg.discharge_interval_seconds < 600) {
                    cfg.discharge_interval_seconds += step;
                    if (cfg.discharge_interval_seconds > 600) cfg.discharge_interval_seconds = 600;
                    dirty = true;
                } else if (selected == 1 && cfg.discharge_duration_seconds < 600) {
                    cfg.discharge_duration_seconds += step;
                    if (cfg.discharge_duration_seconds > 600) cfg.discharge_duration_seconds = 600;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.discharge_pattern++;
                    if (cfg.discharge_pattern > 4) cfg.discharge_pattern = 0;
                    dirty = true;
                } else if (selected == 3 && cfg.drop_step_percent < 100) {
                    cfg.drop_step_percent += step;
                    if (cfg.drop_step_percent > 100) cfg.drop_step_percent = 100;
                    dirty = true;
                } else if (selected == 4 && cfg.drop_notification_seconds < 600) {
                    cfg.drop_notification_seconds += step;
                    if (cfg.drop_notification_seconds > 600) cfg.drop_notification_seconds = 600;
                    dirty = true;
                } else if (selected == 5) {
                    cfg.drop_notification_pattern++;
                    if (cfg.drop_notification_pattern > 4) cfg.drop_notification_pattern = 0;
                    dirty = true;
                }
            }
        } else if (screen == 3) {
            // Low battery screen
            if (kDown & HidNpadButton_Down) {
                selected = (selected + 1) % 3;
            }
            if (kDown & HidNpadButton_Up) {
                selected = (selected + 2) % 3;
            }

            if (kDown & HidNpadButton_Left) {
                if (selected == 0 && cfg.low_battery_threshold > 1) {
                    cfg.low_battery_threshold -= step;
                    if (cfg.low_battery_threshold < 1) cfg.low_battery_threshold = 1;
                    dirty = true;
                } else if (selected == 1 && cfg.low_interval_seconds > 0) {
                    cfg.low_interval_seconds -= step;
                    if (cfg.low_interval_seconds < 0) cfg.low_interval_seconds = 0;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.low_pattern--;
                    if (cfg.low_pattern < 0) cfg.low_pattern = 4;
                    dirty = true;
                }
            }
            if (kDown & HidNpadButton_Right) {
                if (selected == 0 && cfg.low_battery_threshold < 100) {
                    cfg.low_battery_threshold += step;
                    if (cfg.low_battery_threshold > 100) cfg.low_battery_threshold = 100;
                    dirty = true;
                } else if (selected == 1 && cfg.low_interval_seconds < 600) {
                    cfg.low_interval_seconds += step;
                    if (cfg.low_interval_seconds > 600) cfg.low_interval_seconds = 600;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.low_pattern++;
                    if (cfg.low_pattern > 4) cfg.low_pattern = 0;
                    dirty = true;
                }
            }
        } else if (screen == 4) {
            // System / logging screen
            if (kDown & HidNpadButton_Down) {
                selected = (selected + 1) % 3;
            }
            if (kDown & HidNpadButton_Up) {
                selected = (selected + 2) % 3;
            }

            if (kDown & HidNpadButton_Left || kDown & HidNpadButton_Right) {
                if (selected == 0) {
                    cfg.sleep_keep_led = cfg.sleep_keep_led ? 0 : 1;
                    dirty = true;
                } else if (selected == 1) {
                    cfg.sleep_on_full_timeout = cfg.sleep_on_full_timeout ? 0 : 1;
                    dirty = true;
                } else if (selected == 2) {
                    cfg.log_enabled = !cfg.log_enabled;
                    gLogEnabled = cfg.log_enabled;
                    dirty = true;
                }
            }
        }

        consoleClear();
        printf("\x1b[2J\x1b[H");

        if (screen == 0) {
            printf("sys-notif-LED smart mode config\n\n");
            printf("Use Up/Down to choose category, A to open\n");
            printf("Y: set mode to SMART (writes type=smart)\n");
            printf("Press + to save and exit\n");
            printf("\n");

            printf("%c Full / Charging\n", selected == 0 ? '>' : ' ');
            printf("%c Not charging\n",  selected == 1 ? '>' : ' ');
            printf("%c Low battery\n", selected == 2 ? '>' : ' ');
            printf("%c System / Logging\n\n", selected == 3 ? '>' : ' ');

            const char* fullName = "Off";
            if (cfg.full_pattern == 1) fullName = "Solid";
            else if (cfg.full_pattern == 2) fullName = "Dim";
            else if (cfg.full_pattern == 3) fullName = "Fade";
            else if (cfg.full_pattern == 4) fullName = "Blink";

            const char* chargeName = "Off";
            if (cfg.charge_pattern == 1) chargeName = "Solid";
            else if (cfg.charge_pattern == 2) chargeName = "Dim";
            else if (cfg.charge_pattern == 3) chargeName = "Fade";
            else if (cfg.charge_pattern == 4) chargeName = "Blink";

            const char* idleName = "Off";
            if (cfg.discharge_pattern == 1) idleName = "Solid";
            else if (cfg.discharge_pattern == 2) idleName = "Dim";
            else if (cfg.discharge_pattern == 3) idleName = "Fade";
            else if (cfg.discharge_pattern == 4) idleName = "Blink";

            const char* fullSleepName = cfg.sleep_on_full_timeout ? "Sleep" : "No sleep";

            printf("Full: timeout %d min, interval %d s, pattern %s\n",
                   cfg.full_timeout_minutes, cfg.full_blink_toggle_seconds, fullName);
            printf("Charging<100%%: interval %d s, pattern %s\n",
                   cfg.charge_interval_seconds, chargeName);
            printf("Not charging: every %d s for %d s, pattern %s\n",
                   cfg.discharge_interval_seconds, cfg.discharge_duration_seconds, idleName);
            printf("Low: threshold %d%%, interval %d s\n", cfg.low_battery_threshold, cfg.low_interval_seconds);
            printf("System: sleep LED %s, full timeout %s, logging %s\n",
                   cfg.sleep_keep_led ? "keep pattern" : "off at timeout",
                   fullSleepName,
                   cfg.log_enabled ? "On" : "Off");
            printf("\nCurrent mode: %s\n", gCurrentMode);
        } else if (screen == 1) {
            printf("Full / Charging settings (B to go back)\n\n");
            printf("Hold X for steps of 5\n\n");
            const char* fullName2 = "Off";
            if (cfg.full_pattern == 1) fullName2 = "Solid";
            else if (cfg.full_pattern == 2) fullName2 = "Dim";
            else if (cfg.full_pattern == 3) fullName2 = "Fade";
            else if (cfg.full_pattern == 4) fullName2 = "Blink";

            const char* chargeName2 = "Off";
            if (cfg.charge_pattern == 1) chargeName2 = "Solid";
            else if (cfg.charge_pattern == 2) chargeName2 = "Dim";
            else if (cfg.charge_pattern == 3) chargeName2 = "Fade";
            else if (cfg.charge_pattern == 4) chargeName2 = "Blink";

            if (gFullRemainingSeconds >= 0) {
                int rem_min = gFullRemainingSeconds / 60;
                int rem_sec = gFullRemainingSeconds % 60;
                printf("%c Full timeout: %d minutes (rem %d:%02d)\n",
                       selected == 0 ? '>' : ' ', cfg.full_timeout_minutes, rem_min, rem_sec);
            } else {
                printf("%c Full timeout: %d minutes (inactive)\n",
                       selected == 0 ? '>' : ' ', cfg.full_timeout_minutes);
            }
            printf("%c Full interval: %d seconds\n", selected == 1 ? '>' : ' ', cfg.full_blink_toggle_seconds);
            printf("%c Full pattern: %s\n", selected == 2 ? '>' : ' ', fullName2);
            printf("%c Charge<100%% interval: %d seconds (0 = continuous)\n",
                   selected == 3 ? '>' : ' ', cfg.charge_interval_seconds);
            printf("%c Charge<100%% pattern: %s\n", selected == 4 ? '>' : ' ', chargeName2);
        } else if (screen == 2) {
            const char* patName = "Off";
            if (cfg.discharge_pattern == 1) patName = "Solid";
            else if (cfg.discharge_pattern == 2) patName = "Dim";
            else if (cfg.discharge_pattern == 3) patName = "Fade";
            else if (cfg.discharge_pattern == 4) patName = "Blink";

            const char* dropName = "Off";
            if (cfg.drop_notification_pattern == 1) dropName = "Solid";
            else if (cfg.drop_notification_pattern == 2) dropName = "Dim";
            else if (cfg.drop_notification_pattern == 3) dropName = "Fade";
            else if (cfg.drop_notification_pattern == 4) dropName = "Blink";

            printf("Not charging settings (B to go back)\n\n");
            printf("Hold X for steps of 5\n\n");
            if (gIdleRemainingSeconds >= 0) {
                printf("%c Idle interval: %d seconds (rem %d s)\n",
                       selected == 0 ? '>' : ' ', cfg.discharge_interval_seconds, gIdleRemainingSeconds);
            } else {
                printf("%c Idle interval: %d seconds (inactive)\n",
                       selected == 0 ? '>' : ' ', cfg.discharge_interval_seconds);
            }
            printf("%c Idle duration: %d seconds (0 = default)\n", selected == 1 ? '>' : ' ', cfg.discharge_duration_seconds);
            printf("%c Idle pattern: %s\n", selected == 2 ? '>' : ' ', patName);
            printf("%c Drop step: %d%%\n", selected == 3 ? '>' : ' ', cfg.drop_step_percent);
            printf("%c Drop duration: %d seconds (0 = off)\n", selected == 4 ? '>' : ' ', cfg.drop_notification_seconds);
            printf("%c Drop pattern: %s\n", selected == 5 ? '>' : ' ', dropName);
        } else if (screen == 3) {
            const char* lowName2 = "Off";
            if (cfg.low_pattern == 1) lowName2 = "Solid";
            else if (cfg.low_pattern == 2) lowName2 = "Dim";
            else if (cfg.low_pattern == 3) lowName2 = "Fade";
            else if (cfg.low_pattern == 4) lowName2 = "Blink";

            printf("Low battery settings (B to go back)\n\n");
            printf("Hold X for steps of 5\n\n");
            printf("%c Threshold: %d%%\n", selected == 0 ? '>' : ' ', cfg.low_battery_threshold);
            printf("%c Interval: %d seconds (0 = continuous)\n", selected == 1 ? '>' : ' ', cfg.low_interval_seconds);
            printf("%c Pattern: %s\n", selected == 2 ? '>' : ' ', lowName2);
        } else if (screen == 4) {
            const char* sleepName = cfg.sleep_keep_led ? "Keep pattern" : "Off at timeout";
            const char* fullSleepName = cfg.sleep_on_full_timeout ? "Sleep" : "No sleep";
            const char* logName2 = cfg.log_enabled ? "On" : "Off";

            printf("System / Logging settings (B to go back)\n\n");
            printf("Use Left/Right to change\n\n");
            printf("%c Sleep LED: %s\n", selected == 0 ? '>' : ' ', sleepName);
            printf("%c Full timeout: %s\n", selected == 1 ? '>' : ' ', fullSleepName);
            printf("%c Logging: %s\n",   selected == 2 ? '>' : ' ', logName2);
        }

        if (dirty) {
            printf("\nPending changes (will be saved on +)\n");
        }

        // Purple credit line at bottom-right
        PrintConsole* con = consoleGetDefault();
        if (con) {
            int rows = con->consoleHeight;
            int cols = con->consoleWidth;
            const char* credit = "made with <3 by neo0oen";
            int len = (int)strlen(credit);
            int col = cols - len;
            if (col < 0) col = 0;
            // Two-line purple credit, visually larger
            int row = rows - 1;
            if (row < 1) row = 1;
            printf("\x1b[%d;%dH\x1b[35m%s\x1b[0m", row, col + 1, credit);
            printf("\x1b[%d;%dH\x1b[35m%s\x1b[0m", rows, col + 1, credit);
        }

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
