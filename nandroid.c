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
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "../../external/yaffs2/yaffs2/utils/mkyaffs2image.h"
#include "../../external/yaffs2/yaffs2/utils/unyaffs.h"

#include <sys/vfs.h>

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>

#define LIMITED_SPACE 400

void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", dir);
    __system(tmp);
}


void get_android_version(const char* backup_path)
{
	char* ANDROID_VERSION;
	char* result;
	FILE * vers = fopen_path("/system/build.prop", "r");
	if (vers == NULL)
		return NULL;

	char line[512];
	while(fgets(line, sizeof(line), vers) != NULL && fgets(line, sizeof(line), vers) != EOF) { // read a line
		if (strstr(line, "ro.build.display.id") != NULL) {
			char* strptr = strstr(line, "=") + 1;
			result = calloc(strlen(strptr) + 1, sizeof(char));
			strcpy(result, strptr);
			break;
		}
	}
	fclose(vers);
	ensure_path_unmounted("/system");

	int LENGTH = strlen(result);
	int i, k, found;
	for (i=0; i<LENGTH; i++) {
		k = i;
		//Android versions follow this scheme: AAADD, A=A-Z, D=0-9, ICS has an extra digit at the end
		if (isalpha(result[k]) && isalpha(result[k++]) && isalpha(result[k++]) && isdigit(result[k++]) && isdigit(result[k++])) {
			found = 1;
			break;
		}
	}
	if (found) {
		ANDROID_VERSION = calloc(strlen(result) + 1, sizeof(char)); //allocate the length of result plus 1
		int n = 0;
		for(i-=1; i<=k; i++) {
			ANDROID_VERSION[n] = result[i];
			n++;
		}
		ANDROID_VERSION[n] = NULL;
		strcat(backup_path, ANDROID_VERSION);
	}
}

void nandroid_get_backup_path(const char* backup_path)
{
	char tmp[PATH_MAX];
	struct stat st;
	if (stat(USER_DEFINED_BACKUP_MARKER, &st) == 0) {
		FILE *file = fopen_path(USER_DEFINED_BACKUP_MARKER, "r");
		fscanf(file, "%s", &backup_path);
		fclose(file);
	} else {
		sprintf(backup_path, "%s", DEFAULT_BACKUP_PATH);
	}
}

void nandroid_generate_timestamp_path(const char* backup_path)
{
	nandroid_get_backup_path(backup_path);
	get_android_version(backup_path);
    time_t t = time(NULL);
    struct tm *bktime = localtime(&t);
    char tmp[PATH_MAX];
    if (bktime == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(tmp, "%d", tp.tv_sec);
        strcat(backup_path, "-");
        strcat(backup_path, tmp);
	} else {
		strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", bktime);
		strcat(backup_path, "-");
		strcat(backup_path, tmp);
	}
}

static int print_and_error(const char* message) {
    ui_print("%s", message);
    return 1;
}

static int yaffs_files_total = 0;
static int yaffs_files_count = 0;

static void yaffs_callback(const char* filename)
{
    if (filename == NULL)
        return;
    const char* justfile = basename(filename);
    char tmp[PATH_MAX];
    strcpy(tmp, justfile);
    if (tmp[strlen(tmp) - 1] == '\n')
        tmp[strlen(tmp) - 1] = NULL;
    if (strlen(tmp) < 30)
        ui_print("%s", tmp);
    yaffs_files_count++;
    if (yaffs_files_total != 0)
        ui_set_progress((float)yaffs_files_count / (float)yaffs_files_total);
    ui_reset_text_col();
}

static void compute_directory_stats(const char* directory)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "find %s | wc -l > /tmp/dircount", directory);
    __system(tmp);
    char count_text[100];
    FILE* f = fopen("/tmp/dircount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    yaffs_files_count = 0;
    yaffs_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

// We use CLI because it doesn't corrupt backups on thunderc varients.
static int mkyaffs2image_wrapper(const char* name, const char* backup_path) {
	char tmp[PATH_MAX];
	if (strcmp(".android_secure",name) == 0) {
		sprintf(tmp, "/sbin/mkyaffs2image '/sdcard/%s' '%s/%s.img'", name, backup_path, name);
	} else {
		sprintf(tmp, "/sbin/mkyaffs2image '/%s' '%s/%s.img'", name, backup_path, name);
	}
	return __system(tmp);
}

static int unyaffs_wrapper(const char* name, const char* backup_path) {
	char tmp[PATH_MAX];
	if (strcmp(".android_secure",name) == 0) {
		sprintf(tmp, "cd '/sdcard/%s' && /sbin/unyaffs '%s/%s.img'", name, backup_path, name);
	} else {
		sprintf(tmp, "cd '/%s' && /sbin/unyaffs '%s/%s.img'", name, backup_path, name);
	}
	return __system(tmp);
}

int nandroid_backup_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {
    int ret = 0;
    char* name = basename(mount_point);

    struct stat file_info;
    mkyaffs2image_callback callback = NULL;
    if (0 != stat("/sdcard/cotrecovery/.hidenandroidprogress", &file_info)) {
        callback = yaffs_callback;
    }

    ui_print("Backing up %s...\n", name);
    if (0 != (ret = ensure_path_mounted(mount_point) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }
    compute_directory_stats(mount_point);
    char tmp[PATH_MAX];

	ret = mkyaffs2image_wrapper(name, backup_path);

    if (umount_when_finished) {
        ensure_path_unmounted(mount_point);
    }
    if (0 != ret) {
        ui_print("Error while making a yaffs2 image of %s!\n", mount_point);
        return ret;
    }
    return 0;
}

int nandroid_backup_partition(const char* backup_path, const char* root) {
    return nandroid_backup_partition_extended(backup_path, root, 1);
}

int recalc_sdcard_space(const char* backup_path)
{
	struct statfs s;
	int ret;
	Volume* volume = volume_for_path(backup_path);
	//static char mount_point = "/sdcard/"
	if (0 != (ret = statfs(volume->mount_point, &s))) {
		return print_and_error("Can't mount sdcard.\n");
	}
	uint64_t bavail = s.f_bavail;
    uint64_t bsize = s.f_bsize;
    uint64_t sdcard_free = bavail * bsize;
    uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
    return sdcard_free_mb;
}

int nandroid_backup(const char* backup_path)
{
	char tmp[PATH_MAX];
    ui_set_background(BACKGROUND_ICON_INSTALLING);

	if (ensure_path_mounted(backup_path) != 0) {
		sprintf(tmp, "Can't mount %s.\n", backup_path);
		return print_and_error(tmp);
	}

	Volume* volume = volume_for_path(backup_path);
	if (NULL == volume) {
		sprintf(tmp, "Unable to find volume for %s.\n", backup_path);
		return print_and_error(tmp);
	}
    int ret;
    struct statfs s;
    if (0 != (ret = statfs(volume->mount_point, &s)))
        return print_and_error("Unable to stat backup path.\n");

    uint64_t sdcard_free_mb = recalc_sdcard_space(backup_path);

    ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
    if (sdcard_free_mb < LIMITED_SPACE) {
        if (show_lowspace_menu(sdcard_free_mb, backup_path) == 1) {
			return 0;
		}
	}

	ensure_directory(backup_path);

#ifndef BOARD_RECOVERY_IGNORE_BOOTABLES
    ui_print("Backing up boot...\n");
    sprintf(tmp, "%s/%s", backup_path, "boot.img");
    ret = backup_raw_partition("boot", tmp);
    if (0 != ret)
        return print_and_error("Error while dumping boot image!\n");

    ui_print("Backing up recovery...\n");
    sprintf(tmp, "%s/%s", backup_path, "recovery.img");
    ret = backup_raw_partition("recovery", tmp);
    if (0 != ret)
        return print_and_error("Error while dumping recovery image!\n");
#endif

    if (0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

	if (has_datadata()) {
	    if (0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
			return ret;
	}

    if (0 != stat("/sdcard/.android_secure", &s)) {
        ui_print("No /sdcard/.android_secure found. Skipping backup of applications on external storage.\n");
    } else {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }

    if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    Volume *vol = volume_for_path("/sd-ext");
    if (vol == NULL || 0 != stat(vol->device, &s)) {
        //ui_print("No sd-ext found. Skipping backup of sd-ext.\n");
    } else {
        if (0 != ensure_path_mounted("/sd-ext"))
            ui_print("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
        else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
            return ret;
    }

    ui_print("Generating md5 sum...\n");
    sprintf(tmp, "nandroid-md5.sh %s", backup_path);
    if (0 != (ret = __system(tmp))) {
        ui_print("Error while generating md5 sum!\n");
        return ret;
    }

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nBackup complete!\n");
    return 0;
}

int nandroid_advanced_backup(const char* backup_path, int boot, int recovery, int system, int data, int cache, int sdext)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);

	char tmp[PATH_MAX];
	if (ensure_path_mounted(backup_path) != 0) {
		sprintf(tmp, "Can't mount %s\n", backup_path);
        return print_and_error(tmp);
	}

    int ret;
    struct statfs s;
    Volume* volume = volume_for_path(backup_path);
    if (0 != (ret = statfs(volume->mount_point, &s)))
        return print_and_error("Unable to stat backup path.\n");

    uint64_t sdcard_free_mb = recalc_sdcard_space(backup_path);

    ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
    if (sdcard_free_mb < LIMITED_SPACE) {
        if (show_lowspace_menu(sdcard_free_mb, backup_path) == 1) {
			return 0;
		}
	}

	ensure_directory(backup_path);

    if (boot) {
        ui_print("Backing up boot...\n");
        sprintf(tmp, "%s/%s", backup_path, "boot.img");
        ret = backup_raw_partition("boot", tmp);
        if (0 != ret)
            return print_and_error("Error while dumping boot image!\n");
    }

    if (recovery) {
        ui_print("Backing up recovery...\n");
        sprintf(tmp, "%s/%s", backup_path, "recovery.img");
        ret = backup_raw_partition("recovery", tmp);
        if (0 != ret)
            return print_and_error("Error while dumping recovery image!\n");
    }

    if (system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    if (data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (data && has_datadata()) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    if (0 != stat("/sdcard/.android_secure", &s)) {
        ui_print("No /sdcard/.android_secure found. Skipping backup of applications on external storage.\n");
    } else {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }

    if (cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    Volume *vol = volume_for_path("/sd-ext");
    if (sdext && vol == NULL || 0 != stat(vol->device, &s)) {
        ui_print("No sd-ext found. Skipping backup of sd-ext.\n");
    } else if (sdext) {
        if (0 != ensure_path_mounted("/sd-ext"))
            ui_print("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
        else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
            return ret;
    }

    ui_print("Generating md5 sum...\n");
    sprintf(tmp, "nandroid-md5.sh %s", backup_path);
    if (0 != (ret = __system(tmp))) {
        ui_print("Error while generating md5 sum!\n");
        return ret;
    }

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nBackup complete!\n");
    return 0;
}

typedef int (*format_function)(char* root);

int nandroid_restore_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {
	int ret = 0;
	char* name = basename(mount_point);

	char tmp[PATH_MAX];
	bool istarfile = false;

	sprintf(tmp, "%s/%s.img", backup_path, name);
	struct stat file_info;
	if (0 != (ret = statfs(tmp, &file_info))) {
		if (strcmp(".android_secure", name) == 0) {
			ui_print("%s.img not found.\n", name, mount_point);
			sprintf(name,"android_secure");
			sprintf(tmp, "%s/%s.tar", backup_path, name);
			if (0 == (ret = statfs(tmp, &file_info))) {
				ui_print("Found old %s.tar!\n", name, mount_point);
				istarfile = true;
			} else {
				ui_print("%s.tar not found either Skipping restore of .%s.\n", name, name);
				return 0;
			}
		} else {
			ui_print("%s.img not found. Skipping restore of %s\n", name, mount_point);
			return 0;
		}
	}

    ensure_directory(mount_point);

    unyaffs_callback callback = NULL;
    if (0 != stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info)) {
		callback = yaffs_callback;
	}

	if (strcmp("android_secure", name) == 0) {
		ui_print("Restoring .%s...\n", name);
	} else {
		ui_print("Restoring %s...\n", name);
	}

	if (0 != (ret = format_volume(mount_point))) {
		ui_print("Error while formatting %s!\n", mount_point);
		return ret;
	}

	if (0 != (ret = ensure_path_mounted(mount_point))) {
		ui_print("Can't mount %s!\n", mount_point);
		return ret;
	}

	if (istarfile) {
		sprintf(tmp, "/sbin/busybox tar -xf '%s/%s.tar' -C '/sdcard/'", backup_path, name);
		if (0 != (ret = __system(tmp))) {
			ui_print("Error while restoring %s.tar!\n",name);
			return ret;
		}
    } else {
		ret = unyaffs_wrapper(name, backup_path);
		if (0 != ret) {
			ui_print("Error while restoring %s!\n",name);
			return ret;
		}
	}
	if (umount_when_finished) {
		ensure_path_unmounted(mount_point);
	}
	return 0;
}

int nandroid_restore_partition(const char* backup_path, const char* root) {
    return nandroid_restore_partition_extended(backup_path, root, 1);
}

int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    yaffs_files_total = 0;

	char tmp[PATH_MAX];
	if (ensure_path_mounted(backup_path) != 0) {
		sprintf(tmp, "Can't mount %s\n", backup_path);
		return print_and_error(tmp);
	}

    ui_print("Checking MD5 sums...\n");
    sprintf(tmp, "cd %s && md5sum -c nandroid.md5", backup_path);
    if (0 != __system(tmp))
        return print_and_error("MD5 mismatch!\n");

    int ret;
    struct stat st;

#ifndef BOARD_RECOVERY_IGNORE_BOOTABLES
	sprintf(tmp, "%s/boot.img", backup_path);
    if (restore_boot && (stat(tmp, &st) == 0))
    {
        ui_print("Erasing boot before restore...\n");
        if (0 != (ret = format_root_device("BOOT:")))
            return print_and_error("Error while formatting BOOT:!\n");
        sprintf(tmp, "%s/boot.img", backup_path);
        ui_print("Restoring boot image...\n");
        if (0 != (ret = restore_raw_partition("boot", tmp))) {
            ui_print("Error while flashing boot image!");
            return ret;
        }
    } else {
		ui_print("No boot image present, skipping to system...\n");
	}
#endif

    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;

	if (has_datadata()) {
		if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
			return ret;
	}

    if (restore_data && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
        return ret;

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (restore_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nRestore complete!\n");
    return 0;
}

int nandroid_usage()
{
    printf("Usage: nandroid backup\n");
    printf("Usage: nandroid restore <directory>\n");
    return 1;
}

int nandroid_main(int argc, char** argv)
{
    if (argc > 3 || argc < 2)
        return nandroid_usage();

    if (strcmp("backup", argv[1]) == 0) {
        if (argc != 2)
            return nandroid_usage();

        char backup_path[PATH_MAX];
        nandroid_generate_timestamp_path(backup_path);
        return nandroid_backup(backup_path);
    }
    if (strcmp("restore", argv[1]) == 0) {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_restore(argv[2], 1, 1, 1, 1, 1);
    }
    return nandroid_usage();
}
