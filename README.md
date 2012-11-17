<pre> Cannibal Open Touch Recovery
==================================

<ul>
<li>Originally ClockworkMod based recovery we have incorporated and updated touch controls originally written by Napstar of Team Utter Chaos.</li>
<li>Combines features of well-known recoveries like TWRP, ClockworkMod and AmonRA to allow users to easily and effortlessly manage their Android-powered devices.</li>
<li>Learn more about Cannibal Open Touch and Project Open Cannibal at [projectopencannibal.net](http://www.projectopencannibal.net/the-project/) or come join as at our [forums](http://forums.projectopencannibal.net/).</li>
<li>Such features include but are not limited to:</li>
</ul>
	* User defined backup locations.
	* Support for [Open Recovery Scripts](http://www.teamw.in/OpenRecoveryScript).
	* Persistent settings.
	* Sideloadable (sdcard) theme support.
	* ADB Sideload.
<ul>
<li>Please note the following in regards to this branch.</li>
<ul>
	* Inclosed is a hybridized gingerbread recovery with backported ext4 and 'Retouch Binaries' for 3.0 boot image compatibility on the Kindle Fire (first edition).
	* The reboot wrappers and orientation are setup specfically for the Kindle Fire (first edition).
	* Building this branch requires the system/extras repo from Jellybean, specfically 'system/extras/ext4_utils' in order to work.
	* If you are building for the Kindle Fire (first edition) please checkout either our [Hybrid](https://github.com/ProjectOpenCannibal/android_bootable_recovery/tree/hybrid)(stable) or [Jellybean](https://github.com/ProjectOpenCannibal/android_bootable_recovery/tree/jellybean)(experimental) branches instead.
	* Otherwise consider checking out a specific tag (not the ones labeled landscape).
	* This is the current developer branch which means bugs are expected.
	* The following repos are required for all current builds: [Resources](https://github.com/ProjectOpenCannibal/android_bootable_recovery_res) and [Graphics](https://github.com/ProjectOpenCannibal/android_bootable_recovery_gui/tree/gingerbread).
