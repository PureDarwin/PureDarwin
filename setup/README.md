# Creating a bootable PureDarwin image

Before you start, make backups of all your data. Do not use any production machines. **PLEASE DO NOT IGNORE THESE STEPS!**

These instructions are only for technical persons anyway, so they are short. Developers and testers, please provide feedback via the way you prefer. 

## Installation

* Check and edit the `pd_config` configuration file to reflect your needs.
* Get binary roots and binary drivers with `pd_fetch`.
* Thin them with `pd_thin`.
* Then see the usage of `pd_setup':

```
   Set up a bootable PureDarwin system.
   Usage: pd_setup any_output_filename VolumeName

       * Install to physical disk
       pd_setup /Volumes/PureDarwin PureDarwin
       pd_setup /dev/diskX PureDarwin

       * Create an ISO 9660 image (.iso)
       pd_setup /tmp/puredarwin.iso PureDarwin

       * Create a ready-to-run VMware virtual machine (.vmwarevm)
       pd_setup puredarwin.vmwarevm PureDarwin

       * Create a ready-to-run VMware virtual disk (.vmdk)
       pd_setup puredarwin.vmdk PureDarwin
```

## Note

Optionally, it is possible to override few variables present in `pd_config` by exporting them to the environment, allowing some combination.

By default, `PUREDARWIN_RELEASE` is set to "" (full release).

The flow below results in a bootstrap release able to deploy a full release:

```
PUREDARWIN_RELEASE="" ./pd_fetch
PUREDARWIN_RELEASE="" ./pd_thin
PUREDARWIN_RELEASE="bootstrap" ./pd_setup arg1 arg2
```

## Resources

* https://github.com/PureDarwin/PureDarwin
* https://github.com/PureDarwin/PureDarwin/wiki
* http://opensource.apple.com
* http://www.puredarwin.org
* #puredarwin on irc.freenode.net

## Additional Licensing Information

IMPORTANT LICENSING INFORMATION:  The Apple-developed portions of the Source Code and corresponding binary package folders are covered by the Apple Public Source License that can be found in the file `/APPLE_LICENSE.txt`.

The Apple binary drivers and kernel extension files are covered by a separate Apple Binary Driver Software License Agreement that can be found in the file `/APPLE_DRIVER_LICENSE.txt`.

The PureDarwin work is covered by the BSD License that can be found in the
file `/PUREDARWIN_LICENSE.txt`.

Other portions of Darwin may be covered by third party licenses.  Please read these licenses carefully before using any of this software, as your use of this software signifies that you have read the licenses and that you
accept and agree to their respective terms.  Please see the respective projects for more information.

Please read all these licenses carefully before using any of this software,  as your use of this software signifies that you have read the licenses and  that you accept and agree to their respective terms.
