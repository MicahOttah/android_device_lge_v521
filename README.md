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
| Port branch | `lineage-15.1` @ `b06ff03` |
| Build revision | `v521-15.1-build14` (see *The build revision*) |

## Status at a glance

| Area | State |
| --- | --- |
| Boot, SELinux | **working** - enforcing, no third-party denials |
| Radio, LTE data | **working** - voice + data in service |
| Camera | **working** - both sensors, stills, RAW, scene modes |
| Sensors, auto-rotation | **working** - 26/26 running |
| Wi-Fi | **working** |
| DPM (`dpmd`) | **working** - daemon running, NSRM in place |
| Widevine | **working** - **L1** (hardware backed) under enforcing |
| TWRP recovery | **working** - built from this tree, protected |
| Build revision | **working** - `v521-15.1-buildN` in the build number and in the OTA name |
| Swap (zRAM / SD card) | **in progress** - see *Swap* |

## Repositories

Everything the port needs is on GitHub under `MicahOttah`.  The device tree
is the only repository that is a *fork of an upstream project*; the others
are forks of the LineageOS projects they belong to, so that they can be
pulled from here without touching the upstream trees.

| Repository | Branch | What it carries |
| --- | --- | --- |
| [`android_device_lge_v521`](https://github.com/MicahOttah/android_device_lge_v521) (this one) | `lineage-15.1` | The device tree and the whole 15.1 port. |
| [`proprietary_vendor_lge`](https://github.com/MicahOttah/proprietary_vendor_lge) | `lineage-15.1` | The v521 blobs: the CAF protobuf rename, the OEM crypto library for Widevine, the DPM daemon and libraries. |
| [`android_device_lge_common`](https://github.com/MicahOttah/android_device_lge_common) | `lineage-15.1-v521` | Skips the consumer IR HAL for the v521. |
| [`android_system_core`](https://github.com/MicahOttah/android_system_core) | `lineage-15.1-v521` | Stops the system from overwriting the TWRP recovery partition. |
| [`android_kernel_lge_msm8952`](https://github.com/MicahOttah/android_kernel_lge_msm8952) | `lineage-15.1-v521` | The 64-bit `get_user()` path and the drop of the 32-bit binder API. |
| [`android_vendor_lineage`](https://github.com/MicahOttah/android_vendor_lineage) | `lineage-15.1-v521` | `WITH_ADB_INSECURE=true` support. |
| [`android_frameworks_base`](https://github.com/MicahOttah/android_frameworks_base) | `lineage-15.1-v521` | The Android-7.0-era compatibility shims for the stock CNE blobs. |
| [`android_hardware_ril`](https://github.com/MicahOttah/android_hardware_ril) | `lineage-15.1-caf` | The CAF branch of the RIL with the SIM_IO / signal strength / data registration fixes. |
| [`android_bootable_recovery`](https://github.com/MicahOttah/android_bootable_recovery) | `lineage-15.1-v521` | The stock LineageOS recovery: no silent wipe, no reboot out of the recovery. |
| [`android_bootable_recovery-twrp`](https://github.com/MicahOttah/android_bootable_recovery-twrp) | `lineage-15.1-v521` | The TWRP crash handler. **Not pushed yet** - see *Known issues in the tree*. |
| [`android_device_lge_v521_twrp`](https://github.com/MicahOttah/android_device_lge_v521_twrp) | - | Standalone copy of the TWRP build files. Superseded: they live in this tree since `5e3dbf2`. |
| [`android_app_QuickButton`](https://github.com/MicahOttah/android_app_QuickButton) | - | Standalone copy of the QuickButton app. Superseded: it lives in this tree since `b06ff03`. |

The two standalone add-on repositories are optional and no longer needed -
the device tree builds without them and warns what is missing.

## What works

* Boots into LineageOS 15.1 (8.1.0) userdebug.
* **SELinux enforcing.** `BoardConfig.mk` adds
  `androidboot.selinux=enforcing` to the kernel cmdline and the device
  reports `getenforce` = `Enforcing`.  The only avc denials left are the
  two `kernel -> kernel capability` tuples that the platform neverallow
  forbids to be granted (see *What does not work yet*).  Getting there
  needed an explicit `file_contexts` entry for each of the 21 CAF vendor
  daemons, because AOSP `init` takes the domain of a service from the
  file context of the executable - and the v521 blob list installs them
  into `/system/bin`, not `/system/vendor/bin` where the CAF policy
  expects them.
* **The sensor stack.** `dumpsys sensorservice` reports
  `Total 26 h/w sensors, 26 running` - accelerometer, magnetometer,
  gravity, linear acceleration, ...  With 26 sensors the accelerometer is
  present and the **auto-rotation works**.  The board has no gyroscope
  (the accelerometer is an STMicroelectronics K2HHTR), so the 27th entry
  of the old framework list ("GeoMag Rotation Vector Sensor", handle
  `0x5f67656f`) was a fusion pseudo-sensor and not a real one.
* **The camera.** Both sensors work - the back camera and the front
  camera - including preview, stills, RAW capture and the scene modes.
  `dumpsys media.camera` reports `Number of camera devices: 2`, and the
  event log shows the stock camera app (`org.cyanogenmod.snap`) opening
  and closing both of them.  The sensor driver that is probed is the
  Samsung `s5k5e2` (`hi553`).
* HIDL: `manifest.xml` plus passthrough `-impl` wrappers for the legacy
  vendor HALs, the graphics allocator/composer running as `hwbinder`
  services.
* Wi-Fi: `wpa_supplicant` from `/vendor/bin/hw`, `wlan0` up with a DHCP
  lease, DNS and TLS working.
* ADB over USB (root, no auth prompt - see *Bring-up settings*).
* TWRP 3.2.3-0 recovery (built from the tree with `WITH_TWRP=true`).
* The stock LineageOS recovery, built from the same tree
  (`bin/build 15.1 recovery recoveryimage`).  It carries the same kernel
  and the same ramdisk policy as `boot.img`.
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
     must not pull `libmedia.so` back in
     (`MicahOttah/proprietary_vendor_lge`);
  2. the HIDL RIL has to hand the vendor the CAF `RIL_SIM_IO` layout and
     the CAF command numbers, and has to fall back to the voice
     registration state for the data registration request, which CAF does
     not implement (`MicahOttah/android_hardware_ril`);
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

  The blobs came out of the stock ROM of this device and are byte-identical
  to it (`MicahOttah/proprietary_vendor_lge` @ `24a590f`).

* **Widevine L1, under enforcing.** The CDM reports
  `securityLevel = L1` (hardware backed) and the ClearKey plugin keeps
  working next to it.  That needed three changes:

  1. **the CDM has to be loadable at all.**  `libwvdrmengine.so` is the
     Google Widevine CDM and it imports 46 protobuf symbols - 8 more than
     the AOSP runtime provides.  The stock CAF runtime is therefore
     installed under a second name, `/system/vendor/lib/libqc-protobuf-lite.so`,
     and the CDM's `DT_NEEDED` entry is rewritten to point at it.  The new
     name is one character *shorter*, so the string table does not have to
     grow and the binary keeps its size and layout
     (`patch-elf-dynstr.py`, `patch-qc-protobuf.py`).  On Android 8.1 the
     DRM plugins are loaded into the HIDL service
     (`/vendor/bin/hw/android.hardware.drm@1.0-service`), which has no
     AOSP protobuf runtime in it, so the two runtimes never meet.
  2. **the OEM crypto library.**  The CDM loads Qualcomm's `liboemcrypto.so`
     at run time (it is not a `DT_NEEDED` entry) and it is the piece that
     reaches the TEE.  Without it the CDM cannot initialise its crypto
     backend at all.  The library was not in the tree and is now
     (`MicahOttah/proprietary_vendor_lge` @ `57c76d0`).
  3. **the SELinux rules for the provisioning.**  The key box is a separate
     TEE application that the CDM loads through the QSEE client library:
     `/vendor/firmware/widevine.mdt -> /persist-lg/firmware/widevine.mdt`.
     The CAF policy only covered `firmware_file:dir` and
     `firmware_file:file`, not the symlink and not the target, so the DRM
     service was stopped at the link with

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

## What does not work yet

1. **One avc denial pair is left.** The port runs in **enforcing** mode and
   the only denials left are

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

2. **No OTA installed on the device yet.**  The OTAs are built and packaged
   (see *The build revision*), but the device still runs the build13
   system, so the OTA path itself is not proven end to end.

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
   in the tree.  The details and the acceptance test for a future
   V521-specific copy are in `notes/2026-11-16-camera-postproc-abi.md`.

4. **The swap feature is not finished.**  The daemon, the policy and the
   user interface are designed, but nothing is in a release build yet - see
   *Swap*.

## The build revision

LineageOS derives the build number from a SHA1 of the timestamp, so two ROMs
cannot be told apart and nothing says what is in them.  `bin/build` replaces
it with a revision that says what the build is and which attempt it is:

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

The mechanism lives in `build-revision.mk`; it uses
`PRODUCT_PROPERTY_OVERRIDES` because `ADDITIONAL_BUILD_PROPERTIES` is
refused for a device tree (`build/core/main.mk:112`).  A plain `mka` that
does not go through `bin/build` has no revision and says so in the build
output.

The release artefacts are collected in `release-15.1/` with a `REVISION`
file and a `SHA256SUMS` file, and each build gets its own `-buildN` alias
so that an older release can never be overwritten by a newer one.

## The recovery partition

The recovery partition of this device holds **TWRP**, and it must never be
overwritten by a ROM build.  Three things protect it:

1. `system/core` (`5919882`) comments out the `flash_recovery` trigger in
   `init.rc`, so `install-recovery.sh` is never started.  It is commented
   out rather than tied to a property, so that no build can turn it back on
   by accident.
2. `bin/check-recovery-safe` refuses any ROM zip that writes to the
   recovery partition - it checks the updater-script, the presence of
   `recovery-from-boot.p` / `install-recovery.sh` in the system image and
   the `start flash_recovery` lines in the init `.rc` files.
3. The stock LineageOS recovery itself does not wipe `/data` silently and
   does not reboot out of the recovery
   (`MicahOttah/android_bootable_recovery` @ `9ab3795a8`).  LG's aboot
   passes `--wipe_data` to the recovery, and that has to be answered with a
   question, not with a wipe.

The original image of the partition is kept in `recovery-backup/` with its
md5 (`3d14036c5ee77506113e638e08ce91cc`), so it can always be restored.

## Swap

*Work in progress.*  The design is finished and the daemon is written, but
none of it is in a release build yet.

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
`SWAP_FLAG_PRIO_MASK=0x7fff` in `include/linux/swap.h:19`, while `fs_mgr`
assumes the upstream values).

Two details that are specific to this kernel:

* the swap header is the **legacy** layout - the magic `SWAPSPACE2` in the
  last 10 bytes of page 0 and `version` / `last_page` / `nr_badpages` at
  offset 1024 (`kernel/lge/msm8952/include/linux/swap.h:84`).  The helper
  writes the header itself instead of calling `mkswap`.
* this zRAM driver refuses `disksize=0` and only accepts a new size on an
  uninitialised device, so `1` has to be written to
  `/sys/block/zram0/reset` after `swapoff` before a new `disksize` is set
  (`drivers/block/zram/zram_drv.c:872`).  The compressor is `lzo`.

The SD-card backend needs a second partition at the end of the card.  The
card is 7.5 GiB with one FAT32 partition that covers it to the last sector,
and the data ends at cluster 40 295 (about 1.23 GiB), so a 2 GiB tail
partition is safe.  The repartitioning is done on the host with
`bin/sdcard-swap` (the card is unmounted first and the content is backed
up before anything is written); the on-device daemon only activates a
partition that has been prepared.

The SELinux side is a new `v521swap` domain with its own `exec` type, a
`coredomain` for the property socket, `capability sys_admin` for
`swapon()`, `swap_block_device:blk_file rw` for `/dev/block/mmcblk1p2` and
`sysfs_zram:file rw` for the zRAM control files.

## Bring-up settings

`bringup.mk` is temporary and must be dropped before a daily-use build:

* USB debugging is enabled from the first boot and unauthenticated
  (`persist.sys.usb.config=mtp,adb`, `WITH_ADB_INSECURE=true`).
* SELinux is **enforcing** (`androidboot.selinux=enforcing`).
* A persistent logcat is written to `/data/misc/logd`.

## Known issues in the tree

* `bootable/recovery-twrp` **cannot be pushed** with its history.  It is a
  shallow clone (two commits) and its upstream
  (`OmniROM/android_bootable_recovery-twrp`) no longer exists on GitHub, so
  the server rejects the push with
  `remote: fatal: did not receive expected object 32aef7a2...`.  The commit
  (`790c815`) exists locally and the fork
  `MicahOttah/android_bootable_recovery-twrp` exists but is empty; pushing
  it needs an orphan commit with the full tree.
* The 64-bit binder mode of the kernel had a latent bug: `binder_alloc_buf()`
  and `binder_transaction()` place the offsets array with
  `ALIGN(data_size, sizeof(void *))`, which is 4 on this 32-bit kernel,
  while `binder_size_t` is 8 bytes in 64-bit mode. The kernel and the
  userspace could therefore disagree about the offset of the offsets array.
* The arm32 `__get_user()` implementation of this tree only knew about 1, 2
  and 4 byte copies, so an 8 byte `copy_from_user()` silently truncated the
  value to 32 bit.  The CAF blob that carries the data connection hands
  64-bit sizes and pointers to the kernel, so this bit the mobile data
  path.
* Both of the above are fixed in
  `MicahOttah/android_kernel_lge_msm8952` @ `8ed4186f`, which backports
  the upstream arm 8 byte variant (`__get_user_8` plus the `__get_user_xb`
  helper for the big endian case) and builds the binder without
  `ANDROID_BINDER_IPC_32BIT`.
* The kernel is built with `CONFIG_ANDROID_BINDER_IPC_32BIT` off, but the
  userspace is 32-bit (`ro.zygote=zygote32`,
  `ro.product.cpu.abilist=armeabi-v7a,armeabi`). Both sides use the
  64-bit binder ABI, which is what Android 8.0+ HIDL needs.
* A TWRP build in the same output tree installs `init.recovery.*.rc` and
  `file_contexts.bin` into the **boot** ramdisk root
  (`out/target/product/v521/root`), and its shared libraries
  (`libminuitwrp`, `libaosprecovery`, `libgpt_twrp`, ...) into
  `out/target/product/v521/system/lib`.  They have to be removed and the
  boot ramdisk rebuilt before a ROM image is released - the build system
  does **not** prune the outputs of a `PRODUCT_COPY_FILES` entry that was
  taken out of a `.mk` file, so the stale files are packaged into the
  system image otherwise.  `bin/clean-twrp-leftovers` and
  `bin/clean-stale-outputs` in the working directory do this.
* Two debug log lines are still in the tree and are **not** meant to go
  into a release: `frameworks/native/libs/ui/Gralloc2.cpp` and
  `hardware/interfaces/graphics/allocator/2.0/default/Gralloc0Allocator.cpp`
  (the `V521DBG` lines).  They were added while the gralloc allocation
  failure was being chased.

## Commit history

Every commit of the port, per repository.  The device tree is listed first;
the rest are the changes that could not live in it.

### `MicahOttah/android_device_lge_v521` - `lineage-15.1`

| Commit | Date | Subject |
| --- | --- | --- |
| `661a0db` | 2026-11-14 | v521: bring up LineageOS 15.1 |
| `f9c6679` | 2026-11-14 | v521: add a README with the port status |
| `1dbec81` | 2026-11-14 | v521: keep the CAF blobs out of the AOSP protobuf stack |
| `1aedb0a` | 2026-11-14 | v521: turn off the CAF RIL verbose logging |
| `6025bf2` | 2026-11-14 | v521: drop the stock CAF libril.so that shadowed the LineageOS one |
| `a0fbd3c` | 2026-11-14 | v521: update the port status |
| `646ec72` | 2026-11-14 | v521: give the vendor daemons their own SELinux domains |
| `f2c0662` | 2026-11-14 | v521: run in SELinux enforcing mode and fix the last denials |
| `a472105` | 2026-11-14 | v521: update the README to the current state of the port |
| `0ca0748` | 2026-11-15 | v521: recovery: create /dev/block/bootdevice and allow reading the package |
| `ea36fe5` | 2026-11-15 | v521: enable the Qualcomm DPM daemon |
| `0c06849` | 2026-11-15 | v521: add the ELF string patcher |
| `c462ff6` | 2026-11-15 | v521: add a human-readable build revision |
| `9fa038b` | 2026-11-15 | v521: enable Widevine |
| `829e43d` | 2026-11-15 | v521: name the build number buildN |
| `5e3dbf2` | 2026-11-15 | v521: add the TWRP build files |
| `b06ff03` | 2026-11-15 | v521: add the quick button settings app |

### `MicahOttah/proprietary_vendor_lge` - `lineage-15.1`

| Commit | Date | Subject |
| --- | --- | --- |
| `46e5e31` | 2026-11-14 | v521: keep the CAF blobs out of the AOSP protobuf stack |
| `1a9bf59` | 2026-11-14 | v521: drop the stock CAF libril.so that shadowed the LineageOS one |
| `f47a4e5` | 2026-11-15 | v521: build the prebuilt libraries as vendor modules |
| `24a590f` | 2026-11-15 | v521: add the Qualcomm DPM daemon and its libraries |
| `57c76d0` | 2026-11-15 | v521: add the OEM crypto library for Widevine |

### `MicahOttah/android_frameworks_base` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `6a969646` | 2026-11-14 | telephony: add 7.0-era compat APIs for the stock CNE blobs |

### `MicahOttah/android_hardware_ril` - `lineage-15.1-caf`

| Commit | Date | Subject |
| --- | --- | --- |
| `f229a1f` | 2026-11-14 | libril: fix the CAF SIM_IO payload, command mapping, signal strength and data registration |

### `MicahOttah/android_kernel_lge_msm8952` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `8ed4186f` | 2026-11-15 | v521: add the 64 bit get_user path and drop the 32 bit binder API |

### `MicahOttah/android_system_core` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `5919882` | 2026-11-15 | v521: never let the system overwrite the recovery partition |

### `MicahOttah/android_device_lge_common` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `5a675e1` | 2026-11-15 | v521: do not build the consumer IR HAL |

### `MicahOttah/android_vendor_lineage` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `56e571d` | 2026-11-15 | v521: allow building with an insecure adb |

### `MicahOttah/android_bootable_recovery` - `lineage-15.1-v521`

| Commit | Date | Subject |
| --- | --- | --- |
| `9ab3795a8` | 2026-11-15 | v521: do not wipe /data silently and do not reboot out of the recovery |

### `MicahOttah/android_bootable_recovery-twrp` - `lineage-15.1-v521` (not pushed)

| Commit | Date | Subject |
| --- | --- | --- |
| `790c815` | 2026-11-15 | v521: print a crash report when the GUI dies |
