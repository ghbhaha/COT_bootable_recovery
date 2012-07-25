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
#include "settingshandler_lang.h"
#include "power.h"

void show_power_options_menu() {
	static char* headers[] = { "Power Options",
                                "",
                                NULL
    };

	#define POWER_OPTIONS_ITEM_REBOOT	0
#if TARGET_BOOTLOADER_BOARD_NAME == otter
	#define POWER_OPTIONS_ITEM_FASTBOOT 1
	#define POWER_OPTIONS_ITEM_POWEROFF	2
#else
	#define POWER_OPTIONS_ITEM_POWEROFF	1
#endif

#if TARGET_BOOTLOADER_BOARD_NAME == otter
	static char* list[4];
#else
	static char* list[3];
#endif
	list[0] = "Reboot Recovery";
#if TARGET_BOOTLOADER_BOARD_NAME == otter
	list[1] = "Reboot Fastboot";
#else
	list[1] = "Power Off";
#endif
#if TARGET_BOOTLOADER_BOARD_NAME == otter
	list[2] = "Power Off";
#else
	list[2] = NULL;
#endif
#if TARGET_BOOTLOADER_BOARD_NAME == otter
	list[3] = NULL;
#endif
	for (;;) {
		int chosen_item = get_menu_selection(headers, list, 0, 0);
		switch (chosen_item) {
			case GO_BACK:
				return;
			case POWER_OPTIONS_ITEM_REBOOT:
#if TARGET_BOOTLOADER_BOARD_NAME == otter
				__system("/sbin/reboot_recovery");
#else
				reboot_wrapper("recovery");
#endif
				break;
#if TARGET_BOOTLOADER_BOARD_NAME == otter
			case POWER_OPTIONS_ITEM_FASTBOOT:
				__system("/sbin/reboot_fastboot");
				break;
#endif
			case POWER_OPTIONS_ITEM_POWEROFF:
#if TARGET_BOOTLOADER_BOARD_NAME == otter
				__system("/sbin/reboot_system");

#else
				__system("reboot -p");
#endif
				break;
		}
	}
}
