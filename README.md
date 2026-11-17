# android_device_lge_v521

Device tree for the **LG G Pad X 8.0 (V521)** on **LineageOS 15.1**
(Android 8.1), based on the upstream `LineageOS/android_device_lge_v521`
`lineage-15.1` branch.

| | |
| --- | --- |
| Device | LG G Pad X 8.0 (V521), model `b3_tmo_us` |
| SoC | Qualcomm MSM8952 (Snapdragon 625), 32-bit kernel and userspace |
| Android | 8.1.0, LineageOS 15.1, userdebug |

## Status

| Area | State |
| --- | --- |
| Boot, SELinux | working, enforcing |
| Radio, LTE data | working, the network list shows carrier names, MCC+MNC and the radio technology |
| Camera | working (both sensors, stills, RAW, scene modes) |
| Sensors, auto-rotation | working (26/26) |
| Wi-Fi, Bluetooth, audio | working |
| Widevine | working, **L1** |
| DPM (`dpmd`) | working |
| IR blaster | working |
| USB diag port | working, only while USB debugging is on |
| Recovery | working, kept in step with the ROM |
| Swap (zRAM / SD card) | working, *Settings -> System -> Swap* |

## Repositories

Everything the port needs is on GitHub under `MicahOttah`; the history of
each change is in that repository's `git log`.

| Repository | Branch | What it changes |
| --- | --- | --- |
| [`android_device_lge_v521`](https://github.com/MicahOttah/android_device_lge_v521) (this one) | `lineage-15.1` | The device tree. |
| [`proprietary_vendor_lge`](https://github.com/MicahOttah/proprietary_vendor_lge) | `lineage-15.1` | Blobs: CAF protobuf rename, OEM crypto, DPM. |
| [`android_device_lge_common`](https://github.com/MicahOttah/android_device_lge_common) | `lineage-15.1-v521` | Skips LG's UART IR service for the v521. |
| [`android_kernel_lge_msm8952`](https://github.com/MicahOttah/android_kernel_lge_msm8952) | `lineage-15.1-v521` | 64-bit binder on the 32-bit kernel, IR clock rates. |
| [`android_hardware_ril`](https://github.com/MicahOttah/android_hardware_ril) | `lineage-15.1-caf` | CAF RIL compatibility. |
| [`android_frameworks_base`](https://github.com/MicahOttah/android_frameworks_base) | `lineage-15.1-v521` | Compatibility shims for the stock CNE blobs. |
| [`android_vendor_lineage`](https://github.com/MicahOttah/android_vendor_lineage) | `lineage-15.1-v521` | `WITH_ADB_INSECURE` support. |
| [`android_bootable_recovery`](https://github.com/MicahOttah/android_bootable_recovery) | `lineage-15.1-v521` | No silent wipe, no reboot out of recovery. |
| [`android_packages_apps_LineageParts`](https://github.com/MicahOttah/android_packages_apps_LineageParts) | `lineage-15.1-v521` | The swap settings page. |
| [`android_system_core`](https://github.com/MicahOttah/android_system_core) | `lineage-15.1-v521` | Upstream again (README only). |
| `android_frameworks_native`, `android_hardware_interfaces`, `android_hardware_qcom_display` | `lineage-15.1-v521` | Gralloc error logging. |
| [`android_bootable_recovery-twrp`](https://github.com/MicahOttah/android_bootable_recovery-twrp) | `lineage-15.1-v521` | TWRP crash fix (flattened snapshot). |

## Builds

Every build gets a number, `buildN`, which shows up in:

* *Settings -> About phone -> Build number* (`v521-15.1-buildN`),
* `ro.lineage.version` and the zip name
  (`lineage-15.1-<date>-UNOFFICIAL-buildN-v521.zip`),
* the bottom line of the recovery menu.

`build-revision.mk` adds `ro.v521.build.{revision,attempt,commit}` when the
build script passes them in; a plain `mka` builds without a revision.

## Installing

The recovery partition holds the LineageOS recovery from this tree.
`system.prop` sets `persist.sys.recovery_update=true`, so on each boot
`install-recovery.sh` rewrites the recovery to match the installed ROM.
(15.1 Settings has no switch for it; `setprop
persist.sys.recovery_update false` as root turns it off.)

To install a build:

    adb push lineage-...-buildN-v521.zip /data/local/tmp/lineage-ota.zip
    adb root
    adb shell 'echo --update_package=/data/local/tmp/lineage-ota.zip > /cache/recovery/command'
    adb reboot recovery

Reboot right after writing the command; a command left in place across a
normal boot can be lost.  The fallback is *Apply update -> Apply from ADB*
and `adb sideload`.

## Swap

The `v521swap` service applies the swap settings within five seconds,
without a reboot.  It can use compressed RAM (zRAM), a swap partition on
the SD card, or both.  If the card or its swap partition is missing, swap
continues without it.

Settings live in system properties, which the *Settings -> System -> Swap*
page writes:

| Property | Meaning |
| --- | --- |
| `persist.v521.swap.enabled` | `0` / `1` |
| `persist.v521.swap.backend` | `zram`, `sdcard` or `both` |
| `persist.v521.swap.size` | zRAM size in bytes (default 512 MiB) |
| `persist.v521.swap.zram_priority` / `sdcard_priority` | default `1000` / `100`: zRAM first, card as overflow |
| `persist.v521.swap.reclaim` | `auto` (process reclaim while zRAM is on), `on`, `off` |
| `persist.v521.swap.swappiness` | 0 - 100, unset = kernel default |
| `v521.swap.status`, `v521.swap.usage` | read only, published by the service |

Higher priority is used first; equal priorities share the load.

**The SD card swap needs partition 2 of the card set to type `0x82`.**
Partition the card on a computer; the service does not partition it.  **Do
not remove the card while swap is using it** - turn swap off or switch to
zRAM first.

## Not working yet

* Two kernel worker denials (`dac_override`, `dac_read_search`) remain;
  the platform policy forbids granting them.  Nothing fails because of
  them.
* Three optional camera post-processing libraries (temporal noise
  reduction, portrait bokeh, "stillmore") are not in the V521 stock ROM.
  The camera falls back to its default processing.

## Bring-up settings

`bringup.mk` is temporary: USB debugging is on and unauthenticated from the
first boot (`mtp,adb`, `WITH_ADB_INSECURE=true`), and logcat is kept in
`/data/misc/logd`.
