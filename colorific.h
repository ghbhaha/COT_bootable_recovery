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

/* Everything below this has been hacked in for the purpose of custom
 * UI colors */

// Define a location for our configuration file
extern const char *UI_CONFIG_FILE;

extern int UICOLOR0, UICOLOR1, UICOLOR2, bg_icon;

extern void set_ui_color(int i);

extern void get_config_settings();

#endif
