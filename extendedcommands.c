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
#include "eraseandformat.h"

#define ABS_MT_POSITION_X 0x35  /* Center X ellipse position */

int script_assert_enabled = 1;
static const char *SDCARD_UPDATE_FILE = "/sdcard/update.zip";

int
get_filtered_menu_selection(char** headers, char** items, int menu_only, int initial_selection, int items_count) {
    int index;
    int offset = 0;
    int* translate_table = (int*)malloc(sizeof(int) * items_count);
    for (index = 0; index < items_count; index++) {
        if (items[index] == NULL)
            continue;
        char *item = items[index];
        items[index] = NULL;
        items[offset] = item;
        translate_table[offset] = index;
        offset++;
    }
    items[offset] = NULL;

    initial_selection = translate_table[initial_selection];
    int ret = get_menu_selection(headers, items, menu_only, initial_selection);
    if (ret < 0 || ret >= offset) {
        free(translate_table);
        return ret;
    }

    ret = translate_table[ret];
    free(translate_table);
    return ret;
}

void write_string_to_file(const char* filename, const char* string) {
    ensure_path_mounted(filename);
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p $(dirname %s)", filename);
    __system(tmp);
    FILE *file = fopen(filename, "w");
    if( file != NULL) {
        fprintf(file, "%s", string);
        fclose(file);
    }
}

/* Only valid reason for this is application recognition so, we'll want to store
 * these values using the same locations as CWM, regardless of our file
 * locations. */
void write_recovery_version() {
    if(is_data_media()) {
        write_string_to_file("/sdcard/0/clockworkmod/.recovery_version",EXPAND(CWM_RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
    }
    write_string_to_file("/sdcard/clockworkmod/.recovery_version",EXPAND(CWM_RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
}

void toggle_ui_debugging()
{
	switch(UI_COLOR_DEBUG) {
		case 0: {
			ui_print("Enabling UI color debugging; will disable again on reboot.\n");
			UI_COLOR_DEBUG = 1;
			break;
		}
		default: {
			ui_print("Disabling UI color debugging.\n");
			UI_COLOR_DEBUG = 0;
			break;
		}
	}
}

int install_zip(const char* packagefilepath)
{
    ui_print("\n-- Installing: %s\n", packagefilepath);
    if (device_flash_type() == MTD) {
        set_sdcard_update_bootloader_message();
    }
    int status = install_package(packagefilepath);
    ui_reset_progress();
    if (status != INSTALL_SUCCESS) {
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
        ui_print("Installation aborted.\n");
        return 1;
    }
    ui_print("\nInstall from sdcard complete.\n");
    ui_init_icons();
    return 0;
}

#define ITEM_CHOOSE_ZIP       0
#define ITEM_APPLY_SIDELOAD   1
#define ITEM_APPLY_UPDATE     2
#define ITEM_CHOOSE_ZIP_INT   3

void show_install_update_menu()
{
    static char* headers[] = {  "ZIP Flashing",
                                "",
                                NULL
    };
    
    char* install_menu_items[] = {  "Choose ZIP from SD Card",
                                    "Update via sideload",
                                    "Install /sdcard/update.zip",
                                    NULL,
                                    NULL };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        install_menu_items[3] = "Choose ZIP from internal SD Card";
    }
    else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd/";
        install_menu_items[3] = "Choose ZIP from external SD Card";
    }
    
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, install_menu_items, 0, 0);
        switch (chosen_item)
        {
            case ITEM_APPLY_UPDATE:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /sdcard/update.zip"))
                    install_zip(SDCARD_UPDATE_FILE);
                break;
            }
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu("/sdcard/");
                write_recovery_version();
                break;
            case ITEM_APPLY_SIDELOAD:
                apply_from_adb();
                break;
            case ITEM_CHOOSE_ZIP_INT:
                if (other_sd != NULL)
                    show_choose_zip_menu(other_sd);
                break;
            default:
                return;
        }

    }
}

void free_string_array(char** array)
{
    if (array == NULL)
        return;
    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL)
    {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

char** gather_files(const char* directory, const char* fileExtensionOrDirectory, int* numFiles)
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files = NULL;
    int pass;
    *numFiles = 0;
    int dirLen = strlen(directory);

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory.\n");
        return NULL;
    }

    int extension_length = 0;
    if (fileExtensionOrDirectory != NULL)
        extension_length = strlen(fileExtensionOrDirectory);

    int isCounting = 1;
    i = 0;
    for (pass = 0; pass < 2; pass++) {
        while ((de=readdir(dir)) != NULL) {
            // skip hidden files
            if (de->d_name[0] == '.')
                continue;

            // NULL means that we are gathering directories, so skip this
            if (fileExtensionOrDirectory != NULL)
            {
                // make sure that we can have the desired extension (prevent seg fault)
                if (strlen(de->d_name) < extension_length)
                    continue;
                // compare the extension
                if (strcmp(de->d_name + strlen(de->d_name) - extension_length, fileExtensionOrDirectory) != 0)
                    continue;
            }
            else
            {
                struct stat info;
                char fullFileName[PATH_MAX];
                strcpy(fullFileName, directory);
                strcat(fullFileName, de->d_name);
                lstat(fullFileName, &info);
                // make sure it is a directory
                if (!(S_ISDIR(info.st_mode)))
                    continue;
            }

            if (pass == 0)
            {
                total++;
                continue;
            }

            files[i] = (char*) malloc(dirLen + strlen(de->d_name) + 2);
            strcpy(files[i], directory);
            strcat(files[i], de->d_name);
            if (fileExtensionOrDirectory == NULL)
                strcat(files[i], "/");
            i++;
        }
        if (pass == 1)
            break;
        if (total == 0)
            break;
        rewinddir(dir);
        *numFiles = total;
        files = (char**) malloc((total+1)*sizeof(char*));
        files[total]=NULL;
    }

    if(closedir(dir) < 0) {
        LOGE("Failed to close directory.");
    }

    if (total==0) {
        return NULL;
    }

    // sort the result
    if (files != NULL) {
        for (i = 0; i < total; i++) {
            int curMax = -1;
            int j;
            for (j = 0; j < total - i; j++) {
                if (curMax == -1 || strcmp(files[curMax], files[j]) < 0)
                    curMax = j;
            }
            char* temp = files[curMax];
            files[curMax] = files[total - i - 1];
            files[total - i - 1] = temp;
        }
    }

    return files;
}

// pass in NULL for fileExtensionOrDirectory and you will get a directory chooser
char* choose_file_menu(const char* directory, const char* fileExtensionOrDirectory, const char* headers[])
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int numFiles = 0;
    int numDirs = 0;
    int i;
    char* return_value = NULL;
    int dir_len = strlen(directory);

    i = 0;
    while (headers[i]) {
        i++;
    }
    const char** fixed_headers = (const char*)malloc((i + 3) * sizeof(char*));
    i = 0;
    while (headers[i]) {
        fixed_headers[i] = headers[i];
        i++;
    }
    fixed_headers[i] = directory;
    fixed_headers[i + 1] = "";
    fixed_headers[i + 2 ] = NULL;

    char** files = gather_files(directory, fileExtensionOrDirectory, &numFiles);
    char** dirs = NULL;
    if (fileExtensionOrDirectory != NULL)
        dirs = gather_files(directory, NULL, &numDirs);
    int total = numDirs + numFiles;
    if (total == 0)
    {
        ui_print("No files found.\n");
    }
    else
    {
        char** list = (char**) malloc((total + 1) * sizeof(char*));
        list[total] = NULL;


        for (i = 0 ; i < numDirs; i++)
        {
            list[i] = strdup(dirs[i] + dir_len);
        }

        for (i = 0 ; i < numFiles; i++)
        {
            list[numDirs + i] = strdup(files[i] + dir_len);
        }

        for (;;)
        {
            int chosen_item = get_menu_selection(fixed_headers, list, 0, 0);
            if (chosen_item == GO_BACK)
                break;
            static char ret[PATH_MAX];
            if (chosen_item < numDirs)
            {
                char* subret = choose_file_menu(dirs[chosen_item], fileExtensionOrDirectory, headers);
                if (subret != NULL)
                {
                    strcpy(ret, subret);
                    return_value = ret;
                    break;
                }
                continue;
            }
            strcpy(ret, files[chosen_item - numDirs]);
            return_value = ret;
            break;
        }
        free_string_array(list);
    }

    free_string_array(files);
    free_string_array(dirs);
    free(fixed_headers);
    return return_value;
}

void show_choose_zip_menu(const char *mount_point)
{
    if (ensure_path_mounted(mount_point) != 0) {
        LOGE ("Can't mount %s\n", mount_point);
        return;
    }
    
    static char *INSTALL_OR_BACKUP_ITEMS[] = { "Yes - Backup and install",
												"No - Install without backup",
												"Cancel Install",
												NULL};
												
	#define ITEM_BACKUP_AND_INSTALL 0
	#define ITEM_INSTALL_WOUT_BACKUP 1
	#define ITEM_CANCEL_INSTALL 2

    static char* headers[] = {  "Choose a zip to apply",
                                "",
                                NULL
    };

    char* file = choose_file_menu(mount_point, ".zip", headers);
    if (file == NULL)
        return;
        
    if (backupprompt == 0) {
		static char* confirm_install = "Confirm install?";
		static char confirm[PATH_MAX];
		sprintf(confirm, "Yes - Install %s", basename(file));
		if (confirm_selection(confirm_install, confirm)) {
			install_zip(file);
        }
	} else {
		for (;;) {
			int chosen_item = get_menu_selection(headers, INSTALL_OR_BACKUP_ITEMS, 0, 0);
			switch(chosen_item) {
				case ITEM_BACKUP_AND_INSTALL: {
					char backup_path[PATH_MAX];
					nandroid_generate_timestamp_path(backup_path, 0);
					nandroid_backup(backup_path);
					install_zip(file);
					return;
				}
				case ITEM_INSTALL_WOUT_BACKUP:
					install_zip(file);
					return;
				default:
					break;
			}
			break;
		}
    }
}

void show_nandroid_restore_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose an image to restore",
                                "",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm restore?", "Yes - Restore"))
        nandroid_restore(file, 1, 1, 1, 1, 1, 0);
}

void show_nandroid_delete_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose an image to delete",
                                "",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm delete?", "Yes - Delete")) {
        // nandroid_restore(file, 1, 1, 1, 1, 1, 0);
        char tmp[PATH_MAX];
        ui_print("Deleting %s\n", basename(file));
        sprintf(tmp, "rm -rf %s", file);
        __system(tmp);
        ui_print("Backup deleted!\n");
        uint64_t sdcard_free_mb = recalc_sdcard_space(path);
		ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
    }
}

/* This will only be called if there is an OTHER_SD_CARD in existance
 * and the user has chosen to "View and delete old backups" during a
 * backup. */
int show_choose_delete_menu()
{
    static char *CHOOSE_DELETE_MENU_ITEMS[] = { "View and delete backups on /sdcard",
                                                NULL,
                                                NULL };

    #define ITEM_VIEW_BACKUPS_ON_SDCARD 0
    #define ITEM_VIEW_BACKUPS_ON_OTHERSD 1

    static char* headers[] = { "Choose a device to delete",
                               "backups from.",
                               "",
                               NULL
    };

    if(strcasecmp(OTHER_SD_CARD,"/emmc")) {
        CHOOSE_DELETE_MENU_ITEMS[1]="View and delete backups on /emmc";
    } else if(strcasecmp(OTHER_SD_CARD,"/external_sd")) {
        CHOOSE_DELETE_MENU_ITEMS[1]="View and delete backups on /external_sd";
    }

    for (;;) {
        char base_path[PATH_MAX];
        int chosen_item = get_menu_selection(headers, CHOOSE_DELETE_MENU_ITEMS, 0, 0);
        switch(chosen_item) {
            case ITEM_VIEW_BACKUPS_ON_SDCARD:
                nandroid_get_backup_path(base_path, 0);
                break;
            case ITEM_VIEW_BACKUPS_ON_OTHERSD:
                nandroid_get_backup_path(base_path, 1);
                break;
            default:
                ui_print("Cancelling backup.\n");
                return 1;
        }
        show_nandroid_delete_menu(base_path);
    }
}

static void run_dedupe_gc(const char* other_sd) {
    ensure_path_mounted("/sdcard");
	char path[PATH_MAX], tmp[PATH_MAX];
	nandroid_get_assigned_backup_path(path, 0);
    if (other_sd) {
        ensure_path_mounted(other_sd);
		nandroid_get_assigned_backup_path(path, 1);
    }
	sprintf(tmp, "%s/blobs", path);
	nandroid_dedupe_gc(tmp);
    uint64_t sdcard_free_mb = recalc_sdcard_space(path);
    ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
}

int show_lowspace_menu(int i, const char* backup_path)
{
	static char *LOWSPACE_MENU_ITEMS[5] = { "Continue with backup",
											"View and delete old backups",
                                            NULL,
                                            NULL,
											NULL };

	#define ITEM_CONTINUE_BACKUP 0
	#define ITEM_VIEW_DELETE_BACKUPS 1
    #define ITEM_FREE_DATA_OR_CANCEL 2

    if(!backupfmt) {
        LOWSPACE_MENU_ITEMS[2] = "Free unused backup data";
        LOWSPACE_MENU_ITEMS[3] = "Cancel backup";
    } else {
        LOWSPACE_MENU_ITEMS[2] = "Cancel backup";
        LOWSPACE_MENU_ITEMS[3] = NULL;
    }

	static char* headers[] = { "Limited space available!",
								"",
								"There may not be enough space",
								"to continue backup.",
								"",
								"What would you like to do?",
								"",
								NULL
	};

	for (;;) {
		int chosen_item = get_menu_selection(headers, LOWSPACE_MENU_ITEMS, 0, 0);
		switch(chosen_item) {
			case ITEM_CONTINUE_BACKUP: {
				ui_print("Proceeding with backup.\n");
				return 0;
			}
			case ITEM_VIEW_DELETE_BACKUPS: {
                if(OTHER_SD_CARD) {
                    show_choose_delete_menu();
                } else {
                    char base_path[PATH_MAX];
                    nandroid_get_backup_path(base_path, 0);
				    show_nandroid_delete_menu(base_path);
                }
				break;
            }
            case ITEM_FREE_DATA_OR_CANCEL: {
                if(backupfmt) {
                    ui_print("Cancelling backup.\n");
                    return 1;
                }
                char *other_sd = NULL;
                if(OTHER_SD_CARD) {
                    switch(OTHER_SD_CARD) {
                        case EMMC:
                            other_sd = "/emmc";
                            break;
                        case EXTERNALSD:
                            other_sd = "/external_sd";
                            break;
                    }
                }
                run_dedupe_gc(other_sd);
                break;
            }
			default:
				ui_print("Cancelling backup.\n");
				return 1;
		}
	}
}

#define MAX_NUM_USB_VOLUMES 3
#define LUN_FILE_EXPANDS    2

struct lun_node {
    const char *lun_file;
    struct lun_node *next;
};

static struct lun_node *lun_head = NULL;
static struct lun_node *lun_tail = NULL;

int control_usb_storage_set_lun(Volume* vol, bool enable, const char *lun_file) {
    const char *vol_device = enable ? vol->device : "";
    int fd;
    struct lun_node *node;

    // Verify that we have not already used this LUN file
    for(node = lun_head; node; node = node->next) {
        if (!strcmp(node->lun_file, lun_file)) {
            // Skip any LUN files that are already in use
            return -1;
        }
    }

    // Open a handle to the LUN file
    LOGI("Trying %s on LUN file %s\n", vol->device, lun_file);
    if ((fd = open(lun_file, O_WRONLY)) < 0) {
        LOGW("Unable to open ums lunfile %s (%s)\n", lun_file, strerror(errno));
        return -1;
    }

    // Write the volume path to the LUN file
    if ((write(fd, vol_device, strlen(vol_device) + 1) < 0) &&
       (!enable || !vol->device2 || (write(fd, vol->device2, strlen(vol->device2)) < 0))) {
        LOGW("Unable to write to ums lunfile %s (%s)\n", lun_file, strerror(errno));
        close(fd);
        return -1;
    } else {
        // Volume path to LUN association succeeded
        close(fd);

        // Save off a record of this lun_file being in use now
        node = (struct lun_node *)malloc(sizeof(struct lun_node));
        node->lun_file = strdup(lun_file);
        node->next = NULL;
        if (lun_head == NULL)
           lun_head = lun_tail = node;
        else {
           lun_tail->next = node;
           lun_tail = node;
        }

        LOGI("Successfully %sshared %s on LUN file %s\n", enable ? "" : "un", vol->device, lun_file);
        return 0;
    }
}

int control_usb_storage_for_lun(Volume* vol, bool enable) {
    static const char* lun_files[] = {
#ifdef BOARD_UMS_LUNFILE
        BOARD_UMS_LUNFILE,
#endif
#ifdef TARGET_USE_CUSTOM_LUN_FILE_PATH
        TARGET_USE_CUSTOM_LUN_FILE_PATH,
#endif
        "/sys/devices/platform/usb_mass_storage/lun%d/file",
        "/sys/class/android_usb/android0/f_mass_storage/lun/file",
        "/sys/class/android_usb/android0/f_mass_storage/lun_ex/file",
        NULL
    };

    // If recovery.fstab specifies a LUN file, use it
    if (vol->lun) {
        return control_usb_storage_set_lun(vol, enable, vol->lun);
    }

    // Try to find a LUN for this volume
    //   - iterate through the lun file paths
    //   - expand any %d by LUN_FILE_EXPANDS
    int lun_num = 0;
    int i;
    for(i = 0; lun_files[i]; i++) {
        const char *lun_file = lun_files[i];
        for(lun_num = 0; lun_num < LUN_FILE_EXPANDS; lun_num++) {
            char formatted_lun_file[255];
    
            // Replace %d with the LUN number
            bzero(formatted_lun_file, 255);
            snprintf(formatted_lun_file, 254, lun_file, lun_num);
    
            // Attempt to use the LUN file
            if (control_usb_storage_set_lun(vol, enable, formatted_lun_file) == 0) {
                return 0;
            }
        }
    }

    // All LUNs were exhausted and none worked
    LOGW("Could not %sable %s on LUN %d\n", enable ? "en" : "dis", vol->device, lun_num);

    return -1;  // -1 failure, 0 success
}

int control_usb_storage(Volume **volumes, bool enable) {
    int res = -1;
    int i;
    for(i = 0; i < MAX_NUM_USB_VOLUMES; i++) {
        Volume *volume = volumes[i];
        if (volume) {
            int vol_res = control_usb_storage_for_lun(volume, enable);
            if (vol_res == 0) res = 0; // if any one path succeeds, we return success
        }
    }

    // Release memory used by the LUN file linked list
    struct lun_node *node = lun_head;
    while(node) {
       struct lun_node *next = node->next;
       free((void *)node->lun_file);
       free(node);
       node = next;
    }
    lun_head = lun_tail = NULL;

    return res;  // -1 failure, 0 success
}

void show_mount_usb_storage_menu()
{
    // Build a list of Volume objects; some or all may not be valid
    Volume* volumes[MAX_NUM_USB_VOLUMES] = {
        volume_for_path("/sdcard"),
        volume_for_path("/emmc"),
        volume_for_path("/external_sd")
    };

    // Enable USB storage
    if (control_usb_storage(volumes, 1))
        return;

    static char* headers[] = {  "USB Mass Storage device",
                                "Leaving this menu unmount",
                                "your SD card from your PC.",
                                "",
                                NULL
    };

    static char* list[] = { "Unmount", NULL };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == 0)
            break;
    }

    // Disable USB storage
    control_usb_storage(volumes, 0);
}

int confirm_selection(const char* title, const char* confirm)
{
    struct stat info;
    if (0 == stat("/sdcard/cotrecovery/.no_confirm", &info))
        return 1;

    char* confirm_headers[]  = {  title, "  THIS CAN NOT BE UNDONE.", "", NULL };
    if (0 == stat("/sdcard/cotrecovery/.one_confirm", &info)) {
        char* items[] = { "No",
                        confirm, //" Yes -- wipe partition",   // [1]
                        NULL };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        return chosen_item == 1;
    }
    else {
        char* items[] = { "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        confirm, //" Yes -- wipe partition",   // [7]
                        "No",
                        "No",
                        "No",
                        NULL };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        return chosen_item == 7;
    }
}

int confirm_nandroid_backup(const char* title, const char* confirm)
{
    char* confirm_headers[]  = {  title, "THIS IS RECOMMENDED!", "", NULL };
    char* items[] = { "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      confirm, //" Yes -- wipe partition",   // [7
                      "No",
                      "No",
                      "No",
                      NULL };

    int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
    return chosen_item == 7;
}

void show_nandroid_advanced_backup_menu(const char *path, int other_sd) {
	if (ensure_path_mounted(path) != 0) {
		LOGE ("Can't mount %s\n", path);
		return;
	}

	static char* advancedheaders[] = { "Choose the partitions to backup.",
					NULL
    };
    
    int backup_list [7];
    char* list[7];
    
    backup_list[0] = 1;
    backup_list[1] = 1;
    backup_list[2] = 1;
    backup_list[3] = 1;
    backup_list[4] = 1;
    backup_list[5] = NULL;
    
    list[5] = "Perform Backup";
    list[6] = NULL;
    
    int cont = 1;
    for (;cont;) {
		if (backup_list[0] == 1)
			list[0] = "Backup boot: Yes";
		else
			list[0] = "Backup boot: No";
			
		if (backup_list[1] == 1)
	    	list[1] = "Backup recovery: Yes";
	    else
	    	list[1] = "Backup recovery: No";
	    	
	    if (backup_list[2] == 1)
    		list[2] = "Backup system: Yes";
	    else
	    	list[2] = "Backup system: No";

	    if (backup_list[3] == 1)
	    	list[3] = "Backup data: Yes";
	    else
	    	list[3] = "Backup data: No";

	    if (backup_list[4] == 1)
	    	list[4] = "Backup cache: Yes";
	    else
	    	list[4] = "Backup cache: No";
	    	
	    int chosen_item = get_menu_selection (advancedheaders, list, 0, 0);
	    switch (chosen_item) {
			case GO_BACK: return;
			case 0: backup_list[0] = !backup_list[0];
				break;
			case 1: backup_list[1] = !backup_list[1];
				break;
			case 2: backup_list[2] = !backup_list[2];
				break;
			case 3: backup_list[3] = !backup_list[3];
				break;
			case 4: backup_list[4] = !backup_list[4];
				break;	
		   
			case 5: cont = 0;
				break;
		}
	}
	
	nandroid_generate_timestamp_path(path, other_sd);
	return nandroid_advanced_backup(path, backup_list[0], backup_list[1], backup_list[2], backup_list[3], backup_list[4], backup_list[5]);
}

void show_nandroid_advanced_restore_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE ("Can't mount %s\n", path);
        return;
    }

    static char* advancedheaders[] = {  "Choose an image to restore",
                                "",
                                "Choose an image to restore",
                                "first. The next menu will",
                                "give you more options.",
                                "",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, advancedheaders);
    if (file == NULL)
        return;

    static char* headers[] = {  "Advanced Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Restore boot",
                            "Restore system",
                            "Restore data",
                            "Restore cache",
                            "Restore sd-ext",
                            "Restore wimax",
                            NULL
    };
    
    if (0 != get_partition_device("wimax", path)) {
        // disable wimax restore option
        list[5] = NULL;
    }

    static char* confirm_restore  = "Confirm restore?";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            if (confirm_selection(confirm_restore, "Yes - Restore boot"))
                nandroid_restore(file, 1, 0, 0, 0, 0, 0);
            break;
        case 1:
            if (confirm_selection(confirm_restore, "Yes - Restore system"))
                nandroid_restore(file, 0, 1, 0, 0, 0, 0);
            break;
        case 2:
            if (confirm_selection(confirm_restore, "Yes - Restore data"))
                nandroid_restore(file, 0, 0, 1, 0, 0, 0);
            break;
        case 3:
            if (confirm_selection(confirm_restore, "Yes - Restore cache"))
                nandroid_restore(file, 0, 0, 0, 1, 0, 0);
            break;
        case 4:
            if (confirm_selection(confirm_restore, "Yes - Restore sd-ext"))
                nandroid_restore(file, 0, 0, 0, 0, 1, 0);
            break;
        case 5:
            if (confirm_selection(confirm_restore, "Yes - Restore wimax"))
                nandroid_restore(file, 0, 0, 0, 0, 0, 1);
            break;
    }
}

void show_nandroid_menu()
{
    static char* headers[] = {  "Nandroid",
                                "",
                                NULL
    };

    char* list[] = { "Backup",
                            "Restore",
                            "Delete old backups",
                            "Advanced Backup",
                            "Advanced Restore",
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL
    };
    
    if(!backupfmt) list[5] = "Free unused backup data";

    char *other_sd = NULL;
    if(OTHER_SD_CARD == EMMC) {
		list[6] = "Backup to internal sdcard";
		list[7] = "Restore from internal sdcard";
		list[8] = "Delete from internal sdcard";
		list[9] = "Advanced backup to internal sdcard";
		list[10] = "Advanced restore from internal sdcard";
		other_sd = "/emmc";
	} else if (OTHER_SD_CARD == EXTERNALSD) {
		list[6] = "Backup to external sdcard";
		list[7] = "Restore from external sdcard";
		list[8] = "Delete from external sdcard";
		list[9] = "Advanced backup to external sdcard";
		list[10] = "Advanced restore from external sdcard";
		other_sd = "/external_sd";
	}
#ifdef RECOVERY_EXTEND_NANDROID_MENU
    extend_nandroid_menu(list, 10, sizeof(list) / sizeof(char*));
#endif

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
			char backup_path[PATH_MAX];
            case 0:
                {
					nandroid_generate_timestamp_path(backup_path, 0);
                    nandroid_backup(backup_path);
                    write_recovery_version();
					break;
                }
            case 1:
				{
					nandroid_get_backup_path(backup_path, 0);
                	show_nandroid_restore_menu(backup_path);
					write_recovery_version();
                	break;
				}
            case 2:
				{
					nandroid_get_backup_path(backup_path, 0);
                	show_nandroid_delete_menu(backup_path);
                	write_recovery_version();
                	break;
				}
			case 3:
				{
					nandroid_get_backup_path(backup_path, 0);
					show_nandroid_advanced_backup_menu(backup_path, 0);
					write_recovery_version();
					break;
				}
            case 4:
				{
					nandroid_get_backup_path(backup_path, 0);
                	show_nandroid_advanced_restore_menu(backup_path);
                	write_recovery_version();
                	break;
				}
            case 5:
                run_dedupe_gc(other_sd);
                break;
            case 6:
                {
					nandroid_generate_timestamp_path(backup_path, 1);
                    nandroid_backup(backup_path);
                    write_recovery_version();
					break;
                }
            case 7:
                if (other_sd != NULL) {
					nandroid_get_backup_path(backup_path, 1);
                    show_nandroid_restore_menu(backup_path);
                    write_recovery_version();
                }
                break;
            case 8:
                if (other_sd != NULL) {
					nandroid_get_backup_path(backup_path, 1);
                    show_nandroid_delete_menu(backup_path);
                    write_recovery_version();
                }
            case 9:
                if (other_sd != NULL) {
					nandroid_get_backup_path(backup_path, 1);
					show_nandroid_advanced_backup_menu(backup_path, 1);
					write_recovery_version();
					break;
				}
            case 10:
                if (other_sd != NULL) {
					nandroid_get_backup_path(backup_path, 1);
                    show_nandroid_advanced_restore_menu(backup_path);
                    write_recovery_version();
                }
                break;

                break;
            default:
#ifdef RECOVERY_EXTEND_NANDROID_MENU
                handle_nandroid_menu(11, chosen_item);
#endif
                break;
        }
    }
}

void partition_sdcard(const char* volume) {
    if (!can_partition(volume)) {
        ui_print("Can't partition device: %s\n", volume);
        return;
    }

    static char* ext_sizes[] = { "128M",
                                 "256M",
                                 "512M",
                                 "1024M",
                                 "2048M",
                                 "4096M",
                                 NULL };

    static char* swap_sizes[] = { "0M",
                                  "32M",
                                  "64M",
                                  "128M",
                                  "256M",
                                  NULL };

    static char* ext_headers[] = { "Ext Size", "", NULL };
    static char* swap_headers[] = { "Swap Size", "", NULL };

    int ext_size = get_menu_selection(ext_headers, ext_sizes, 0, 0);
    if (ext_size == GO_BACK)
        return;

    int swap_size = get_menu_selection(swap_headers, swap_sizes, 0, 0);
    if (swap_size == GO_BACK)
        return;

    char sddevice[256];
    Volume *vol = volume_for_path(volume);
    strcpy(sddevice, vol->device);
    // we only want the mmcblk, not the partition
    sddevice[strlen("/dev/block/mmcblkX")] = NULL;
    char cmd[PATH_MAX];
    setenv("SDPATH", sddevice, 1);
    sprintf(cmd, "sdparted -es %s -ss %s -efs ext3 -s", ext_sizes[ext_size], swap_sizes[swap_size]);
    ui_print("Partitioning SD Card... please wait...\n");
    if (0 == __system(cmd))
        ui_print("Done!\n");
    else
        ui_print("An error occured while partitioning your SD Card. Please see /tmp/recovery.log for more details.\n");
}

int can_partition(const char* volume) {
    Volume *vol = volume_for_path(volume);
    if (vol == NULL) {
        LOGI("Can't format unknown volume: %s\n", volume);
        return 0;
    }

    /* Forbid partitioning anything that can't be formatted (the Kindle
     * Fire as mentioned below isn't handled properly; since we shouldn't
     * really be formatting it either, we can go ahead and block both
     * functions using the system.prop) */
    if (!is_safe_to_format(volume)) {
		LOGI("Can't partition, format forbidden on: %s\n", volume);
		return 0;
	}

    int vol_len = strlen(vol->device);

    /* This does not work properly with the Kindle Fire (otter); instead of
     * removing the option it shows up in fubard text. Device ends in p12 */

    // do not allow partitioning of a device that isn't mmcblkX or mmcblkXp1
    if (vol->device[vol_len - 2] == 'p' && vol->device[vol_len - 2] != '1') {
        LOGI("Can't partition unsafe device: %s\n", vol->device);
        return 0;
    }
    
    if (strcmp(vol->fs_type, "vfat") != 0) {
        LOGI("Can't partition non-vfat: %s\n", vol->fs_type);
        return 0;
    }

    return 1;
}

void write_fstab_root(char *path, FILE *file)
{
    Volume *vol = volume_for_path(path);
    if (vol == NULL) {
        LOGW("Unable to get recovery.fstab info for %s during fstab generation!\n", path);
        return;
    }
    char device[200];
    if (vol->device[0] != '/')
        get_partition_device(vol->device, device);
    else
        strcpy(device, vol->device);

    fprintf(file, "%s ", device);
    fprintf(file, "%s ", path);
    // special case rfs cause auto will mount it as vfat on samsung.
    fprintf(file, "%s rw\n", vol->fs_type2 != NULL && strcmp(vol->fs_type, "rfs") != 0 ? "auto" : vol->fs_type);
}

void create_fstab()
{
    struct stat info;
    __system("touch /etc/mtab");
    FILE *file = fopen("/etc/fstab", "w");
    if (file == NULL) {
        LOGW("Unable to create /etc/fstab!\n");
        return;
    }
    Volume *vol = volume_for_path("/boot");
    if (NULL != vol && strcmp(vol->fs_type, "mtd") != 0 && strcmp(vol->fs_type, "emmc") != 0 && strcmp(vol->fs_type, "bml") != 0)
    write_fstab_root("/boot", file);
    write_fstab_root("/cache", file);
    write_fstab_root("/data", file);
    write_fstab_root("/datadata", file);
    write_fstab_root("/emmc", file);
    write_fstab_root("/system", file);
    write_fstab_root("/sdcard", file);
    write_fstab_root("/sd-ext", file);
    write_fstab_root("/external_sd", file);
    fclose(file);
    LOGI("Completed outputting fstab.\n");
}

int bml_check_volume(const char *path) {
    ui_print("Checking %s...\n", path);
    ensure_path_unmounted(path);
    if (0 == ensure_path_mounted(path)) {
        ensure_path_unmounted(path);
        return 0;
    }

    Volume *vol = volume_for_path(path);
    if (vol == NULL) {
        LOGE("Unable process volume! Skipping...\n");
        return 0;
    }

    ui_print("%s may be rfs. Checking...\n", path);
    char tmp[PATH_MAX];
    sprintf(tmp, "mount -t rfs %s %s", vol->device, path);
    int ret = __system(tmp);
    printf("%d\n", ret);
    return ret == 0 ? 1 : 0;
}

void process_volumes() {
    create_fstab();

    if (is_data_media()) {
        setup_data_media();
    }

    return;

    // dead code.
    if (device_flash_type() != BML)
        return;

    ui_print("Checking for ext4 partitions...\n");
    int ret = 0;
    ret = bml_check_volume("/system");
    ret |= bml_check_volume("/data");
    if (has_datadata())
        ret |= bml_check_volume("/datadata");
    ret |= bml_check_volume("/cache");

    if (ret == 0) {
        ui_print("Done!\n");
        return;
    }

    char backup_path[PATH_MAX];
    time_t t = time(NULL);
    char backup_name[PATH_MAX];
    struct timeval tp;
    gettimeofday(&tp, NULL);
    sprintf(backup_name, "before-ext4-convert-%d", tp.tv_sec);
    struct stat st;

	char base_path[PATH_MAX];
	nandroid_get_base_backup_path(base_path, 0);

    ui_set_show_text(1);
    ui_print("Filesystems need to be converted to ext4.\n");
    ui_print("A backup and restore will now take place.\n");
    ui_print("If anything goes wrong, your backup will be\n");
    ui_print("named %s. Try restoring it\n", backup_name);
    ui_print("in case of error.\n");

    nandroid_backup(backup_path);
    nandroid_restore(backup_path, 1, 1, 1, 1, 1, 0);
    ui_set_show_text(0);
}

void handle_failure(int ret)
{
    if (ret == 0)
        return;

    // ToDo: add a function to let users force logs to be saved to sdcard...
    char tmp[PATH_MAX];
    if(OTHER_SD_CARD == EMMC) {
        if(0 != ensure_path_mounted("/emmc")) {
            ui_print("Can't mount /emmc.\n");
            return -1;
        }
        strcpy(tmp, "/emmc/cotrecovery" );
    } else {
        if(0 != ensure_path_mounted("/sdcard")) {
            ui_print("Can't mount /sdcard.\n");
            return -1;
        }
        strcpy(tmp, "/sdcard/cotrecovery");
    }

    mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);

    char cmd[PATH_MAX];
	sprintf(cmd, "cp /tmp/recovery.log %s/recovery.log", tmp);
    __system(cmd);

    //sprintf(cmd, "A copy of the recovery log has been copied to %s/recovery.log. Please submit this file with your bug report.\n", tmp);
    ui_print("WTF?!?!?");
}

int is_path_mounted(const char* path) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        return 0;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return 0;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv) {
        // volume is already mounted
        return 1;
    }
    return 0;
}

int has_datadata() {
    Volume *vol = volume_for_path("/datadata");
    return vol != NULL;
}

int volume_main(int argc, char **argv) {
    load_volume_table();
    return 0;
}

int verify_root_and_recovery() {
    if (ensure_path_mounted("/system") != 0)
        return 0;

    int ret = 0;
    struct stat st;
    // check to see if install-recovery.sh is going to clobber recovery
    // install-recovery.sh is also used to run the su daemon on stock rom for 4.3+
    // so verify that doesn't exist...
    if (0 != lstat("/system/etc/.installed_su_daemon", &st)) {
        // check install-recovery.sh exists and is executable
        if (0 == lstat("/system/etc/install-recovery.sh", &st)) {
            if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                ui_show_text(1);
                ret = 1;
                if (confirm_selection("ROM may flash stock recovery on boot. Fix?", "Yes - Disable recovery flash")) {
                    __system("chmod -x /system/etc/install-recovery.sh");
                }
            }
        }
    }


    int exists = 0;
    if (0 == lstat("/system/bin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            if ((st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_show_text(1);
                ret = 1;
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/bin/su)")) {
                    __system("chmod 6755 /system/bin/su");
                }
            }
        }
    }

    if (0 == lstat("/system/xbin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            if ((st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_show_text(1);
                ret = 1;
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/xbin/su)")) {
                    __system("chmod 6755 /system/xbin/su");
                }
            }
        }
    }

    if (!exists) {
        ui_show_text(1);
        ret = 1;
        if (confirm_selection("Root access is missing. Root device?", "Yes - Root device (/system/xbin/su)")) {
            __system("/sbin/install-su.sh");
        }
    }

    ensure_path_unmounted("/system");
    return ret;
}
