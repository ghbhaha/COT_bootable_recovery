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
#include "make_ext4fs.h"
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
#include "cutils/android_reboot.h"

#include "settings.h"
#include "settingshandler.h"
#include "settingshandler_lang.h"

#define ABS_MT_POSITION_X 0x35  /* Center X ellipse position */

int UICOLOR0 = 0;
int UICOLOR1 = 0;
int UICOLOR2 = 0;
int UITHEME = 0;

int UI_COLOR_DEBUG = 0;

void show_cot_options_menu() {
    static char* headers[] = { "COT Options",
                                "",
                                NULL
    };

	#define COT_OPTIONS_ITEM_RECDEBUG	0
	#define COT_OPTIONS_ITEM_SETTINGS	1
	#define COT_OPTIONS_ITEM_QUICKFIXES	2

#ifdef BOARD_HAS_QUICKFIXES
	static char* list[4];
	list[0] = "Recovery Debugging";
	list[1] = "COT Settings";
	list[2] = "Quick Fixes";
	list[3] = NULL;
#else
	static char* list[3];
	list[0] = "Recovery Debugging";
	list[1] = "COT Settings";
	list[2] = NULL;
#endif
	for (;;) {
		int chosen_item = get_menu_selection(headers, list, 0, 0);
		switch (chosen_item) {
			case GO_BACK:
				return;
			case COT_OPTIONS_ITEM_RECDEBUG:
				show_recovery_debugging_menu();
				break;
			case COT_OPTIONS_ITEM_SETTINGS:
				show_settings_menu();
				break;
			case COT_OPTIONS_ITEM_QUICKFIXES:
			{
				static char* fixes_headers[3];
				fixes_headers[0] = "Quick Fixes";
				fixes_headers[1] = "\n";
				fixes_headers[2] = NULL;
#ifdef BOARD_NEEDS_RECOVERY_FIX
				static char* fixes_list[2];
				fixes_list[0] = "Fix Recovery Boot Loop";
				fixes_list[1] = NULL;
#else
				static char* fixes_list[1];
				fixes_list[0] = NULL;
#endif
				int chosen_fix = get_menu_selection(fixes_headers, fixes_list, 0, 0);
				switch (chosen_fix) {
					case GO_BACK:
						continue;
					case 0:
#ifdef BOARD_NEEDS_RECOVERY_FIX
						format_root_device("MISC:");
						format_root_device("PERSIST:");
						reboot(RB_AUTOBOOT);
						break;
#else
						break;
#endif
				}
			}
		}
	}
}

void show_recovery_debugging_menu()
{
	static char* headers[] = { "Recovery Debugging",
								"",
								NULL
	};

	static char* list[] = { "Fix Permissions",
							"Report Error",
							"Key Test",
							"Show log",
							"Toggle UI Debugging",
							NULL
	};

	for (;;)
	{
		int chosen_item = get_menu_selection(headers, list, 0, 0);
		if(chosen_item == GO_BACK)
			break;
		switch(chosen_item)
		{
			case 0:
			{
				ensure_path_mounted("/system");
				ensure_path_mounted("/data");
				ui_print("Fixing permissions...\n");
				__system("fix_permissions");
				ui_print("Done!\n");
				break;
			}
			case 1:
				handle_failure(1);
				break;
			case 2:
			{
				ui_print("Outputting key codes.\n");
				ui_print("Go back to end debugging.\n");
				struct keyStruct{
					int code;
					int x;
					int y;
				}*key;
				int action;
				do
				{
					key = ui_wait_key();
					if(key->code == ABS_MT_POSITION_X)
					{
						action = device_handle_mouse(key, 1);
						ui_print("Touch: X: %d\tY: %d\n", key->x, key->y);
					}
					else
					{
						action = device_handle_key(key->code, 1);
						ui_print("Key: %x\n", key->code);
					}
				}
				while (action != GO_BACK);
				break;
			}
			case 3:
				ui_printlogtail(12);
				break;
			case 4:
				toggle_ui_debugging();
				break;
		}
	}
}

void show_settings_menu() {
    static char* headers[] = { "COT Settings",
                                "",
                                NULL
    };
    
    #define SETTINGS_ITEM_LANGUAGE      0
    #define SETTINGS_ITEM_THEME         1
    #define SETTINGS_ITEM_ORS_REBOOT    2
    #define SETTINGS_ITEM_ORS_WIPE      3
    #define SETTINGS_ITEM_NAND_PROMPT   4
    #define SETTINGS_ITEM_SIGCHECK      5
    #define SETTINGS_ITEM_DEV_OPTIONS   6

    static char* list[7];
	
    list[0] = "Language";
    list[1] = "Theme";
    if (orsreboot == 1) {
		list[2] = "Disable forced reboots";
	} else {
		list[2] = "Enable forced reboots";
	}
	if (orswipeprompt == 1) {
		list[3] = "Disable wipe prompt";
	} else {
		list[3] = "Enable wipe prompt";
	}
	if (backupprompt == 1) {
		list[4] = "Disable zip flash nandroid prompt";
	} else {
		list[4] = "Enable zip flash nandroid prompt";
	}
    if (signature_check_enabled == 1) {
		list[5] = "Disable md5 signature check";
	} else {
		list[5] = "Enable md5 signature check";
	}
    list[6] = NULL;

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
                    switch(ui_color) {
                        case 0:
                            currenttheme = "hydro";
                            break;
                        case 1:
                            currenttheme = "bloodred";
                            break;
                        case 2:
                            currenttheme = "keylimepie";
                            break;
                        case 3:
                            currenttheme = "citrusorange";
                            break;
                        case 4:
                            currenttheme = "dooderbuttblue";
                            break;
					}
					break;
				}
            }
            case SETTINGS_ITEM_ORS_REBOOT:
			{
                if (orsreboot == 1) {
					ui_print("Disabling forced reboots.\n");
					list[1] = "Enable forced reboots";
					orsreboot = 0;
				} else {
					ui_print("Enabling forced reboots.\n");
					list[1] = "Disable forced reboots";
					orsreboot = 1;
				}
                break;
            }
            case SETTINGS_ITEM_ORS_WIPE:
			{
                if (orswipeprompt == 1) {
					ui_print("Disabling wipe prompt.\n");
					list[2] = "Enable wipe prompt";
					orswipeprompt = 0;
				} else {
					ui_print("Enabling wipe prompt.\n");
					list[2] = "Disable wipe prompt";
					orswipeprompt = 1;
				}
                break;
            }
            case SETTINGS_ITEM_NAND_PROMPT:
			{
                if (backupprompt == 1) {
					ui_print("Disabling zip flash nandroid prompt.\n");
					list[3] = "Enable zip flash nandroid prompt";
					backupprompt = 0;
				} else {
					ui_print("Enabling zip flash nandroid prompt.\n");
					list[3] = "Disable zip flash nandroid prompt";
					backupprompt = 1;
				}
                break;
            }
            case SETTINGS_ITEM_SIGCHECK:
            {
				if (signature_check_enabled == 1) {
					ui_print("Disabling md5 signature check.\n");
					list[4] = "Enable md5 signature check";
					signature_check_enabled = 0;
				} else {
					ui_print("Enabling md5 signature check.\n");
					list[4] = "Disable md5 signature check";
					signature_check_enabled = 1;
				}
				break;
			}
            case SETTINGS_ITEM_LANGUAGE:
            {
                static char* lang_list[] = {"English",
#if DEV_BUILD == 1
                                            "Use Custom Language",
#endif
                                            NULL
                };
                static char* lang_headers[] = {"Language", "", NULL};

                int result = get_menu_selection(lang_headers, lang_list, 0, 0);
                if(result == GO_BACK) {
                    continue;
                } else if (result == 0) {
                    language = "en";
                } else if (result == 1) {
                    language = "custom";
                }

                break;
            }
            default:
                return;
        }
        update_cot_settings();
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
