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
#include "power.h"

void show_power_options_menu() {
	static char* headers[] = { "Power Options",
                                "",
                                NULL
    };

	#define POWER_OPTIONS_ITEM_REBOOT	0
	#define POWER_OPTIONS_ITEM_POWEROFF	1

	static char* list[3];
	list[0] = "Reboot Recovery";
	list[1] = "Power Off";
	list[2] = NULL;
	for (;;) {
		int chosen_item = get_menu_selection(headers, list, 0, 0);
		switch (chosen_item) {
			case GO_BACK:
				return;
			case POWER_OPTIONS_ITEM_REBOOT:
				android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
				break;
			case POWER_OPTIONS_ITEM_POWEROFF:
				__system("reboot -p");
				break;
		}
	}
}
