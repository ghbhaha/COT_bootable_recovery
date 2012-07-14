/*
 * Copyright (C) 2012 Drew Walton & Nathan Bass
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <reboot/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "colorific.h"
#include "iniparse/ini.h"

int backupprompt = 0;
int orswipeprompt = 0;
int orsreboot = 0;

typedef struct {
    const char* theme;
    int orsreboot;
    int orswipeprompt;
    int backupprompt;
} settings;

typedef struct {
    int uicolor0;
    int uicolor1;
    int uicolor2;
    int bgicon;
} theme;

int settings_handler(void* user, const char* section, const char* name,
                   const char* value)
{
    settings* pconfig = (settings*)user;

    #define MATCH(s, n) strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0
    if (MATCH("settings", "theme")) {
        pconfig->theme = strdup(value);
    } else if (MATCH("settings", "orsreboot")) {
        pconfig->orsreboot = atoi(value);
    } else if (MATCH("settings", "orswipeprompt")) {
        pconfig->orswipeprompt = atoi(value);
    } else if (MATCH("settings", "backupprompt")) {
        pconfig->backupprompt = atoi(value);
    } else {
        return 0;
    }
    return 1;
}

int theme_handler(void* user, const char* section, const char* name,
                   const char* value)
{
    theme* pconfig = (theme*)user;

    #define MATCH(s, n) strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0
    if (MATCH("theme", "uicolor0")) {
        pconfig->uicolor0 = atoi(value);
    } else if (MATCH("theme", "uicolor1")) {
        pconfig->uicolor1 = atoi(value);
    } else if (MATCH("theme", "uicolor2")) {
        pconfig->uicolor2 = atoi(value);
    } else if (MATCH("theme", "bgicon")) {
        pconfig->bgicon = atoi(value);
    } else {
        return 0;
    }
    return 1;
}

void create_default_settings(void) {
    ensure_path_mounted("/sdcard");
    FILE    *   ini ;

    ini = fopen("/sdcard/clockworkmod/settings.ini", "w");
    fprintf(ini,
    ";\n"
    "; COT Settings INI\n"
    ";\n"
    "\n"
    "[Settings]\n"
    "Theme = hydro ;\n"
    "ORSReboot = FALSE ;\n"
    "ORSWipePrompt = TRUE ;\n"
    "BackupPrompt = TRUE ;\n"
    "\n");
    fclose(ini);
}

void parse_settings() {
    ensure_path_mounted("/sdcard");
    settings config;

    if (ini_parse("/sdcard/clockworkmod/settings.ini", settings_handler, &config) < 0) {
        ui_print("Can't load COT settings!\n");
        return 1;
    }
    ui_print("COT Settings loaded!\n");
    orsreboot = config.orsreboot;
    orswipeprompt = config.orswipeprompt;
    backupprompt = config.backupprompt;
    handle_theme(config.theme);
}

void handle_theme(char * theme_name) {
    char full_theme_file[1000];
    char * theme_base;
    char * theme_end;

    theme_base = "/res/theme/theme_";
    theme_end = ".ini";
    strcpy(full_theme_file, theme_base);
    strcat(full_theme_file, theme_name);
    strcat(full_theme_file, theme_end);
    theme themeconfig;

    ui_print("%s\n", full_theme_file);
    
    if (ini_parse(full_theme_file, theme_handler, &themeconfig) < 0) {
        ui_print("Can't load theme!\n");
        return 1;
    }
    ui_print("Theme loaded!\n");

    UICOLOR0 = themeconfig.uicolor0;
    UICOLOR1 = themeconfig.uicolor1;
    UICOLOR2 = themeconfig.uicolor2;
    bg_icon = themeconfig.bgicon;

    ui_dyn_background();
    ui_reset_icons();

    /*    
    if (strcmp(theme_name, "hydro") == 0) {
        set_ui_color(0);
        ui_reset_icons();
    } else if (strcmp(theme_name, "bloodred") == 0) {
        set_ui_color(1);
        ui_reset_icons();
    } else if (strcmp(theme_name, "keylimepie") == 0) {
        set_ui_color(2);
        ui_reset_icons();
    } else if (strcmp(theme_name, "citrusorange") == 0) {
        set_ui_color(3);
        ui_reset_icons();
    } else if (strcmp(theme_name, "dooderbuttblue") == 0) {
        set_ui_color(4);
        ui_reset_icons();
    }
    */
}
