### CyanogenMod Bootable Recovery directory
* This is clockwork based recovery with touch controls written by Napstar.
* The reboot wrappers have been specifically configured for use with the Kindle Fire (originally configured by DooMLoRD).

### Please note:
* While this is based off of the gingerbread branch it includes ICS commit 'Retouch Binaries' for 3.0 boot image compatibility; this requires that 'system/extras/ext4_utils' be replaced by the ICS version.
* These are included in utilities/ext4_utils however I seem to be failing at properly using them instead of the gingerbread native version left for the ROM build. Format functions will if built using the gingerbread ext4_utils; make sure to replace the gingerbread version with the included version until a fix can be worked out (please not this will cause the rom build to fail).
