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
 
int erase_volume(const char *volume);

void wipe_data(int confirm);

void erase_cache(int orscallback);

void erase_dalvik_cache(int orscallback);

void wipe_all(int orscallback);

int format_device(const char *device, const char *path, const char *fs_type);

int format_unknown_device(const char *device, const char* path, const char *fs_type);

int is_safe_to_format(char* name);

void show_partition_menu();


