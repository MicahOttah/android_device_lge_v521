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

## Repositories

| Repository | Contents |
| --- | --- |
| `MicahOttah/android_device_lge_v521` (this one) | The device tree and the 15.1 port. |
| `MicahOttah/android_device_lge_v521_twrp` | TWRP build files (`twrp.mk`, `twrp.fstab`, vendor recovery init). Clone into `device/lge/v521/`. |
| `MicahOttah/android_app_QuickButton` | The side QuickButton app. Clone into `device/lge/v521/QuickButton`. |
| `MicahOttah/android_hardware_ril` | The CAF branch of the RIL (`lineage-15.1-caf`), with the SIM_IO / signal-strength / data-registration fixes. |
| `MicahOttah/android_frameworks_base` | The two Android-7.0-era compatibility shims for the stock CNE blobs (`lineage-15.1-v521`). |
| `MicahOttah/proprietary_vendor_lge` | The v521 blob fixes: the CAF protobuf runtime rename and the removal of the stock CAF `libril.so`. |

The three add-ons are optional: the device tree builds without them and warns
what is missing.

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

2. **No OTA installed.** The LineageOS recovery is built from this tree,
   but no OTA package has been installed.

3. **DPM (`dpmd`) is not installed.** The blobs are not in the tree yet and
   there is no `dpmd` service, so the device has no device policy manager.

4. **Widevine is not working.** `system/vendor/lib/mediadrm/libwvdrmengine.so`
   and `system/vendor/lib/mediacas/libclearkeycasplugin.so` are CAF-built
   and still import the CAF protobuf runtime into a process that also has
   AOSP's runtime.  Deliberately left alone.

5. **Three optional camera post-processing blobs are missing**:
   `libmmcamera_sw_tnr.so` (software temporal noise reduction),
   `libmmcamera_trueportrait_lib.so` (portrait bokeh) and
   `libmmcamera_stillmore_lib.so`.  They are not part of the V521 blob set
   in any upstream repository, so the camera daemon logs
   `Error opening frameproc library ...` for them and falls back to the
   default processing chain.  Stills, preview, RAW capture and the scene
   modes all work without them.

## Bring-up settings

`bringup.mk` is temporary and must be dropped before a daily-use build:

* USB debugging is enabled from the first boot and unauthenticated
  (`persist.sys.usb.config=mtp,adb`, `WITH_ADB_INSECURE=true`).
* SELinux is **enforcing** (`androidboot.selinux=enforcing`).
* A persistent logcat is written to `/data/misc/logd`.

## Known issues in the tree

* The 64-bit binder mode of the kernel has a latent bug: `binder_alloc_buf()`
  and `binder_transaction()` place the offsets array with
  `ALIGN(data_size, sizeof(void *))`, which is 4 on this 32-bit kernel,
  while `binder_size_t` is 8 bytes in 64-bit mode. The kernel and the
  userspace can therefore disagree about the offset of the offsets array.
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
