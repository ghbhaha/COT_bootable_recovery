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

#include "settingshandler.h"
#include "settings.h"
#include "iniparse/ini.h"

int language_handler(void* user, const char* section, const char* name, const char* value);
void parse_language();

// common words and phrases
extern char* no;
extern char* yes;
extern char* batterylevel;
extern char* backmenudisabled;
extern char* backmenuenabled;
extern char* choosepackage;
extern char* install;
extern char* installing;
extern char* mounted;
extern char* unmounted;
extern char* installabort;
extern char* rebooting;
extern char* shutdown;
extern char* enabled;
extern char* disabled;
extern char* mounterror;
extern char* unmounterror;
extern char* skipformat;
extern char* installconfirm;
extern char* yesinstall;
extern char* deleteconfirm;
extern char* freespacesd;
extern char* yesdelete;
extern char* deleting;
extern char* confirmformat;
extern char* yesformat;
extern char* formatting;
extern char* formaterror;
extern char* done;
extern char* doneexc;
extern char* size;
extern char* boot;
extern char* recovery;
extern char* notfound;
extern char* foundold;
extern char* on;
extern char* off;
// menu headers
extern char* wipedataheader1;
extern char* wipedataheader2;
extern char* zipinstallheader;
extern char* deletebackupheader;
extern char* lowspaceheader1;
extern char* lowspaceheader2;
extern char* lowspaceheader3;
extern char* lowspaceheader4;
extern char* zipchooseheader;
extern char* nandroidrestoreheader;
extern char* usbmsheader1;
extern char* usbmsheader2;
extern char* usbmsheader3;
extern char* recommended;
extern char* showpartitionheader;
extern char* advbackupheader;
extern char* advrestoreheader1;
extern char* advrestoreheader2;
extern char* advrestoreheader3;
extern char* advrestoreheader11;
extern char* nandroidheader;
extern char* debuggingheader;
extern char* advoptionsheader;
// menu items for main screen
extern char* rebootnow;
extern char* wipedatafactory;
extern char* wipecache;
extern char* wipeall;
extern char* installzip;
extern char* nandroid;
extern char* mountsstorage;
extern char* advanced;
extern char* langpoweroff;
// menu items for extendedcommands
extern char* signaturecheck;
extern char* scriptasserts;
extern char* zipchoosezip;
extern char* zipapplyupdatezip;
extern char* ziptogglesig;
extern char* ziptoggleasserts;
extern char* backupproceed;
extern char* backupcancel;
extern char* zipchooseyesbackup;
extern char* zipchoosenobackup;
extern char* zipcancelinstall;
extern char* usbmsunmount;
extern char* usbmsmount;
extern char* nandroidbackup;
extern char* nandroidrestore;
extern char* nandroidrestoreboot;
extern char* nandroidrestoresys;
extern char* nandroidrestoredata;
extern char* nandroidrestorecache;
extern char* nandroidrestoresd;
extern char* nandroidadvbackup;
extern char* nandroidadvrestore;
extern char* nandroiddeleteold;
extern char* debugfixperm;
extern char* debugfixloop;
extern char* debugreporterror;
extern char* debugkeytest;
extern char* debugshowlog;
extern char* debugtoggledebugui;
extern char* advreboot;
extern char* advwipedalvik;
extern char* advpartitionsd;
extern char* advcotsettings;
extern char* advdebugopts;
extern char* lowspacecontinuebackup;
extern char* lowspaceviewdelete;
extern char* lowspacecancel;
// partition wipe prompts
extern char* yesdeletedata;
extern char* wipingdata;
extern char* datawipecomplete;
extern char* datawipeskip;
extern char* datawipefail;
extern char* confirmwipe;
extern char* yeswipecache;
extern char* wipingcache;
extern char* cachewipecomplete;
extern char* cachewipeskip;
extern char* cachewipefail;
extern char* confirmwipeall;
extern char* yeswipeall;
extern char* wipingall;
extern char* wipeallcomplete;
extern char* yeswipedalvik;
extern char* wipingdalvik;
extern char* wipedalvikcomplete;
extern char* wipedalvikskip;
extern char* partitioningsd;
extern char* partitionsderror;
// miscellaneous prompts
extern char* rebootingsystemtimed;
extern char* diropenfail;
extern char* nofilesfound;
extern char* ext4checking;
// ors-specific prompts
extern char* orssdmountwait;
extern char* orssdmounted;
extern char* orssdcontinuing;
extern char* orscommandis;
extern char* orsnovalue;
extern char* orsinstallingzip;
extern char* orszipinstallerror;
extern char* orsrecursivemkdir;
extern char* orsrebootfound;
extern char* orscmdnovalue;
extern char* orsunknowncmd;
extern char* orsscriptdone;
extern char* orsscripterror;
// nandroid prompts
extern char* nandroidbackupfolderset;
extern char* nandroidbackupcomplete;
extern char* nandroidsettingrestoreoptions;
extern char* nandroidrestorecomplete;
extern char* nandroidbackupdeletecomplete;
extern char* nandroidconfirmrestore;
extern char* nandroidyesrestore;
extern char* backup;
extern char* restore;
extern char* performbackup;
extern char* nandroidbackingup;
extern char* nandroidcantmount;
extern char* nandroidyaffs2error;
extern char* nandroidsdfreespace;
extern char* bootdumperror;
extern char* recoverydumperror;
extern char* nandroidandsecnotfound;
extern char* nandroidsdextmountfail;
extern char* nandroidmd5generate;
extern char* nandroidmd5check;
extern char* nandroidmd5fail;
extern char* nandroidtarnotfound;
extern char* nandroidimgnotfound;
extern char* restoring;
extern char* formaterror;
extern char* restoreerror;
extern char* nandroideraseboot;
extern char* nandroidrestoringboot;
extern char* nandroidbootflasherror;
extern char* nandroidnobootimg;
extern char* nandroidbackupbootyes;
extern char* nandroidbackupbootno;
extern char* nandroidbackuprecyes;
extern char* nandroidbackuprecno;
extern char* nandroidbackupsysyes;
extern char* nandroidbackupsysno;
extern char* nandroidbackupdatayes;
extern char* nandroidbackupdatano;
extern char* nandroidbackupcacheyes;
extern char* nandroidbackupcacheno;
extern char* nandroidbackupsdyes;
extern char* nandroidbackupsdno;
// secure fs prompts
extern char* securefsenable;
extern char* securefsdisable;
extern char* securefsinvalid;
extern char* securefsabortdefault;
extern char* securefsabort;
extern char* securefsupdate;
// debugging prompts
extern char* uidebugenable;
extern char* uidebugdisable;
extern char* fixingperm;
extern char* outputkeycodes;
extern char* kcgoback;
// zip flashing prompts
extern char* installcomplete;
extern char* yesinstallupdate;
// format prompts
extern char* a2sdnotfound;
extern char* device;
extern char* unmounterror;
extern char* ext3;
extern char* ext2;
// bml prompts
extern char* bmlchecking;
extern char* bmlmayberfs;
// fs convert prompt
extern char* fsconv1;
extern char* fsconv2;
extern char* fsconv3;
extern char* fsconv4;
extern char* fsconv5;
extern char* fsconv6;
// failure prompt
extern char* failprompt;
// update.zip messages
extern char* installingupdate;
extern char* findupdatepackage;
extern char* openupdatepackage;
extern char* verifyupdatepackage;
// edify prompts
extern char* edifyformatting;
extern char* edifyformatdatadata;
extern char* edifywaitsdmount;
extern char* edifysdmounted;
extern char* edifysdtimeout;
extern char* edifysdverifymarker;
extern char* edifysdmarkernotfound;
extern char* edifycheckinternalmarker;
extern char* edifyinternalmarkernotfound;
extern char* edifyrmscripterror;
// commands
extern char* formatcmd;
extern char* deletecmd;
extern char* copycmd;
extern char* firmwriteextracting;
extern char* firmwriteimage;
extern char* writeimage;
// cot settings
extern char* settingsloaderror1;
extern char* settingsloaderror2;
extern char* settingsloaded;
extern char* settingsthemeerror;
extern char* settingsthemeloaded;
// cot settings headers
extern char* cotmainheader;
extern char* cotthemeheader;
extern char* cotorsrebootheader;
extern char* cotorswipepromptheader;
extern char* cotzippromptheader;
extern char* cotlangheader;
// cot settings list items
extern char* cotmainlisttheme;
extern char* cotmainlistrebootforce;
extern char* cotmainlistwipeprompt;
extern char* cotmainlistzipprompt;
extern char* cotmainlistlanguage;
extern char* cotthemehydro;
extern char* cotthemeblood;
extern char* cotthemelime;
extern char* cotthemecitrus;
extern char* cotthemedooderbutt;
extern char* cotlangen;
// cot settings theme engine
extern char* setthemedefault;
extern char* setthemeblood;
extern char* setthemelime;
extern char* setthemecitrus;
extern char* setthemedooderbutt;
