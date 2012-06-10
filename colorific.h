/*
 * Copyright (C) 2009 Drew Walton
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

#ifndef _COLORIFIC_H
#define _COLORIFIC_H

// Throw this in here to easier tell which UI color is which
#define HYDRO_UI		0
#define BLOOD_RED_UI		1
#define KEY_LIME_PIE_UI		2
#define CITRUS_ORANGE_UI	3
#define DOODERBUTT_BLUE_UI	4

// Define a location for our configuration file
extern const char *UI_CONFIG_FILE;

int UICOLOR0, UICOLOR1, UICOLOR2, bg_icon;

extern void set_ui_color(int i);

extern void get_config_settings();

void ui_dyn_background();

void set_bg_icon(int icon);
#endif
