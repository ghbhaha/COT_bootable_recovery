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

// Common words and phrases
extern char* No;
extern char* Yes;
extern char* BatteryLevel;
extern char* BackMenuDisabled;
extern char* BackMenuEnabled;
extern char* ChoosePackage;
extern char* Install;
extern char* Installing;
extern char* Mounted;
extern char* Unmounted;
extern char* InstallAbort;
extern char* Rebooting;
extern char* ShutDown;
extern char* Enabled;
extern char* Disabled;
extern char* MountError;
extern char* UnmountError;
extern char* SkipFormat;
extern char* InstallConfirm;
extern char* YesInstall;
extern char* DeleteConfirm;
extern char* FreeSpaceSD;
extern char* YesDelete;
extern char* Deleting;
extern char* ConfirmFormat;
extern char* YesFormat;
extern char* Formatting;
extern char* FormatError;
extern char* Done;
extern char* DoneExc;
extern char* Size;
extern char* Boot;
extern char* Recovery;
extern char* NotFound;
extern char* FoundOld;
extern char* On;
extern char* Off;
// Menu headers
extern char* WipeDataHeader1;
extern char* WipeDataHeader2;
extern char* ZipInstallHeader;
extern char* DeleteBackupHeader;
extern char* LowSpaceHeader1;
extern char* LowSpaceHeader2;
extern char* LowSpaceHeader3;
extern char* LowSpaceHeader4;
extern char* ZipChooseHeader;
extern char* NandroidRestoreHeader;
extern char* USBMSHeader1;
extern char* USBMSHeader2;
extern char* USBMSHeader3;
extern char* Recommended;
extern char* ShowPartitionHeader;
extern char* AdvBackupHeader;
extern char* AdvRestoreHeader1;
extern char* AdvRestoreHeader2;
extern char* AdvRestoreHeader3;
extern char* AdvRestoreHeader11;
extern char* NandroidHeader;
extern char* DebuggingHeader;
extern char* AdvOptionsHeader;
// Menu items for main screen
extern char* RebootNow;
extern char* WipeDataFactory;
extern char* WipeCache;
extern char* WipeAll;
extern char* InstallZip;
extern char* Nandroid;
extern char* MountsStorage;
extern char* Advanced;
extern char* PowerOff;
// Menu items for extendedcommands
extern char* SignatureCheck;
extern char* ScriptAsserts;
extern char* ZipChooseZip;
extern char* ZipApplyUpdateZip;
extern char* ZipToggleSig;
extern char* ZipToggleAsserts;
extern char* BackupProceed;
extern char* BackupCancel;
extern char* ZipChooseYesBackup;
extern char* ZipChooseNoBackup;
extern char* ZipCancelInstall;
extern char* USBMSUnmount;
extern char* NandroidBackup;
extern char* NandroidRestore;
extern char* NandroidAdvBackup;
extern char* NandroidAdvRestore;
extern char* NandroidDeleteOld;
extern char* DebugFixPerm;
extern char* DebugFixLoop;
extern char* DebugReportError;
extern char* DebugKeyTest;
extern char* DebugShowLog;
extern char* DebugToggleDebugUI;
extern char* AdvReboot;
extern char* AdvWipeDalvik;
extern char* AdvPartitionSD;
extern char* AdvCOTSettings;
extern char* AdvDebugOpts;
// Partition wipe prompts
extern char* YesDeleteData;
extern char* WipingData;
extern char* DataWipeComplete;
extern char* DataWipeSkip;
extern char* DataWipeFail;
extern char* ConfirmWipe;
extern char* YesWipeCache;
extern char* WipingCache;
extern char* CacheWipeComplete;
extern char* CacheWipeSkip;
extern char* CacheWipeFail;
extern char* ConfirmWipeAll;
extern char* YesWipeAll;
extern char* WipingAll;
extern char* WipeAllComplete;
extern char* YesWipeDalvik;
extern char* WipingDalvik;
extern char* WipeDalvikComplete;
extern char* WipeDalvikSkip;
extern char* PartitioningSD;
extern char* PartitionSDError;
// Miscellaneous prompts
extern char* RebootingSystemTimed;
extern char* DirOpenFail;
extern char* NoFilesFound;
extern char* EXT4Checking;
// ORS-specific prompts
extern char* ORSSDMountWait;
extern char* ORSSDMounted;
extern char* ORSSDContinuing;
extern char* ORSCommandIs;
extern char* ORSNoValue;
extern char* ORSInstallingZip;
extern char* ORSZipInstallError;
extern char* ORSRecursiveMkdir;
extern char* ORSRebootFound;
extern char* ORSCmdNoValue;
extern char* ORSUnknownCmd;
extern char* ORSScriptDone;
extern char* ORSScriptError;
// Nandroid prompts
extern char* NandroidBackupFolderSet;
extern char* NandroidBackupComplete;
extern char* NandroidSettingRestoreOptions;
extern char* NandroidRestoreComplete;
extern char* NandroidBackupDeleteComplete;
extern char* NandroidConfirmRestore;
extern char* NandroidYesRestore;
extern char* Backup;
extern char* Restore;
extern char* PerformBackup;
extern char* NandroidBackingUp;
extern char* NandroidCantMount;
extern char* NandroidYAFFS2Error;
extern char* NandroidSDFreeSpace;
extern char* BootDumpError;
extern char* RecoveryDumpError;
extern char* NandroidAndSecNotFound;
extern char* NandroidSDExtMountFail;
extern char* NandroidMD5Generate;
extern char* NandroidMD5Check;
extern char* NandroidMD5Fail;
extern char* NandroidTarNotFound;
extern char* NandroidImgNotFound;
extern char* Restoring;
extern char* FormatError;
extern char* RestoreError;
extern char* NandroidEraseBoot;
extern char* NandroidRestoringBoot;
extern char* NandroidBootFlashError;
extern char* NandroidNoBootImg;
// Secure FS prompts
extern char* SecureFSEnable;
extern char* SecureFSDisable;
extern char* SecureFSInvalid;
extern char* SecureFSAbortDefault;
extern char* SecureFSAbort;
extern char* SecureFSUpdate;
// Debugging prompts
extern char* UIDebugEnable;
extern char* UIDebugDisable;
extern char* FixingPerm;
extern char* OutputKeyCodes;
extern char* KCGoBack;
// ZIP flashing prompts
extern char* InstallComplete;
extern char* YesInstallUpdate;
// Format prompts
extern char* A2SDNotFound;
extern char* Device;
extern char* UnmountError;
extern char* EXT3;
extern char* EXT2;
// BML prompts
extern char* BMLChecking;
extern char* BMLMaybeRFS;
// FS convert prompt
extern char* FSConv1;
extern char* FSConv2;
extern char* FSConv3;
extern char* FSConv4;
extern char* FSConv5;
extern char* FSConv6;
// Failure prompt
extern char* FailPrompt;
// update.zip messages
extern char* InstallingUpdate;
extern char* FindUpdatePackage;
extern char* OpenUpdatePackage;
extern char* VerifyUpdatePackage;
// Edify prompts
extern char* EdifyFormatting;
extern char* EdifyFormatDataData;
extern char* EdifyWaitSDMount;
extern char* EdifySDMounted;
extern char* EdifySDTimeOut;
extern char* EdifySDVerifyMarker;
extern char* EdifySDMarkerNotFound;
extern char* EdifyCheckInternalMarker;
extern char* EdifyInternalMarkerNotFound;
extern char* EdifyRMScriptError;
// Commands
extern char* FormatCmd;
extern char* DeleteCmd;
extern char* CopyCmd;
extern char* FirmWriteExtracting;
extern char* FirmWriteImage;
extern char* WriteImage;
// COT Settings
extern char* SettingsLoadError1;
extern char* SettingsLoadError2;
extern char* SettingsLoaded;
extern char* SettingsThemeError;
extern char* SettingsThemeLoaded;
// COT Settings Headers
extern char* COTMainHeader;
extern char* COTThemeHeader;
extern char* COTORSRebootHeader;
extern char* COTORSWipePromptHeader;
extern char* COTZipPromptHeader;
extern char* COTLangHeader;
// COT Settings List Items
extern char* COTMainListTheme;
extern char* COTMainListRebootForce;
extern char* COTMainListWipePrompt;
extern char* COTMainListZipPrompt;
extern char* COTMainListLanguage;
extern char* COTThemeHydro;
extern char* COTThemeBlood;
extern char* COTThemeLime;
extern char* COTThemeCitrus;
extern char* COTThemeDooderbutt;
extern char* COTLangEn;
// COT Settings Theme engine
extern char* SetThemeDefault;
extern char* SetThemeBlood;
extern char* SetThemeLime;
extern char* SetThemeCitrus;
extern char* SetThemeDooderbutt;
