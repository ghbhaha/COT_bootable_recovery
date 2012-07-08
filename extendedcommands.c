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

#include "../../external/yaffs2/yaffs2/utils/mkyaffs2image.h"
#include "../../external/yaffs2/yaffs2/utils/unyaffs.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"

#include "colorific.h"

#define ABS_MT_POSITION_X 0x35  /* Center X ellipse position */

int signature_check_enabled = 1;
int script_assert_enabled = 1;
static const char *SDCARD_UPDATE_FILE = "/sdcard/update.zip";

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
}

void toggle_script_asserts()
{
    script_assert_enabled = !script_assert_enabled;
    ui_print("Script Asserts: %s\n", script_assert_enabled ? "Enabled" : "Disabled");
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
        ui_set_background(BACKGROUND_ICON_ERROR);
        int err_i = 0;
        for ( err_i = 0; err_i < 4; err_i++ ) {
            vibrate(15);
        }
        ui_print("Installation aborted.\n");
        return 1;
    }
    ui_set_background(BACKGROUND_ICON_NONE);
    vibrate(60);
    ui_print("\nInstall from sdcard complete.\n");
    return 0;
}

int format_non_mtd_device(const char* root)
{
    // if this is SDEXT:, don't worry about it.
    if (0 == strcmp(root, "SDEXT:"))
    {
        struct stat st;
        if (0 != stat("/dev/block/mmcblk0p2", &st))
        {
            ui_print("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    char path[PATH_MAX];
    translate_root_path(root, path, PATH_MAX);
    if (0 != ensure_root_path_mounted(root))
    {
        ui_print("Error mounting %s!\n", path);
        ui_print("Skipping format...\n");
        return 0;
    }

    static char tmp[PATH_MAX];
    sprintf(tmp, "rm -rf %s/*", path);
    __system(tmp);
    sprintf(tmp, "rm -rf %s/.*", path);
    __system(tmp);

    ensure_root_path_unmounted(root);
    return 0;
}

char* INSTALL_MENU_ITEMS[] = {  "choose zip from sdcard",
                                "apply /sdcard/update.zip",
                                "toggle signature verification",
                                "toggle script asserts",
                                NULL };
#define ITEM_CHOOSE_ZIP       0
#define ITEM_APPLY_SDCARD     1
#define ITEM_SIG_CHECK        2
#define ITEM_ASSERTS          3

void show_install_update_menu()
{
    static char* headers[] = {  "Apply .zip file on SD card",
                                "",
                                NULL
    };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, INSTALL_MENU_ITEMS, 0, 0);
        switch (chosen_item)
        {
            case ITEM_ASSERTS:
                toggle_script_asserts();
                break;
            case ITEM_SIG_CHECK:
                toggle_signature_check();
                break;
            case ITEM_APPLY_SDCARD:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /sdcard/update.zip"))
                    install_zip(SDCARD_UPDATE_FILE);
                break;
            }
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu("/sdcard/");
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
                stat(fullFileName, &info);
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
            int chosen_item = get_menu_selection(headers, list, 0, 0);
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
    return return_value;
}

void show_view_and_delete_backups(const char *mount_point, const char *backup_path)
{
	if (ensure_path_mounted(mount_point) != 0) {
		LOGE("Can't mount %s\n", mount_point);
		return;
	}

	static char* headers[] = { "Choose a backup to delete",
								 "",
								 NULL
	};

	char* file = choose_file_menu(mount_point, NULL, headers);
	if(file == NULL)
		return;
	static char* confirm_delete = "Confirm delete?";
	static char confirm[PATH_MAX];
	sprintf(confirm, "Yes - Delete %s", basename(file));
	if(confirm_selection(confirm_delete, confirm)) {
		static char* tmp[PATH_MAX];
		sprintf(tmp, "rm -rf %s", file);
		__system(tmp);
		uint64_t sdcard_free_mb = recalc_sdcard_space(backup_path);
		ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
	}
}

void delete_old_backups(const char *mount_point)
{
	if (ensure_path_mounted(mount_point) != 0) {
		LOGE("Can't mount %s\n", mount_point);
		return;
	}

	static char* headers[] = { "Choose a backup to delete",
								 "",
								 NULL
	};

	char* file = choose_file_menu(mount_point, NULL, headers);
	if(file == NULL)
		return;
	static char* confirm_delete = "Confirm delete?";
	static char confirm[PATH_MAX];
	sprintf(confirm, "Yes - Delete %s", basename(file));
	if(confirm_selection(confirm_delete, confirm)) {
		static char* tmp[PATH_MAX];
        ui_print("Deleting %s\n", basename(file));
		sprintf(tmp, "rm -rf %s", file);
		__system(tmp);
        ui_print("Backup deleted!\n");
	}
}

int show_lowspace_menu(int i, const char* backup_path)
{
	static char *LOWSPACE_MENU_ITEMS[] = { "Continue with backup",
											"View and delete old backups",
											"Cancel backup",
											NULL };
	#define ITEM_CONTINUE_BACKUP 0
	#define ITEM_VIEW_DELETE_BACKUPS 1
	#define ITEM_CANCEL_BACKUP 2

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
				static char tmp;
				ui_print("Proceeding with backup.\n");
				return 0;
			}
			case ITEM_VIEW_DELETE_BACKUPS:
				show_view_and_delete_backups("/sdcard/clockworkmod/backup/",backup_path);
				break;
			default:
				ui_print("Cancelling backup.\n");
				return 1;
		}
	}
}

void show_choose_zip_menu(const char *mount_point)
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char *INSTALL_OR_BACKUP_ITEMS[] = { "Yes - Backup and install",
												 "No - Install without backup",
												 "Cancel install",
												 NULL };
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

    struct stat info;
    if (0 == stat("/sdcard/clockworkmod/.cotconfirmnandroid", &info)) {
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
					time_t t = time(NULL);
					struct tm *tmp = localtime(&t);
					if (tmp == NULL) {
						struct timeval tp;
						gettimeofday(&tp, NULL);
						sprintf(backup_path, "/sdcard/clockworkmod/backup/%d", tp.tv_sec);
					} else {
						strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
					}
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
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* headers[] = {  "Choose an image to restore",
                                "",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm restore?", "Yes - Restore"))
        nandroid_restore(file, 1, 1, 1, 1, 1);
}

#ifndef BOARD_UMS_LUNFILE
#define BOARD_UMS_LUNFILE	"/sys/devices/platform/usb_mass_storage/lun0/file"
#endif

void show_mount_usb_storage_menu()
{
    int fd;
    Volume *vol = volume_for_path("/sdcard");
    if ((fd = open(BOARD_UMS_LUNFILE, O_WRONLY)) < 0) {
        LOGE("Unable to open ums lunfile (%s)", strerror(errno));
        return -1;
    }

    if ((write(fd, vol->device, strlen(vol->device)) < 0) &&
        (!vol->device2 || (write(fd, vol->device, strlen(vol->device2)) < 0))) {
        LOGE("Unable to write to ums lunfile (%s)", strerror(errno));
        close(fd);
        return -1;
    }
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

    if ((fd = open(BOARD_UMS_LUNFILE, O_WRONLY)) < 0) {
        LOGE("Unable to open ums lunfile (%s)", strerror(errno));
        return -1;
    }

    char ch = 0;
    if (write(fd, &ch, 1) < 0) {
        LOGE("Unable to write to ums lunfile (%s)", strerror(errno));
        close(fd);
        return -1;
    }
}

int confirm_selection(const char* title, const char* confirm)
{
    struct stat info;
    if (0 == stat("/sdcard/clockworkmod/.no_confirm", &info))
        return 1;

    char* confirm_headers[]  = {  title, "  THIS CAN NOT BE UNDONE.", "", NULL };
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

#define MKE2FS_BIN      "/sbin/mke2fs"
#define TUNE2FS_BIN     "/sbin/tune2fs"
#define E2FSCK_BIN      "/sbin/e2fsck"

int format_device(const char *device, const char *path, const char *fs_type) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        // no /sdcard? let's assume /data/media
        if (strstr(path, "/sdcard") == path && is_data_media()) {
            return format_unknown_device(NULL, path, NULL);
        }
        // silent failure for sd-ext
        if (strcmp(path, "/sd-ext") == 0)
            return -1;
        LOGE("unknown volume \"%s\"\n", path);
        return -1;
    }
    if (strcmp(fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", path);
        return -1;
    }

    if (strcmp(fs_type, "rfs") == 0) {
        if (ensure_path_unmounted(path) != 0) {
            LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
            return -1;
        }
        if (0 != format_rfs_device(device, path)) {
            LOGE("format_volume: format_rfs_device failed on %s\n", device);
            return -1;
        }
        return 0;
    }

    if (strcmp(v->mount_point, path) != 0) {
        return format_unknown_device(v->device, path, NULL);
    }

    if (ensure_path_unmounted(path) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (strcmp(fs_type, "yaffs2") == 0 || strcmp(fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n",device);
            return -1;
        }
        return 0;
    }

    if (strcmp(fs_type, "ext4") == 0) {
        reset_ext4fs_info();
        int result = make_ext4fs(device, NULL, NULL, 0, 0, 0);
        if (result != 0) {
            LOGE("format_volume: make_extf4fs failed on %s\n", device);
            return -1;
        }
        return 0;
    }

    return format_unknown_device(device, path, fs_type);
}

int format_unknown_device(const char *device, const char* path, const char *fs_type)
{
    LOGI("Formatting unknown device.\n");

    if (fs_type != NULL && get_flash_type(fs_type) != UNSUPPORTED)
        return erase_raw_partition(device);

    // if this is SDEXT:, don't worry about it if it does not exist.
    if (0 == strcmp(path, "/sd-ext"))
    {
        struct stat st;
        Volume *vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != stat(vol->device, &st))
        {
            ui_print("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    if (NULL != fs_type) {
        if (strcmp("ext3", fs_type) == 0) {
            LOGI("Formatting ext3 device.\n");
            if (0 != ensure_path_unmounted(path)) {
                LOGE("Error while unmounting %s.\n", path);
                return -12;
            }
            return format_ext3_device(device);
        }

        if (strcmp("ext2", fs_type) == 0) {
            LOGI("Formatting ext2 device.\n");
            if (0 != ensure_path_unmounted(path)) {
                LOGE("Error while unmounting %s.\n", path);
                return -12;
            }
            return format_ext2_device(device);
        }
    }

    if (0 != ensure_path_mounted(path))
    {
        ui_print("Error mounting %s!\n", path);
        ui_print("Skipping format...\n");
        return 0;
    }

    static char tmp[PATH_MAX];
    if (strcmp(path, "/data") == 0) {
        sprintf(tmp, "cd /data ; for f in $(ls -a | grep -v ^media$); do rm -rf $f; done");
        __system(tmp);
    }
    else {
        sprintf(tmp, "rm -rf %s/*", path);
        __system(tmp);
        sprintf(tmp, "rm -rf %s/.*", path);
        __system(tmp);
    }

    ensure_path_unmounted(path);
    return 0;
}

//#define MOUNTABLE_COUNT 5
//#define DEVICE_COUNT 4
//#define MMC_COUNT 2

typedef struct {
    char mount[255];
    char unmount[255];
    Volume* v;
} MountMenuEntry;

typedef struct {
    char txt[255];
    Volume* v;
} FormatMenuEntry;

int is_safe_to_format(char* name)
{
    char str[255];
    char* partition;
    property_get("ro.cwm.forbid_format", str, "/misc,/radio,/bootloader,/recovery,/efs");

    partition = strtok(str, ", ");
    while (partition != NULL) {
        if (strcmp(name, partition) == 0) {
            return 0;
        }
        partition = strtok(NULL, ", ");
    }

    return 1;
}

#define MOUNTABLE_COUNT 5
#define MTD_COUNT 4
#define MMC_COUNT 2

void show_partition_menu()
{
    static char* headers[] = {  "Mounts and Storage Menu",
                                "",
                                NULL
    };

    typedef char* string;
    string mounts[MOUNTABLE_COUNT][3] = {
        { "mount /system", "unmount /system", "SYSTEM:" },
        { "mount /data", "unmount /data", "DATA:" },
        { "mount /cache", "unmount /cache", "CACHE:" },
        { "mount /sdcard", "unmount /sdcard", "SDCARD:" },
        { "mount /sd-ext", "unmount /sd-ext", "SDEXT:" }
        };

    string mtds[MTD_COUNT][2] = {
        { "format boot", "BOOT:" },
        { "format system", "SYSTEM:" },
        { "format data", "DATA:" },
        { "format cache", "CACHE:" },
    };

    string mmcs[MMC_COUNT][3] = {
      { "format sdcard", "SDCARD:" },
      { "format sd-ext", "SDEXT:" }
    };

    static char* confirm_format  = "Confirm format?";
    static char* confirm = "Yes - Format";

    for (;;)
    {
        int ismounted[MOUNTABLE_COUNT];
        int i;
        static string options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT + 1 + 1]; // mountables, format mtds, format mmcs, usb storage, null
        for (i = 0; i < MOUNTABLE_COUNT; i++)
        {
            ismounted[i] = is_root_path_mounted(mounts[i][2]);
            options[i] = ismounted[i] ? mounts[i][1] : mounts[i][0];
        }

        for (i = 0; i < MTD_COUNT; i++)
        {
            options[MOUNTABLE_COUNT + i] = mtds[i][0];
        }

        for (i = 0; i < MMC_COUNT; i++)
        {
            options[MOUNTABLE_COUNT + MTD_COUNT + i] = mmcs[i][0];
        }

        options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT] = "mount USB storage";
        options[MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT + 1] = NULL;

        int chosen_item = get_menu_selection(headers, &options, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        if (chosen_item == MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT)
        {
            show_mount_usb_storage_menu();
        }
        else if (chosen_item < MOUNTABLE_COUNT)
        {
            if (ismounted[chosen_item])
            {
                if (0 != ensure_root_path_unmounted(mounts[chosen_item][2]))
                    ui_print("Error unmounting %s!\n", mounts[chosen_item][2]);
            }
            else
            {
                if (0 != ensure_root_path_mounted(mounts[chosen_item][2]))
                    ui_print("Error mounting %s!\n", mounts[chosen_item][2]);
            }
        }
        else if (chosen_item < MOUNTABLE_COUNT + MTD_COUNT)
        {
            chosen_item = chosen_item - MOUNTABLE_COUNT;
            if (!confirm_selection(confirm_format, confirm))
                continue;
            ui_print("Formatting %s...\n", mtds[chosen_item][1]);
            if (0 != format_root_device(mtds[chosen_item][1]))
                ui_print("Error formatting %s!\n", mtds[chosen_item][1]);
            else
                ui_print("Done.\n");
        }
        else if (chosen_item < MOUNTABLE_COUNT + MTD_COUNT + MMC_COUNT)
        {
            chosen_item = chosen_item - MOUNTABLE_COUNT - MTD_COUNT;
            if (!confirm_selection(confirm_format, confirm))
                continue;
            ui_print("Formatting %s...\n", mmcs[chosen_item][1]);
            if (0 != format_non_mtd_device(mmcs[chosen_item][1]))
                ui_print("Error formatting %s!\n", mmcs[chosen_item][1]);
            else
                ui_print("Done.\n");
        }
    }
}

void show_nandroid_advanced_backup_menu(){
    static char* advancedheaders[] = { "Choose the partitions to backup.",
                                        "",
					NULL
    };

    int backup_list [6];
    char* list[7];

    backup_list[0] = 1;
    backup_list[1] = 1;
    backup_list[2] = 1;
    backup_list[3] = 1;
    backup_list[4] = 1;
    backup_list[5] = 1;



    list[6] = "Perform Backup";
    list[7] = NULL;

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

	    if (backup_list[5] == 1)
	    	list[5] = "Backup sd-ext: Yes";
	    else
	    	list[5] = "Backup sd-ext: No";

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
	    case 5: backup_list[5] = !backup_list[5];
		    break;

	    case 6: cont = 0;
	    	    break;
	}
    }

    char backup_path[PATH_MAX];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL){
	struct timeval tp;
	gettimeofday(&tp, NULL);
	sprintf(backup_path, "/sdcard/clockworkmod/backup/%d", tp.tv_sec);
   }else{
	strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
   }

   return nandroid_advanced_backup(backup_path, backup_list[0], backup_list[1], backup_list[2], backup_list[3], backup_list[4], backup_list[5]);
}

void show_nandroid_advanced_restore_menu(const char* path)
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* advancedheaders[] = {  "Choose an image to restore",
                                "",
                                "Choose an image to restore",
                                "first. The next menu will",
                                "you more options.",
                                "",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, advancedheaders);
    if (file == NULL)
        return;

    static char* headers[] = {  "Nandroid Advanced Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Restore boot",
                            "Restore system",
                            "Restore data",
                            "Restore cache",
                            "Restore sd-ext",
                            NULL
    };

    static char* confirm_restore  = "Confirm restore?";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            if (confirm_selection(confirm_restore, "Yes - Restore boot"))
                nandroid_restore(file, 1, 0, 0, 0, 0);
            break;
        case 1:
            if (confirm_selection(confirm_restore, "Yes - Restore system"))
                nandroid_restore(file, 0, 1, 0, 0, 0);
            break;
        case 2:
            if (confirm_selection(confirm_restore, "Yes - Restore data"))
                nandroid_restore(file, 0, 0, 1, 0, 0);
            break;
        case 3:
            if (confirm_selection(confirm_restore, "Yes - Restore cache"))
                nandroid_restore(file, 0, 0, 0, 1, 0);
            break;
        case 4:
            if (confirm_selection(confirm_restore, "Yes - Restore sd-ext"))
                nandroid_restore(file, 0, 0, 0, 0, 1);
            break;
    }
}

void show_nandroid_menu()
{
    static char* headers[] = {  "Nandroid",
                                "",
                                NULL
    };

    static char* list[] = { "backup",
                            "restore",
                            "advanced backup",
                            "advanced restore",
                            "delete old backups",
                            NULL
    };

	for (;;) {
		int chosen_item = get_menu_selection(headers, list, 0, 0);
		switch (chosen_item)
		{
			case 0:
				{
					char backup_path[PATH_MAX];
					time_t t = time(NULL);
					struct tm *tmp = localtime(&t);
					if (tmp == NULL)
					{
						struct timeval tp;
						gettimeofday(&tp, NULL);
						sprintf(backup_path, "/sdcard/clockworkmod/backup/%d", tp.tv_sec);
					}
					else
					{
						strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
					}
					nandroid_backup(backup_path);
				}
				return;
			case 1:
				show_nandroid_restore_menu("/sdcard");
				return;
			case 2:
				show_nandroid_advanced_backup_menu();
				return;
			case 3:
				show_nandroid_advanced_restore_menu("/sdcard");
				return;
			case 4:
				delete_old_backups("/sdcard/clockworkmod/backup/");
				break;
			default:
				return;
		}
	}
}

void show_advanced_debugging_menu()
{
	static char* headers[] = { "Debugging Options",
								"",
								NULL
	};

	static char* list[] = { "Fix Permissions",
							"Fix Recovery Boot Loop",
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
			{
				format_root_device("MISC:");
				format_root_device("PERSIST:");
				reboot(RB_AUTOBOOT);
				break;
			}
			case 2:
				handle_failure(1);
				break;
			case 3:
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
			case 4:
				ui_printlogtail(12);
				break;
			case 5:
				toggle_ui_debugging();
				break;
		}
	}
}

void show_advanced_menu()
{
    static char* headers[] = {  "Advanced Options",
                                "",
                                NULL
    };

    static char* list[] = { "Reboot Recovery",
                            "Wipe Dalvik Cache",
#ifndef BOARD_HAS_SMALL_RECOVERY
							"Partition SD Card",
#endif
							"Set UI Color",
							"Debugging Options",
                            NULL
    };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                reboot_wrapper("recovery");
                break;
            case 1:
            {
                if (0 != ensure_path_mounted("/data"))
                    break;
                ensure_path_mounted("/sd-ext");
                ensure_path_mounted("/cache");
                if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
                    __system("rm -r /data/dalvik-cache");
                    __system("rm -r /cache/dalvik-cache");
                    __system("rm -r /sd-ext/dalvik-cache");
                    ui_print("Dalvik Cache wiped.\n");
                }
                ensure_path_unmounted("/data");
                break;
            }
            case 2:
            {
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
                    continue;

                int swap_size = get_menu_selection(swap_headers, swap_sizes, 0, 0);
                if (swap_size == GO_BACK)
                    continue;

                char sddevice[256];
                Volume *vol = volume_for_path("/sdcard");
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
                break;
			}
			case 3:
			{
				static char* ui_colors[] = {"Hydro (default)",
											"Blood Red",
											"Key Lime Pie",
											"Citrus Orange",
											"Dooderbutt Blue",
											NULL
				};
				static char* ui_header[] = {"UI Color", "", NULL};

				int ui_color = get_menu_selection(ui_header, ui_colors, 0, 0);
				if(ui_color == GO_BACK)
					continue;
				else {
					set_ui_color(ui_color);
					ui_reset_icons();
					break;
				}
			}
			case 4:
				show_advanced_debugging_menu();
				break;
			default:
				return;
        }
    }
}

void write_fstab_root(char *root_path, FILE *file)
{
    RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        LOGW("Unable to get root info for %s during fstab generation!", root_path);
        return;
    }
    char device[PATH_MAX];
    int ret = get_root_partition_device(root_path, device);
    if (ret == 0)
    {
        fprintf(file, "%s ", device);
    }
    else
    {
        fprintf(file, "%s ", info->device);
    }

    fprintf(file, "%s ", info->mount_point);
    fprintf(file, "%s %s\n", info->filesystem, info->filesystem_options == NULL ? "rw" : info->filesystem_options);
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
    write_fstab_root("CACHE:", file);
    write_fstab_root("DATA:", file);
#ifdef BOARD_HAS_DATADATA
    write_fstab_root("DATADATA:", file);
#endif
    write_fstab_root("SYSTEM:", file);
    write_fstab_root("SDCARD:", file);
    write_fstab_root("SDEXT:", file);
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
    sprintf(backup_path, "/sdcard/clockworkmod/backup/%s", backup_name);

    ui_set_show_text(1);
    ui_print("Filesystems need to be converted to ext4.\n");
    ui_print("A backup and restore will now take place.\n");
    ui_print("If anything goes wrong, your backup will be\n");
    ui_print("named %s. Try restoring it\n", backup_name);
    ui_print("in case of error.\n");

    nandroid_backup(backup_path);
    //nandroid_restore(backup_path, 1, 1, 1, 1, 1, 0);
    ui_set_show_text(0);
}

void handle_failure(int ret)
{
    if (ret == 0)
        return;
    if (0 != ensure_path_mounted("/sdcard"))
        return;
    mkdir("/sdcard/clockworkmod", S_IRWXU);
    __system("cp /tmp/recovery.log /sdcard/clockworkmod/recovery.log");
    ui_print("/tmp/recovery.log was copied to /sdcard/clockworkmod/recovery.log. Please open ROM Manager to report the issue.\n");
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
