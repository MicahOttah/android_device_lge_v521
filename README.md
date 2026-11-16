# android_device_lge_v521

Device tree for the **LG G Pad X 8.0 (V521)** on **LineageOS 15.1**
(Android 8.1), started from the upstream `LineageOS/android_device_lge_v521`
`lineage-15.1` branch.

| | |
| --- | --- |
| Device | LG G Pad X 8.0 (V521), model `b3_tmo_us` |
| SoC | Qualcomm MSM8952 (Snapdragon 625), armv7 32-bit kernel |
| Android | 8.1.0, LineageOS 15.1, userdebug |
| Upstream base | `LineageOS/android_device_lge_v521` @ `127dea5` (`lineage-15.1`) |
| Port branch | `lineage-15.1` (commits in *Commit history*) |

## Status

| Area | State |
| --- | --- |
| Boot, SELinux | working - enforcing, no third-party denials |
| Radio, LTE data | working - voice + data in service |
| Camera | working - both sensors, stills, RAW, scene modes |
| Sensors, auto-rotation | working - 26/26 running |
| Wi-Fi | working |
| DPM (`dpmd`) | working - daemon running, NSRM in place |
| Widevine | working - **L1** (hardware backed) under enforcing |
| IR | working - the LED transmits (checked with a camera); needs the IR clock table in the kernel |
| USB diag port | working - only while USB debugging is enabled (`adb`, `mtp,adb`, `ptp,adb`); the modem answers |
| TWRP recovery | working - built from this tree, protected |
| Build revision | working - `v521-15.1-buildN` in the build number and in the OTA name |
| Swap (zRAM / SD card) | SD card tested by hand, ROM build pending |

## Repositories

Everything the port needs is on GitHub under `MicahOttah`.

| Repository | Branch | Contents |
| --- | --- | --- |
| [`android_device_lge_v521`](https://github.com/MicahOttah/android_device_lge_v521) (this one) | `lineage-15.1` | The device tree and the whole 15.1 port. |
| [`proprietary_vendor_lge`](https://github.com/MicahOttah/proprietary_vendor_lge) | `lineage-15.1` | The v521 blobs: CAF protobuf rename, OEM crypto library, DPM daemon. |
| [`android_device_lge_common`](https://github.com/MicahOttah/android_device_lge_common) | `lineage-15.1-v521` | Skips LG's own consumer IR service for the v521. |
| [`android_system_core`](https://github.com/MicahOttah/android_system_core) | `lineage-15.1-v521` | Stops the system from overwriting the recovery partition. |
| [`android_kernel_lge_msm8952`](https://github.com/MicahOttah/android_kernel_lge_msm8952) | `lineage-15.1-v521` | The 64-bit `get_user()` path, no 32-bit binder, the IR carrier rates of the SPI clock. |
| [`android_vendor_lineage`](https://github.com/MicahOttah/android_vendor_lineage) | `lineage-15.1-v521` | `WITH_ADB_INSECURE=true` support. |
| [`android_frameworks_base`](https://github.com/MicahOttah/android_frameworks_base) | `lineage-15.1-v521` | The Android-7.0-era compatibility shims for the stock CNE blobs. |
| [`android_frameworks_native`](https://github.com/MicahOttah/android_frameworks_native) | `lineage-15.1-v521` | Gralloc debug log lines. |
| [`android_hardware_interfaces`](https://github.com/MicahOttah/android_hardware_interfaces) | `lineage-15.1-v521` | Gralloc debug log lines. |
| [`android_hardware_qcom_display`](https://github.com/MicahOttah/android_hardware_qcom_display) | `lineage-15.1-v521-msm8952` | Gralloc debug log lines for the msm8952. |
| [`android_hardware_ril`](https://github.com/MicahOttah/android_hardware_ril) | `lineage-15.1-caf` | The CAF RIL: SIM_IO, command mapping, signal strength, data registration. |
| [`android_bootable_recovery`](https://github.com/MicahOttah/android_bootable_recovery) | `lineage-15.1-v521` | The stock LineageOS recovery: no silent wipe, no reboot out of it. |
| [`android_bootable_recovery-twrp`](https://github.com/MicahOttah/android_bootable_recovery-twrp) | `lineage-15.1-v521` | The TWRP crash handler. Flattened snapshot, see *Known issues*. |

## What works

* **SELinux enforcing.** `BoardConfig.mk` adds
  `androidboot.selinux=enforcing` to the kernel cmdline; the device reports
  `getenforce` = `Enforcing`.  Getting there needed an explicit
  `file_contexts` entry for each of the 21 CAF vendor daemons, because AOSP
  `init` takes the domain of a service from the file context of the
  executable - and the v521 blob list installs them into `/system/bin`, not
  `/system/vendor/bin` where the CAF policy expects them.
* **The sensor stack.** `dumpsys sensorservice` reports
  `Total 26 h/w sensors, 26 running`.  With 26 sensors the accelerometer is
  present and the **auto-rotation works**.  The board has no gyroscope (the
  accelerometer is an STMicroelectronics K2HHTR), so the 27th entry of the
  old framework list ("GeoMag Rotation Vector Sensor", handle `0x5f67656f`)
  was a fusion pseudo-sensor and not a real one.
* **The camera.** Both sensors work - back and front - including preview,
  stills, RAW capture and the scene modes.  `dumpsys media.camera` reports
  `Number of camera devices: 2`.  The probed sensor driver is the Samsung
  `s5k5e2` (`hi553`).
* **The radio and mobile data.** The SIM registers on LTE, the EF files are
  readable, and the data path comes up on `rmnet_data0`:

  ```
  init.svc.ril-daemon=running
  mVoiceRegState=0(IN_SERVICE)  mDataRegState=0(IN_SERVICE)  mDataConnectionState=2
  mRilVoiceRadioTechnology=14(LTE)  mRilDataRadioTechnology=14(LTE)
  47 EF reads succeed (responseIccIo: e=0)
  ICCID 8901260026190481359   mcc 310  mnc 260   gsm.operator.numeric=310260
  gsm.network.type=LTE   gsm.sim.state=READY
  rmnet_data0: up
  ```

  Getting there needed three pieces of work, each in its own repository:
  1. the CAF protobuf runtime has to be renamed so that the CAF blobs and
     the AOSP ones do not fight over the same symbols, and the CAF blobs
     must not pull `libmedia.so` back in (`proprietary_vendor_lge`);
  2. the HIDL RIL has to hand the vendor the CAF `RIL_SIM_IO` layout and
     the CAF command numbers, and has to fall back to the voice
     registration state for the data registration request, which CAF does
     not implement (`android_hardware_ril`);
  3. the vendor blob list must **not** install LG's stock CAF
     `libril.so` into `/system/lib` - it shadows the LineageOS
     `hardware/ril-caf` one in `/system/vendor/lib` and the radio daemon
     then cannot link (`ril_service_name`).
* **DPM (device policy manager).** `dpmd` is installed as `/system/bin/dpmd`
  with the NSRM configuration and the TCM client, and it runs:

  ```
  init.svc.dpmd=running
  persist.dpm.feature=11
  persist.sys.dpmd.tcm=2
  ```
* **Widevine L1, under enforcing.** The CDM reports `securityLevel = L1`
  (hardware backed) and the ClearKey plugin keeps working next to it.  That
  needed three changes:
  1. **the CDM has to be loadable at all.**  `libwvdrmengine.so` is the
     Google Widevine CDM and it imports 46 protobuf symbols - 8 more than
     the AOSP runtime provides.  The stock CAF runtime is therefore
     installed under a second name,
     `/system/vendor/lib/libqc-protobuf-lite.so`, and the CDM's `DT_NEEDED`
     entry is rewritten to point at it.  The new name is one character
     *shorter*, so the string table does not have to grow and the binary
     keeps its size and layout.  On Android 8.1 the DRM plugins are loaded
     into the HIDL service, which has no AOSP protobuf runtime in it, so
     the two runtimes never meet.
  2. **the OEM crypto library.**  The CDM loads Qualcomm's `liboemcrypto.so`
     at run time (it is not a `DT_NEEDED` entry) and it is the piece that
     reaches the TEE.  Without it the CDM cannot initialise its crypto
     backend at all.
  3. **the SELinux rules for the provisioning.**  The key box is a separate
     TEE application that the CDM loads through the QSEE client library:
     `/vendor/firmware/widevine.mdt -> /persist-lg/firmware/widevine.mdt`.
     The CAF policy only covered `firmware_file:dir` and
     `firmware_file:file`, not the symlink and not the target, so the DRM
     service was stopped at the link:

     ```
     avc: denied { read } for comm="drm@1.0-service" name="widevine.mdt"
         scontext=u:r:hal_drm_default:s0 tcontext=u:object_r:firmware_file:s0
         tclass=lnk_file
     E QSEECOMAPI: : Error::Cannot open the file /vendor/firmware/widevine.mdt
     ```

     `sepolicy/drm.te` adds the missing `firmware_file:lnk_file` and
     `persist_drm_file` rules.  Without them the same build reports **L3**
     (software only) - the three rules are exactly the difference between
     L3 and L1.
* **The OTA install path.**  A real 392 MB LineageOS OTA was installed end
  to end from the stock LineageOS recovery: the package was verified
  against RSA key 0, 994 590 720 bytes were written to `/system`, and
  `/cache/recovery/last_install` says `1` (= `INSTALL_SUCCESS`).  That was
  done for the enforcing ROM itself.
* **Consumer IR.**  The AOSP `android.hardware.ir@1.0-service` loads the
  CAF blob `consumerir.msm8952.so` (`/system/lib/hw/`), which drives LG's
  `sdc_spi_ir` transmitter on SPI6 through
  `/sys/class/spi_master/spi6/spi6.0/ir_data`.  The LED transmits; that was
  checked by eye through a camera.  Three things had to be fixed, one
  behind the other:

  1. The driver creates `ir_data` as `root:root 0660` and the HAL runs as
     `system`.  `init.target.rc` hands `ir_data` and `ir_state` over, as
     the stock `init.b3_product.rc` does.
  2. AOSP gives `hal_ir_default` no access to sysfs.  The two attributes
     get their own type, `sysfs_ir` (`sepolicy/genfs_contexts`,
     `sepolicy/ir.te`), and the HAL is allowed that type only.
  3. The kernel had no SPI clock rate for an IR carrier.  The blob writes
     one 16 bit word per carrier period and the driver passes the carrier
     frequency itself as the SPI speed; the stock LG kernel resolves that
     with 84 extra entries in the clock table (the entry *named* 38000 is
     really 800 MHz / 12 / 97, 18 SPI bit times per carrier period).
     Without them the kernel logged `msm_spi_clock_set: no match found for
     requested clock frequency:38000` and a 500 ms frame left the LED in
     10 ms.  The entries were read out of the stock kernel and added to
     the kernel fork for the IR bus only.

  With all three in place a 501 ms pattern at 38 kHz takes 503 ms on the
  bus, under SELinux enforcing and without a denial.  LG's own IR service
  is not built for the v521 - see *Known issues*.
* **The diag port on USB.**  The serial (`acm`) and Qualcomm `diag`
  interfaces that QPST and QXDM use are presented **only while USB
  debugging is enabled** (Developer options): in the `adb`, `mtp,adb`,
  `ptp,adb` and `rndis,adb` modes.  With USB debugging off, the MTP, PTP
  and tethering modes carry nothing but their own function.  The MTP mode
  used to have two USB configurations with the diag interface in the
  second one, which a host does not select by itself.  Checked from a
  Linux host by sending the diag version, build id and status requests to
  the interface in the `adb`, `mtp,adb` and `ptp,adb` modes: the modem
  answers in each.  Not checked: QPST/QXDM on Windows.  The interface
  number of the diag port is 2 in the `adb` mode and 3 in the MTP and PTP
  modes.

  These two fixes are in the tree and were tested with a boot image built
  from it; they are not part of a numbered release build yet.

## What does not work yet

1. **One avc denial pair is left.**  The only denials left are

   ```
   avc: denied { dac_override } for comm="kworker/0:4"
       scontext=u:r:kernel:s0 tcontext=u:r:kernel:s0 tclass=capability
   avc: denied { dac_read_search } for comm="kworker/0:4"
       scontext=u:r:kernel:s0 tcontext=u:r:kernel:s0 tclass=capability
   ```

   The platform policy forbids granting them
   (`neverallow kernel self:capability { dac_override dac_read_search };`,
   `system/sepolicy/public/kernel.te:104`), so this one needs a
   kernel-side decision.  The worker still completes its job.
2. **The revision-named OTA has not been installed yet.**  The installed
   OTAs carried the old SHA1 build numbers, because the revision mechanism
   came later.  The device still runs the build before the revision
   mechanism, so the package *name* and the build number of a release have
   not been through the recovery yet.
3. **Three optional camera post-processing blobs are missing**:
   `libmmcamera_sw_tnr.so` (software temporal noise reduction),
   `libmmcamera_trueportrait_lib.so` (portrait bokeh) and
   `libmmcamera_stillmore_lib.so`.  The camera daemon logs
   `Error opening frameproc library ...` for them and falls back to the
   default processing chain.  Stills, preview, RAW capture and the scene
   modes all work without them.

   They are **not** added on purpose.  The V521 stock ROM does not contain
   them, and the copies that exist in the tree for other devices are from
   an older CAF camera generation: they export `img_algo_init` and friends
   where the V521 camera stack resolves `module_stillmore_init` and
   friends, so the `dlopen()` would find the file and then fail to find
   the entry point.  `libmmcamera_sw_tnr.so` does not exist for any device
   in the tree.
4. **The swap feature is not finished.**  The daemon, its build, its init
   service and its SELinux policy (zRAM and SD card) are in the tree, and
   the SD-card swap was switched on by hand on the device - but no ROM
   with the service has been installed yet, and the Settings UI is still
   open.  See *Swap*.

## The build revision

LineageOS derives the build number from a SHA1 of the timestamp, so two ROMs
cannot be told apart and nothing says what is in them.  The build script
replaces it with a revision that says what the build is and which attempt it
is:

```
ro.build.version.incremental = v521-15.1-build14
ro.v521.build.revision      = v521-15.1-build14
ro.v521.build.attempt       = 14
ro.v521.build.commit        = 829e43d
```

The revision **is** the build number, so the same string shows up in
*Settings -> About phone -> Build number* and in the name of the OTA
package:

```
lineage_v521-ota-v521-15.1-build14.zip
```

The mechanism lives in `build-revision.mk`.  It uses
`PRODUCT_PROPERTY_OVERRIDES`, because `ADDITIONAL_BUILD_PROPERTIES` is
refused for a device tree (`build/core/main.mk:112`), and it only adds the
properties when the build script passed them in, because an empty property
value is not valid in `build.prop`.  A plain `mka` has no revision and says
so in the build output.

## The recovery partition

The recovery partition of this device holds **TWRP**, and it must never be
overwritten by a ROM build.  Three things protect it:

1. `system/core` comments out the `flash_recovery` trigger in `init.rc`, so
   `install-recovery.sh` is never started.  It is commented out rather than
   tied to a property, so that no build can turn it back on by accident.
2. The build script refuses any ROM zip that writes to the recovery
   partition - it checks the updater-script, the presence of
   `recovery-from-boot.p` / `install-recovery.sh` in the system image and
   the `start flash_recovery` lines in the init `.rc` files.
3. The stock LineageOS recovery itself does not wipe `/data` silently and
   does not reboot out of the recovery.  LG's aboot passes `--wipe_data` to
   the recovery, and that has to be answered with a question, not with a
   wipe.

## Swap

*Work in progress.*  The daemon, its build file (`swap/Android.bp`,
registered through `subdirs` in `Android.bp`), its init service
(`v521swap` in `rootdir/etc/init.target.rc`) and its SELinux policy
(`sepolicy/swap.te`, `property.te`, the `file_contexts` and
`property_contexts` entries) are all in the tree.

**Tested so far:** the daemon binary, run by hand from a root shell on
build 17, found the SD-card swap partition and activated it
(`/dev/block/mmcblk1p2`, 2 GiB, priority 100).  **Not yet tested:** the
service and the confined `v521swap` domain in an installed ROM, and the
zRAM backend.  **Still open:** the Settings UI.

The plan is a user-configurable swap in *Settings -> System -> LineageParts*,
with three properties as the interface:

| Property | Meaning |
| --- | --- |
| `persist.v521.swap.enabled` | `0` or `1` |
| `persist.v521.swap.backend` | `zram`, `sdcard` or `both` |
| `persist.v521.swap.size` | the size in bytes |
| `v521.swap.status` | published by the daemon: `off`, `on:<backend>:<bytes>` or `error:<reason>` |

A dedicated `v521swap` daemon reconciles the state every five seconds from
those properties, so the user can turn swap on and off at any time and the
change takes effect without a reboot.

The swap is deliberately **not** done through the `fstab` / `swapon_all`
route: the fstab format has no user control, a missing SD card produces
per-boot errors, and this kernel encodes the swap flags differently from
what `fs_mgr` assumes (`SWAP_FLAG_PREFER=0x8000`,
`SWAP_FLAG_PRIO_MASK=0x7fff` in `include/linux/swap.h:19`).

Two details are specific to this kernel:

* the swap header is the **legacy** layout - the magic `SWAPSPACE2` in the
  last 10 bytes of page 0 and `version` / `last_page` / `nr_badpages` at
  offset 1024 (`kernel/lge/msm8952/include/linux/swap.h:84`).  The helper
  writes the header itself instead of calling `mkswap`.
* this zRAM driver refuses `disksize=0` and only accepts a new size on an
  uninitialised device, so `1` has to be written to
  `/sys/block/zram0/reset` after `swapoff` before a new `disksize` is set
  (`drivers/block/zram/zram_drv.c:872`).  The compressor is `lzo`.

The SD-card backend needs a swap partition (MBR type `0x82`) as
**partition 2** of the card.  The partitioning is done on the host with
the card in a reader; the on-device daemon only activates a partition that
has been prepared.

* `bin/sdcard-swap /dev/sdX [--commit [--no-backup]]` shrinks the FAT32
  partition in place with `fatresize` and adds the swap partition behind
  it.  On the 7.5 GiB card `fatresize` 1.1.0 aborted with
  `*** buffer overflow detected ***` (it wrote nothing), so
* `bin/sdcard-rebuild /dev/sdX <dir>` is the fallback that was used: it
  writes the new table (FAT32 5631 MiB + swap 2048 MiB), recreates the
  FAT32 **with the old volume serial** (so Android still sees the same
  `/storage/104D-0D10`), copies the files back from a verified copy and
  compares every file byte for byte.

The SELinux side (`sepolicy/swap.te`) is a new `v521swap` domain created
with `init_daemon_domain`, holding `capability sys_admin` for `swapon()`,
`ram_device:blk_file rw` for `/dev/block/zram0` (the platform already
labels the zRAM nodes `ram_device`), `sysfs_zram:file rw` for the zRAM
control files, `proc:file r` for `/proc/swaps`, `random_device:chr_file
r` for the header uuid (the label `/dev/urandom` carries), and `set_prop`
on the new `v521_swap_prop` type.  Both backends need `search` on the
`/dev/block` directory (`block_device:dir`).  For the SD card,
`file_contexts` labels `/dev/block/mmcblk1p2` as `swap_block_device` (the
platform type for swap areas; otherwise it is a generic `block_device`
node that only init may open), the daemon gets `rw` on it and `r` on the
whole-disk node `/dev/block/mmcblk1` (`sd_device`, from CAF) to read the
MBR.

To drive it by hand during bring-up (the Settings UI does the same):

    adb shell setprop persist.v521.swap.backend zram
    adb shell setprop persist.v521.swap.size $((384 * 1024 * 1024))
    adb shell setprop persist.v521.swap.enabled 1
    adb shell getprop v521.swap.status      # -> on:zram:402653184
    adb shell cat /proc/swaps
    adb shell setprop persist.v521.swap.enabled 0   # turn it back off

For the SD card use `backend sdcard` (or `both`); the status then reads
`on:sdcard:<bytes>`.

## Bring-up settings

`bringup.mk` is temporary and must be dropped before a daily-use build:

* USB debugging is enabled from the first boot and unauthenticated
  (`persist.sys.usb.config=mtp,adb`, `WITH_ADB_INSECURE=true`).
* SELinux is **enforcing** (`androidboot.selinux=enforcing`).
* A persistent logcat is written to `/data/misc/logd`.

## Known issues

* **LG's own consumer IR service is not built for the v521.**  It is not
  the AOSP service: it drives LG's IR transmitter over the device's own
  UART (`/dev/ttyHSL1`) and gets the transmit function from the CAF blob
  `libcir_driver.so`, which is only in the blob sets of the g2, g3, g4 and
  msm8996 (joan) devices.  `device/lge/common/Android.mk` includes every
  makefile under it for every LG device, so the module was built for the
  v521 as well and the build stopped with

  ```
  device/lge/common/consumerir/Android.mk: error:
      android.hardware.ir@1.0-service.lge (EXECUTABLES android-arm)
      missing libcir_driver (SHARED_LIBRARIES android-arm)
  ```

  The v521 does not need it - its transmitter is on SPI6, and the AOSP
  service loads the CAF blob `consumerir.msm8952.so` instead.
* **The 64-bit binder mode of the kernel had a latent bug.**
  `binder_alloc_buf()` and `binder_transaction()` place the offsets array
  with `ALIGN(data_size, sizeof(void *))`, which is 4 on this 32-bit
  kernel, while `binder_size_t` is 8 bytes in 64-bit mode, so the kernel
  and the userspace could disagree about the offset of the offsets array.
* **The arm32 `__get_user()` only knew about 1, 2 and 4 byte copies**, so an
  8 byte `copy_from_user()` silently truncated the value to 32 bit.  The CAF
  blob that carries the data connection hands 64-bit sizes and pointers to
  the kernel, so this bit the mobile data path.

  Both of the above are fixed in `android_kernel_lge_msm8952`, which
  backports the upstream arm 8 byte variant (`__get_user_8` plus the
  `__get_user_xb` helper for the big endian case) and builds the binder
  without `ANDROID_BINDER_IPC_32BIT`.  The kernel is built with it off, but
  the userspace is 32-bit (`ro.zygote=zygote32`,
  `ro.product.cpu.abilist=armeabi-v7a,armeabi`); both sides use the
  64-bit binder ABI, which is what Android 8.0+ HIDL needs.
* **`bootable/recovery-twrp` cannot be pushed with its history.**  It is a
  shallow clone (two commits) and its upstream
  (`OmniROM/android_bootable_recovery-twrp`) no longer exists on GitHub, so
  the server rejects the push with
  `remote: fatal: did not receive expected object 32aef7a2...`.  The branch
  on GitHub is therefore a **flattened snapshot** with exactly the same
  tree as the local history.
* **A TWRP build in the same output tree** installs `init.recovery.*.rc`
  and `file_contexts.bin` into the **boot** ramdisk root, and its shared
  libraries into the system image.  They have to be removed and the boot
  ramdisk rebuilt before a ROM image is released - the build system does
  **not** prune the outputs of a `PRODUCT_COPY_FILES` entry that was taken
  out of a `.mk` file, so the stale files are packaged into the system
  image otherwise.
* **Three debug log lines** are in the tree, all of them on the error path
  of the gralloc allocator: `frameworks/native/libs/ui/Gralloc2.cpp`,
  `hardware/interfaces/graphics/allocator/2.0/default/Gralloc0Allocator.cpp`
  and `hardware/qcom/display-caf/msm8952/libgralloc/gpu.cpp`.  They print
  the error code, the dimensions, the format and the usage flags of an
  allocation that failed, which is what tells a format the allocator does
  not know apart from a usage combination it rejects.

## Commit history

The commits of the port, per repository.

| Repository | Branch | Commits |
| --- | --- | --- |
| `android_device_lge_v521` | `lineage-15.1` | `f2c3bf0` bring up 15.1, `e0bd761` README, `c52c2b6` CAF blobs out of the protobuf stack, `566ee0d` CAF RIL logging off, `164f73b` drop the stock CAF libril.so, `7396d7d` port status, `cccc833` SELinux domains for the vendor daemons, `ec68ed8` SELinux enforcing, `bdbf752` README, `baffb98` recovery: /dev/block/bootdevice, `61880ad` DPM daemon, `de4f2e1` ELF string patcher, `83d6ffd` build revision, `fee61f0` Widevine, `bfabf1c` name the build number buildN, `eac79b4` TWRP build files, `e856f33` QuickButton app, `bf51acb` README, `dd2c567` README, `08b0a8c` README, `6bd0bc5` README, `ed91e28` README, `2146fc9` IR HAL access to the SPI IR driver, `42527cd` diag port in the MTP and PTP modes |
| `proprietary_vendor_lge` | `lineage-15.1` | `12617af` CAF blobs out of the protobuf stack, `98680e5` drop the stock CAF libril.so, `7dfb70d` vendor modules, `f8f1f62` DPM daemon and libraries, `35b9cc9` OEM crypto library, `ca0b734` README |
| `android_device_lge_common` | `lineage-15.1-v521` | `7a9c238` skip LG's own consumer IR service, `3f8d7df` README |
| `android_system_core` | `lineage-15.1-v521` | `5e3b6fb` never overwrite the recovery partition, `373c684` README |
| `android_kernel_lge_msm8952` | `lineage-15.1-v521` | `55a07099` 64 bit get_user, no 32 bit binder, `8561be9c` README, `1665b7e0` README, `c6825292` IR carrier rates in the SPI clock table |
| `android_vendor_lineage` | `lineage-15.1-v521` | `530bd69` insecure adb, `1be2a6f` README |
| `android_frameworks_base` | `lineage-15.1-v521` | `11e63f07` 7.0-era compat APIs for the CNE blobs, `e67a0b17` README |
| `android_frameworks_native` | `lineage-15.1-v521` | `8167e48` gralloc debug log lines |
| `android_hardware_interfaces` | `lineage-15.1-v521` | `ece6c15` gralloc debug log lines |
| `android_hardware_qcom_display` | `lineage-15.1-v521-msm8952` | `7f26f7d` gralloc debug log lines |
| `android_hardware_ril` | `lineage-15.1-caf` | `904ec8a` CAF SIM_IO, command mapping, signal strength, data registration, `38352e9` README |
| `android_bootable_recovery` | `lineage-15.1-v521` | `08bbbb07e` no silent wipe, no reboot out of the recovery, `897195b5` README |
| `android_bootable_recovery-twrp` | `lineage-15.1-v521` | `e765bd1` flattened snapshot of `790c815` (crash report) and `a137904` (README) |
