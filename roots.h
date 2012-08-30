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

#ifndef RECOVERY_ROOTS_H_
#define RECOVERY_ROOTS_H_

#include "common.h"
#include "minzip/Zip.h"
#include "flashutils/flashutils.h"
#include "mtdutils/mtdutils.h"
#include "mmcutils/mmcutils.h"

#ifndef BOARD_USES_MMCUTILS
#define DEFAULT_FILESYSTEM "yaffs2"
#else
#define DEFAULT_FILESYSTEM "ext3"
#endif

#ifndef BOARD_SDCARD_DEVICE_PRIMARY
#define BOARD_SDCARD_DEVICE_PRIMARY "/dev/block/mmcblk0p1"
#endif

#ifndef BOARD_SDCARD_DEVICE_SECONDARY
#define BOARD_SDCARD_DEVICE_SECONDARY "/dev/block/mmcblk0"
#endif

#ifndef BOARD_SDCARD_DEVICE_INTERNAL
#define BOARD_SDCARD_DEVICE_INTERNAL g_default_device
#endif

#ifndef BOARD_SDEXT_DEVICE
#define BOARD_SDEXT_DEVICE "/dev/block/mmcblk0p2"
#endif

#ifndef BOARD_SDEXT_FILESYSTEM
#define BOARD_SDEXT_FILESYSTEM "auto"
#endif

#ifndef BOARD_DATA_DEVICE
#define BOARD_DATA_DEVICE g_default_device
#endif

#ifndef BOARD_DATA_FILESYSTEM
#define BOARD_DATA_FILESYSTEM DEFAULT_FILESYSTEM
#endif

#ifndef BOARD_DATADATA_DEVICE
#define BOARD_DATADATA_DEVICE g_default_device
#endif

#ifndef BOARD_DATADATA_FILESYSTEM
#define BOARD_DATADATA_FILESYSTEM DEFAULT_FILESYSTEM
#endif

#ifndef BOARD_CACHE_DEVICE
#define BOARD_CACHE_DEVICE g_default_device
#endif

#ifndef BOARD_CACHE_FILESYSTEM
#define BOARD_CACHE_FILESYSTEM DEFAULT_FILESYSTEM
#endif

#ifndef BOARD_SYSTEM_DEVICE
#define BOARD_SYSTEM_DEVICE g_default_device
#endif

#ifndef BOARD_SYSTEM_FILESYSTEM
#define BOARD_SYSTEM_FILESYSTEM DEFAULT_FILESYSTEM
#endif

#ifndef BOARD_DATA_FILESYSTEM_OPTIONS
#define BOARD_DATA_FILESYSTEM_OPTIONS NULL
#endif

#ifndef BOARD_CACHE_FILESYSTEM_OPTIONS
#define BOARD_CACHE_FILESYSTEM_OPTIONS NULL
#endif

#ifndef BOARD_DATADATA_FILESYSTEM_OPTIONS
#define BOARD_DATADATA_FILESYSTEM_OPTIONS NULL
#endif

#ifndef BOARD_SYSTEM_FILESYSTEM_OPTIONS
#define BOARD_SYSTEM_FILESYSTEM_OPTIONS NULL
#endif

/* Any of the "root_path" arguments can be paths with relative
 * components, like "SYSTEM:a/b/c".
 */

/* Associate this package with the package root "PKG:".
 */
int register_package_root(const ZipArchive *package, const char *package_path);

/* Returns non-zero iff root_path points inside a package.
 */
int is_package_root_path(const char *root_path);

/* Takes a string like "SYSTEM:lib" and turns it into a string
 * like "/system/lib".  The translated path is put in out_buf,
 * and out_buf is returned if the translation succeeded.
 */
const char *translate_root_path(const char *root_path,
        char *out_buf, size_t out_buf_len);

// Load and parse volume data from /etc/recovery.fstab.
void load_volume_table();

// Return the Volume* record for this path (or NULL).
Volume* volume_for_path(const char* path);

// Make sure that the volume 'path' is on is mounted.  Returns 0 on
// success (volume is mounted).
int ensure_path_mounted(const char* path);
int ensure_path_mounted_at_mount_point(const char* path, const char* mount_point);

// Make sure that the volume 'path' is on is mounted.  Returns 0 on
// success (volume is unmounted);
int ensure_path_unmounted(const char* path);

// Reformat the given volume (must be the mount point only, eg
// "/cache"), no paths permitted.  Attempts to unmount the volume if
// it is mounted.
int format_volume(const char* volume);

int get_num_volumes();

Volume* get_device_volumes();

int is_data_media();
void setup_data_media();
int is_data_media_volume_path(const char* path);

/* "root" must be the exact name of the root; no relative path is permitted.
 * If the named root is mounted, this will attempt to unmount it first.
 */
int format_root_device(const char *root);

typedef struct {
    const char *name;
    const char *device;
    const char *device2;  // If the first one doesn't work (may be NULL)
    const char *partition_name;
    const char *mount_point;
    const char *filesystem;
    const char *filesystem_options;
} RootInfo;

#endif  // RECOVERY_ROOTS_H_
