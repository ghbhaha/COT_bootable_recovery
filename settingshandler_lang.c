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

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "settingshandler_lang.h"

// common words and phrases
char* no;
char* yes;
char* batterylevel;
char* backmenudisabled;
char* backmenuenabled;
char* choosepackage;
char* install;
char* installing;
char* mounted;
char* unmounted;
char* installabort;
char* rebooting;
char* shutdown;
char* enabled;
char* disabled;
char* mounterror;
char* unmounterror;
char* skipformat;
char* installconfirm;
char* yesinstall;
char* deleteconfirm;
char* freespacesd;
char* yesdelete;
char* deleting;
char* confirmformat;
char* yesformat;
char* formatting;
char* formaterror;
char* done;
char* doneexc;
char* size;
char* boot;
char* recovery;
char* notfound;
char* foundold;
char* on;
char* off;
// menu headers
char* wipedataheader1;
char* wipedataheader2;
char* zipinstallheader;
char* deletebackupheader;
char* lowspaceheader1;
char* lowspaceheader2;
char* lowspaceheader3;
char* lowspaceheader4;
char* zipchooseheader;
char* nandroidrestoreheader;
char* usbmsheader1;
char* usbmsheader2;
char* usbmsheader3;
char* recommended;
char* showpartitionheader;
char* advbackupheader;
char* advrestoreheader1;
char* advrestoreheader2;
char* advrestoreheader3;
char* advrestoreheader11;
char* nandroidheader;
char* debuggingheader;
char* advoptionsheader;
// menu items for main screen
char* rebootnow;
char* wipedatafactory;
char* wipecache;
char* wipeall;
char* installzip;
char* nandroid;
char* mountsstorage;
char* advanced;
char* langpoweroff;
// menu items for extendedcommands
char* signaturecheck;
char* scriptasserts;
char* zipchoosezip;
char* zipapplyupdatezip;
char* ziptogglesig;
char* ziptoggleasserts;
char* backupproceed;
char* backupcancel;
char* zipchooseyesbackup;
char* zipchoosenobackup;
char* zipcancelinstall;
char* usbmsunmount;
char* nandroidbackup;
char* nandroidrestore;
char* nandroidadvbackup;
char* nandroidadvrestore;
char* nandroiddeleteold;
char* debugfixperm;
char* debugfixloop;
char* debugreporterror;
char* debugkeytest;
char* debugshowlog;
char* debugtoggledebugui;
char* advreboot;
char* advwipedalvik;
char* advpartitionsd;
char* advcotsettings;
char* advdebugopts;
// partition wipe prompts
char* yesdeletedata;
char* wipingdata;
char* datawipecomplete;
char* datawipeskip;
char* datawipefail;
char* confirmwipe;
char* yeswipecache;
char* wipingcache;
char* cachewipecomplete;
char* cachewipeskip;
char* cachewipefail;
char* confirmwipeall;
char* yeswipeall;
char* wipingall;
char* wipeallcomplete;
char* yeswipedalvik;
char* wipingdalvik;
char* wipedalvikcomplete;
char* wipedalvikskip;
char* partitioningsd;
char* partitionsderror;
// miscellaneous prompts
char* rebootingsystemtimed;
char* diropenfail;
char* nofilesfound;
char* ext4checking;
// ors-specific prompts
char* orssdmountwait;
char* orssdmounted;
char* orssdcontinuing;
char* orscommandis;
char* orsnovalue;
char* orsinstallingzip;
char* orszipinstallerror;
char* orsrecursivemkdir;
char* orsrebootfound;
char* orscmdnovalue;
char* orsunknowncmd;
char* orsscriptdone;
char* orsscripterror;
// nandroid prompts
char* nandroidbackupfolderset;
char* nandroidbackupcomplete;
char* nandroidsettingrestoreoptions;
char* nandroidrestorecomplete;
char* nandroidbackupdeletecomplete;
char* nandroidconfirmrestore;
char* nandroidyesrestore;
char* backup;
char* restore;
char* performbackup;
char* nandroidbackingup;
char* nandroidcantmount;
char* nandroidyaffs2error;
char* nandroidsdfreespace;
char* bootdumperror;
char* recoverydumperror;
char* nandroidandsecnotfound;
char* nandroidsdextmountfail;
char* nandroidmd5generate;
char* nandroidmd5check;
char* nandroidmd5fail;
char* nandroidtarnotfound;
char* nandroidimgnotfound;
char* restoring;
char* restoreerror;
char* nandroideraseboot;
char* nandroidrestoringboot;
char* nandroidbootflasherror;
char* nandroidnobootimg;
// secure fs prompts
char* securefsenable;
char* securefsdisable;
char* securefsinvalid;
char* securefsabortdefault;
char* securefsabort;
char* securefsupdate;
// debugging prompts
char* uidebugenable;
char* uidebugdisable;
char* fixingperm;
char* outputkeycodes;
char* kcgoback;
// zip flashing prompts
char* installcomplete;
char* yesinstallupdate;
// format prompts
char* a2sdnotfound;
char* device;
char* ext3;
char* ext2;
// bml prompts
char* bmlchecking;
char* bmlmayberfs;
// fs convert prompt
char* fsconv1;
char* fsconv2;
char* fsconv3;
char* fsconv4;
char* fsconv5;
char* fsconv6;
// failure prompt
char* failprompt;
// update.zip messages
char* installingupdate;
char* findupdatepackage;
char* openupdatepackage;
char* verifyupdatepackage;
// edify prompts
char* edifyformatting;
char* edifyformatdatadata;
char* edifywaitsdmount;
char* edifysdmounted;
char* edifysdtimeout;
char* edifysdverifymarker;
char* edifysdmarkernotfound;
char* edifycheckinternalmarker;
char* edifyinternalmarkernotfound;
char* edifyrmscripterror;
// commands
char* formatcmd;
char* deletecmd;
char* copycmd;
char* firmwriteextracting;
char* firmwriteimage;
char* writeimage;
// cot settings
char* settingsloaderror1;
char* settingsloaderror2;
char* settingsloaded;
char* settingsthemeerror;
char* settingsthemeloaded;
// cot settings headers
char* cotmainheader;
char* cotthemeheader;
char* cotorsrebootheader;
char* cotorswipepromptheader;
char* cotzippromptheader;
char* cotlangheader;
// cot settings list items
char* cotmainlisttheme;
char* cotmainlistrebootforce;
char* cotmainlistwipeprompt;
char* cotmainlistzipprompt;
char* cotmainlistlanguage;
char* cotthemehydro;
char* cotthemeblood;
char* cotthemelime;
char* cotthemecitrus;
char* cotthemedooderbutt;
char* cotlangen;
// cot settings theme engine
char* setthemedefault;
char* setthemeblood;
char* setthemelime;
char* setthemecitrus;
char* setthemedooderbutt;

typedef struct {
    const char* no;
    const char* yes;
    const char* batterylevel;
    const char* backmenudisabled;
    const char* backmenuenabled;
    const char* choosepackage;
    const char* install;
    const char* installing;
    const char* mounted;
    const char* unmounted;
    const char* installabort;
    const char* rebooting;
    const char* shutdown;
    const char* enabled;
    const char* disabled;
    const char* mounterror;
    const char* unmounterror;
    const char* skipformat;
    const char* installconfirm;
    const char* yesinstall;
    const char* deleteconfirm;
    const char* freespacesd;
    const char* yesdelete;
    const char* deleting;
    const char* confirmformat;
    const char* yesformat;
    const char* formatting;
    const char* formaterror;
    const char* done;
    const char* doneexc;
    const char* size;
    const char* boot;
    const char* recovery;
    const char* notfound;
    const char* foundold;
    const char* on;
    const char* off;
    // menu headers
    const char* wipedataheader1;
    const char* wipedataheader2;
    const char* zipinstallheader;
    const char* deletebackupheader;
    const char* lowspaceheader1;
    const char* lowspaceheader2;
    const char* lowspaceheader3;
    const char* lowspaceheader4;
    const char* zipchooseheader;
    const char* nandroidrestoreheader;
    const char* usbmsheader1;
    const char* usbmsheader2;
    const char* usbmsheader3;
    const char* recommended;
    const char* showpartitionheader;
    const char* advbackupheader;
    const char* advrestoreheader1;
    const char* advrestoreheader2;
    const char* advrestoreheader3;
    const char* advrestoreheader11;
    const char* nandroidheader;
    const char* debuggingheader;
    const char* advoptionsheader;
    // menu items for main screen
    const char* rebootnow;
    const char* wipedatafactory;
    const char* wipecache;
    const char* wipeall;
    const char* installzip;
    const char* nandroid;
    const char* mountsstorage;
    const char* advanced;
    const char* langpoweroff;
    // menu items for extendedcommands
    const char* signaturecheck;
    const char* scriptasserts;
    const char* zipchoosezip;
    const char* zipapplyupdatezip;
    const char* ziptogglesig;
    const char* ziptoggleasserts;
    const char* backupproceed;
    const char* backupcancel;
    const char* zipchooseyesbackup;
    const char* zipchoosenobackup;
    const char* zipcancelinstall;
    const char* usbmsunmount;
    const char* nandroidbackup;
    const char* nandroidrestore;
    const char* nandroidadvbackup;
    const char* nandroidadvrestore;
    const char* nandroiddeleteold;
    const char* debugfixperm;
    const char* debugfixloop;
    const char* debugreporterror;
    const char* debugkeytest;
    const char* debugshowlog;
    const char* debugtoggledebugui;
    const char* advreboot;
    const char* advwipedalvik;
    const char* advpartitionsd;
    const char* advcotsettings;
    const char* advdebugopts;
    // partition wipe prompts
    const char* yesdeletedata;
    const char* wipingdata;
    const char* datawipecomplete;
    const char* datawipeskip;
    const char* datawipefail;
    const char* confirmwipe;
    const char* yeswipecache;
    const char* wipingcache;
    const char* cachewipecomplete;
    const char* cachewipeskip;
    const char* cachewipefail;
    const char* confirmwipeall;
    const char* yeswipeall;
    const char* wipingall;
    const char* wipeallcomplete;
    const char* yeswipedalvik;
    const char* wipingdalvik;
    const char* wipedalvikcomplete;
    const char* wipedalvikskip;
    const char* partitioningsd;
    const char* partitionsderror;
    // miscellaneous prompts
    const char* rebootingsystemtimed;
    const char* diropenfail;
    const char* nofilesfound;
    const char* ext4checking;
    // ors-specific prompts
    const char* orssdmountwait;
    const char* orssdmounted;
    const char* orssdcontinuing;
    const char* orscommandis;
    const char* orsnovalue;
    const char* orsinstallingzip;
    const char* orszipinstallerror;
    const char* orsrecursivemkdir;
    const char* orsrebootfound;
    const char* orscmdnovalue;
    const char* orsunknowncmd;
    const char* orsscriptdone;
    const char* orsscripterror;
    // nandroid prompts
    const char* nandroidbackupfolderset;
    const char* nandroidbackupcomplete;
    const char* nandroidsettingrestoreoptions;
    const char* nandroidrestorecomplete;
    const char* nandroidbackupdeletecomplete;
    const char* nandroidconfirmrestore;
    const char* nandroidyesrestore;
    const char* backup;
    const char* restore;
    const char* performbackup;
    const char* nandroidbackingup;
    const char* nandroidcantmount;
    const char* nandroidyaffs2error;
    const char* nandroidsdfreespace;
    const char* bootdumperror;
    const char* recoverydumperror;
    const char* nandroidandsecnotfound;
    const char* nandroidsdextmountfail;
    const char* nandroidmd5generate;
    const char* nandroidmd5check;
    const char* nandroidmd5fail;
    const char* nandroidtarnotfound;
    const char* nandroidimgnotfound;
    const char* restoring;
    const char* restoreerror;
    const char* nandroideraseboot;
    const char* nandroidrestoringboot;
    const char* nandroidbootflasherror;
    const char* nandroidnobootimg;
    // secure fs prompts
    const char* securefsenable;
    const char* securefsdisable;
    const char* securefsinvalid;
    const char* securefsabortdefault;
    const char* securefsabort;
    const char* securefsupdate;
    // debugging prompts
    const char* uidebugenable;
    const char* uidebugdisable;
    const char* fixingperm;
    const char* outputkeycodes;
    const char* kcgoback;
    // zip flashing prompts
    const char* installcomplete;
    const char* yesinstallupdate;
    // format prompts
    const char* a2sdnotfound;
    const char* device;
    const char* ext3;
    const char* ext2;
    // bml prompts
    const char* bmlchecking;
    const char* bmlmayberfs;
    // fs convert prompt
    const char* fsconv1;
    const char* fsconv2;
    const char* fsconv3;
    const char* fsconv4;
    const char* fsconv5;
    const char* fsconv6;
    // failure prompt
    const char* failprompt;
    // update.zip messages
    const char* installingupdate;
    const char* findupdatepackage;
    const char* openupdatepackage;
    const char* verifyupdatepackage;
    // edify prompts
    const char* edifyformatting;
    const char* edifyformatdatadata;
    const char* edifywaitsdmount;
    const char* edifysdmounted;
    const char* edifysdtimeout;
    const char* edifysdverifymarker;
    const char* edifysdmarkernotfound;
    const char* edifycheckinternalmarker;
    const char* edifyinternalmarkernotfound;
    const char* edifyrmscripterror;
    // commands
    const char* formatcmd;
    const char* deletecmd;
    const char* copycmd;
    const char* firmwriteextracting;
    const char* firmwriteimage;
    const char* writeimage;
    // cot settings
    const char* settingsloaderror1;
    const char* settingsloaderror2;
    const char* settingsloaded;
    const char* settingsthemeerror;
    const char* settingsthemeloaded;
    // cot settings headers
    const char* cotmainheader;
    const char* cotthemeheader;
    const char* cotorsrebootheader;
    const char* cotorswipepromptheader;
    const char* cotzippromptheader;
    const char* cotlangheader;
    // cot settings list items
    const char* cotmainlisttheme;
    const char* cotmainlistrebootforce;
    const char* cotmainlistwipeprompt;
    const char* cotmainlistzipprompt;
    const char* cotmainlistlanguage;
    const char* cotthemehydro;
    const char* cotthemeblood;
    const char* cotthemelime;
    const char* cotthemecitrus;
    const char* cotthemedooderbutt;
    const char* cotlangen;
    // cot settings theme engine
    const char* setthemedefault;
    const char* setthemeblood;
    const char* setthemelime;
    const char* setthemecitrus;
    const char* setthemedooderbutt;
} lang_keywords;

int language_handler(void* user, const char* section, const char* name, const char* value) {
    lang_keywords* pconfig = (lang_keywords*)user;

	#define MATCH(s, n) strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0
	if (MATCH("settings", "no")) {
		pconfig->no = strdup(value);
	} else if (MATCH("settings", "yes")) {
		pconfig->yes = strdup(value);
	} else if (MATCH("settings", "batterylevel")) {
		pconfig->batterylevel = strdup(value);
	} else if (MATCH("settings", "backmenudisabled")) {
		pconfig->backmenudisabled = strdup(value);
	} else if (MATCH("settings", "backmenuenabled")) {
		pconfig->backmenuenabled = strdup(value);
	} else if (MATCH("settings", "choosepackage")) {
		pconfig->choosepackage = strdup(value);
	} else if (MATCH("settings", "install")) {
		pconfig->install = strdup(value);
	} else if (MATCH("settings", "installing")) {
		pconfig->installing = strdup(value);
	} else if (MATCH("settings", "mounted")) {
		pconfig->mounted = strdup(value);
	} else if (MATCH("settings", "unmounted")) {
		pconfig->unmounted = strdup(value);
	} else if (MATCH("settings", "installabort")) {
		pconfig->installabort = strdup(value);
	} else if (MATCH("settings", "rebooting")) {
		pconfig->rebooting = strdup(value);
	} else if (MATCH("settings", "shutdown")) {
		pconfig->shutdown = strdup(value);
	} else if (MATCH("settings", "enabled")) {
		pconfig->enabled = strdup(value);
	} else if (MATCH("settings", "disabled")) {
		pconfig->disabled = strdup(value);
	} else if (MATCH("settings", "mounterror")) {
		pconfig->mounterror = strdup(value);
	} else if (MATCH("settings", "skipformat")) {
		pconfig->skipformat = strdup(value);
	} else if (MATCH("settings", "installconfirm")) {
		pconfig->installconfirm = strdup(value);
	} else if (MATCH("settings", "yesinstall")) {
		pconfig->yesinstall = strdup(value);
	} else if (MATCH("settings", "deleteconfirm")) {
		pconfig->deleteconfirm = strdup(value);
	} else if (MATCH("settings", "freespacesd")) {
		pconfig->freespacesd = strdup(value);
	} else if (MATCH("settings", "yesdelete")) {
		pconfig->yesdelete = strdup(value);
	} else if (MATCH("settings", "deleting")) {
		pconfig->deleting = strdup(value);
	} else if (MATCH("settings", "confirmformat")) {
		pconfig->confirmformat = strdup(value);
	} else if (MATCH("settings", "yesformat")) {
		pconfig->yesformat = strdup(value);
	} else if (MATCH("settings", "formatting")) {
		pconfig->formatting = strdup(value);
	} else if (MATCH("settings", "formaterror")) {
		pconfig->formaterror = strdup(value);
	} else if (MATCH("settings", "done")) {
		pconfig->done = strdup(value);
	} else if (MATCH("settings", "doneexc")) {
		pconfig->doneexc = strdup(value);
	} else if (MATCH("settings", "size")) {
		pconfig->size = strdup(value);
	} else if (MATCH("settings", "boot")) {
		pconfig->boot = strdup(value);
	} else if (MATCH("settings", "recovery")) {
		pconfig->recovery = strdup(value);
	} else if (MATCH("settings", "notfound")) {
		pconfig->notfound = strdup(value);
	} else if (MATCH("settings", "foundold")) {
		pconfig->foundold = strdup(value);
	} else if (MATCH("settings", "on")) {
		pconfig->on = strdup(value);
	} else if (MATCH("settings", "off")) {
		pconfig->off = strdup(value);
	} else if (MATCH("settings", "wipedataheader1")) {
		pconfig->wipedataheader1 = strdup(value);
	} else if (MATCH("settings", "wipedataheader2")) {
		pconfig->wipedataheader2 = strdup(value);
	} else if (MATCH("settings", "zipinstallheader")) {
		pconfig->zipinstallheader = strdup(value);
	} else if (MATCH("settings", "deletebackupheader")) {
		pconfig->deletebackupheader = strdup(value);
	} else if (MATCH("settings", "lowspaceheader1")) {
		pconfig->lowspaceheader1 = strdup(value);
	} else if (MATCH("settings", "lowspaceheader2")) {
		pconfig->lowspaceheader2 = strdup(value);
	} else if (MATCH("settings", "lowspaceheader3")) {
		pconfig->lowspaceheader3 = strdup(value);
	} else if (MATCH("settings", "lowspaceheader4")) {
		pconfig->lowspaceheader4 = strdup(value);
	} else if (MATCH("settings", "zipchooseheader")) {
		pconfig->zipchooseheader = strdup(value);
	} else if (MATCH("settings", "nandroidrestoreheader")) {
		pconfig->nandroidrestoreheader = strdup(value);
	} else if (MATCH("settings", "usbmsheader1")) {
		pconfig->usbmsheader1 = strdup(value);
	} else if (MATCH("settings", "usbmsheader2")) {
		pconfig->usbmsheader2 = strdup(value);
	} else if (MATCH("settings", "usbmsheader3")) {
		pconfig->usbmsheader3 = strdup(value);
	} else if (MATCH("settings", "recommended")) {
		pconfig->recommended = strdup(value);
	} else if (MATCH("settings", "showpartitionheader")) {
		pconfig->showpartitionheader = strdup(value);
	} else if (MATCH("settings", "advbackupheader")) {
		pconfig->advbackupheader = strdup(value);
	} else if (MATCH("settings", "advrestoreheader1")) {
		pconfig->advrestoreheader1 = strdup(value);
	} else if (MATCH("settings", "advrestoreheader2")) {
		pconfig->advrestoreheader2 = strdup(value);
	} else if (MATCH("settings", "advrestoreheader3")) {
		pconfig->advrestoreheader3 = strdup(value);
	} else if (MATCH("settings", "advrestoreheader11")) {
		pconfig->advrestoreheader11 = strdup(value);
	} else if (MATCH("settings", "nandroidheader")) {
		pconfig->nandroidheader = strdup(value);
	} else if (MATCH("settings", "debuggingheader")) {
		pconfig->debuggingheader = strdup(value);
	} else if (MATCH("settings", "advoptionsheader")) {
		pconfig->advoptionsheader = strdup(value);
	} else if (MATCH("settings", "rebootnow")) {
		pconfig->rebootnow = strdup(value);
	} else if (MATCH("settings", "wipedatafactory")) {
		pconfig->wipedatafactory = strdup(value);
	} else if (MATCH("settings", "wipecache")) {
		pconfig->wipecache = strdup(value);
	} else if (MATCH("settings", "wipeall")) {
		pconfig->wipeall = strdup(value);
	} else if (MATCH("settings", "installzip")) {
		pconfig->installzip = strdup(value);
	} else if (MATCH("settings", "nandroid")) {
		pconfig->nandroid = strdup(value);
	} else if (MATCH("settings", "mountsstorage")) {
		pconfig->mountsstorage = strdup(value);
	} else if (MATCH("settings", "advanced")) {
		pconfig->advanced = strdup(value);
	} else if (MATCH("settings", "langpoweroff")) {
		pconfig->langpoweroff = strdup(value);
	} else if (MATCH("settings", "signaturecheck")) {
		pconfig->signaturecheck = strdup(value);
	} else if (MATCH("settings", "scriptasserts")) {
		pconfig->scriptasserts = strdup(value);
	} else if (MATCH("settings", "zipchoosezip")) {
		pconfig->zipchoosezip = strdup(value);
	} else if (MATCH("settings", "zipapplyupdatezip")) {
		pconfig->zipapplyupdatezip = strdup(value);
	} else if (MATCH("settings", "ziptogglesig")) {
		pconfig->ziptogglesig = strdup(value);
	} else if (MATCH("settings", "ziptoggleasserts")) {
		pconfig->ziptoggleasserts = strdup(value);
	} else if (MATCH("settings", "backupproceed")) {
		pconfig->backupproceed = strdup(value);
	} else if (MATCH("settings", "backupcancel")) {
		pconfig->backupcancel = strdup(value);
	} else if (MATCH("settings", "zipchooseyesbackup")) {
		pconfig->zipchooseyesbackup = strdup(value);
	} else if (MATCH("settings", "zipchoosenobackup")) {
		pconfig->zipchoosenobackup = strdup(value);
	} else if (MATCH("settings", "zipcancelinstall")) {
		pconfig->zipcancelinstall = strdup(value);
	} else if (MATCH("settings", "usbmsunmount")) {
		pconfig->usbmsunmount = strdup(value);
	} else if (MATCH("settings", "nandroidbackup")) {
		pconfig->nandroidbackup = strdup(value);
	} else if (MATCH("settings", "nandroidrestore")) {
		pconfig->nandroidrestore = strdup(value);
	} else if (MATCH("settings", "nandroidadvbackup")) {
		pconfig->nandroidadvbackup = strdup(value);
	} else if (MATCH("settings", "nandroidadvrestore")) {
		pconfig->nandroidadvrestore = strdup(value);
	} else if (MATCH("settings", "nandroiddeleteold")) {
		pconfig->nandroiddeleteold = strdup(value);
	} else if (MATCH("settings", "debugfixperm")) {
		pconfig->debugfixperm = strdup(value);
	} else if (MATCH("settings", "debugfixloop")) {
		pconfig->debugfixloop = strdup(value);
	} else if (MATCH("settings", "debugreporterror")) {
		pconfig->debugreporterror = strdup(value);
	} else if (MATCH("settings", "debugkeytest")) {
		pconfig->debugkeytest = strdup(value);
	} else if (MATCH("settings", "debugshowlog")) {
		pconfig->debugshowlog = strdup(value);
	} else if (MATCH("settings", "debugtoggledebugui")) {
		pconfig->debugtoggledebugui = strdup(value);
	} else if (MATCH("settings", "advreboot")) {
		pconfig->advreboot = strdup(value);
	} else if (MATCH("settings", "advwipedalvik")) {
		pconfig->advwipedalvik = strdup(value);
	} else if (MATCH("settings", "advpartitionsd")) {
		pconfig->advpartitionsd = strdup(value);
	} else if (MATCH("settings", "advcotsettings")) {
		pconfig->advcotsettings = strdup(value);
	} else if (MATCH("settings", "advdebugopts")) {
		pconfig->advdebugopts = strdup(value);
	} else if (MATCH("settings", "yesdeletedata")) {
		pconfig->yesdeletedata = strdup(value);
	} else if (MATCH("settings", "wipingdata")) {
		pconfig->wipingdata = strdup(value);
	} else if (MATCH("settings", "datawipecomplete")) {
		pconfig->datawipecomplete = strdup(value);
	} else if (MATCH("settings", "datawipeskip")) {
		pconfig->datawipeskip = strdup(value);
	} else if (MATCH("settings", "datawipefail")) {
		pconfig->datawipefail = strdup(value);
	} else if (MATCH("settings", "confirmwipe")) {
		pconfig->confirmwipe = strdup(value);
	} else if (MATCH("settings", "yeswipecache")) {
		pconfig->yeswipecache = strdup(value);
	} else if (MATCH("settings", "wipingcache")) {
		pconfig->wipingcache = strdup(value);
	} else if (MATCH("settings", "cachewipecomplete")) {
		pconfig->cachewipecomplete = strdup(value);
	} else if (MATCH("settings", "cachewipeskip")) {
		pconfig->cachewipeskip = strdup(value);
	} else if (MATCH("settings", "cachewipefail")) {
		pconfig->cachewipefail = strdup(value);
	} else if (MATCH("settings", "confirmwipeall")) {
		pconfig->confirmwipeall = strdup(value);
	} else if (MATCH("settings", "yeswipeall")) {
		pconfig->yeswipeall = strdup(value);
	} else if (MATCH("settings", "wipingall")) {
		pconfig->wipingall = strdup(value);
	} else if (MATCH("settings", "wipeallcomplete")) {
		pconfig->wipeallcomplete = strdup(value);
	} else if (MATCH("settings", "yeswipedalvik")) {
		pconfig->yeswipedalvik = strdup(value);
	} else if (MATCH("settings", "wipingdalvik")) {
		pconfig->wipingdalvik = strdup(value);
	} else if (MATCH("settings", "wipedalvikcomplete")) {
		pconfig->wipedalvikcomplete = strdup(value);
	} else if (MATCH("settings", "wipedalvikskip")) {
		pconfig->wipedalvikskip = strdup(value);
	} else if (MATCH("settings", "partitioningsd")) {
		pconfig->partitioningsd = strdup(value);
	} else if (MATCH("settings", "partitionsderror")) {
		pconfig->partitionsderror = strdup(value);
	} else if (MATCH("settings", "rebootingsystemtimed")) {
		pconfig->rebootingsystemtimed = strdup(value);
	} else if (MATCH("settings", "diropenfail")) {
		pconfig->diropenfail = strdup(value);
	} else if (MATCH("settings", "nofilesfound")) {
		pconfig->nofilesfound = strdup(value);
	} else if (MATCH("settings", "ext4checking")) {
		pconfig->ext4checking = strdup(value);
	} else if (MATCH("settings", "orssdmountwait")) {
		pconfig->orssdmountwait = strdup(value);
	} else if (MATCH("settings", "orssdmounted")) {
		pconfig->orssdmounted = strdup(value);
	} else if (MATCH("settings", "orssdcontinuing")) {
		pconfig->orssdcontinuing = strdup(value);
	} else if (MATCH("settings", "orscommandis")) {
		pconfig->orscommandis = strdup(value);
	} else if (MATCH("settings", "orsnovalue")) {
		pconfig->orsnovalue = strdup(value);
	} else if (MATCH("settings", "orsinstallingzip")) {
		pconfig->orsinstallingzip = strdup(value);
	} else if (MATCH("settings", "orszipinstallerror")) {
		pconfig->orszipinstallerror = strdup(value);
	} else if (MATCH("settings", "orsrecursivemkdir")) {
		pconfig->orsrecursivemkdir = strdup(value);
	} else if (MATCH("settings", "orsrebootfound")) {
		pconfig->orsrebootfound = strdup(value);
	} else if (MATCH("settings", "orscmdnovalue")) {
		pconfig->orscmdnovalue = strdup(value);
	} else if (MATCH("settings", "orsunknowncmd")) {
		pconfig->orsunknowncmd = strdup(value);
	} else if (MATCH("settings", "orsscriptdone")) {
		pconfig->orsscriptdone = strdup(value);
	} else if (MATCH("settings", "orsscripterror")) {
		pconfig->orsscripterror = strdup(value);
	} else if (MATCH("settings", "nandroidbackupfolderset")) {
		pconfig->nandroidbackupfolderset = strdup(value);
	} else if (MATCH("settings", "nandroidbackupcomplete")) {
		pconfig->nandroidbackupcomplete = strdup(value);
	} else if (MATCH("settings", "nandroidsettingrestoreoptions")) {
		pconfig->nandroidsettingrestoreoptions = strdup(value);
	} else if (MATCH("settings", "nandroidrestorecomplete")) {
		pconfig->nandroidrestorecomplete = strdup(value);
	} else if (MATCH("settings", "nandroidbackupdeletecomplete")) {
		pconfig->nandroidbackupdeletecomplete = strdup(value);
	} else if (MATCH("settings", "nandroidconfirmrestore")) {
		pconfig->nandroidconfirmrestore = strdup(value);
	} else if (MATCH("settings", "nandroidyesrestore")) {
		pconfig->nandroidyesrestore = strdup(value);
	} else if (MATCH("settings", "backup")) {
		pconfig->backup = strdup(value);
	} else if (MATCH("settings", "restore")) {
		pconfig->restore = strdup(value);
	} else if (MATCH("settings", "performbackup")) {
		pconfig->performbackup = strdup(value);
	} else if (MATCH("settings", "nandroidbackingup")) {
		pconfig->nandroidbackingup = strdup(value);
	} else if (MATCH("settings", "nandroidcantmount")) {
		pconfig->nandroidcantmount = strdup(value);
	} else if (MATCH("settings", "nandroidyaffs2error")) {
		pconfig->nandroidyaffs2error = strdup(value);
	} else if (MATCH("settings", "nandroidsdfreespace")) {
		pconfig->nandroidsdfreespace = strdup(value);
	} else if (MATCH("settings", "bootdumperror")) {
		pconfig->bootdumperror = strdup(value);
	} else if (MATCH("settings", "recoverydumperror")) {
		pconfig->recoverydumperror = strdup(value);
	} else if (MATCH("settings", "nandroidandsecnotfound")) {
		pconfig->nandroidandsecnotfound = strdup(value);
	} else if (MATCH("settings", "nandroidsdextmountfail")) {
		pconfig->nandroidsdextmountfail = strdup(value);
	} else if (MATCH("settings", "nandroidmd5generate")) {
		pconfig->nandroidmd5generate = strdup(value);
	} else if (MATCH("settings", "nandroidmd5check")) {
		pconfig->nandroidmd5check = strdup(value);
	} else if (MATCH("settings", "nandroidmd5fail")) {
		pconfig->nandroidmd5fail = strdup(value);
	} else if (MATCH("settings", "nandroidtarnotfound")) {
		pconfig->nandroidtarnotfound = strdup(value);
	} else if (MATCH("settings", "nandroidimgnotfound")) {
		pconfig->nandroidimgnotfound = strdup(value);
	} else if (MATCH("settings", "restoring")) {
		pconfig->restoring = strdup(value);
	} else if (MATCH("settings", "restoreerror")) {
		pconfig->restoreerror = strdup(value);
	} else if (MATCH("settings", "nandroideraseboot")) {
		pconfig->nandroideraseboot = strdup(value);
	} else if (MATCH("settings", "nandroidrestoringboot")) {
		pconfig->nandroidrestoringboot = strdup(value);
	} else if (MATCH("settings", "nandroidbootflasherror")) {
		pconfig->nandroidbootflasherror = strdup(value);
	} else if (MATCH("settings", "nandroidnobootimg")) {
		pconfig->nandroidnobootimg = strdup(value);
	} else if (MATCH("settings", "securefsenable")) {
		pconfig->securefsenable = strdup(value);
	} else if (MATCH("settings", "securefsdisable")) {
		pconfig->securefsdisable = strdup(value);
	} else if (MATCH("settings", "securefsinvalid")) {
		pconfig->securefsinvalid = strdup(value);
	} else if (MATCH("settings", "securefsabortdefault")) {
		pconfig->securefsabortdefault = strdup(value);
	} else if (MATCH("settings", "securefsabort")) {
		pconfig->securefsabort = strdup(value);
	} else if (MATCH("settings", "securefsupdate")) {
		pconfig->securefsupdate = strdup(value);
	} else if (MATCH("settings", "uidebugenable")) {
		pconfig->uidebugenable = strdup(value);
	} else if (MATCH("settings", "uidebugdisable")) {
		pconfig->uidebugdisable = strdup(value);
	} else if (MATCH("settings", "fixingperm")) {
		pconfig->fixingperm = strdup(value);
	} else if (MATCH("settings", "outputkeycodes")) {
		pconfig->outputkeycodes = strdup(value);
	} else if (MATCH("settings", "kcgoback")) {
		pconfig->kcgoback = strdup(value);
	} else if (MATCH("settings", "installcomplete")) {
		pconfig->installcomplete = strdup(value);
	} else if (MATCH("settings", "yesinstallupdate")) {
		pconfig->yesinstallupdate = strdup(value);
	} else if (MATCH("settings", "a2sdnotfound")) {
		pconfig->a2sdnotfound = strdup(value);
	} else if (MATCH("settings", "device")) {
		pconfig->device = strdup(value);
	} else if (MATCH("settings", "unmounterror")) {
		pconfig->unmounterror = strdup(value);
	} else if (MATCH("settings", "ext3")) {
		pconfig->ext3 = strdup(value);
	} else if (MATCH("settings", "ext2")) {
		pconfig->ext2 = strdup(value);
	} else if (MATCH("settings", "bmlchecking")) {
		pconfig->bmlchecking = strdup(value);
	} else if (MATCH("settings", "bmlmayberfs")) {
		pconfig->bmlmayberfs = strdup(value);
	} else if (MATCH("settings", "fsconv1")) {
		pconfig->fsconv1 = strdup(value);
	} else if (MATCH("settings", "fsconv2")) {
		pconfig->fsconv2 = strdup(value);
	} else if (MATCH("settings", "fsconv3")) {
		pconfig->fsconv3 = strdup(value);
	} else if (MATCH("settings", "fsconv4")) {
		pconfig->fsconv4 = strdup(value);
	} else if (MATCH("settings", "fsconv5")) {
		pconfig->fsconv5 = strdup(value);
	} else if (MATCH("settings", "fsconv6")) {
		pconfig->fsconv6 = strdup(value);
	} else if (MATCH("settings", "failprompt")) {
		pconfig->failprompt = strdup(value);
	} else if (MATCH("settings", "installingupdate")) {
		pconfig->installingupdate = strdup(value);
	} else if (MATCH("settings", "findupdatepackage")) {
		pconfig->findupdatepackage = strdup(value);
	} else if (MATCH("settings", "openupdatepackage")) {
		pconfig->openupdatepackage = strdup(value);
	} else if (MATCH("settings", "verifyupdatepackage")) {
		pconfig->verifyupdatepackage = strdup(value);
	} else if (MATCH("settings", "edifyformatting")) {
		pconfig->edifyformatting = strdup(value);
	} else if (MATCH("settings", "edifyformatdatadata")) {
		pconfig->edifyformatdatadata = strdup(value);
	} else if (MATCH("settings", "edifywaitsdmount")) {
		pconfig->edifywaitsdmount = strdup(value);
	} else if (MATCH("settings", "edifysdmounted")) {
		pconfig->edifysdmounted = strdup(value);
	} else if (MATCH("settings", "edifysdtimeout")) {
		pconfig->edifysdtimeout = strdup(value);
	} else if (MATCH("settings", "edifysdverifymarker")) {
		pconfig->edifysdverifymarker = strdup(value);
	} else if (MATCH("settings", "edifysdmarkernotfound")) {
		pconfig->edifysdmarkernotfound = strdup(value);
	} else if (MATCH("settings", "edifycheckinternalmarker")) {
		pconfig->edifycheckinternalmarker = strdup(value);
	} else if (MATCH("settings", "edifyinternalmarkernotfound")) {
		pconfig->edifyinternalmarkernotfound = strdup(value);
	} else if (MATCH("settings", "edifyrmscripterror")) {
		pconfig->edifyrmscripterror = strdup(value);
	} else if (MATCH("settings", "formatcmd")) {
		pconfig->formatcmd = strdup(value);
	} else if (MATCH("settings", "deletecmd")) {
		pconfig->deletecmd = strdup(value);
	} else if (MATCH("settings", "copycmd")) {
		pconfig->copycmd = strdup(value);
	} else if (MATCH("settings", "firmwriteextracting")) {
		pconfig->firmwriteextracting = strdup(value);
	} else if (MATCH("settings", "firmwriteimage")) {
		pconfig->firmwriteimage = strdup(value);
	} else if (MATCH("settings", "writeimage")) {
		pconfig->writeimage = strdup(value);
	} else if (MATCH("settings", "settingsloaderror1")) {
		pconfig->settingsloaderror1 = strdup(value);
	} else if (MATCH("settings", "settingsloaderror2")) {
		pconfig->settingsloaderror2 = strdup(value);
	} else if (MATCH("settings", "settingsloaded")) {
		pconfig->settingsloaded = strdup(value);
	} else if (MATCH("settings", "settingsthemeerror")) {
		pconfig->settingsthemeerror = strdup(value);
	} else if (MATCH("settings", "settingsthemeloaded")) {
		pconfig->settingsthemeloaded = strdup(value);
	} else if (MATCH("settings", "cotmainheader")) {
		pconfig->cotmainheader = strdup(value);
	} else if (MATCH("settings", "cotthemeheader")) {
		pconfig->cotthemeheader = strdup(value);
	} else if (MATCH("settings", "cotorsrebootheader")) {
		pconfig->cotorsrebootheader = strdup(value);
	} else if (MATCH("settings", "cotorswipepromptheader")) {
		pconfig->cotorswipepromptheader = strdup(value);
	} else if (MATCH("settings", "cotzippromptheader")) {
		pconfig->cotzippromptheader = strdup(value);
	} else if (MATCH("settings", "cotlangheader")) {
		pconfig->cotlangheader = strdup(value);
	} else if (MATCH("settings", "cotmainlisttheme")) {
		pconfig->cotmainlisttheme = strdup(value);
	} else if (MATCH("settings", "cotmainlistrebootforce")) {
		pconfig->cotmainlistrebootforce = strdup(value);
	} else if (MATCH("settings", "cotmainlistwipeprompt")) {
		pconfig->cotmainlistwipeprompt = strdup(value);
	} else if (MATCH("settings", "cotmainlistzipprompt")) {
		pconfig->cotmainlistzipprompt = strdup(value);
	} else if (MATCH("settings", "cotmainlistlanguage")) {
		pconfig->cotmainlistlanguage = strdup(value);
	} else if (MATCH("settings", "cotthemehydro")) {
		pconfig->cotthemehydro = strdup(value);
	} else if (MATCH("settings", "cotthemeblood")) {
		pconfig->cotthemeblood = strdup(value);
	} else if (MATCH("settings", "cotthemelime")) {
		pconfig->cotthemelime = strdup(value);
	} else if (MATCH("settings", "cotthemecitrus")) {
		pconfig->cotthemecitrus = strdup(value);
	} else if (MATCH("settings", "cotthemedooderbutt")) {
		pconfig->cotthemedooderbutt = strdup(value);
	} else if (MATCH("settings", "cotlangen")) {
		pconfig->cotlangen = strdup(value);
	} else if (MATCH("settings", "setthemedefault")) {
		pconfig->setthemedefault = strdup(value);
	} else if (MATCH("settings", "setthemeblood")) {
		pconfig->setthemeblood = strdup(value);
	} else if (MATCH("settings", "setthemelime")) {
		pconfig->setthemelime = strdup(value);
	} else if (MATCH("settings", "setthemecitrus")) {
		pconfig->setthemecitrus = strdup(value);
	} else if (MATCH("settings", "setthemedooderbutt")) {
		pconfig->setthemedooderbutt = strdup(value);
	} else {
		return 0;
	}
	return 1;
}

void parse_language() {
    ensure_path_mounted("/sdcard");
	lang_keywords config;

	ini_parse("/res/lang/lang_en.ini", language_handler, &config);
	LOGI("English language loaded!\n");
	no = config.no;
	yes = config.yes;
	batterylevel = config.batterylevel;
	backmenudisabled = config.backmenudisabled;
	backmenuenabled = config.backmenuenabled;
	choosepackage = config.choosepackage;
	install = config.install;
	installing = config.installing;
	mounted = config.mounted;
	unmounted = config.unmounted;
	installabort = config.installabort;
	rebooting = config.rebooting;
	shutdown = config.shutdown;
	enabled = config.enabled;
	disabled = config.disabled;
	mounterror = config.mounterror;
	unmounterror = config.unmounterror;
	skipformat = config.skipformat;
	installconfirm = config.installconfirm;
	yesinstall = config.yesinstall;
	deleteconfirm = config.deleteconfirm;
	freespacesd = config.freespacesd;
	yesdelete = config.yesdelete;
	deleting = config.deleting;
	confirmformat = config.confirmformat;
	yesformat = config.yesformat;
	formatting = config.formatting;
	formaterror = config.formaterror;
	done = config.done;
	doneexc = config.doneexc;
	size = config.size;
	boot = config.boot;
	recovery = config.recovery;
	notfound = config.notfound;
	foundold = config.foundold;
	on = config.on;
	off = config.off;
	wipedataheader1 = config.wipedataheader1;
	wipedataheader2 = config.wipedataheader2;
	zipinstallheader = config.zipinstallheader;
	deletebackupheader = config.deletebackupheader;
	lowspaceheader1 = config.lowspaceheader1;
	lowspaceheader2 = config.lowspaceheader2;
	lowspaceheader3 = config.lowspaceheader3;
	lowspaceheader4 = config.lowspaceheader4;
	zipchooseheader = config.zipchooseheader;
	nandroidrestoreheader = config.nandroidrestoreheader;
	usbmsheader1 = config.usbmsheader1;
	usbmsheader2 = config.usbmsheader2;
	usbmsheader3 = config.usbmsheader3;
	recommended = config.recommended;
	showpartitionheader = config.showpartitionheader;
	advbackupheader = config.advbackupheader;
	advrestoreheader1 = config.advrestoreheader1;
	advrestoreheader2 = config.advrestoreheader2;
	advrestoreheader3 = config.advrestoreheader3;
	advrestoreheader11 = config.advrestoreheader11;
	nandroidheader = config.nandroidheader;
	debuggingheader = config.debuggingheader;
	advoptionsheader = config.advoptionsheader;
	rebootnow = config.rebootnow;
	wipedatafactory = config.wipedatafactory;
	wipecache = config.wipecache;
	wipeall = config.wipeall;
	installzip = config.installzip;
	nandroid = config.nandroid;
	mountsstorage = config.mountsstorage;
	advanced = config.advanced;
	langpoweroff = config.langpoweroff;
	signaturecheck = config.signaturecheck;
	scriptasserts = config.scriptasserts;
	zipchoosezip = config.zipchoosezip;
	zipapplyupdatezip = config.zipapplyupdatezip;
	ziptogglesig = config.ziptogglesig;
	ziptoggleasserts = config.ziptoggleasserts;
	backupproceed = config.backupproceed;
	backupcancel = config.backupcancel;
	zipchooseyesbackup = config.zipchooseyesbackup;
	zipchoosenobackup = config.zipchoosenobackup;
	zipcancelinstall = config.zipcancelinstall;
	usbmsunmount = config.usbmsunmount;
	nandroidbackup = config.nandroidbackup;
	nandroidrestore = config.nandroidrestore;
	nandroidadvbackup = config.nandroidadvbackup;
	nandroidadvrestore = config.nandroidadvrestore;
	nandroiddeleteold = config.nandroiddeleteold;
	debugfixperm = config.debugfixperm;
	debugfixloop = config.debugfixloop;
	debugreporterror = config.debugreporterror;
	debugkeytest = config.debugkeytest;
	debugshowlog = config.debugshowlog;
	debugtoggledebugui = config.debugtoggledebugui;
	advreboot = config.advreboot;
	advwipedalvik = config.advwipedalvik;
	advpartitionsd = config.advpartitionsd;
	advcotsettings = config.advcotsettings;
	advdebugopts = config.advdebugopts;
	yesdeletedata = config.yesdeletedata;
	wipingdata = config.wipingdata;
	datawipecomplete = config.datawipecomplete;
	datawipeskip = config.datawipeskip;
	datawipefail = config.datawipefail;
	confirmwipe = config.confirmwipe;
	yeswipecache = config.yeswipecache;
	wipingcache = config.wipingcache;
	cachewipecomplete = config.cachewipecomplete;
	cachewipeskip = config.cachewipeskip;
	cachewipefail = config.cachewipefail;
	confirmwipeall = config.confirmwipeall;
	yeswipeall = config.yeswipeall;
	wipingall = config.wipingall;
	wipeallcomplete = config.wipeallcomplete;
	yeswipedalvik = config.yeswipedalvik;
	wipingdalvik = config.wipingdalvik;
	wipedalvikcomplete = config.wipedalvikcomplete;
	wipedalvikskip = config.wipedalvikskip;
	partitioningsd = config.partitioningsd;
	partitionsderror = config.partitionsderror;
	rebootingsystemtimed = config.rebootingsystemtimed;
	diropenfail = config.diropenfail;
	nofilesfound = config.nofilesfound;
	ext4checking = config.ext4checking;
	orssdmountwait = config.orssdmountwait;
	orssdmounted = config.orssdmounted;
	orssdcontinuing = config.orssdcontinuing;
	orscommandis = config.orscommandis;
	orsnovalue = config.orsnovalue;
	orsinstallingzip = config.orsinstallingzip;
	orszipinstallerror = config.orszipinstallerror;
	orsrecursivemkdir = config.orsrecursivemkdir;
	orsrebootfound = config.orsrebootfound;
	orscmdnovalue = config.orscmdnovalue;
	orsunknowncmd = config.orsunknowncmd;
	orsscriptdone = config.orsscriptdone;
	orsscripterror = config.orsscripterror;
	nandroidbackupfolderset = config.nandroidbackupfolderset;
	nandroidbackupcomplete = config.nandroidbackupcomplete;
	nandroidsettingrestoreoptions = config.nandroidsettingrestoreoptions;
	nandroidrestorecomplete = config.nandroidrestorecomplete;
	nandroidbackupdeletecomplete = config.nandroidbackupdeletecomplete;
	nandroidconfirmrestore = config.nandroidconfirmrestore;
	nandroidyesrestore = config.nandroidyesrestore;
	backup = config.backup;
	restore = config.restore;
	performbackup = config.performbackup;
	nandroidbackingup = config.nandroidbackingup;
	nandroidcantmount = config.nandroidcantmount;
	nandroidyaffs2error = config.nandroidyaffs2error;
	nandroidsdfreespace = config.nandroidsdfreespace;
	bootdumperror = config.bootdumperror;
	recoverydumperror = config.recoverydumperror;
	nandroidandsecnotfound = config.nandroidandsecnotfound;
	nandroidsdextmountfail = config.nandroidsdextmountfail;
	nandroidmd5generate = config.nandroidmd5generate;
	nandroidmd5check = config.nandroidmd5check;
	nandroidmd5fail = config.nandroidmd5fail;
	nandroidtarnotfound = config.nandroidtarnotfound;
	nandroidimgnotfound = config.nandroidimgnotfound;
	restoring = config.restoring;
	restoreerror = config.restoreerror;
	nandroideraseboot = config.nandroideraseboot;
	nandroidrestoringboot = config.nandroidrestoringboot;
	nandroidbootflasherror = config.nandroidbootflasherror;
	nandroidnobootimg = config.nandroidnobootimg;
	securefsenable = config.securefsenable;
	securefsdisable = config.securefsdisable;
	securefsinvalid = config.securefsinvalid;
	securefsabortdefault = config.securefsabortdefault;
	securefsabort = config.securefsabort;
	securefsupdate = config.securefsupdate;
	uidebugenable = config.uidebugenable;
	uidebugdisable = config.uidebugdisable;
	fixingperm = config.fixingperm;
	outputkeycodes = config.outputkeycodes;
	kcgoback = config.kcgoback;
	installcomplete = config.installcomplete;
	yesinstallupdate = config.yesinstallupdate;
	a2sdnotfound = config.a2sdnotfound;
	device = config.device;
	ext3 = config.ext3;
	ext2 = config.ext2;
	bmlchecking = config.bmlchecking;
	bmlmayberfs = config.bmlmayberfs;
	fsconv1 = config.fsconv1;
	fsconv2 = config.fsconv2;
	fsconv3 = config.fsconv3;
	fsconv4 = config.fsconv4;
	fsconv5 = config.fsconv5;
	fsconv6 = config.fsconv6;
	failprompt = config.failprompt;
	installingupdate = config.installingupdate;
	findupdatepackage = config.findupdatepackage;
	openupdatepackage = config.openupdatepackage;
	verifyupdatepackage = config.verifyupdatepackage;
	edifyformatting = config.edifyformatting;
	edifyformatdatadata = config.edifyformatdatadata;
	edifywaitsdmount = config.edifywaitsdmount;
	edifysdmounted = config.edifysdmounted;
	edifysdtimeout = config.edifysdtimeout;
	edifysdverifymarker = config.edifysdverifymarker;
	edifysdmarkernotfound = config.edifysdmarkernotfound;
	edifycheckinternalmarker = config.edifycheckinternalmarker;
	edifyinternalmarkernotfound = config.edifyinternalmarkernotfound;
	edifyrmscripterror = config.edifyrmscripterror;
	formatcmd = config.formatcmd;
	deletecmd = config.deletecmd;
	copycmd = config.copycmd;
	firmwriteextracting = config.firmwriteextracting;
	firmwriteimage = config.firmwriteimage;
	writeimage = config.writeimage;
	settingsloaderror1 = config.settingsloaderror1;
	settingsloaderror2 = config.settingsloaderror2;
	settingsloaded = config.settingsloaded;
	settingsthemeerror = config.settingsthemeerror;
	settingsthemeloaded = config.settingsthemeloaded;
	cotmainheader = config.cotmainheader;
	cotthemeheader = config.cotthemeheader;
	cotorsrebootheader = config.cotorsrebootheader;
	cotorswipepromptheader = config.cotorswipepromptheader;
	cotzippromptheader = config.cotzippromptheader;
	cotlangheader = config.cotlangheader;
	cotmainlisttheme = config.cotmainlisttheme;
	cotmainlistrebootforce = config.cotmainlistrebootforce;
	cotmainlistwipeprompt = config.cotmainlistwipeprompt;
	cotmainlistzipprompt = config.cotmainlistzipprompt;
	cotmainlistlanguage = config.cotmainlistlanguage;
	cotthemehydro = config.cotthemehydro;
	cotthemeblood = config.cotthemeblood;
	cotthemelime = config.cotthemelime;
	cotthemecitrus = config.cotthemecitrus;
	cotthemedooderbutt = config.cotthemedooderbutt;
	cotlangen = config.cotlangen;
	setthemedefault = config.setthemedefault;
	setthemeblood = config.setthemeblood;
	setthemelime = config.setthemelime;
	setthemecitrus = config.setthemecitrus;
	setthemedooderbutt = config.setthemedooderbutt;
}
