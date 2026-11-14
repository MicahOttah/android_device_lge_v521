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

Both add-ons are optional: the device tree builds without them and warns what
is missing.

## What works

* Boots into LineageOS 15.1 (8.1.0) userdebug.
* HIDL: `manifest.xml` plus passthrough `-impl` wrappers for the legacy
  vendor HALs, the graphics allocator/composer running as `hwbinder`
  services.
* Wi-Fi: `wpa_supplicant` from `/vendor/bin/hw`, `wlan0` up with a DHCP
  lease, DNS and TLS working.
* ADB over USB (root, no auth prompt - see *Bring-up settings*).
* TWRP 3.2.3-0 recovery (built from the tree with `WITH_TWRP=true`).

## What does not work yet

1. **SystemUI crashes when Recents is opened** ("System UI has stopped").
   The HIDL graphics allocator's *reply* buffer cannot be allocated in
   SystemUI's address space: the kernel logs
   `binder: <pid>: binder_alloc_buf, no vma` for the `hwbinder` context,
   the reply fails with `BR_FAILED_REPLY`, the client sees
   `EX_TRANSACTION_FAILED` and `GraphicBufferAllocator: Failed to allocate
   (1008 x 112)`. The `hwbinder` proc of SystemUI has `proc->vma == NULL`
   while its `/dev/hwbinder` mapping is still present, so
   `binder_vma_close()` cleared it while `binder_mmap()` never set it
   again. Reproduces on every boot and right after a `kill -9` of
   SystemUI, so it is broken from process birth and not caused by
   something the process does later.
   A diagnostic kernel with `V521DBG` prints in `binder_open()`,
   `binder_mmap()`, `binder_vma_close()`, `binder_alloc_buf()` and
   `binder_update_page_range()` is built and not yet flashed.

2. **The radio and the mobile data are in a crash loop.** Four binaries
   restart in a loop, so there is no mobile data and no IMS:
   * `/vendor/bin/hw/rild` - `cannot locate symbol "ril_service_name"`
   * `/system/bin/cnd` - `cannot locate symbol
     "google::protobuf::internal::StringTypeHandlerBase::New()" referenced
     by "/system/vendor/lib/libcne.so"`
   * `/system/bin/netmgrd` and `/system/bin/imsdatadaemon` - `cannot
     locate symbol
     "google::protobuf::io::CodedInputStream::ReadVarint32Fallback(unsigned
     int*)" referenced by "/system/vendor/lib/libcneapiclient.so"`

   The vendor blobs are from a newer Android than 8.1 and expect a
   protobuf version that is not in the tree. Needs either matching
   protobuf blobs or a shim for the three symbols.

3. **SELinux is permissive.** `BoardConfig.mk` adds
   `androidboot.selinux=permissive` to the kernel cmdline because the
   sepolicy for the HIDL HALs is not complete yet. Enforcing has not been
   attempted; expect `avc: denied` until the policy is filled in.

4. **Sensors are mostly missing.** The sensor HAL enumerates exactly one
   sensor (`LGE Cap Sensor`); the platform has 27. `sensors.v521` does not
   load the full sensor list yet.

5. **Camera is untested.** The HIDL camera passthrough is wired up, but no
   picture has been taken and the legacy `camera.msm8952` blob has not been
   validated on 8.1.

6. **Recovery/OTA are untested.** The stock recovery is built from this
   tree, but no OTA package has been installed.

7. **The build fingerprint is not the stock one.** The static stock
   fingerprint was removed: in 15.1 it only sets `ro.build.fingerprint`
   and not `ro.vendor.build.fingerprint`, so `Build.isBuildConsistent()`
   fails and the device shows "There's an internal problem with your
   device" at boot.

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
  (`out/target/product/v521/root`). They have to be removed and the boot
  ramdisk rebuilt before a ROM image is released.
