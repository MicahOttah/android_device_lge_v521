/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * v521swap - swap manager for the LG V521.
 *
 * The device has 2 GB of RAM and no swap at all, which is what makes the
 * low memory killer so aggressive.  The kernel does support swap
 * (CONFIG_SWAP=y) and has a zram device (CONFIG_ZRAM=y, lzo is the
 * compressor this kernel was built with), so this daemon turns swap on
 * and keeps it in the state the user asked for.
 *
 * Two backends are supported:
 *
 *   zram    - a compressed RAM backed swap area (/dev/block/zram0).
 *   sdcard  - a real swap partition on the removable SD card.  The
 *             partition has to be created once with bin/sdcard-swap on
 *             the host; this daemon only activates it.
 *
 * The configuration comes from three system properties, which the
 * "Swap settings" page of LineageParts writes:
 *
 *   persist.v521.swap.enabled   "1" to have swap active, "0" otherwise
 *   persist.v521.swap.backend   "zram" | "sdcard" | "both"
 *   persist.v521.swap.size      zram size in bytes
 *
 * The daemon publishes what it did in one property:
 *
 *   v521.swap.status            "off" | "on:<backend>:<bytes>" |
 *                               "error:<reason>"
 *
 * and it reconciles the desired state every few seconds, so a swap
 * partition that appears later (the user inserts the card) is picked up
 * without a reboot, and the state survives a reboot because the
 * configuration is persisted.
 *
 * Why not the fstab route?  Android 8.1 can activate swap from the
 * fstab (system/core/fs_mgr/fs_mgr.cpp:1206, fs_mgr_swapon_all()), and
 * init exposes it as the "swapon_all" builtin.  That was not used here:
 *
 *   - it is static.  The fstab is read once at boot, so the user could
 *     not turn swap on and off, or change the size, without a reboot.
 *   - a missing device is an error.  The SD card is not always
 *     inserted, so a fstab entry for it produces an error message on
 *     every boot that does not have the card in it (and with the "wait"
 *     flag it would stall the boot for 20 seconds).
 *   - this kernel does not use the usual swap flag encoding.  fs_mgr
 *     assumes SWAP_FLAG_PRIO_SHIFT=10 and SWAP_FLAG_PRIO_MASK=0x3ff
 *     (upstream Linux), while this kernel uses shift 0 and a 15 bit
 *     mask with SWAP_FLAG_PREFER=0x8000
 *     (kernel/lge/msm8952/include/linux/swap.h:19), so a priority that
 *     fs_mgr computes would be misinterpreted.
 *
 * Notes on the kernel specifics of this device:
 *
 *   - The swap header is the old (Linux 2.2) layout: the "SWAPSPACE2"
 *     magic sits in the last 10 bytes of the first page and the version
 *     / last_page / nr_badpages fields are at offset 1024 of that page
 *     (kernel/lge/msm8952/include/linux/swap.h:84).  A modern mkswap
 *     would not be understood by this kernel; toybox's mkswap writes
 *     the old layout, and so does this daemon.
 *   - The zram driver of this kernel refuses a disksize of 0 and only
 *     accepts a new disksize on a device that has not been initialised
 *     yet, so the device has to be reset through
 *     /sys/block/zram0/reset before the size can be changed
 *     (kernel/lge/msm8952/drivers/block/zram/zram_drv.c:872).
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/system_properties.h>

/* ALOGE/ALOGI/ALOGW live in <log/log.h> (liblog); <android/log.h> only
 * declares __android_log_print and would leave the ALOG* macros
 * undefined. */
#include <log/log.h>

#define LOG_TAG "v521swap"

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

#define PROP_ENABLED    "persist.v521.swap.enabled"
#define PROP_BACKEND    "persist.v521.swap.backend"
#define PROP_SIZE       "persist.v521.swap.size"
#define PROP_STATUS     "v521.swap.status"

#define BACKEND_ZRAM    "zram"
#define BACKEND_SDCARD  "sdcard"
#define BACKEND_BOTH    "both"

#define DEFAULT_ZRAM_SIZE  (512ULL * 1024ULL * 1024ULL)
#define MIN_ZRAM_SIZE      (64ULL * 1024ULL * 1024ULL)
#define MAX_ZRAM_SIZE      (1024ULL * 1024ULL * 1024ULL)

/* ------------------------------------------------------------------ */
/* Kernel interfaces                                                   */
/* ------------------------------------------------------------------ */

/*
 * Swap priorities.  This kernel does not use the upstream encoding:
 *
 *   kernel/lge/msm8952/include/linux/swap.h:19
 *     #define SWAP_FLAG_PREFER   0x8000
 *     #define SWAP_FLAG_PRIO_MASK 0x7fff
 *     #define SWAP_FLAG_PRIO_SHIFT 0
 *
 * so the priority is simply the low 15 bits of the flags and
 * SWAP_FLAG_PREFER turns it on.
 */
#define V521_SWAP_FLAG_PREFER     0x8000
#define V521_SWAP_FLAG_PRIO_MASK  0x7fff
#define V521_SWAP_FLAG_PRIO_SHIFT 0

#define SWAP_PRIO_ZRAM    1000
#define SWAP_PRIO_SDCARD  100

#define ZRAM_DEV        "/dev/block/zram0"
#define ZRAM_SYSFS      "/sys/block/zram0"
#define ZRAM_DISKSIZE   ZRAM_SYSFS "/disksize"
#define ZRAM_RESET      ZRAM_SYSFS "/reset"
#define ZRAM_STREAMS    ZRAM_SYSFS "/max_comp_streams"

/*
 * The removable card is mmcblk1.  Only the partition nodes are touched:
 *
 *   - /dev/block/mmcblk1 is the whole disk; it carries the type
 *     "sd_device" (device/qcom/sepolicy/common/file_contexts:45) and is
 *     only read, to get the partition table.
 *   - /dev/block/mmcblk1p2 is the swap partition; it carries the type
 *     "swap_block_device" (device/lge/v521/sepolicy/file_contexts) and is
 *     the only node that is written.
 *
 * A domain that is not init, kernel or recovery may not open a node of
 * the type "block_device" for reading or writing
 * (system/sepolicy/public/domain.te:332), which is what every partition
 * of the internal disk and every partition of the card that is not
 * covered by a more specific entry would get.
 */
#define SD_DISK         "/dev/block/mmcblk1"
#define SD_PART_PREFIX  "/dev/block/mmcblk1p"
#define SD_PART_PREFIX_LEN (sizeof(SD_PART_PREFIX) - 1)
#define SD_MAX_PARTS    8

#define PROC_SWAPS      "/proc/swaps"

#define MBR_TYPE_SWAP   0x82

#define PAGE_SIZE_4K    4096

#define MAX_SWAP_ENTRIES 8

struct swap_entry {
    char device[PATH_MAX];
    char type[32];
    unsigned long long size_kb;
    unsigned long long used_kb;
    int priority;
};

struct sd_partition {
    unsigned char type;
    uint32_t start_lba;
    uint32_t num_sectors;
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void sleep_seconds(unsigned int seconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* continue with the remaining time */
    }
}

static int prop_get(const char *name, char *value, size_t len) {
    return __system_property_get(name, value) > 0 ? 0 : -1;
}

static int prop_get_int(const char *name, int fallback) {
    char value[PROP_VALUE_MAX];
    char *end;
    long parsed;

    if (prop_get(name, value, sizeof(value)) != 0)
        return fallback;

    errno = 0;
    parsed = strtol(value, &end, 0);
    if (errno != 0 || end == value)
        return fallback;

    return (int)parsed;
}

static unsigned long long prop_get_size(const char *name, unsigned long long fallback) {
    char value[PROP_VALUE_MAX];
    char *end;
    unsigned long long parsed;

    if (prop_get(name, value, sizeof(value)) != 0)
        return fallback;

    errno = 0;
    parsed = strtoull(value, &end, 0);
    if (errno != 0 || end == value || parsed == 0)
        return fallback;

    return parsed;
}

static void prop_set(const char *name, const char *value) {
    int ret = __system_property_set(name, value);
    if (ret != 0) {
        ALOGE("could not set %s=%s: %s", name, value, strerror(ret));
        return;
    }
    ALOGI("set %s=%s", name, value);
}

static int write_file(const char *path, const void *data, size_t len) {
    int fd;
    ssize_t written;

    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("open %s failed: %s", path, strerror(errno));
        return -1;
    }

    written = write(fd, data, len);
    if (written < 0 || (size_t)written != len) {
        ALOGE("write %s failed: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int write_sysfs(const char *path, const char *value) {
    return write_file(path, value, strlen(value));
}

/* ------------------------------------------------------------------ */
/* Swap table                                                          */
/* ------------------------------------------------------------------ */

/*
 * /proc/swaps looks like this:
 *
 *   Filename                             Type        Size    Used  Priority
 *   /dev/block/zram0                     partition  524284      0      -2
 */
static int read_swap_table(struct swap_entry *entries, int max_entries) {
    FILE *fp;
    char line[512];
    int count = 0;

    fp = fopen(PROC_SWAPS, "r");
    if (fp == NULL)
        return -1;

    /* skip the header line */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    while (count < max_entries && fgets(line, sizeof(line), fp) != NULL) {
        struct swap_entry *entry = &entries[count];
        char type[32];
        unsigned long long size_kb, used_kb;
        int priority;
        char device[PATH_MAX];

        if (sscanf(line, "%4095s %31s %llu %llu %d",
                   device, type, &size_kb, &used_kb, &priority) != 5)
            continue;

        snprintf(entry->device, sizeof(entry->device), "%s", device);
        snprintf(entry->type, sizeof(entry->type), "%s", type);
        entry->size_kb = size_kb;
        entry->used_kb = used_kb;
        entry->priority = priority;
        count++;
    }

    fclose(fp);
    return count;
}

static int swap_table_has(const struct swap_entry *entries, int count, const char *device) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].device, device) == 0)
            return 1;
    }
    return 0;
}

static unsigned long long swap_table_size_of(const struct swap_entry *entries, int count,
                                             const char *device) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].device, device) == 0)
            return entries[i].size_kb * 1024ULL;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Kernel calls                                                        */
/* ------------------------------------------------------------------ */

static int do_swapon(const char *path, int priority) {
    int flags = V521_SWAP_FLAG_PREFER |
                ((priority << V521_SWAP_FLAG_PRIO_SHIFT) & V521_SWAP_FLAG_PRIO_MASK);

    if (syscall(__NR_swapon, path, flags) != 0) {
        ALOGE("swapon(%s) failed: %s", path, strerror(errno));
        return -1;
    }

    ALOGI("swapon(%s) ok, priority %d", path, priority);
    return 0;
}

static int do_swapoff(const char *path) {
    if (syscall(__NR_swapoff, path) != 0) {
        ALOGE("swapoff(%s) failed: %s", path, strerror(errno));
        return -1;
    }

    ALOGI("swapoff(%s) ok", path);
    return 0;
}

/*
 * Write a swap header in the layout this kernel understands.
 *
 * The magic is in the last ten bytes of the first page, the version,
 * last_page and nr_badpages fields are at offset 1024 of that page
 * (kernel/lge/msm8952/include/linux/swap.h:84).  This is the same
 * layout that toybox's mkswap writes.
 */
static int write_swap_header(const char *path, unsigned long long size_bytes) {
    unsigned char page[PAGE_SIZE_4K];
    uint32_t *info = (uint32_t *)(page + 1024);
    unsigned long long pages;
    int fd;
    ssize_t written;

    if (size_bytes < PAGE_SIZE_4K * 16ULL) {
        ALOGE("swap area %s is too small (%" PRIu64 " bytes)", path, size_bytes);
        return -1;
    }

    memset(page, 0, sizeof(page));

    pages = size_bytes / PAGE_SIZE_4K;
    if (pages == 0)
        pages = 1;

    info[0] = 1;                      /* version */
    info[1] = (uint32_t)(pages - 1);  /* last_page */
    info[2] = 0;                      /* nr_badpages */

    /* uuid (offset 1036) and volume label (offset 1052) */
    {
        int urandom = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (urandom >= 0) {
            if (read(urandom, page + 1036, 16) != 16)
                memset(page + 1036, 0, 16);
            close(urandom);
        }
    }
    memcpy(page + 1052, "v521swap", 8);

    memcpy(page + PAGE_SIZE_4K - 10, "SWAPSPACE2", 10);

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("open %s failed: %s", path, strerror(errno));
        return -1;
    }

    written = write(fd, page, sizeof(page));
    if (written < 0 || (size_t)written != sizeof(page)) {
        ALOGE("write swap header to %s failed: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (fsync(fd) != 0)
        ALOGW("fsync %s failed: %s", path, strerror(errno));

    close(fd);
    return 0;
}

/* ------------------------------------------------------------------ */
/* zram                                                                */
/* ------------------------------------------------------------------ */

static int zram_reset(void) {
    /*
     * The reset attribute refuses to work while the device is in use
     * (bd_openers), so this has to happen after the swapoff.
     */
    if (write_sysfs(ZRAM_RESET, "1") != 0) {
        /* Not initialised yet is not an error. */
        ALOGI("zram reset failed (probably not initialised yet)");
        return -1;
    }
    return 0;
}

static int zram_configure(unsigned long long size) {
    char value[32];

    snprintf(value, sizeof(value), "%" PRIu64, size);
    if (write_sysfs(ZRAM_DISKSIZE, value) != 0) {
        ALOGE("could not set zram disksize to %s", value);
        return -1;
    }

    /* More compression streams help when several pages are in flight. */
    write_sysfs(ZRAM_STREAMS, "4");

    return 0;
}

static int zram_activate(unsigned long long size) {
    /* A device that was used before has to be reset before the size can
     * be set again (the driver refuses disksize 0 and an already
     * initialised device). */
    zram_reset();

    if (zram_configure(size) != 0)
        return -1;

    if (write_swap_header(ZRAM_DEV, size) != 0)
        return -1;

    return do_swapon(ZRAM_DEV, SWAP_PRIO_ZRAM);
}

/* ------------------------------------------------------------------ */
/* SD card                                                             */
/* ------------------------------------------------------------------ */

static int read_mbr(struct sd_partition *parts, int max_parts) {
    unsigned char mbr[512];
    int fd;
    ssize_t got;
    int i, count = 0;

    fd = open(SD_DISK, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGI("cannot open %s: %s", SD_DISK, strerror(errno));
        return -1;
    }

    got = read(fd, mbr, sizeof(mbr));
    close(fd);

    if (got != (ssize_t)sizeof(mbr)) {
        ALOGE("cannot read the MBR of %s", SD_DISK);
        return -1;
    }

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        ALOGE("%s does not have an MBR signature", SD_DISK);
        return -1;
    }

    for (i = 0; i < 4 && count < max_parts; i++) {
        const unsigned char *entry = mbr + 0x1BE + i * 16;
        uint32_t start, sectors;

        if (entry[4] == 0)
            continue;

        start = (uint32_t)entry[8] | ((uint32_t)entry[9] << 8) |
                ((uint32_t)entry[10] << 16) | ((uint32_t)entry[11] << 24);
        sectors = (uint32_t)entry[12] | ((uint32_t)entry[13] << 8) |
                  ((uint32_t)entry[14] << 16) | ((uint32_t)entry[15] << 24);

        parts[count].type = entry[4];
        parts[count].start_lba = start;
        parts[count].num_sectors = sectors;
        count++;
    }

    return count;
}

static unsigned long long device_size(const char *path) {
    int fd;
    unsigned long long size = 0;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;

#ifdef BLKGETSIZE64
    if (ioctl(fd, BLKGETSIZE64, &size) != 0)
        size = 0;
#else
    {
        off_t end = lseek(fd, 0, SEEK_END);
        if (end > 0)
            size = (unsigned long long)end;
    }
#endif

    close(fd);
    return size;
}

/*
 * Find the swap partition on the SD card.  The MBR entry of type 0x82
 * gives the partition number, the size of the block device is used to
 * make sure that the node really is the partition the MBR describes
 * (the kernel numbers the partitions after the entries of the table).
 */
static int find_sd_swap(char *out, size_t out_len) {
    struct sd_partition parts[8];
    int count, i;

    count = read_mbr(parts, 8);
    if (count <= 0)
        return -1;

    for (i = 0; i < count; i++) {
        char candidate[PATH_MAX];
        unsigned long long size;

        if (parts[i].type != MBR_TYPE_SWAP)
            continue;

        snprintf(candidate, sizeof(candidate), "/dev/block/mmcblk1p%d", i + 1);

        size = device_size(candidate);
        if (size == 0)
            continue;

        if (size != (unsigned long long)parts[i].num_sectors * 512ULL) {
            ALOGW("%s is %" PRIu64 " bytes but the MBR says %" PRIu64 ", skipping",
                 candidate, size, (unsigned long long)parts[i].num_sectors * 512ULL);
            continue;
        }

        snprintf(out, out_len, "%s", candidate);
        return 0;
    }

    return -1;
}

static int sdcard_activate(const char *device, unsigned long long size) {
    if (write_swap_header(device, size) != 0)
        return -1;

    return do_swapon(device, SWAP_PRIO_SDCARD);
}

/* ------------------------------------------------------------------ */
/* Reconciliation                                                      */
/* ------------------------------------------------------------------ */

static void publish(const char *status) {
    if (strcmp(status, "") == 0)
        return;

    char current[PROP_VALUE_MAX];
    if (prop_get(PROP_STATUS, current, sizeof(current)) == 0 &&
        strcmp(current, status) == 0)
        return;

    prop_set(PROP_STATUS, status);
}

static void reconcile(void) {
    struct swap_entry entries[MAX_SWAP_ENTRIES];
    int count;
    int enabled = prop_get_int(PROP_ENABLED, 0);
    char backend[32];
    unsigned long long zram_size;
    int want_zram = 0, want_sdcard = 0;
    char sd_device[PATH_MAX];
    int sd_available = 0;
    char status[256];
    int i;
    int active_zram = 0, active_sdcard = 0;

    if (prop_get(PROP_BACKEND, backend, sizeof(backend)) != 0)
        snprintf(backend, sizeof(backend), "%s", BACKEND_ZRAM);

    zram_size = prop_get_size(PROP_SIZE, DEFAULT_ZRAM_SIZE);
    if (zram_size < MIN_ZRAM_SIZE)
        zram_size = MIN_ZRAM_SIZE;
    if (zram_size > MAX_ZRAM_SIZE)
        zram_size = MAX_ZRAM_SIZE;

    if (!enabled) {
        count = read_swap_table(entries, MAX_SWAP_ENTRIES);
        for (i = 0; i < count; i++) {
            if (strcmp(entries[i].device, ZRAM_DEV) == 0 ||
                strncmp(entries[i].device, "/dev/block/mmcblk1p", 21) == 0)
                do_swapoff(entries[i].device);
        }
        if (count > 0)
            zram_reset();
        publish("off");
        return;
    }

    if (strcmp(backend, BACKEND_ZRAM) == 0) {
        want_zram = 1;
    } else if (strcmp(backend, BACKEND_SDCARD) == 0) {
        want_sdcard = 1;
    } else if (strcmp(backend, BACKEND_BOTH) == 0) {
        want_zram = 1;
        want_sdcard = 1;
    } else {
        snprintf(backend, sizeof(backend), "%s", BACKEND_ZRAM);
        want_zram = 1;
    }

    if (want_sdcard) {
        if (find_sd_swap(sd_device, sizeof(sd_device)) == 0)
            sd_available = 1;
    }

    count = read_swap_table(entries, MAX_SWAP_ENTRIES);

    for (i = 0; i < count; i++) {
        const char *device = entries[i].device;

        if (strcmp(device, ZRAM_DEV) == 0) {
            if (!want_zram) {
                do_swapoff(device);
                zram_reset();
                continue;
            }
            if (swap_table_size_of(entries, count, ZRAM_DEV) != zram_size) {
                ALOGI("zram size changed, reactivating");
                do_swapoff(device);
                zram_reset();
                continue;
            }
            active_zram = 1;
            continue;
        }

        if (strncmp(device, "/dev/block/mmcblk1p", 21) == 0) {
            if (!want_sdcard || !sd_available ||
                strcmp(device, sd_device) != 0) {
                do_swapoff(device);
                continue;
            }
            active_sdcard = 1;
            continue;
        }

        /* Something else is using swap - leave it alone. */
    }

    if (want_zram && !active_zram) {
        if (zram_activate(zram_size) == 0)
            active_zram = 1;
    }

    if (want_sdcard && !active_sdcard) {
        if (!sd_available) {
            ALOGW("no swap partition on the SD card");
            publish("error:no-sd-swap-partition");
            return;
        }
        if (sdcard_activate(sd_device, device_size(sd_device)) == 0)
            active_sdcard = 1;
    }

    if (active_zram && active_sdcard) {
        snprintf(status, sizeof(status), "on:%s,%s:%" PRIu64,
                 BACKEND_ZRAM, BACKEND_SDCARD, zram_size);
    } else if (active_zram) {
        snprintf(status, sizeof(status), "on:%s:%" PRIu64, BACKEND_ZRAM, zram_size);
    } else if (active_sdcard) {
        snprintf(status, sizeof(status), "on:%s:%" PRIu64, BACKEND_SDCARD,
                 swap_table_size_of(entries, count, sd_device));
    } else {
        snprintf(status, sizeof(status), "error:swapon-failed");
    }

    publish(status);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [--once] [--status]\n"
            "\n"
            "  --once     reconcile the swap state once and exit\n"
            "  --status   print the current swap state and exit\n",
            argv0);
}

static void print_status(void) {
    struct swap_entry entries[MAX_SWAP_ENTRIES];
    int count = read_swap_table(entries, MAX_SWAP_ENTRIES);
    int i;

    if (count <= 0) {
        printf("no swap device is active\n");
        return;
    }

    for (i = 0; i < count; i++) {
        printf("%s type=%s size=%" PRIu64 "KB used=%" PRIu64 "KB priority=%d\n",
               entries[i].device, entries[i].type, entries[i].size_kb,
               entries[i].used_kb, entries[i].priority);
    }
}

int main(int argc, char **argv) {
    int once = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) {
            once = 1;
        } else if (strcmp(argv[i], "--status") == 0) {
            print_status();
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    if (once) {
        reconcile();
        return 0;
    }

    ALOGI("v521swap started");
    while (!g_stop) {
        reconcile();
        sleep_seconds(5);
    }
    ALOGI("v521swap stopped");

    return 0;
}
