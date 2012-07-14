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

#include "settings.h"
#include "settingshandler.h"

int UICOLOR0 = 0;
int UICOLOR1 = 0;
int UICOLOR2 = 0;
int UITHEME = 0;

int UI_COLOR_DEBUG = 0;

void show_settings_menu() {
    static char* headers[] = { "COT Settings",
                                "",
                                NULL
    };

    #define SETTINGS_ITEM_THEME         0
    #define SETTINGS_ITEM_ORS_REBOOT    1
    #define SETTINGS_ITEM_ORS_WIPE      2
    #define SETTINGS_ITEM_NAND_PROMPT   3
    #define SETTINGS_ITEM_LANGUAGE      4

    static char* list[] = { "Theme",
                            "ORS Forced Reboots",
                            "ORS Wipe Prompt",
                            "ZIP flash Nandroid Prompt",
                            "Language",
                            NULL
    };

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        switch (chosen_item) {
            case GO_BACK:
                return;
            case SETTINGS_ITEM_THEME:
            {
                static char* ui_colors[] = {"Hydro (default)",
                                                    "Blood Red",
                                                    "Key Lime Pie",
                                                    "Citrus Orange",
                                                    "Dooderbutt Blue",
                                                    NULL
                };
                static char* ui_header[] = {"COT Theme", "", NULL};

                int ui_color = get_menu_selection(ui_header, ui_colors, 0, 0);
                if(ui_color == GO_BACK)
                    continue;
                else {
                    if(ui_color == 0) {
                        currenttheme = "hydro";
                        update_cot_settings();
                    } else if (ui_color == 1) {
                        currenttheme = "bloodred";
                        update_cot_settings();
                    } else if (ui_color == 2) {
                        currenttheme = "keylimepie";
                        update_cot_settings();
                    } else if (ui_color == 3) {
                        currenttheme = "citrusorange";
                        update_cot_settings();
                    } else if (ui_color == 4) {
                        currenttheme = "dooderbuttblue";
                        update_cot_settings();
                    }
                    
                    break;
                }
            }
            case SETTINGS_ITEM_ORS_REBOOT:
            {
                static char* ors_list[] = {"On",
                                            "Off",
                                            NULL
                };
                static char* ors_headers[] = {"ORS Forced Reboots", "", NULL};

                int result = get_menu_selection(ors_headers, ors_list, 0, 0);
                if(result == GO_BACK) {
                    continue;
                } else if (result == 0) {
                    orsreboot = 1;
                    update_cot_settings();
                } else if (result == 1) {
                    orsreboot = 0;
                    update_cot_settings();
                }

                break;
            }
            case SETTINGS_ITEM_ORS_WIPE:
            {
                static char* ors_list[] = {"On",
                                            "Off",
                                            NULL
                };
                static char* ors_headers[] = {"ORS Wipe Prompt", "", NULL};

                int result = get_menu_selection(ors_headers, ors_list, 0, 0);
                if(result == GO_BACK) {
                    continue;
                } else if (result == 0) {
                    orswipeprompt = 1;
                    update_cot_settings();
                } else if (result == 1) {
                    orswipeprompt = 0;
                    update_cot_settings();
                }

                break;
            }
            case SETTINGS_ITEM_NAND_PROMPT:
            {
                static char* ors_list[] = {"On",
                                            "Off",
                                            NULL
                };
                static char* ors_headers[] = {"ZIP flash Nandroid Prompt", "", NULL};

                int result = get_menu_selection(ors_headers, ors_list, 0, 0);
                if(result == GO_BACK) {
                    continue;
                } else if (result == 0) {
                    backupprompt = 1;
                    update_cot_settings();
                } else if (result == 1) {
                    backupprompt = 0;
                    update_cot_settings();
                }

                break;
            }
            case SETTINGS_ITEM_LANGUAGE:
            {
                static char* lang_list[] = {"English",
                                            NULL
                };
                static char* lang_headers[] = {"Language", "", NULL};

                int result = get_menu_selection(lang_headers, lang_list, 0, 0);
                if(result == GO_BACK) {
                    continue;
                } else if (result == 0) {
                    language = "en";
                    update_cot_settings();
                }

                break;
            }
            default:
                return;
        }
    }
}

void set_ui_default() {
	UICOLOR0 = 0;
	UICOLOR1 = 191;
	UICOLOR2 = 255;
	UITHEME = HYDRO_UI;
	sprintf(currenttheme, "hydro");
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "ECDEF_UICOLOR0: ", UICOLOR0);
		LOGI("%s %i\n", "ECDEF_UICOLOR1: ", UICOLOR1);
		LOGI("%s %i\n", "ECDEF_UICOLOR2: ", UICOLOR2);
		LOGI("%s %i\n", "ECDEF_UITHEME: ", UITHEME);
	}
}

// This should really be done with a struct instead of a switch...
void set_ui_color(int i) {
	switch(i) {
		case HYDRO_UI: {
			ui_print("Setting UI Color to Default.\n");
			set_ui_default();
			ui_dyn_background();
			return;
		}
		case BLOOD_RED_UI: {
			ui_print("Setting UI Color to Blood Red.\n");
			UICOLOR0 = 255;
			UICOLOR1 = 0;
			UICOLOR2 = 0;
			UITHEME = BLOOD_RED_UI;
			sprintf(currenttheme, "bloodred");
			ui_dyn_background();
			break;
		}
		case KEY_LIME_PIE_UI: {
			ui_print("Setting UI Color to Key Lime Pie.\n");
			UICOLOR0 = 0;
			UICOLOR1 = 255;
			UICOLOR2 = 0;
			UITHEME = KEY_LIME_PIE_UI;
			sprintf(currenttheme, "keylimepie");
			ui_dyn_background();
			break;
		}
		case CITRUS_ORANGE_UI: {
			ui_print("Setting UI Color to Citrus Orange.\n");
			UICOLOR0 = 238;
			UICOLOR1 = 148;
			UICOLOR2 = 74;
			UITHEME = CITRUS_ORANGE_UI;
			sprintf(currenttheme, "citrusorange");
			ui_dyn_background();
			break;
		}
		case DOODERBUTT_BLUE_UI: {
			ui_print("Setting UI Color to Dooderbutt Blue.\n");
			UICOLOR0 = 0;
			UICOLOR1 = 0;
			UICOLOR2 = 255;
			UITHEME = DOODERBUTT_BLUE_UI;
			sprintf(currenttheme, "dooderbuttblue");
			ui_dyn_background();
			break;
		}
	}
}

void ui_dyn_background()
{
	if(UI_COLOR_DEBUG) {
		LOGI("%s %i\n", "DYN_BG:", UITHEME);
	}
	switch(UITHEME) {
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
