# every kext a nix build produces, the KC lists and per-kext derivations come from here
# kc entries come first, in x86_64 KC link order
#   name: bundle, dir: under src/Kernel/Extensions, shared dirs share a derivation
#   target: cmake target if not the bundle name, headers: kext dirs it includes (transitive)
#   extra: other source paths from the repo root, arches: builds that produce it
#   kc: arches whose boot KC lists it, plistOnly: arches that only configure it
#   headerTargets: its CMakeLists.txt defines header targets, then stops for dependents
let
  x86 = [ "x86_64" ];
  arm = [ "arm64" ];
  x86arm = [ "x86_64" "arm64" ];
  all = [ "x86_64" "arm64" "riscv64" ];
  x86riscv = [ "x86_64" "riscv64" ];
  armriscv = [ "arm64" "riscv64" ];
  fbsdkpi = [ "src/Kernel/fbsdkpi/fbsdkpi.cmake" "src/Kernel/fbsdkpi/freebsd" "src/Kernel/fbsdkpi/haiku"
    "src/Kernel/fbsdkpi/pd" "src/Kernel/fbsdkpi/xnu" "src/Kernel/fbsdkpi/pd_dri_cdev.h"
    "src/Kernel/fbsdkpi/pd_fbsd_os.h" ];
in
[
  { name = "corecrypto.kext"; dir = "corecrypto"; arches = all; kc = x86; }
  { name = "pthread.kext"; dir = "pthread"; arches = all; kc = x86; }
  { name = "amfi.kext"; dir = "amfi"; arches = all; kc = x86; }
  { name = "Sandbox.kext"; dir = "Sandbox"; extra = [ "src/Libraries/libsandbox" ]; arches = all; kc = x86; }
  { name = "IOACPIFamily.kext"; dir = "IOACPIFamily"; headerTargets = true; target = "IOACPIFamily"; arches = x86; kc = x86; }
  { name = "PDACPIPlatform.kext"; dir = "PDACPIPlatform"; target = "PDACPIPlatform";
    headers = [ "IOACPIFamily" "AppleAPIC" ]; arches = x86; kc = x86; }
  { name = "IOPCIFamily.kext"; dir = "IOPCIFamily"; headerTargets = true; target = "IOPCIFamily";
    headers = [ "IOACPIFamily" ]; arches = all; kc = x86; }
  { name = "AppleAPIC.kext"; dir = "AppleAPIC"; target = "AppleAPIC"; arches = x86; kc = x86; }
  # left out of the KC
  { name = "AppleI386GenericPlatform.kext"; dir = "AppleI386GenericPlatform"; target = "AppleI386GenericPlatform";
    headers = [ "AppleAPIC" ]; arches = x86; }
  { name = "AppleI386PCI.kext"; dir = "AppleI386PCI"; target = "AppleI386PCI";
    headers = [ "IOPCIFamily" ]; arches = x86; kc = x86; }
  { name = "IOStorageFamily.kext"; dir = "IOStorageFamily"; headerTargets = true; target = "IOStorageFamily"; arches = all; kc = x86; }
  { name = "IONVMEFamily.kext"; dir = "IONVMEFamily"; headers = [ "IOPCIFamily" "IOStorageFamily" ];
    arches = x86arm; kc = x86; }
  { name = "IOATAFamily.kext"; dir = "IOATAFamily"; headerTargets = true; target = "IOATAFamily";
    headers = [ "IOStorageFamily" ]; arches = x86; kc = x86; }
  { name = "AppleIntelPIIXATA.kext"; dir = "AppleIntelPIIXATA"; target = "AppleIntelPIIXATA";
    headers = [ "IOACPIFamily" "IOATAFamily" "IOPCIFamily" "IOStorageFamily" ]; arches = x86; kc = x86; }
  { name = "IOATABlockStorage.kext"; dir = "IOATABlockStorage"; headers = [ "IOStorageFamily" "IOATAFamily" ];
    arches = x86; kc = x86; }
  { name = "ext4.kext"; dir = "ext4"; arches = all; kc = x86; }
  { name = "msdosfs.kext"; dir = "msdosfs"; arches = all; kc = x86; }
  { name = "cd9660.kext"; dir = "cd9660"; arches = x86; kc = x86; }
  { name = "apfs.kext"; dir = "apfs"; extra = [ "projects/libapfsrw" ]; arches = all; kc = x86; }
  { name = "HFSEncodings.kext"; dir = "HFSEncodings"; headers = [ "hfs" ]; arches = all; kc = x86; }
  { name = "hfs.kext"; dir = "hfs"; headers = [ "corecrypto" "IOStorageFamily" ]; arches = all; kc = x86; }
  { name = "AppleFileSystemDriver.kext"; dir = "AppleFileSystemDriver"; headers = [ "IOStorageFamily" ];
    arches = all; kc = x86; }
  { name = "Ext4FileSystemDriver.kext"; dir = "Ext4FileSystemDriver"; headers = [ "IOStorageFamily" ];
    arches = all; kc = x86; }
  { name = "ApfsFileSystemDriver.kext"; dir = "ApfsFileSystemDriver"; headers = [ "IOStorageFamily" "apfs" ];
    extra = [ "projects/libapfsrw/include" ]; arches = all; kc = x86; }
  { name = "IOHIDFamily.kext"; dir = "IOHIDFamily"; arches = all; kc = x86; }
  { name = "PDI2CFamily.kext"; dir = "PDI2CFamily"; headerTargets = true; arches = x86arm; kc = x86; }
  { name = "PDDesignWareI2C.kext"; dir = "PDDesignWareI2C"; headers = [ "PDI2CFamily" ]; arches = x86; kc = x86; }
  { name = "PDI2CHID.kext"; dir = "PDI2CHID"; headers = [ "PDI2CFamily" "IOHIDFamily" ]; arches = x86arm; kc = x86; }
  { name = "ApplePS2Controller.kext"; dir = "ApplePS2Controller"; target = "ApplePS2Controller";
    headers = [ "IOHIDFamily" ]; arches = x86; kc = x86; }
  { name = "IOUSBFamily.kext"; dir = "IOUSBFamily"; headerTargets = true; target = "IOUSBFamily";
    headers = [ "IOStorageFamily" "IOPCIFamily" ]; arches = all; kc = x86; }
  { name = "IOUSBCompositeDriver.kext"; dir = "IOUSBFamily/IOUSBCompositeDriver"; headers = [ "IOUSBFamily" ];
    arches = all; kc = x86; }
  { name = "AppleUSBMergeNub.kext"; dir = "IOUSBFamily/AppleUSBMergeNub"; headers = [ "IOUSBFamily" ];
    arches = all; kc = x86; }
  { name = "IOUSBHIDDriver.kext"; dir = "IOUSBFamily/IOUSBHIDDriver"; headers = [ "IOUSBFamily" "IOHIDFamily" ];
    arches = all; kc = x86; }
  { name = "AppleUSBEHCI.kext"; dir = "IOUSBFamily"; arches = x86arm; kc = x86; }
  { name = "AppleUSBOHCI.kext"; dir = "IOUSBFamily"; arches = x86arm; kc = x86; }
  { name = "AppleUSBUHCI.kext"; dir = "IOUSBFamily"; arches = x86; kc = x86; }
  { name = "RavynAHCIPort.kext"; dir = "RavynAHCIPort"; headers = [ "IOPCIFamily" "IOStorageFamily" ];
    arches = x86arm; kc = x86; }
  { name = "RavynXHCIPort.kext"; dir = "RavynXHCIPort";
    headers = [ "IOUSBFamily" "IOHIDFamily" "IOPCIFamily" "IOStorageFamily" ]; arches = x86arm; kc = x86; }
  { name = "IOGraphicsFamily.kext"; dir = "IOGraphicsFamily";
    headers = [ "GMetric" "GTrace" "IOACPIFamily" "IOHIDFamily" "IONDRVSupport" "IOPCIFamily" ];
    extra = [ "src/Kernel/Extensions/GMetric" "src/Kernel/Extensions/GTrace" ]; arches = all; kc = x86; }
  { name = "IOGOPFramebuffer.kext"; dir = "IOGOPFramebuffer"; headers = [ "IOGraphicsFamily" "IOHIDFamily" "IOPCIFamily" ];
    arches = x86arm; kc = x86; }
  # /dev/dri for the drm kexts in the aux KC
  { name = "PDDRMDri.kext"; dir = "PDDRMDri"; extra = [ "src/Kernel/fbsdkpi/pd_dri_cdev.h" "src/Kernel/fbsdkpi/pd_fbsd_os.h" ];
    arches = x86; kc = x86; }
  { name = "IONetworkingFamily.kext"; dir = "IONetworkingFamily"; arches = all; kc = x86; }
  { name = "IOVirtIOFamily.kext"; dir = "IOVirtIOFamily"; headers = [ "IOPCIFamily" ]; arches = all; kc = x86; }
  { name = "IOVirtIOGPU.kext"; dir = "IOVirtIOGPU"; headers = [ "IOGraphicsFamily" "IOHIDFamily" "IOPCIFamily" "IOVirtIOFamily" ];
    extra = [ "src/Libraries/PDSurface/include" ]; arches = all; kc = x86; }
  { name = "IOVirtIONet.kext"; dir = "IOVirtIONet"; headers = [ "IONetworkingFamily" "IOPCIFamily" "IOVirtIOFamily" ];
    arches = all; kc = x86; }
  { name = "IOVirtIOBlock.kext"; dir = "IOVirtIOBlock"; headers = [ "IOPCIFamily" "IOStorageFamily" "IOVirtIOFamily" ];
    arches = all; kc = x86; }
  { name = "PDHyperV.kext"; dir = "PDHyperV"; headers = [ "IONetworkingFamily" "IOStorageFamily" ]; arches = x86; kc = x86; }
  { name = "PDRadeonFramebuffer.kext"; dir = "PDRadeonFramebuffer"; headers = [ "IOGraphicsFamily" "IOHIDFamily" "IOPCIFamily" ];
    arches = x86; kc = x86; }
  { name = "PDAmdGPU.kext"; dir = "PDAmdGPU"; headers = [ "IOGraphicsFamily" "IOHIDFamily" "IOPCIFamily" ];
    arches = x86; kc = x86; }
  { name = "PDE1000.kext"; dir = "PDE1000"; headers = [ "IONetworkingFamily" "IOPCIFamily" ]; arches = x86arm; kc = x86; }
  { name = "PDFBSDem.kext"; dir = "PDFBSDem"; headers = [ "IONetworkingFamily" "IOPCIFamily" ]; extra = fbsdkpi;
    arches = x86arm; kc = x86; }
  { name = "PDFBSDigc.kext"; dir = "PDFBSDigc"; headers = [ "IONetworkingFamily" "IOPCIFamily" ]; extra = fbsdkpi;
    arches = x86arm; kc = x86; }
  # freebsd ix for the intel 10g nics, loaded from the gpu images' aux KC like rtwn
  { name = "PDFBSDix.kext"; dir = "PDFBSDix"; headers = [ "IONetworkingFamily" "IOPCIFamily" ]; extra = fbsdkpi;
    arches = x86; kc = [ ]; }
  { name = "PDFBSDrge.kext"; dir = "PDFBSDrge"; headers = [ "IONetworkingFamily" "IOPCIFamily" ]; extra = fbsdkpi;
    arches = x86arm; kc = x86; }
  { name = "PDRealtek8111.kext"; dir = "PDRealtek8111"; headers = [ "IONetworkingFamily" "IOPCIFamily" ];
    arches = x86arm; kc = x86; }
  { name = "IO80211Family.kext"; dir = "IO80211Family"; headerTargets = true; headers = [ "IONetworkingFamily" ]; arches = x86arm; kc = x86; }
  # links against IO80211Family, so it must come after it
  { name = "PDFBSDrtw89.kext"; dir = "PDFBSDrtw89"; headers = [ "IO80211Family" "IOPCIFamily" ]; extra = fbsdkpi;
    arches = x86; kc = x86; }
  # loaded from the gpu images' aux KC, with net80211 and linuxkpi it would push the boot KC past the loader's limit
  { name = "PDFBSDrtwn.kext"; dir = "PDFBSDrtwn"; headers = [ "IO80211Family" "IOPCIFamily" "IOUSBFamily" ]; extra = fbsdkpi;
    arches = x86; kc = [ ]; }
  # freebsd run for the ralink usb dongles, aux KC like rtwn
  { name = "PDFBSDrun.kext"; dir = "PDFBSDrun"; headers = [ "IO80211Family" "IOPCIFamily" "IOUSBFamily" ]; extra = fbsdkpi;
    arches = x86; kc = [ ]; }
  { name = "PDRalinkUSB.kext"; dir = "PDRalinkUSB"; headers = [ "IO80211Family" "IOUSBFamily" ]; arches = x86; kc = x86; }
  { name = "PDQwx.kext"; dir = "PDQwx"; headers = [ "IO80211Family" "IOPCIFamily" ]; arches = x86arm; kc = x86; }
  { name = "PDUSBNCM.kext"; dir = "PDUSBNCM"; headers = [ "IONetworkingFamily" "IOUSBFamily" ]; arches = x86; kc = x86; }
  { name = "IOAudioFamily.kext"; dir = "IOAudioFamily"; arches = x86; kc = x86; }
  { name = "RavynHDAudio.kext"; dir = "RavynHDAudio"; headers = [ "IOAudioFamily" "IOPCIFamily" ]; arches = x86arm; kc = x86; }

  # built but left out of the boot KC
  { name = "ApplePIODMA.kext"; dir = "IOPCIFamily/ApplePIODMA"; target = "ApplePIODMA"; headers = [ "IOPCIFamily" ];
    arches = x86; }
  { name = "IOCDStorageFamily.kext"; dir = "IOCDStorageFamily"; headers = [ "IOStorageFamily" ]; arches = all; }
  { name = "IODVDStorageFamily.kext"; dir = "IODVDStorageFamily"; headers = [ "IOCDStorageFamily" "IOStorageFamily" ];
    arches = all; }
  { name = "IOBDStorageFamily.kext"; dir = "IOBDStorageFamily";
    headers = [ "IOCDStorageFamily" "IODVDStorageFamily" "IOStorageFamily" ]; arches = all; }
  { name = "PDAuxHello.kext"; dir = "PDAuxHello"; arches = x86; }
  { name = "PDWatchdog.kext"; dir = "PDWatchdog"; arches = all; }
  { name = "PDDiskImages2.kext"; dir = "PDDiskImages2"; headers = [ "IOStorageFamily" ]; arches = x86arm; }
  { name = "PDLifs.kext"; dir = "PDLifs"; arches = x86arm; }
  { name = "PDSerialHID.kext"; dir = "PDSerialHID"; headers = [ "IOHIDFamily" ]; arches = x86arm; }
  { name = "PDKeyStore.kext"; dir = "PDKeyStore"; arches = x86arm; }
  { name = "PDIOSurface.kext"; dir = "PDIOSurface"; headers = [ "IOGraphicsFamily" "PDSun60iDisplay" ];
    arches = all; plistOnly = x86; }
  { name = "PDVirtualFramebuffer.kext"; dir = "PDVirtualFramebuffer"; headers = [ "IOGraphicsFamily" "IOHIDFamily" ];
    arches = x86arm; plistOnly = x86; }
  { name = "PDBcm2835SD.kext"; dir = "PDBcm2835SD"; headers = [ "IOStorageFamily" ]; arches = x86arm; plistOnly = x86; }

  # arm64 and riscv64 only
  { name = "PDArmPlatformExpert.kext"; dir = "PDArmPlatformExpert"; target = "PDArmPlatformExpert"; arches = arm; }
  { name = "PDArmPCI.kext"; dir = "PDArmPCI"; target = "PDArmPCI"; headers = [ "IOPCIFamily" ]; arches = arm; }
  { name = "PDEcamPCI.kext"; dir = "PDEcamPCI"; target = "PDEcamPCI"; headers = [ "IOPCIFamily" ]; arches = armriscv; }
  { name = "PDRiscvPlatformExpert.kext"; dir = "PDRiscvPlatformExpert"; target = "PDRiscvPlatformExpert";
    arches = [ "riscv64" ]; }
  { name = "PDSun50iMMC.kext"; dir = "PDSun50iMMC"; headers = [ "IOStorageFamily" ]; arches = arm; }
  { name = "PDSg2002SD.kext"; dir = "PDSg2002SD"; headers = [ "IOStorageFamily" ]; arches = arm; }
  { name = "PDSg2002Eth.kext"; dir = "PDSg2002Eth"; headers = [ "IONetworkingFamily" ]; arches = arm; }
  { name = "PDQcomTLMM.kext"; dir = "PDQcomTLMM"; arches = arm; }
  { name = "PDQcomGeniI2C.kext"; dir = "PDQcomGeniI2C"; headers = [ "PDI2CFamily" ]; arches = arm; }
  { name = "PDPowerVR.kext"; dir = "PDPowerVR"; headers = [ "PDSun60iDisplay" ]; arches = arm; }
  { name = "PDSun60iDisplay.kext"; dir = "PDSun60iDisplay"; arches = arm; }
]
