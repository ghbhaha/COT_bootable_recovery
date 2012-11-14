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
 
#define HYDRO_UI			0
#define BLOOD_RED_UI		1
#define LLOYD_UI			2
#define CITRUS_ORANGE_UI	3
#define DOODERBUTT_BLUE_UI	4
#define EASTEREGG			5
#define CUSTOM_UI			6

extern int UI_COLOR_DEBUG;

int UICOLOR0, UICOLOR1, UICOLOR2, UITHEME;

void show_recovery_debugging_menu();
void show_settings_menu();
void show_ors_reboot_menu();
void show_ors_nandroid_prompt_menu();
void show_nandroid_prompt_menu();
void ts_calibrate();
void clear_screen();
