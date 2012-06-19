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

int UICOLOR0 = 0;
int UICOLOR1 = 0;
int UICOLOR2 = 0;
int bg_icon = 0;
const char *UI_CONFIG_FILE = "/sdcard/clockworkmod/.conf";

UI_COLOR_DEBUG = 0;

/* Set the UI color to default (hydro), give it it's own function to
 * avoid repeats and having to reboot on setting to default. */
void set_ui_default() {
	UICOLOR0 = 0;
	UICOLOR1 = 191;
	UICOLOR2 = 255;
	bg_icon = HYDRO_UI;
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "ECDEF_UICOLOR0: ", UICOLOR0);
		LOGI("%s %i\n", "ECDEF_UICOLOR1: ", UICOLOR1);
		LOGI("%s %i\n", "ECDEF_UICOLOR2: ", UICOLOR2);
		LOGI("%s %i\n", "ECDEF_bg_icon: ", bg_icon);
	}
}

/* Get the custom UI configuration settings; as you may be able to tell
 * this doesn't fully function yet. */
void get_config_settings() {
	FILE *in_file;
	int i, j, k, l;

	ensure_path_mounted("/sdcard");
	ensure_directory("/sdcard/clockworkmod");

	if(in_file = fopen(UI_CONFIG_FILE, "r")) {
		fscanf(in_file, "%d%d%d%d", &i, &j, &k, &l);
		UICOLOR0 = i;
		UICOLOR1 = j;
		UICOLOR2 = k;
		bg_icon = l;
		if(UI_COLOR_DEBUG) {
			LOGI("%s %i\n", "EC_UICOLOR0: ", UICOLOR0);
			LOGI("%s %i\n", "EC_UICOLOR1: ", UICOLOR1);
			LOGI("%s %i\n", "EC_UICOLOR2: ", UICOLOR2);
			LOGI("%s %i\n", "EC_bg_icon: ", bg_icon);
		}
		fclose(in_file);
	} else {
		set_ui_default();
	}
}	

/* There's probably a better way to do this but for now here it is,
 * write a config file to store the ui color to be set from the
 * advanced menu */
void set_config_file_contents(int i, int j, int k, int l) {
	FILE *out_file;

	ensure_path_mounted("/sdcard");
	// Open the config file to confirm it's presence (to remove it).
	if(out_file = fopen(UI_CONFIG_FILE, "r")) {
		fclose(out_file);
		remove(UI_CONFIG_FILE);
	}
	/* Regardless of if it existed prior it will be gone now.
	 * Create the .conf file and reopen it with write privs */
	__system("touch /sdcard/clockworkmod/.conf");
	out_file = fopen(UI_CONFIG_FILE, "w");

	// Write our integers to 1 line separated by a space
	fprintf(out_file, "%d ", i);
	fprintf(out_file, "%d ", j);
	fprintf(out_file, "%d ", k);
	fprintf(out_file, "%d", l);

	fclose(out_file);
	ensure_path_unmounted("/sdcard");
}

void set_bg_icon(int icon) {
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "CO_ICON:", icon);
	}
	bg_icon = icon;
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "CO_BG:", bg_icon);
	}
	return;
}

/* This should really be done with a struct instead of a switch...
 * 
 * n, m and o are to be tracked as our html color codes as UICOLOR0,
 * UICOLOR1 and UICOLOR2 respectively. P is always equal to the case
 * but should be defined by the UI color in use these colors are
 * defined in common.h */
void set_ui_color(int i) {
	int n, m, o, p;
	switch(i) {
		case HYDRO_UI: {
			/* Since Hydro is the default and the only thing handled
			 * by our config file is color, simply remove it when
			 * selecting default */
			ensure_path_mounted("/sdcard");
			ui_print("Setting UI Color to Default.\n");
			remove(UI_CONFIG_FILE);
			ensure_path_unmounted("/sdcard");
			// Set the default colors to avoid the need for a reboot
			set_ui_default();
			ui_dyn_background();
			/* Return from the function entirely instead of just
			 * breaking from the loop; subsequently cancelling the
			 * later call of set_config_file_contents */
			return;
		}
		case BLOOD_RED_UI: {
			ui_print("Setting UI Color to Blood Red.\n");
			n = 255;
			m = 0;
			o = 0;
			p = BLOOD_RED_UI;
			bg_icon = BLOOD_RED_UI;
			ui_dyn_background();
			break;
		}
		case KEY_LIME_PIE_UI: {
			ui_print("Setting UI Color to Key Lime Pie.\n");
			n = 0;
			m = 255;
			o = 0;
			p = KEY_LIME_PIE_UI;
			bg_icon = KEY_LIME_PIE_UI;
			ui_dyn_background();
			break;
		}
		case CITRUS_ORANGE_UI: {
			ui_print("Setting UI Color to Citrus Orange.\n");
			n = 238;
			m = 148;
			o = 74;
			p = CITRUS_ORANGE_UI;
			bg_icon = CITRUS_ORANGE_UI;
			ui_dyn_background();
			break;
		}
		case DOODERBUTT_BLUE_UI: {
			ui_print("Setting UI Color to Dooderbutt Blue.\n");
			n = 0;
			m = 0;
			o = 255;
			p = DOODERBUTT_BLUE_UI;
			bg_icon = DOODERBUTT_BLUE_UI;
			ui_dyn_background();
			break;
		}
	}
	set_config_file_contents(n,m,o,p);
}

void ui_dyn_background()
{
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "DYN_BG:", bg_icon);
	}
	switch(bg_icon) {
		case BLOOD_RED_UI:
			ui_set_background(BACKGROUND_ICON_BLOODRED);
			break;
		case KEY_LIME_PIE_UI:
			ui_set_background(BACKGROUND_ICON_KEYLIMEPIE);
			break;
		case CITRUS_ORANGE_UI:
			ui_set_background(BACKGROUND_ICON_CITRUSORANGE);
			break;
		case DOODERBUTT_BLUE_UI:
			ui_set_background(BACKGROUND_ICON_DOODERBUTT);
			break;
    	// Anything else is the clockwork icon
		default:
			ui_set_background(BACKGROUND_ICON_CLOCKWORK);
			break;
	}
}
