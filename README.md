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
* HIDL: `manifest.xml` plus passthrough `-impl` wrappers for the legacy
  vendor HALs, the graphics allocator/composer running as `hwbinder`
  services.
* Wi-Fi: `wpa_supplicant` from `/vendor/bin/hw`, `wlan0` up with a DHCP
  lease, DNS and TLS working.
* ADB over USB (root, no auth prompt - see *Bring-up settings*).
* TWRP 3.2.3-0 recovery (built from the tree with `WITH_TWRP=true`).
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

1. **SELinux is permissive.** `BoardConfig.mk` adds
   `androidboot.selinux=permissive` to the kernel cmdline because the
   sepolicy for the HIDL HALs is not complete yet. Enforcing has not been
   attempted; expect `avc: denied` until the policy is filled in.
   Known gaps: `qmuxd` (`block_suspend`), `netmgrd` (`netlink_route_socket`,
   socket `ioctl`/`create`), and the init services `qmuxd` and
   `rmt_storage` have no domain at all.

2. **One sensor is missing.** The vendor sensor HAL reports *26 h/w
   sensors, 26 running*; the framework's sensor list has 27 entries
   (handles `0x01`-`0x1a` plus one with the handle `0x5f67656f`,
   "GeoMag Rotation Vector Sensor").  The sensor configuration
   (`/etc/sensors/sensor_def_common.conf`, `sensor_def_variable.conf`) is
   byte-identical to the stock ROM and the vendor `sensors.qcom` differs
   from the stock copy only in relocation bytes, so the difference is
   somewhere else.  The board has no gyroscope fitted (the accelerometer
   is an STMicroelectronics K2HHTR), which is the most likely candidate.

3. **Camera is untested.** The HIDL camera passthrough is wired up, but no
   picture has been taken and the legacy `camera.msm8952` blob has not been
   validated on 8.1.

4. **No OTA installed.** The stock recovery is built from this tree, but no
   OTA package has been installed.

5. **DPM (`dpmd`) is not installed.** The blobs are not in the tree yet and
   there is no `dpmd` service, so the device has no device policy manager.

6. **Widevine is not working.** `system/vendor/lib/mediadrm/libwvdrmengine.so`
   and `system/vendor/lib/mediacas/libclearkeycasplugin.so` are CAF-built
   and still import the CAF protobuf runtime into a process that also has
   AOSP's runtime.  Deliberately left alone.

## Bring-up settings

`bringup.mk` is temporary and must be dropped before a daily-use build:

* USB debugging is enabled from the first boot and unauthenticated
  (`persist.sys.usb.config=mtp,adb`, `WITH_ADB_INSECURE=true`).
* SELinux is permissive (see above).
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
