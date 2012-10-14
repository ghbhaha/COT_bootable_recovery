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

extern char *COTSETTINGS;

void create_default_settings();
void parse_settings();
void handle_theme(char * theme_name);

int settings_handler(void* user, const char* section, const char* name, const char* value);
int theme_handler(void* user, const char* section, const char* name, const char* value);
int themename_handler(void* user, const char* section, const char* name, const char* value);

extern int fallback_settings;

extern int backupprompt;
extern int orswipeprompt;
extern int orsreboot;
extern int signature_check_enabled;
extern int is_sd_theme;
extern int first_boot;
extern int backupfmt;
extern char* currenttheme;
extern char* language;
extern char* themename;

void update_cot_settings(void);
void show_welcome_text();
