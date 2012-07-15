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

// Common words and phrases
char* No;
char* Yes;
char* BatteryLevel;
char* BackMenuDisabled;
char* BackMenuEnabled;
char* ChoosePackage;
char* Install;
char* Installing;
char* Mounted;
char* Unmounted;
char* InstallAbort;
char* Rebooting;
char* ShutDown;
char* Enabled;
char* Disabled;
char* MountError;
char* UnmountError;
char* SkipFormat;
char* InstallConfirm;
char* YesInstall;
char* DeleteConfirm;
char* FreeSpaceSD;
char* YesDelete;
char* Deleting;
char* ConfirmFormat;
char* YesFormat;
char* Formatting;
char* FormatError;
char* Done;
char* DoneExc;
char* Size;
char* Boot;
char* Recovery;
char* NotFound;
char* FoundOld;
char* On;
char* Off;
// Menu headers
char* WipeDataHeader1;
char* WipeDataHeader2;
char* ZipInstallHeader;
char* DeleteBackupHeader;
char* LowSpaceHeader1;
char* LowSpaceHeader2;
char* LowSpaceHeader3;
char* LowSpaceHeader4;
char* ZipChooseHeader;
char* NandroidRestoreHeader;
char* USBMSHeader1;
char* USBMSHeader2;
char* USBMSHeader3;
char* Recommended;
char* ShowPartitionHeader;
char* AdvBackupHeader;
char* AdvRestoreHeader1;
char* AdvRestoreHeader2;
char* AdvRestoreHeader3;
char* AdvRestoreHeader11;
char* NandroidHeader;
char* DebuggingHeader;
char* AdvOptionsHeader;
// Menu items for main screen
char* RebootNow;
char* WipeDataFactory;
char* WipeCache;
char* WipeAll;
char* InstallZip;
char* Nandroid;
char* MountsStorage;
char* Advanced;
char* PowerOff;
// Menu items for extendedcommands
char* SignatureCheck;
char* ScriptAsserts;
char* ZipChooseZip;
char* ZipApplyUpdateZip;
char* ZipToggleSig;
char* ZipToggleAsserts;
char* BackupProceed;
char* BackupCancel;
char* ZipChooseYesBackup;
char* ZipChooseNoBackup;
char* ZipCancelInstall;
char* USBMSUnmount;
char* NandroidBackup;
char* NandroidRestore;
char* NandroidAdvBackup;
char* NandroidAdvRestore;
char* NandroidDeleteOld;
char* DebugFixPerm;
char* DebugFixLoop;
char* DebugReportError;
char* DebugKeyTest;
char* DebugShowLog;
char* DebugToggleDebugUI;
char* AdvReboot;
char* AdvWipeDalvik;
char* AdvPartitionSD;
char* AdvCOTSettings;
char* AdvDebugOpts;
// Partition wipe prompts
char* YesDeleteData;
char* WipingData;
char* DataWipeComplete;
char* DataWipeSkip;
char* DataWipeFail;
char* ConfirmWipe;
char* YesWipeCache;
char* WipingCache;
char* CacheWipeComplete;
char* CacheWipeSkip;
char* CacheWipeFail;
char* ConfirmWipeAll;
char* YesWipeAll;
char* WipingAll;
char* WipeAllComplete;
char* YesWipeDalvik;
char* WipingDalvik;
char* WipeDalvikComplete;
char* WipeDalvikSkip;
char* PartitioningSD;
char* PartitionSDError;
// Miscellaneous prompts
char* RebootingSystemTimed;
char* DirOpenFail;
char* NoFilesFound;
char* EXT4Checking;
// ORS-specific prompts
char* ORSSDMountWait;
char* ORSSDMounted;
char* ORSSDContinuing;
char* ORSCommandIs;
char* ORSNoValue;
char* ORSInstallingZip;
char* ORSZipInstallError;
char* ORSRecursiveMkdir;
char* ORSRebootFound;
char* ORSCmdNoValue;
char* ORSUnknownCmd;
char* ORSScriptDone;
char* ORSScriptError;
// Nandroid prompts
char* NandroidBackupFolderSet;
char* NandroidBackupComplete;
char* NandroidSettingRestoreOptions;
char* NandroidRestoreComplete;
char* NandroidBackupDeleteComplete;
char* NandroidConfirmRestore;
char* NandroidYesRestore;
char* Backup;
char* Restore;
char* PerformBackup;
char* NandroidBackingUp;
char* NandroidCantMount;
char* NandroidYAFFS2Error;
char* NandroidSDFreeSpace;
char* BootDumpError;
char* RecoveryDumpError;
char* NandroidAndSecNotFound;
char* NandroidSDExtMountFail;
char* NandroidMD5Generate;
char* NandroidMD5Check;
char* NandroidMD5Fail;
char* NandroidTarNotFound;
char* NandroidImgNotFound;
char* Restoring;
char* FormatError;
char* RestoreError;
char* NandroidEraseBoot;
char* NandroidRestoringBoot;
char* NandroidBootFlashError;
char* NandroidNoBootImg;
// Secure FS prompts
char* SecureFSEnable;
char* SecureFSDisable;
char* SecureFSInvalid;
char* SecureFSAbortDefault;
char* SecureFSAbort;
char* SecureFSUpdate;
// Debugging prompts
char* UIDebugEnable;
char* UIDebugDisable;
char* FixingPerm;
char* OutputKeyCodes;
char* KCGoBack;
// ZIP flashing prompts
char* InstallComplete;
char* YesInstallUpdate;
// Format prompts
char* A2SDNotFound;
char* Device;
char* UnmountError;
char* EXT3;
char* EXT2;
// BML prompts
char* BMLChecking;
char* BMLMaybeRFS;
// FS convert prompt
char* FSConv1;
char* FSConv2;
char* FSConv3;
char* FSConv4;
char* FSConv5;
char* FSConv6;
// Failure prompt
char* FailPrompt;
// update.zip messages
char* InstallingUpdate;
char* FindUpdatePackage;
char* OpenUpdatePackage;
char* VerifyUpdatePackage;
// Edify prompts
char* EdifyFormatting;
char* EdifyFormatDataData;
char* EdifyWaitSDMount;
char* EdifySDMounted;
char* EdifySDTimeOut;
char* EdifySDVerifyMarker;
char* EdifySDMarkerNotFound;
char* EdifyCheckInternalMarker;
char* EdifyInternalMarkerNotFound;
char* EdifyRMScriptError;
// Commands
char* FormatCmd;
char* DeleteCmd;
char* CopyCmd;
char* FirmWriteExtracting;
char* FirmWriteImage;
char* WriteImage;
// COT Settings
char* SettingsLoadError1;
char* SettingsLoadError2;
char* SettingsLoaded;
char* SettingsThemeError;
char* SettingsThemeLoaded;
// COT Settings Headers
char* COTMainHeader;
char* COTThemeHeader;
char* COTORSRebootHeader;
char* COTORSWipePromptHeader;
char* COTZipPromptHeader;
char* COTLangHeader;
// COT Settings List Items
char* COTMainListTheme;
char* COTMainListRebootForce;
char* COTMainListWipePrompt;
char* COTMainListZipPrompt;
char* COTMainListLanguage;
char* COTThemeHydro;
char* COTThemeBlood;
char* COTThemeLime;
char* COTThemeCitrus;
char* COTThemeDooderbutt;
char* COTLangEn;
// COT Settings Theme engine
char* SetThemeDefault;
char* SetThemeBlood;
char* SetThemeLime;
char* SetThemeCitrus;
char* SetThemeDooderbutt;

typedef struct {
    // Common words and phrases
    const char* No;
    const char* Yes;
    const char* BatteryLevel;
    const char* BackMenuDisabled;
    const char* BackMenuEnabled;
    const char* ChoosePackage;
    const char* Install;
    const char* Installing;
    const char* Mounted;
    const char* Unmounted;
    const char* InstallAbort;
    const char* Rebooting;
    const char* ShutDown;
    const char* Enabled;
    const char* Disabled;
    const char* MountError;
    const char* UnmountError;
    const char* SkipFormat;
    const char* InstallConfirm;
    const char* YesInstall;
    const char* DeleteConfirm;
    const char* FreeSpaceSD;
    const char* YesDelete;
    const char* Deleting;
    const char* ConfirmFormat;
    const char* YesFormat;
    const char* Formatting;
    const char* FormatError;
    const char* Done;
    const char* DoneExc;
    const char* Size;
    const char* Boot;
    const char* Recovery;
    const char* NotFound;
    const char* FoundOld;
    const char* On;
    const char* Off;
    // Menu headers
    const char* WipeDataHeader1;
    const char* WipeDataHeader2;
    const char* ZipInstallHeader;
    const char* DeleteBackupHeader;
    const char* LowSpaceHeader1;
    const char* LowSpaceHeader2;
    const char* LowSpaceHeader3;
    const char* LowSpaceHeader4;
    const char* ZipChooseHeader;
    const char* NandroidRestoreHeader;
    const char* USBMSHeader1;
    const char* USBMSHeader2;
    const char* USBMSHeader3;
    const char* Recommended;
    const char* ShowPartitionHeader;
    const char* AdvBackupHeader;
    const char* AdvRestoreHeader1;
    const char* AdvRestoreHeader2;
    const char* AdvRestoreHeader3;
    const char* AdvRestoreHeader11;
    const char* NandroidHeader;
    const char* DebuggingHeader;
    const char* AdvOptionsHeader;
    // Menu items for main screen
    const char* RebootNow;
    const char* WipeDataFactory;
    const char* WipeCache;
    const char* WipeAll;
    const char* InstallZip;
    const char* Nandroid;
    const char* MountsStorage;
    const char* Advanced;
    const char* PowerOff;
    // Menu items for extendedcommands
    const char* SignatureCheck;
    const char* ScriptAsserts;
    const char* ZipChooseZip;
    const char* ZipApplyUpdateZip;
    const char* ZipToggleSig;
    const char* ZipToggleAsserts;
    const char* BackupProceed;
    const char* BackupCancel;
    const char* ZipChooseYesBackup;
    const char* ZipChooseNoBackup;
    const char* ZipCancelInstall;
    const char* USBMSUnmount;
    const char* NandroidBackup;
    const char* NandroidRestore;
    const char* NandroidAdvBackup;
    const char* NandroidAdvRestore;
    const char* NandroidDeleteOld;
    const char* DebugFixPerm;
    const char* DebugFixLoop;
    const char* DebugReportError;
    const char* DebugKeyTest;
    const char* DebugShowLog;
    const char* DebugToggleDebugUI;
    const char* AdvReboot;
    const char* AdvWipeDalvik;
    const char* AdvPartitionSD;
    const char* AdvCOTSettings;
    const char* AdvDebugOpts;
    // Partition wipe prompts
    const char* YesDeleteData;
    const char* WipingData;
    const char* DataWipeComplete;
    const char* DataWipeSkip;
    const char* DataWipeFail;
    const char* ConfirmWipe;
    const char* YesWipeCache;
    const char* WipingCache;
    const char* CacheWipeComplete;
    const char* CacheWipeSkip;
    const char* CacheWipeFail;
    const char* ConfirmWipeAll;
    const char* YesWipeAll;
    const char* WipingAll;
    const char* WipeAllComplete;
    const char* YesWipeDalvik;
    const char* WipingDalvik;
    const char* WipeDalvikComplete;
    const char* WipeDalvikSkip;
    const char* PartitioningSD;
    const char* PartitionSDError;
    // Miscellaneous prompts
    const char* RebootingSystemTimed;
    const char* DirOpenFail;
    const char* NoFilesFound;
    const char* EXT4Checking;
    // ORS-specific prompts
    const char* ORSSDMountWait;
    const char* ORSSDMounted;
    const char* ORSSDContinuing;
    const char* ORSCommandIs;
    const char* ORSNoValue;
    const char* ORSInstallingZip;
    const char* ORSZipInstallError;
    const char* ORSRecursiveMkdir;
    const char* ORSRebootFound;
    const char* ORSCmdNoValue;
    const char* ORSUnknownCmd;
    const char* ORSScriptDone;
    const char* ORSScriptError;
    // Nandroid prompts
    const char* NandroidBackupFolderSet;
    const char* NandroidBackupComplete;
    const char* NandroidSettingRestoreOptions;
    const char* NandroidRestoreComplete;
    const char* NandroidBackupDeleteComplete;
    const char* NandroidConfirmRestore;
    const char* NandroidYesRestore;
    const char* Backup;
    const char* Restore;
    const char* PerformBackup;
    const char* NandroidBackingUp;
    const char* NandroidCantMount;
    const char* NandroidYAFFS2Error;
    const char* NandroidSDFreeSpace;
    const char* BootDumpError;
    const char* RecoveryDumpError;
    const char* NandroidAndSecNotFound;
    const char* NandroidSDExtMountFail;
    const char* NandroidMD5Generate;
    const char* NandroidMD5Check;
    const char* NandroidMD5Fail;
    const char* NandroidTarNotFound;
    const char* NandroidImgNotFound;
    const char* Restoring;
    const char* FormatError;
    const char* RestoreError;
    const char* NandroidEraseBoot;
    const char* NandroidRestoringBoot;
    const char* NandroidBootFlashError;
    const char* NandroidNoBootImg;
    // Secure FS prompts
    const char* SecureFSEnable;
    const char* SecureFSDisable;
    const char* SecureFSInvalid;
    const char* SecureFSAbortDefault;
    const char* SecureFSAbort;
    const char* SecureFSUpdate;
    // Debugging prompts
    const char* UIDebugEnable;
    const char* UIDebugDisable;
    const char* FixingPerm;
    const char* OutputKeyCodes;
    const char* KCGoBack;
    // ZIP flashing prompts
    const char* InstallComplete;
    const char* YesInstallUpdate;
    // Format prompts
    const char* A2SDNotFound;
    const char* Device;
    const char* UnmountError;
    const char* EXT3;
    const char* EXT2;
    // BML prompts
    const char* BMLChecking;
    const char* BMLMaybeRFS;
    // FS convert prompt
    const char* FSConv1;
    const char* FSConv2;
    const char* FSConv3;
    const char* FSConv4;
    const char* FSConv5;
    const char* FSConv6;
    // Failure prompt
    const char* FailPrompt;
    // update.zip messages
    const char* InstallingUpdate;
    const char* FindUpdatePackage;
    const char* OpenUpdatePackage;
    const char* VerifyUpdatePackage;
    // Edify prompts
    const char* EdifyFormatting;
    const char* EdifyFormatDataData;
    const char* EdifyWaitSDMount;
    const char* EdifySDMounted;
    const char* EdifySDTimeOut;
    const char* EdifySDVerifyMarker;
    const char* EdifySDMarkerNotFound;
    const char* EdifyCheckInternalMarker;
    const char* EdifyInternalMarkerNotFound;
    const char* EdifyRMScriptError;
    // Commands
    const char* FormatCmd;
    const char* DeleteCmd;
    const char* CopyCmd;
    const char* FirmWriteExtracting;
    const char* FirmWriteImage;
    const char* WriteImage;
    // COT Settings
    const char* SettingsLoadError1;
    const char* SettingsLoadError2;
    const char* SettingsLoaded;
    const char* SettingsThemeError;
    const char* SettingsThemeLoaded;
    // COT Settings Headers
    const char* COTMainHeader;
    const char* COTThemeHeader;
    const char* COTORSRebootHeader;
    const char* COTORSWipePromptHeader;
    const char* COTZipPromptHeader;
    const char* COTLangHeader;
    // COT Settings List Items
    const char* COTMainListTheme;
    const char* COTMainListRebootForce;
    const char* COTMainListWipePrompt;
    const char* COTMainListZipPrompt;
    const char* COTMainListLanguage;
    const char* COTThemeHydro;
    const char* COTThemeBlood;
    const char* COTThemeLime;
    const char* COTThemeCitrus;
    const char* COTThemeDooderbutt;
    const char* COTLangEn;
    // COT Settings Theme engine
    const char* SetThemeDefault;
    const char* SetThemeBlood;
    const char* SetThemeLime;
    const char* SetThemeCitrus;
    const char* SetThemeDooderbutt;
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
	} else if (MATCH("settings", "unmounterror")) {
		pconfig->unmounterror = strdup(value);
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
	} else if (MATCH("settings", "poweroff")) {
		pconfig->poweroff = strdup(value);
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
	} else if (MATCH("settings", "formaterror")) {
		pconfig->formaterror = strdup(value);
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

	ini_parse("/res/lang/lang_en.ini", language_handler, &config)
	LOGI("English language loaded!\n");
	No = config.no;
	Yes = config.yes;
	BatteryLevel = config.batterylevel;
	BackMenuDisabled = config.backmenudisabled;
	BackMenuEnabled = config.backmenuenabled;
	ChoosePackage = config.choosepackage;
	Install = config.install;
	Installing = config.installing;
	Mounted = config.mounted;
	Unmounted = config.unmounted;
	InstallAbort = config.installabort;
	Rebooting = config.rebooting;
	ShutDown = config.shutdown;
	Enabled = config.enabled;
	Disabled = config.disabled;
	MountError = config.mounterror;
	UnmountError = config.unmounterror;
	SkipFormat = config.skipformat;
	InstallConfirm = config.installconfirm;
	YesInstall = config.yesinstall;
	DeleteConfirm = config.deleteconfirm;
	FreeSpaceSD = config.freespacesd;
	YesDelete = config.yesdelete;
	Deleting = config.deleting;
	ConfirmFormat = config.confirmformat;
	YesFormat = config.yesformat;
	Formatting = config.formatting;
	FormatError = config.formaterror;
	Done = config.done;
	DoneExc = config.doneexc;
	Size = config.size;
	Boot = config.boot;
	Recovery = config.recovery;
	NotFound = config.notfound;
	FoundOld = config.foundold;
	On = config.on;
	Off = config.off;
	WipeDataHeader1 = config.wipedataheader1;
	WipeDataHeader2 = config.wipedataheader2;
	ZipInstallHeader = config.zipinstallheader;
	DeleteBackupHeader = config.deletebackupheader;
	LowSpaceHeader1 = config.lowspaceheader1;
	LowSpaceHeader2 = config.lowspaceheader2;
	LowSpaceHeader3 = config.lowspaceheader3;
	LowSpaceHeader4 = config.lowspaceheader4;
	ZipChooseHeader = config.zipchooseheader;
	NandroidRestoreHeader = config.nandroidrestoreheader;
	USBMSHeader1 = config.usbmsheader1;
	USBMSHeader2 = config.usbmsheader2;
	USBMSHeader3 = config.usbmsheader3;
	Recommended = config.recommended;
	ShowPartitionHeader = config.showpartitionheader;
	AdvBackupHeader = config.advbackupheader;
	AdvRestoreHeader1 = config.advrestoreheader1;
	AdvRestoreHeader2 = config.advrestoreheader2;
	AdvRestoreHeader3 = config.advrestoreheader3;
	AdvRestoreHeader11 = config.advrestoreheader11;
	NandroidHeader = config.nandroidheader;
	DebuggingHeader = config.debuggingheader;
	AdvOptionsHeader = config.advoptionsheader;
	RebootNow = config.rebootnow;
	WipeDataFactory = config.wipedatafactory;
	WipeCache = config.wipecache;
	WipeAll = config.wipeall;
	InstallZip = config.installzip;
	Nandroid = config.nandroid;
	MountsStorage = config.mountsstorage;
	Advanced = config.advanced;
	PowerOff = config.poweroff;
	SignatureCheck = config.signaturecheck;
	ScriptAsserts = config.scriptasserts;
	ZipChooseZip = config.zipchoosezip;
	ZipApplyUpdateZip = config.zipapplyupdatezip;
	ZipToggleSig = config.ziptogglesig;
	ZipToggleAsserts = config.ziptoggleasserts;
	BackupProceed = config.backupproceed;
	BackupCancel = config.backupcancel;
	ZipChooseYesBackup = config.zipchooseyesbackup;
	ZipChooseNoBackup = config.zipchoosenobackup;
	ZipCancelInstall = config.zipcancelinstall;
	USBMSUnmount = config.usbmsunmount;
	NandroidBackup = config.nandroidbackup;
	NandroidRestore = config.nandroidrestore;
	NandroidAdvBackup = config.nandroidadvbackup;
	NandroidAdvRestore = config.nandroidadvrestore;
	NandroidDeleteOld = config.nandroiddeleteold;
	DebugFixPerm = config.debugfixperm;
	DebugFixLoop = config.debugfixloop;
	DebugReportError = config.debugreporterror;
	DebugKeyTest = config.debugkeytest;
	DebugShowLog = config.debugshowlog;
	DebugToggleDebugUI = config.debugtoggledebugui;
	AdvReboot = config.advreboot;
	AdvWipeDalvik = config.advwipedalvik;
	AdvPartitionSD = config.advpartitionsd;
	AdvCOTSettings = config.advcotsettings;
	AdvDebugOpts = config.advdebugopts;
	YesDeleteData = config.yesdeletedata;
	WipingData = config.wipingdata;
	DataWipeComplete = config.datawipecomplete;
	DataWipeSkip = config.datawipeskip;
	DataWipeFail = config.datawipefail;
	ConfirmWipe = config.confirmwipe;
	YesWipeCache = config.yeswipecache;
	WipingCache = config.wipingcache;
	CacheWipeComplete = config.cachewipecomplete;
	CacheWipeSkip = config.cachewipeskip;
	CacheWipeFail = config.cachewipefail;
	ConfirmWipeAll = config.confirmwipeall;
	YesWipeAll = config.yeswipeall;
	WipingAll = config.wipingall;
	WipeAllComplete = config.wipeallcomplete;
	YesWipeDalvik = config.yeswipedalvik;
	WipingDalvik = config.wipingdalvik;
	WipeDalvikComplete = config.wipedalvikcomplete;
	WipeDalvikSkip = config.wipedalvikskip;
	PartitioningSD = config.partitioningsd;
	PartitionSDError = config.partitionsderror;
	RebootingSystemTimed = config.rebootingsystemtimed;
	DirOpenFail = config.diropenfail;
	NoFilesFound = config.nofilesfound;
	EXT4Checking = config.ext4checking;
	ORSSDMountWait = config.orssdmountwait;
	ORSSDMounted = config.orssdmounted;
	ORSSDContinuing = config.orssdcontinuing;
	ORSCommandIs = config.orscommandis;
	ORSNoValue = config.orsnovalue;
	ORSInstallingZip = config.orsinstallingzip;
	ORSZipInstallError = config.orszipinstallerror;
	ORSRecursiveMkdir = config.orsrecursivemkdir;
	ORSRebootFound = config.orsrebootfound;
	ORSCmdNoValue = config.orscmdnovalue;
	ORSUnknownCmd = config.orsunknowncmd;
	ORSScriptDone = config.orsscriptdone;
	ORSScriptError = config.orsscripterror;
	NandroidBackupFolderSet = config.nandroidbackupfolderset;
	NandroidBackupComplete = config.nandroidbackupcomplete;
	NandroidSettingRestoreOptions = config.nandroidsettingrestoreoptions;
	NandroidRestoreComplete = config.nandroidrestorecomplete;
	NandroidBackupDeleteComplete = config.nandroidbackupdeletecomplete;
	NandroidConfirmRestore = config.nandroidconfirmrestore;
	NandroidYesRestore = config.nandroidyesrestore;
	Backup = config.backup;
	Restore = config.restore;
	PerformBackup = config.performbackup;
	NandroidBackingUp = config.nandroidbackingup;
	NandroidCantMount = config.nandroidcantmount;
	NandroidYAFFS2Error = config.nandroidyaffs2error;
	NandroidSDFreeSpace = config.nandroidsdfreespace;
	BootDumpError = config.bootdumperror;
	RecoveryDumpError = config.recoverydumperror;
	NandroidAndSecNotFound = config.nandroidandsecnotfound;
	NandroidSDExtMountFail = config.nandroidsdextmountfail;
	NandroidMD5Generate = config.nandroidmd5generate;
	NandroidMD5Check = config.nandroidmd5check;
	NandroidMD5Fail = config.nandroidmd5fail;
	NandroidTarNotFound = config.nandroidtarnotfound;
	NandroidImgNotFound = config.nandroidimgnotfound;
	Restoring = config.restoring;
	FormatError = config.formaterror;
	RestoreError = config.restoreerror;
	NandroidEraseBoot = config.nandroideraseboot;
	NandroidRestoringBoot = config.nandroidrestoringboot;
	NandroidBootFlashError = config.nandroidbootflasherror;
	NandroidNoBootImg = config.nandroidnobootimg;
	SecureFSEnable = config.securefsenable;
	SecureFSDisable = config.securefsdisable;
	SecureFSInvalid = config.securefsinvalid;
	SecureFSAbortDefault = config.securefsabortdefault;
	SecureFSAbort = config.securefsabort;
	SecureFSUpdate = config.securefsupdate;
	UIDebugEnable = config.uidebugenable;
	UIDebugDisable = config.uidebugdisable;
	FixingPerm = config.fixingperm;
	OutputKeyCodes = config.outputkeycodes;
	KCGoBack = config.kcgoback;
	InstallComplete = config.installcomplete;
	YesInstallUpdate = config.yesinstallupdate;
	A2SDNotFound = config.a2sdnotfound;
	Device = config.device;
	UnmountError = config.unmounterror;
	EXT3 = config.ext3;
	EXT2 = config.ext2;
	BMLChecking = config.bmlchecking;
	BMLMaybeRFS = config.bmlmayberfs;
	FSConv1 = config.fsconv1;
	FSConv2 = config.fsconv2;
	FSConv3 = config.fsconv3;
	FSConv4 = config.fsconv4;
	FSConv5 = config.fsconv5;
	FSConv6 = config.fsconv6;
	FailPrompt = config.failprompt;
	InstallingUpdate = config.installingupdate;
	FindUpdatePackage = config.findupdatepackage;
	OpenUpdatePackage = config.openupdatepackage;
	VerifyUpdatePackage = config.verifyupdatepackage;
	EdifyFormatting = config.edifyformatting;
	EdifyFormatDataData = config.edifyformatdatadata;
	EdifyWaitSDMount = config.edifywaitsdmount;
	EdifySDMounted = config.edifysdmounted;
	EdifySDTimeOut = config.edifysdtimeout;
	EdifySDVerifyMarker = config.edifysdverifymarker;
	EdifySDMarkerNotFound = config.edifysdmarkernotfound;
	EdifyCheckInternalMarker = config.edifycheckinternalmarker;
	EdifyInternalMarkerNotFound = config.edifyinternalmarkernotfound;
	EdifyRMScriptError = config.edifyrmscripterror;
	FormatCmd = config.formatcmd;
	DeleteCmd = config.deletecmd;
	CopyCmd = config.copycmd;
	FirmWriteExtracting = config.firmwriteextracting;
	FirmWriteImage = config.firmwriteimage;
	WriteImage = config.writeimage;
	SettingsLoadError1 = config.settingsloaderror1;
	SettingsLoadError2 = config.settingsloaderror2;
	SettingsLoaded = config.settingsloaded;
	SettingsThemeError = config.settingsthemeerror;
	SettingsThemeLoaded = config.settingsthemeloaded;
	COTMainHeader = config.cotmainheader;
	COTThemeHeader = config.cotthemeheader;
	COTORSRebootHeader = config.cotorsrebootheader;
	COTORSWipePromptHeader = config.cotorswipepromptheader;
	COTZipPromptHeader = config.cotzippromptheader;
	COTLangHeader = config.cotlangheader;
	COTMainListTheme = config.cotmainlisttheme;
	COTMainListRebootForce = config.cotmainlistrebootforce;
	COTMainListWipePrompt = config.cotmainlistwipeprompt;
	COTMainListZipPrompt = config.cotmainlistzipprompt;
	COTMainListLanguage = config.cotmainlistlanguage;
	COTThemeHydro = config.cotthemehydro;
	COTThemeBlood = config.cotthemeblood;
	COTThemeLime = config.cotthemelime;
	COTThemeCitrus = config.cotthemecitrus;
	COTThemeDooderbutt = config.cotthemedooderbutt;
	COTLangEn = config.cotlangen;
	SetThemeDefault = config.setthemedefault;
	SetThemeBlood = config.setthemeblood;
	SetThemeLime = config.setthemelime;
	SetThemeCitrus = config.setthemecitrus;
	SetThemeDooderbutt = config.setthemedooderbutt;
}
