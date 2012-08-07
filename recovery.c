/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "bootloader.h"
#include "commands.h"
#include "common.h"
#include "cutils/properties.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"
#include "encryptedfs_provisioning.h"
#include "settings.h"
#include "settingshandler.h"
#include "settingshandler_lang.h"
#include "power.h"
#include "utilities.h"

#include "extendedcommands.h"
#include "flashutils/flashutils.h"
#include "eraseandformat.h"

#define ABS_MT_POSITION_X 0x35  /* Center X ellipse position */
#define ABS_MT_POSITION_Y 0x36  /* Center Y ellipse position */

static const struct option OPTIONS[] = {
  { "send_intent", required_argument, NULL, 's' },
  { "update_package", required_argument, NULL, 'u' },
  { "wipe_data", no_argument, NULL, 'w' },
  { "wipe_cache", no_argument, NULL, 'c' },
  { "set_encrypted_filesystems", required_argument, NULL, 'e' },
  { "show_text", no_argument, NULL, 't' },
  { NULL, 0, NULL, 0 },
};

static const char *COMMAND_FILE = "CACHE:recovery/command";
static const char *INTENT_FILE = "CACHE:recovery/intent";
static const char *LOG_FILE = "CACHE:recovery/log";
static const char *LAST_LOG_FILE = "CACHE:recovery/last_log";
static const char *SDCARD_ROOT = "/sdcard";
static int allow_display_toggle = 1;
static int poweroff = 0;
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";
static const char *SIDELOAD_TEMP_DIR = "/tmp/sideload";

const char *DEFAULT_BACKUP_PATH = "/sdcard/cotrecovery/backup/";
const char *USER_DEFINED_BACKUP_MARKER = "/sdcard/cotrecovery/.userdefinedbackups";

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 *
 * SECURE FILE SYSTEMS ENABLE/DISABLE
 * 1. user selects "enable encrypted file systems"
 * 2. main system writes "--set_encrypted_filesystems=on|off" to
 *    /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and
 *    "--set_encrypted_filesystems=on|off"
 *    -- after this, rebooting will restart the transition --
 * 5. read_encrypted_fs_info() retrieves encrypted file systems settings from /data
 *    Settings include: property to specify the Encrypted FS istatus and
 *    FS encryption key if enabled (not yet implemented)
 * 6. erase_volume() reformats /data
 * 7. erase_volume() reformats /cache
 * 8. restore_encrypted_fs_info() writes required encrypted file systems settings to /data
 *    Settings include: property to specify the Encrypted FS status and
 *    FS encryption key if enabled (not yet implemented)
 * 9. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 10. main() calls reboot() to boot main system
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

// Ensure a directory exists
void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", dir);
    __system(tmp);
}

// open a file given in root:path format, mounting partitions as necessary
static FILE*
fopen_root_path(const char *root_path, const char *mode) {
    if (ensure_root_path_mounted(root_path) != 0) {
        LOGE("Can't mount %s\n", root_path);
        return NULL;
    }

    char path[PATH_MAX] = "";
    if (translate_root_path(root_path, path, sizeof(path)) == NULL) {
        LOGE("Bad path %s\n", root_path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1);

    FILE *fp = fopen(path, mode);
    if (fp == NULL && root_path != COMMAND_FILE) LOGE("Can't open %s\n", path);
    return fp;
}

// open a given path, mounting partitions as necessary
FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1);

    FILE *fp = fopen(path, mode);
    if (fp == NULL && path != COMMAND_FILE) LOGE("Can't open %s\n", path);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    if (device_flash_type() == MTD) {
        get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure
    }

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", sizeof(boot.status), boot.status);
    }

    struct stat file_info;

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1 && 0 != stat("/tmp/.ignorebootmessage", &file_info)) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_root_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                (*argv)[*argc] = strdup(strtok(buf, "\r\n"));  // Strip newline.
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    if (device_flash_type() == MTD) {
        set_bootloader_message(&boot);
    }
}

void
set_sdcard_update_bootloader_message() {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
long tmplog_offset = 0;

static void
copy_log_file(const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if (log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(TEMPORARY_LOG_FILE, "r");
        if (tmplog == NULL) {
            LOGE("Can't open %s\n", TEMPORARY_LOG_FILE);
        } else {
            if (append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if (append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, TEMPORARY_LOG_FILE);
        }
        check_and_fclose(log, destination);
    }
}


// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
static void
finish_recovery(const char *send_intent) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_root_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    // Copy logs to cache so the system can find out what happened.
    FILE *log = fopen_root_path(LOG_FILE, "a");
    if (log == NULL) {
        LOGE("Can't open %s\n", LOG_FILE);
    } else {
        FILE *tmplog = fopen(TEMPORARY_LOG_FILE, "r");
        if (tmplog == NULL) {
            LOGE("Can't open %s\n", TEMPORARY_LOG_FILE);
        } else {
            static long tmplog_offset = 0;
            fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            tmplog_offset = ftell(tmplog);
            check_and_fclose(tmplog, TEMPORARY_LOG_FILE);
        }
        check_and_fclose(log, LOG_FILE);
    }

    if (device_flash_type() == MTD) {
        // Reset to mormal system boot so recovery won't cycle indefinitely.
        struct bootloader_message boot;
        memset(&boot, 0, sizeof(boot));
        set_bootloader_message(&boot);
    }

    // Remove the command file, so recovery won't repeat indefinitely.
    char path[PATH_MAX] = "";
    if (ensure_root_path_mounted(COMMAND_FILE) != 0 ||
        translate_root_path(COMMAND_FILE, path, sizeof(path)) == NULL ||
        (unlink(path) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    sync();  // For good measure.
}

static char*
copy_sideloaded_package(const char* original_path) {
  if (ensure_path_mounted(original_path) != 0) {
    LOGE("Can't mount %s\n", original_path);
    return NULL;
  }

  if (ensure_path_mounted(SIDELOAD_TEMP_DIR) != 0) {
    LOGE("Can't mount %s\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }

  if (mkdir(SIDELOAD_TEMP_DIR, 0700) != 0) {
    if (errno != EEXIST) {
      LOGE("Can't mkdir %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
      return NULL;
    }
  }

  // verify that SIDELOAD_TEMP_DIR is exactly what we expect: a
  // directory, owned by root, readable and writable only by root.
  struct stat st;
  if (stat(SIDELOAD_TEMP_DIR, &st) != 0) {
    LOGE("failed to stat %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
    return NULL;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOGE("%s isn't a directory\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }
  if ((st.st_mode & 0777) != 0700) {
    LOGE("%s has perms %o\n", SIDELOAD_TEMP_DIR, st.st_mode);
    return NULL;
  }
  if (st.st_uid != 0) {
    LOGE("%s owned by %lu; not root\n", SIDELOAD_TEMP_DIR, st.st_uid);
    return NULL;
  }

  char copy_path[PATH_MAX];
  strcpy(copy_path, SIDELOAD_TEMP_DIR);
  strcat(copy_path, "/package.zip");

  char* buffer = malloc(BUFSIZ);
  if (buffer == NULL) {
    LOGE("Failed to allocate buffer\n");
    return NULL;
  }

  size_t read;
  FILE* fin = fopen(original_path, "rb");
  if (fin == NULL) {
    LOGE("Failed to open %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }
  FILE* fout = fopen(copy_path, "wb");
  if (fout == NULL) {
    LOGE("Failed to open %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  while ((read = fread(buffer, 1, BUFSIZ, fin)) > 0) {
    if (fwrite(buffer, 1, read, fout) != read) {
      LOGE("Short write of %s (%s)\n", copy_path, strerror(errno));
      return NULL;
    }
  }

  free(buffer);

  if (fclose(fout) != 0) {
    LOGE("Failed to close %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  if (fclose(fin) != 0) {
    LOGE("Failed to close %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }

  // "adb push" is happy to overwrite read-only files when it's
  // running as root, but we'll try anyway.
  if (chmod(copy_path, 0400) != 0) {
    LOGE("Failed to chmod %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  return strdup(copy_path);
}

int get_battery_level(void)
{
	static int lastVal = -1;
	static time_t nextSecCheck = 0;

	struct timeval curTime;
	gettimeofday(&curTime, NULL);
	if (curTime.tv_sec > nextSecCheck)
	{
		char cap_s[4];
		FILE * cap = fopen("/sys/class/power_supply/battery/capacity","rt");
		if (cap)
		{
			fgets(cap_s, 4, cap);
			fclose(cap);
			lastVal = atoi(cap_s);
			if (lastVal > 100)  lastVal = 100;
			if (lastVal < 0)    lastVal = 0;
		}
		nextSecCheck = curTime.tv_sec + 60;
	}
	return lastVal;
}

char* print_batt_cap() {
	char* full_cap_s = (char*)malloc(30);
	char full_cap_a[30];

	int cap_i = get_battery_level();

	// Get a usable time
	struct tm *current;
	time_t now;
	now = time(0);
	current = localtime(&now);

	sprintf(full_cap_a, "Battery Level: %i%% @ %02D:%02D", cap_i, current->tm_hour, current->tm_min);
	strcpy(full_cap_s, full_cap_a);

	return full_cap_s;
}

char** prepend_title(char** headers) {
    char* title[] = { EXPAND(RECOVERY_VERSION),
                      "",
		      print_batt_cap(),
		      "",
                      NULL };

    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 0;
    char** p;
    for (p = title; *p; ++p, ++count);
    for (p = headers; *p; ++p, ++count);

    char** new_headers = malloc((count+1) * sizeof(char*));
    char** h = new_headers;
    for (p = title; *p; ++p, ++h) *h = *p;
    for (p = headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int
get_menu_selection(char** headers, char** items, int menu_only,
                   int initial_selection) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui_clear_key_queue();

    int item_count = ui_start_menu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1;

    // Some users with dead enter keys need a way to turn on power to select.
    // Jiggering across the wrapping menu is one "secret" way to enable it.
    // We can't rely on /cache or /sdcard since they may not be available.
    int wrap_count = 0;

    while (chosen_item < 0 && chosen_item != GO_BACK) {
		struct keyStruct *key;
		key = ui_wait_key();

        int visible = ui_text_visible();

		int action;
		if(key->code == ABS_MT_POSITION_X)
	        action = device_handle_mouse(key, visible);
		else
	        action = device_handle_key(key->code, visible);

        int old_selected = selected;

        if (action < 0) {
            switch (action) {
                case HIGHLIGHT_UP:
                    --selected;
                    selected = ui_menu_select(selected);
                    break;
                case HIGHLIGHT_DOWN:
                    ++selected;
                    selected = ui_menu_select(selected);
                    break;
                case SELECT_ITEM:
                    chosen_item = selected;
                    if (ui_get_showing_back_button()) {
                        if (chosen_item == item_count) {
                            chosen_item = GO_BACK;
                        }
                    }
                    break;
                case NO_ACTION:
                    break;
                case GO_BACK:
                    chosen_item = GO_BACK;
                    break;
            }
        } else if (!menu_only) {
            chosen_item = action;
        }

        if (abs(selected - old_selected) > 1) {
            wrap_count++;
            if (wrap_count == 300) {
                wrap_count = 0;
                if (ui_get_showing_back_button()) {
                    ui_print("Back menu button disabled.\n");
                    ui_set_showing_back_button(0);
                }
                else {
                    ui_print("Back menu button enabled.\n");
                    ui_set_showing_back_button(1);
                }
            }
        }
    }

    ui_end_menu();
    ui_clear_key_queue();
    return chosen_item;
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static int
sdcard_directory(const char* path) {
    ensure_path_mounted(SDCARD_ROOT);

    const char* MENU_HEADERS[] = { "Choose a package to install:",
                                   path,
                                   "",
                                   NULL };
    DIR* d;
    struct dirent* de;
    d = opendir(path);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", path, strerror(errno));
        ensure_path_unmounted(SDCARD_ROOT);
        return 0;
    }

    char** headers = prepend_title(MENU_HEADERS);

    int d_size = 0;
    int d_alloc = 10;
    char** dirs = malloc(d_alloc * sizeof(char*));
    int z_size = 1;
    int z_alloc = 10;
    char** zips = malloc(z_alloc * sizeof(char*));
    zips[0] = strdup("../");

    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
            // skip "." and ".." entries
            if (name_len == 1 && de->d_name[0] == '.') continue;
            if (name_len == 2 && de->d_name[0] == '.' &&
                de->d_name[1] == '.') continue;

            if (d_size >= d_alloc) {
                d_alloc *= 2;
                dirs = realloc(dirs, d_alloc * sizeof(char*));
            }
            dirs[d_size] = malloc(name_len + 2);
            strcpy(dirs[d_size], de->d_name);
            dirs[d_size][name_len] = '/';
            dirs[d_size][name_len+1] = '\0';
            ++d_size;
        } else if (de->d_type == DT_REG &&
                   name_len >= 4 &&
                   strncasecmp(de->d_name + (name_len-4), ".zip", 4) == 0) {
            if (z_size >= z_alloc) {
                z_alloc *= 2;
                zips = realloc(zips, z_alloc * sizeof(char*));
            }
            zips[z_size++] = strdup(de->d_name);
        }
    }
    closedir(d);

    qsort(dirs, d_size, sizeof(char*), compare_string);
    qsort(zips, z_size, sizeof(char*), compare_string);

    // append dirs to the zips list
    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    int result;
    int chosen_item = 0;
    do {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item);

        char* item = zips[chosen_item];
        int item_len = strlen(item);
        if (chosen_item == 0) {          // item 0 is always "../"
            // go up but continue browsing (if the caller is sdcard_directory)
            result = -1;
            break;
        } else if (item[item_len-1] == '/') {
            // recurse down into a subdirectory
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);
            new_path[strlen(new_path)-1] = '\0';  // truncate the trailing '/'
            result = sdcard_directory(new_path);
            if (result >= 0) break;
        } else {
            // selected a zip file:  attempt to install it, and return
            // the status to the caller.
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);

            ui_print("\n-- Install %s ...\n", path);
            set_sdcard_update_bootloader_message();
            char* copy = copy_sideloaded_package(new_path);
            ensure_path_unmounted(SDCARD_ROOT);
            if (copy) {
                result = install_package(copy);
                free(copy);
            } else {
                result = INSTALL_ERROR;
            }
            break;
        }
    } while (true);

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);
    free(headers);

    ensure_path_unmounted(SDCARD_ROOT);
    return result;
}

#define TEST_AMEND 0
#if TEST_AMEND
static void
test_amend()
{
    extern int test_symtab(void);
    extern int test_cmd_fn(void);
    int ret;
    LOGD("Testing symtab...\n");
    ret = test_symtab();
    LOGD("  returned %d\n", ret);
    LOGD("Testing cmd_fn...\n");
    ret = test_cmd_fn();
    LOGD("  returned %d\n", ret);
}
#endif  // TEST_AMEND

static void
prompt_and_wait() {
    char** headers = prepend_title((const char**)MENU_HEADERS);

    for (;;) {
        finish_recovery(NULL);
        ui_reset_progress();

        allow_display_toggle = 1;
        int chosen_item = get_menu_selection(headers, MENU_ITEMS, 0, 0);
        allow_display_toggle = 0;

        // device-specific code may take some action here.  It may
        // return one of the core actions handled in the switch
        // statement below.
        chosen_item = device_perform_action(chosen_item);

        switch (chosen_item) {
            case ITEM_REBOOT:
                poweroff=0;
                return;

            case ITEM_WIPE_DATA:
                wipe_data(ui_text_visible());
                if (!ui_text_visible()) return;
                break;
			case ITEM_WIPE_ALL:
				wipe_all(0);
				break;
            case ITEM_INSTALL_ZIP:
                show_install_update_menu();
                break;
            case ITEM_NANDROID:
                show_nandroid_menu();
                break;
            case ITEM_PARTITION:
                show_partition_menu();
                break;
            case ITEM_COTOPTIONS:
                show_cot_options_menu();
                break;
			case ITEM_UTILITIES:
				show_utilities_menu();
				break;
            case ITEM_POWEROPTIONS:
                //poweroff=1;
				show_power_options_menu();
                break;
        }
    }
}

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

// open recovery script code
void delayed_reboot() {
	int i;
	for (i = 3; i > 0; i--) {
		ui_print("Rebooting system in (%d)\n", i);
		sleep(1);
	}
	reboot(RB_AUTOBOOT);
}

static const char *SCRIPT_FILE_CACHE = "/cache/recovery/openrecoveryscript";
static const char *SCRIPT_FILE_TMP = "/tmp/openrecoveryscript";
#define SCRIPT_COMMAND_SIZE 512

int check_for_script_file(void) {
	FILE *fp = fopen(SCRIPT_FILE_CACHE, "r");
	int ret_val = 0;
	char exec[512];

	if (fp != NULL) {
		ret_val = 1;
		LOGI("Script file found: '%s'\n", SCRIPT_FILE_CACHE);
		fclose(fp);
		// Copy script file to /tmp
		strcpy(exec, "cp ");
		strcat(exec, SCRIPT_FILE_CACHE);
		strcat(exec, " ");
		strcat(exec, SCRIPT_FILE_TMP);
		__system(exec);
		// Delete the file from /cache
		strcpy(exec, "rm ");
		strcat(exec, SCRIPT_FILE_CACHE);
		__system(exec);
	}
	return ret_val;
}

int run_script_file(void) {
	FILE *fp = fopen(SCRIPT_FILE_TMP, "r");
	struct stat st;
	int ret_val = 0, cindex, line_len, i, remove_nl;
	char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
		 value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
		 value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
	char *val_start, *tok;
	int ors_system = 0;
	int ors_data = 0;
	int ors_cache = 0;
	int ors_recovery = 0;
	int ors_boot = 0;
	int ors_andsec = 0;
	int ors_sdext = 0;
	int ors_no_confirm = 0;

	if (fp != NULL) {
		for (i = 20; i > 0; i--) {
			ui_print("Waiting for SD Card to mount (%ds)\n", i);
			if (ensure_path_mounted(SDCARD_ROOT) ==0) {
				ui_print("SD Card Mounted...\nContinuing...\n");
				break;
			}
			sleep(1);
		}
		if (orswipeprompt == 1) {
			ors_no_confirm = 1;
		}
		while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
			cindex = 0;
			line_len = strlen(script_line);
			//if (line_len > 2)
				//continue; // there's a blank line at the end of the file, we're done!
			//ui_print("script line: '%s'\n", script_line);
			for (i=0; i<line_len; i++) {
				if ((int)script_line[i] == 32) {
					cindex = i;
					i = line_len;
				}
			}
			memset(command, 0, sizeof(command));
			memset(value, 0, sizeof(value));
			if ((int)script_line[line_len - 1] == 10)
					remove_nl = 2;
				else
					remove_nl = 1;
			if (cindex != 0) {
				strncpy(command, script_line, cindex);
				LOGI("command is: '%s' and ", command);
				val_start = script_line;
				val_start += cindex + 1;
				strncpy(value, val_start, line_len - cindex - remove_nl);
				LOGI("value is: '%s'\n", value);
			} else {
				strncpy(command, script_line, line_len - remove_nl + 1);
				ui_print("command is: '%s' and there is no value\n", command);
			}
			if (strcmp(command, "install") == 0) {
				// Install zip
				ensure_path_mounted(SDCARD_ROOT);
				ui_print("Installing zip file '%s'\n", value);
				ret_val = install_zip(value);
				if (ret_val != INSTALL_SUCCESS) {
					LOGE("Error installing zip file '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "wipe") == 0) {
				// Wipe -- ToDo: Make this use the same wipe functionality as normal wipes
				if (strcmp(value, "cache") == 0 || strcmp(value, "/cache") == 0) {
					erase_cache(1);
				} else if (strcmp(value, "dalvik") == 0 || strcmp(value, "dalvick") == 0 || strcmp(value, "dalvikcache") == 0 || strcmp(value, "dalvickcache") == 0) {
					erase_dalvik_cache(1);
				} else if (strcmp(value, "data") == 0 || strcmp(value, "/data") == 0 || strcmp(value, "factory") == 0 || strcmp(value, "factoryreset") == 0) {
					if(ors_no_confirm || confirm_selection("Confirm wipe?", "Yes - Wipe Data")) {
						ui_print("-- Wiping Data Partition...\n");
						wipe_data(0);
						ui_print("-- Data Partition Wipe Complete!\n");
					} else {
						ui_print("Skipping data wipe...\n");
					}
				} else {
					LOGE("Error with wipe command value: '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "backup") == 0) {
				// Backup
				char backup_path[PATH_MAX];

				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					line_len = strlen(tok);
					if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13) {
						if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13)
							remove_nl = 2;
						else
							remove_nl = 1;
					} else
						remove_nl = 0;
					strncpy(value2, tok, line_len - remove_nl);
					ui_print("Backup folder set to '%s'\n", value2);
					nandroid_get_backup_path(backup_path);
					strcat(backup_path, value2);
				} else {
					nandroid_generate_timestamp_path(backup_path);
				}
				//ui_print("Backup options are ignored in CWMR: '%s'\n", value1);
				nandroid_backup(backup_path);
			} else if (strcmp(command, "restore") == 0) {
				// Restore
				tok = strtok(value, " ");
				strcpy(value1, tok);
				ui_print("Restoring '%s'\n", value1);
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					ors_system = 0;
					ors_data = 0;
					ors_cache = 0;
					ors_boot = 0;
					ors_sdext = 0;

					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					ui_print("Setting restore options:\n");
					line_len = strlen(value2);
					for (i=0; i<line_len; i++) {
						if (value2[i] == 'S' || value2[i] == 's') {
							ors_system = 1;
							ui_print("System\n");
						} else if (value2[i] == 'D' || value2[i] == 'd') {
							ors_data = 1;
							ui_print("Data\n");
						} else if (value2[i] == 'C' || value2[i] == 'c') {
							ors_cache = 1;
							ui_print("Cache\n");
						} else if (value2[i] == 'R' || value2[i] == 'r') {
							//ui_print("Option for recovery ignored in CWMR\n");
						} else if (value2[i] == '1') {
							//ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == '2') {
							//ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == '3') {
							//ui_print("%s\n", "Option for special1 ignored in CWMR");
						} else if (value2[i] == 'B' || value2[i] == 'b') {
							ors_boot = 1;
							ui_print("Boot\n");
						} else if (value2[i] == 'A' || value2[i] == 'a') {
							//ui_print("Option for android secure ignored in CWMR\n");
						} else if (value2[i] == 'E' || value2[i] == 'e') {
							ors_sdext = 1;
							ui_print("SD-Ext\n");
						} else if (value2[i] == 'M' || value2[i] == 'm') {
							//ui_print("MD5 check skip option ignored in CWMR\n");
						}
					}
				} else
					LOGI("No restore options set\n");
				nandroid_restore(value1, ors_boot, ors_system, ors_data, ors_cache, ors_sdext, 0);
				ui_print("Restore complete!\n");
			} else if (strcmp(command, "mount") == 0) {
				// Mount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_mounted(mount);
				ui_print("Mounted '%s'\n", mount);
			} else if (strcmp(command, "unmount") == 0 || strcmp(command, "umount") == 0) {
				// Unmount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_unmounted(mount);
				ui_print("Unmounted '%s'\n", mount);
			} else if (strcmp(command, "set") == 0) {
				// Set value
				/*
				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				strcpy(value2, tok);
				ui_print("Setting function disabled in CWMR: '%s' to '%s'\n", value1, value2);
				*/
			} else if (strcmp(command, "mkdir") == 0) {
				// Make directory (recursive)
				ensure_directory(value); // Untested from ORS
			} else if (strcmp(command, "reboot") == 0) {
				// Reboot
				ui_print("Reboot command found...\n");
				fclose(fp);
				if(is_path_mounted("sdcard/"))
					ensure_path_unmounted("sdcard/");
				delayed_reboot();
			} else if (strcmp(command, "cmd") == 0) {
				if (cindex != 0) {
					__system(value);
				} else {
					LOGE("No value given for cmd\n");
				}
			} else {
				LOGE("Unrecognized script command: '%s'\n", command);
				ret_val = 1;
			}
		}
		fclose(fp);
		ui_print("Done processing script file\n");
		if(ret_val != 1 && orsreboot == 1) {
			if(is_path_mounted("sdcard/"))
				ensure_path_unmounted("sdcard/");
			delayed_reboot();
		}
	} else {
		LOGE("Error opening script file '%s'\n");
		return 1;
	}
	return ret_val;
}

// end of open recovery script file code

int
main(int argc, char **argv) {
	if (strcmp(basename(argv[0]), "recovery") != 0)
	{
	    if (strstr(argv[0], "flash_image") != NULL)
	        return flash_image_main(argc, argv);
	    if (strstr(argv[0], "volume") != NULL)
	        return volume_main(argc, argv);
	    if (strstr(argv[0], "edify") != NULL)
	        return edify_main(argc, argv);
	    if (strstr(argv[0], "dump_image") != NULL)
	        return dump_image_main(argc, argv);
	    if (strstr(argv[0], "erase_image") != NULL)
	        return erase_image_main(argc, argv);
        if (strstr(argv[0], "e2fsck") != NULL)
            return cannibal_e2fsck_main(argc, argv);
        if (strstr(argv[0], "mke2fs") != NULL)
            return cannibal_mke2fs_main(argc, argv);
        if (strstr(argv[0], "tune2fs") != NULL)
            return cannibal_tune2fs_main(argc, argv);
	    if (strstr(argv[0], "mkyaffs2image") != NULL)
	        return mkyaffs2image_main(argc, argv);
	    if (strstr(argv[0], "unyaffs") != NULL)
	        return unyaffs_main(argc, argv);
        if (strstr(argv[0], "nandroid"))
            return nandroid_main(argc, argv);
        if (strstr(argv[0], "reboot"))
            return reboot_main(argc, argv);
#ifdef BOARD_RECOVERY_HANDLES_MOUNT
        if (strstr(argv[0], "mount") && argc == 2 && !strstr(argv[0], "umount"))
        {
            load_volume_table();
            return ensure_path_mounted(argv[1]);
        }
#endif
        if (strstr(argv[0], "poweroff")){
            return reboot_main(argc, argv);
        }
        if (strstr(argv[0], "setprop"))
            return setprop_main(argc, argv);
		return busybox_driver(argc, argv);
	}
    __system("/sbin/postrecoveryboot.sh");
    __system("mkdir -p /storage");
    __system("ln -s /sdcard /storage/sdcard0");

    int is_user_initiated_recovery = 0;
    time_t start = time(NULL);

    // If these fail, there's not really anywhere to complain...
    freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);
    printf("Starting recovery on %s", ctime(&start));
	load_volume_table();
    process_volumes();
    ui_init();
    //ui_print(EXPAND(RECOVERY_VERSION)"\n");

    LOGI("Processing arguments.\n");
    get_args(&argc, &argv);

    int previous_runs = 0;
    const char *send_intent = NULL;
    const char *update_package = NULL;
    const char *encrypted_fs_mode = NULL;
    int wipe_data = 0, wipe_cache = 0;
    int toggle_secure_fs = 0;
    encrypted_fs_info encrypted_fs_data;

    LOGI("Checking arguments.\n");
    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 'p': previous_runs = atoi(optarg); break;
        case 's': send_intent = optarg; break;
        case 'u': update_package = optarg; break;
        case 'w': 
#ifndef BOARD_RECOVERY_ALWAYS_WIPES
		wipe_data = wipe_cache = 1;
#endif
		break;
        case 'c': wipe_cache = 1; break;
        case 'e': encrypted_fs_mode = optarg; toggle_secure_fs = 1; break;
        case 't': ui_show_text(1); break;
        case '?':
            LOGE("Invalid command argument\n");
            continue;
        }
    }

    LOGI("device_recovery_start()\n");
    device_recovery_start();

    printf("Command:");
    for (arg = 0; arg < argc; arg++) {
        printf(" \"%s\"", argv[arg]);
    }
    printf("\n");

    if (update_package) {
        // For backwards compatibility on the cache partition only, if
        // we're given an old 'root' path "CACHE:foo", change it to
        // "/cache/foo".
        if (strncmp(update_package, "CACHE:", 6) == 0) {
            int len = strlen(update_package) + 10;
            char* modified_path = malloc(len);
            strlcpy(modified_path, "/cache/", len);
            strlcat(modified_path, update_package+6, len);
            printf("(replacing path \"%s\" with \"%s\")\n",
                   update_package, modified_path);
            update_package = modified_path;
        }
    }
    printf("\n");

    property_list(print_property, NULL);
    printf("\n");

#if TEST_AMEND
    test_amend();
#endif
    RecoveryCommandContext ctx = { NULL };
    if (register_update_commands(&ctx)) {
        LOGE("Can't install update commands\n");
    }

    int status = INSTALL_SUCCESS;

    if (toggle_secure_fs) {
        if (strcmp(encrypted_fs_mode,"on") == 0) {
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_ENABLED;
            ui_print("Enabling Encrypted FS.\n");
        } else if (strcmp(encrypted_fs_mode,"off") == 0) {
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_DISABLED;
            ui_print("Disabling Encrypted FS.\n");
        } else {
            ui_print("Error: invalid Encrypted FS setting.\n");
            status = INSTALL_ERROR;
        }

        // Recovery strategy: if the data partition is damaged, disable encrypted file systems.
        // This preventsthe device recycling endlessly in recovery mode.
        if ((encrypted_fs_data.mode == MODE_ENCRYPTED_FS_ENABLED) &&
                (read_encrypted_fs_info(&encrypted_fs_data))) {
            ui_print("Encrypted FS change aborted, resetting to disabled state.\n");
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_DISABLED;
        }

        if (status != INSTALL_ERROR) {
            if (erase_volume("/data")) {
                ui_print("Data wipe failed.\n");
                status = INSTALL_ERROR;
            } else if (erase_volume("/cache")) {
                ui_print("Cache wipe failed.\n");
                status = INSTALL_ERROR;
            } else if ((encrypted_fs_data.mode == MODE_ENCRYPTED_FS_ENABLED) &&
                      (restore_encrypted_fs_info(&encrypted_fs_data))) {
                ui_print("Encrypted FS change aborted.\n");
                status = INSTALL_ERROR;
            } else {
                ui_print("Successfully updated Encrypted FS.\n");
                status = INSTALL_SUCCESS;
            }
        }
    } else if (update_package != NULL) {
        status = install_package(update_package);
        if (status != INSTALL_SUCCESS) ui_print("Installation aborted.\n");
    } else if (wipe_data) {
        if (device_wipe_data()) status = INSTALL_ERROR;
        if (erase_volume("/data")) status = INSTALL_ERROR;
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Data wipe failed.\n");
    } else if (wipe_cache) {
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Cache wipe failed.\n");
    } else {
        LOGI("Checking for extendedcommand...\n");
        status = INSTALL_ERROR;  // No command specified
        // we are starting up in user initiated recovery here
        // let's set up some default options
        script_assert_enabled = 0;
        is_user_initiated_recovery = 1;
        ui_set_show_text(1);
        parse_settings();

        if (check_for_script_file()) run_script_file();
        if (extendedcommand_file_exists()) {
            LOGI("Running extendedcommand...\n");
            int ret;
            if (0 == (ret = run_and_remove_extendedcommand())) {
                status = INSTALL_SUCCESS;
                ui_set_show_text(0);
            }
            else {
                handle_failure(ret);
            }
        } else {
            LOGI("Skipping execution of extendedcommand, file not found...\n");
        }
    }

    if (status != INSTALL_SUCCESS && !is_user_initiated_recovery) ui_set_background(BACKGROUND_ICON_ERROR);
    if (status != INSTALL_SUCCESS || ui_text_visible()) {
        prompt_and_wait();
    }

    // If there is a radio image pending, reboot now to install it.
    maybe_install_firmware_update(send_intent);

    // Otherwise, get ready to boot the main system...
    finish_recovery(send_intent);
    if(!poweroff)
        ui_print("%s\n", rebooting);
    else
        ui_print("%s\n", shutdown);
    sync();
    reboot((!poweroff) ? RB_AUTOBOOT : RB_POWER_OFF);
    return EXIT_SUCCESS;
}

int get_allow_toggle_display() {
    return allow_display_toggle;
}
