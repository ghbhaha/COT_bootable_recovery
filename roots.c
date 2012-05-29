/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "mtdutils/mtdutils.h"
#include "mounts.h"
#include "roots.h"
#include "common.h"
#include "make_ext4fs.h"

#include "flashutils/flashutils.h"
#include "extendedcommands.h"
#include "extracommands.h"

/* Canonical pointers.
xxx may just want to use enums
 */
static const char g_default_device[] = "@\0g_default_device";
static const char g_mtd_device[] = "@\0g_mtd_device";
static const char g_raw[] = "@\0g_raw";
static const char g_package_file[] = "@\0g_package_file";

static RootInfo g_roots[] = {
    { "BOOT:", g_default_device, NULL, "boot", NULL, g_raw, NULL },
    { "CACHE:", BOARD_CACHE_DEVICE, NULL, "cache", "/cache", BOARD_CACHE_FILESYSTEM, BOARD_CACHE_FILESYSTEM_OPTIONS },
    { "DATA:", BOARD_DATA_DEVICE, NULL, "userdata", "/data", BOARD_DATA_FILESYSTEM, BOARD_DATA_FILESYSTEM_OPTIONS },
#ifdef BOARD_HAS_DATADATA
    { "DATADATA:", BOARD_DATADATA_DEVICE, NULL, "datadata", "/datadata", BOARD_DATADATA_FILESYSTEM, BOARD_DATADATA_FILESYSTEM_OPTIONS },
#endif
    { "MISC:", g_default_device, NULL, "misc", NULL, g_raw, NULL },
    { "PERSIST:", g_default_device, NULL, "persist", NULL, g_raw, NULL },
    { "PACKAGE:", NULL, NULL, NULL, NULL, g_package_file, NULL },
    { "RECOVERY:", g_default_device, NULL, "recovery", "/", g_raw, NULL },
    { "SDCARD:", BOARD_SDCARD_DEVICE_PRIMARY, BOARD_SDCARD_DEVICE_SECONDARY, NULL, "/sdcard", "vfat", NULL },
#ifdef BOARD_HAS_SDCARD_INTERNAL
    { "SDINTERNAL:", BOARD_SDCARD_DEVICE_INTERNAL, NULL, NULL, "/emmc", "vfat", NULL },
#endif
    { "SDEXT:", BOARD_SDEXT_DEVICE, NULL, NULL, "/sd-ext", BOARD_SDEXT_FILESYSTEM, NULL },
    { "SYSTEM:", BOARD_SYSTEM_DEVICE, NULL, "system", "/system", BOARD_SYSTEM_FILESYSTEM, BOARD_SYSTEM_FILESYSTEM_OPTIONS },
    { "MBM:", g_default_device, NULL, "mbm", NULL, g_raw, NULL },
    { "TMP:", NULL, NULL, NULL, "/tmp", NULL, NULL },
};
#define NUM_ROOTS (sizeof(g_roots) / sizeof(g_roots[0]))

// TODO: for SDCARD:, try /dev/block/mmcblk0 if mmcblk0p1 fails

const RootInfo *
get_root_info_for_path(const char *root_path)
{
    const char *c;

    /* Find the first colon.
     */
    c = root_path;
    while (*c != '\0' && *c != ':') {
        c++;
    }
    if (*c == '\0') {
        return NULL;
    }
    size_t len = c - root_path + 1;
    size_t i;
    for (i = 0; i < NUM_ROOTS; i++) {
        RootInfo *info = &g_roots[i];
        if (strncmp(info->name, root_path, len) == 0) {
            return info;
        }
    }
    return NULL;
}

static const ZipArchive *g_package = NULL;
static char *g_package_path = NULL;

int
register_package_root(const ZipArchive *package, const char *package_path)
{
    if (package != NULL) {
        package_path = strdup(package_path);
        if (package_path == NULL) {
            return -1;
        }
        g_package_path = (char *)package_path;
    } else {
        free(g_package_path);
        g_package_path = NULL;
    }
    g_package = package;
    return 0;
}

int
is_package_root_path(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    return info != NULL && info->filesystem == g_package_file;
}

const char *
translate_package_root_path(const char *root_path,
        char *out_buf, size_t out_buf_len, const ZipArchive **out_package)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->filesystem != g_package_file) {
        return NULL;
    }

    /* Strip the package root off of the path.
     */
    size_t root_len = strlen(info->name);
    root_path += root_len;
    size_t root_path_len = strlen(root_path);

    if (out_buf_len < root_path_len + 1) {
        return NULL;
    }
    strcpy(out_buf, root_path);
    *out_package = g_package;
    return out_buf;
}

/* Takes a string like "SYSTEM:lib" and turns it into a string
 * like "/system/lib".  The translated path is put in out_buf,
 * and out_buf is returned if the translation succeeded.
 */
const char *
translate_root_path(const char *root_path, char *out_buf, size_t out_buf_len)
{
    if (out_buf_len < 1) {
        return NULL;
    }

    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->mount_point == NULL) {
        return NULL;
    }

    /* Find the relative part of the non-root part of the path.
     */
    root_path += strlen(info->name);  // strip off the "root:"
    while (*root_path != '\0' && *root_path == '/') {
        root_path++;
    }

    size_t mp_len = strlen(info->mount_point);
    size_t rp_len = strlen(root_path);
    if (mp_len + 1 + rp_len + 1 > out_buf_len) {
        return NULL;
    }

    /* Glue the mount point to the relative part of the path.
     */
    memcpy(out_buf, info->mount_point, mp_len);
    if (out_buf[mp_len - 1] != '/') out_buf[mp_len++] = '/';

    memcpy(out_buf + mp_len, root_path, rp_len);
    out_buf[mp_len + rp_len] = '\0';

    return out_buf;
}

static int
internal_root_mounted(const RootInfo *info)
{
    if (info->mount_point == NULL) {
        return -1;
    }
//xxx if TMP: (or similar) just say "yes"

    /* See if this root is already mounted.
     */
    int ret = scan_mounted_volumes();
    if (ret < 0) {
        return ret;
    }
    const MountedVolume *volume;
    volume = find_mounted_volume_by_mount_point(info->mount_point);
    if (volume != NULL) {
        /* It's already mounted.
         */
        return 0;
    }
    return -1;
}

int
is_root_path_mounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        return -1;
    }
    return internal_root_mounted(info) >= 0;
}

static int mount_internal(const char* device, const char* mount_point, const char* filesystem, const char* filesystem_options)
{
    if (strcmp(filesystem, "auto") != 0 && filesystem_options == NULL){
        return mount(device, mount_point, filesystem, MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
    } else {
        char mount_cmd[PATH_MAX];
        const char* options = filesystem_options == NULL ? "noatime,nodiratime,nodev" : filesystem_options;
        sprintf(mount_cmd, "mount -t %s -o%s %s %s", filesystem, options, device, mount_point);
        return __system(mount_cmd);
    }

}

int
ensure_root_path_mounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
	ui_print(root_path, "\n");
	ui_print("Info = NULL\n");
        return -1;
    }

    int ret = internal_root_mounted(info);
    if (ret >= 0) {
        /* It's already mounted.
         */
        return 0;
    }

    /* It's not mounted.
     */
    if (info->device == NULL || info->mount_point == NULL ||
        info->filesystem == NULL ||
        info->filesystem == g_raw ||
        info->filesystem == g_package_file) {
	ui_print("Nothing works here\n");
        return -1;
    }

    if (info->device == g_default_device) {
        if (info->partition_name == NULL) {
	    ui_print("Partition name = NULL\n");
            return -1;
        }
        return mount_partition(info->partition_name, info->mount_point, info->filesystem, 0);
    }

    mkdir(info->mount_point, 0755);  // in case it doesn't already exist
    if (mount_internal(info->device, info->mount_point, info->filesystem, info->filesystem_options)) {
        if (info->device2 == NULL) {
	    ui_print("First cant mount\n");
            LOGE("Can't mount %s\n(%s)\n", info->device, strerror(errno));
            return -1;
        } else if (mount(info->device2, info->mount_point, info->filesystem,
                MS_NOATIME | MS_NODEV | MS_NODIRATIME, "")) {
	    ui_print("Second cant mount\n");
            LOGE("Can't mount %s (or %s)\n(%s)\n",
                    info->device, info->device2, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int
ensure_root_path_unmounted(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL) {
        return -1;
    }
    if (info->mount_point == NULL) {
        /* This root can't be mounted, so by definition it isn't.
         */
        return 0;
    }
//xxx if TMP: (or similar) just return error

    /* See if this root is already mounted.
     */
    int ret = scan_mounted_volumes();
    if (ret < 0) {
        return ret;
    }
    const MountedVolume *volume;
    volume = find_mounted_volume_by_mount_point(info->mount_point);
    if (volume == NULL) {
        /* It's not mounted.
         */
        return 0;
    }

    return unmount_mounted_volume(volume);
}

int
get_root_partition_device(const char *root_path, char *device)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL)
    {
        return NULL;
    }
    if (info->device == g_default_device)
        return get_partition_device(info->partition_name, device);
    return info->device;
}

const MtdPartition *
get_root_mtd_partition(const char *root_path)
{
    const RootInfo *info = get_root_info_for_path(root_path);
    if (info == NULL || info->device != g_mtd_device ||
            info->partition_name == NULL)
    {
        return NULL;
    }
    mtd_scan_partitions();
    return mtd_find_partition_by_name(info->partition_name);
}

int
format_root_device(const char *root)
{
    /* Be a little safer here; require that "root" is just
     * a device with no relative path after it.
     */
    const char *c = root;
    while (*c != '\0' && *c != ':') {
        c++;
    }
    /*
    if (c[0] != ':' || c[1] != '\0') {
        LOGW("format_root_device: bad root name \"%s\"\n", root);
        return -1;
    }
    */

    const RootInfo *info = get_root_info_for_path(root);
    if (info == NULL || info->device == NULL) {
        LOGW("format_root_device: can't resolve \"%s\"\n", root);
        return -1;
    }

    if (info->mount_point != NULL && info->device == g_default_device) {
        /* Don't try to format a mounted device.
         */
        int ret = ensure_root_path_unmounted(root);
        if (ret < 0) {
            LOGW("format_root_device: can't unmount \"%s\"\n", root);
            return ret;
        }
    }

    /* Format the device.
     */
    if (info->device == g_default_device) {
        int ret = 0;
        if (info->filesystem == g_raw)
            ret = erase_raw_partition(info->partition_name);
        else
            ret = erase_partition(info->partition_name, info->filesystem);
        
        if (ret != 0)
            LOGE("Error erasing device %s\n", info->device);
        return ret;
    }

    return format_non_mtd_device(root);
}

int num_volumes;
Volume* device_volumes;

int get_num_volumes() {
    return num_volumes;
}

Volume* get_device_volumes() {
    return device_volumes;
}

static int is_null(const char* sz) {
    if (sz == NULL)
        return 1;
    if (strcmp("NULL", sz) == 0)
        return 1;
    return 0;
}

static char* dupe_string(const char* sz) {
    if (is_null(sz))
        return NULL;
    return strdup(sz);
}

void load_volume_table() {
    int alloc = 2;
    device_volumes = malloc(alloc * sizeof(Volume));

    // Insert an entry for /tmp, which is the ramdisk and is always mounted.
    device_volumes[0].mount_point = "/tmp";
    device_volumes[0].fs_type = "ramdisk";
    device_volumes[0].device = NULL;
    device_volumes[0].device2 = NULL;
    device_volumes[0].fs_options = NULL;
    device_volumes[0].fs_options2 = NULL;
    num_volumes = 1;

    FILE* fstab = fopen("/etc/fstab", "r");
    if (fstab == NULL) {
        LOGE("failed to open /etc/fstab (%s)\n", strerror(errno));
        return;
    }

    char buffer[1024];
    int i;
    while (fgets(buffer, sizeof(buffer)-1, fstab)) {
        for (i = 0; buffer[i] && isspace(buffer[i]); ++i);
        if (buffer[i] == '\0' || buffer[i] == '#') continue;

        char* original = strdup(buffer);

        char* mount_point = strtok(buffer+i, " \t\n");
        char* fs_type = strtok(NULL, " \t\n");
        char* device = strtok(NULL, " \t\n");
        // lines may optionally have a second device, to use if
        // mounting the first one fails.
        char* device2 = strtok(NULL, " \t\n");
        char* fs_type2 = strtok(NULL, " \t\n");
        char* fs_options = strtok(NULL, " \t\n");
        char* fs_options2 = strtok(NULL, " \t\n");

        if (mount_point && fs_type && device) {
            while (num_volumes >= alloc) {
                alloc *= 2;
                device_volumes = realloc(device_volumes, alloc*sizeof(Volume));
            }
            device_volumes[num_volumes].mount_point = strdup(mount_point);
            device_volumes[num_volumes].fs_type = !is_null(fs_type2) ? strdup(fs_type2) : strdup(fs_type);
            device_volumes[num_volumes].device = strdup(device);
            device_volumes[num_volumes].device2 =
                !is_null(device2) ? strdup(device2) : NULL;
            device_volumes[num_volumes].fs_type2 = !is_null(fs_type2) ? strdup(fs_type) : NULL;

            if (!is_null(fs_type2)) {
                device_volumes[num_volumes].fs_options2 = dupe_string(fs_options);
                device_volumes[num_volumes].fs_options = dupe_string(fs_options2);
            }
            else {
                device_volumes[num_volumes].fs_options2 = NULL;
                device_volumes[num_volumes].fs_options = dupe_string(fs_options);
            }
            ++num_volumes;
        } else {
            LOGE("skipping malformed recovery.fstab line: %s\n", original);
        }
        free(original);
    }

    fclose(fstab);

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = &device_volumes[i];
        printf("  %d %s %s %s %s\n", i, v->mount_point, v->fs_type,
               v->device, v->device2);
    }
    printf("\n");
}

Volume* volume_for_path(const char* path) {
    int i;
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = device_volumes+i;
        int len = strlen(v->mount_point);
        if (strncmp(path, v->mount_point, len) == 0 &&
            (path[len] == '\0' || path[len] == '/')) {
            return v;
        }
    }
    return NULL;
}

int try_mount(const char* device, const char* mount_point, const char* fs_type, const char* fs_options) {
    if (device == NULL || mount_point == NULL || fs_type == NULL)
        return -1;
    int ret = 0;
    if (fs_options == NULL) {
        ret = mount(device, mount_point, fs_type,
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
    }
    else {
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount -t %s -o%s %s %s", fs_type, fs_options, device, mount_point);
        ret = __system(mount_cmd);
    }
    if (ret == 0)
        return 0;
    LOGW("failed to mount %s (%s)\n", device, strerror(errno));
    return ret;
}

int is_data_media() {
    Volume *data = volume_for_path("/data");
    return data != NULL && strcmp(data->fs_type, "auto") == 0 && volume_for_path("/sdcard") == NULL;
}

void setup_data_media() {
    rmdir("/sdcard");
    mkdir("/data/media", 0755);
    symlink("/data/media", "/sdcard");
}

int ensure_path_mounted(const char* path) {
    return ensure_path_mounted_at_mount_point(path, NULL);
}

int ensure_path_mounted_at_mount_point(const char* path, const char* mount_point) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        // no /sdcard? let's assume /data/media
        if (strstr(path, "/sdcard") == path && is_data_media()) {
            LOGW("using /data/media, no /sdcard found.\n");
            int ret;
            if (0 != (ret = ensure_path_mounted("/data")))
                return ret;
            setup_data_media();
            return 0;
        }
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    if (NULL == mount_point)
        mount_point = v->mount_point;

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(mount_point);
    if (mv) {
        // volume is already mounted
        return 0;
    }

    mkdir(mount_point, 0755);  // in case it doesn't already exist

    if (strcmp(v->fs_type, "yaffs2") == 0) {
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->device);
        if (partition == NULL) {
            LOGE("failed to find \"%s\" partition to mount at \"%s\"\n",
                 v->device, mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, mount_point, v->fs_type, 0);
    } else if (strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "ext3") == 0 ||
               strcmp(v->fs_type, "rfs") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        if ((result = try_mount(v->device, mount_point, v->fs_type, v->fs_options)) == 0)
            return 0;
        if ((result = try_mount(v->device2, mount_point, v->fs_type, v->fs_options)) == 0)
            return 0;
        if ((result = try_mount(v->device, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        if ((result = try_mount(v->device2, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        return result;
    } else {
        // let's try mounting with the mount binary and hope for the best.
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount %s", path);
        return __system(mount_cmd);
    }

    LOGE("unknown fs_type \"%s\" for %s\n", v->fs_type, mount_point);
    return -1;
}

int ensure_path_unmounted(const char* path) {
    // if we are using /data/media, do not ever unmount volumes /data or /sdcard
    if (volume_for_path("/sdcard") == NULL && (strstr(path, "/sdcard") == path || strstr(path, "/data") == path)) {
        return 0;
    }

    Volume* v = volume_for_path(path);
    if (v == NULL) {
        // no /sdcard? let's assume /data/media
        if (strstr(path, "/sdcard") == path && is_data_media()) {
            return ensure_path_unmounted("/data");
        }
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    return unmount_mounted_volume(mv);
}

int format_volume(const char* volume) {
    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        // no /sdcard? let's assume /data/media
        if (strstr(volume, "/sdcard") == volume && is_data_media()) {
            return format_unknown_device(NULL, volume, NULL);
        }
        // silent failure for sd-ext
        if (strcmp(volume, "/sd-ext") == 0)
            return -1;
        LOGE("unknown volume \"%s\"\n", volume);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", volume);
        return -1;
    }
    if (strcmp(v->mount_point, volume) != 0) {
#if 0
        LOGE("can't give path \"%s\" to format_volume\n", volume);
        return -1;
#endif
        return format_unknown_device(v->device, volume, NULL);
    }

    if (ensure_path_unmounted(volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (strcmp(v->fs_type, "yaffs2") == 0 || strcmp(v->fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(v->device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", v->device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", v->device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", v->device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n", v->device);
            return -1;
        }
        return 0;
    }

    if (strcmp(v->fs_type, "ext4") == 0) {
        reset_ext4fs_info();
        int result = make_ext4fs(v->device, NULL, NULL, 0, 0, 0);
        if (result != 0) {
            LOGE("format_volume: make_extf4fs failed on %s\n", v->device);
            return -1;
        }
        return 0;
    }

#if 0
    LOGE("format_volume: fs_type \"%s\" unsupported\n", v->fs_type);
    return -1;
#endif
    return format_unknown_device(v->device, volume, v->fs_type);
}
