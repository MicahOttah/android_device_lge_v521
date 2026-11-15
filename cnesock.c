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
 * The two functions below are the whole of what LG's CAF blob libwqe.so
 * imports from AOSP's libandroid.so.  They are copied from
 * frameworks/base/native/android/net.c (Apache 2.0, AOSP), which is what
 * libandroid.so is built from.
 *
 * They live in their own library so that libwqe.so can be re-pointed at it:
 * libandroid.so pulls in libandroid_runtime.so -> libhwui.so / libmedia.so ->
 * libmediadrm.so -> AOSP's protobuf runtime, and in a process that already
 * has CAF's protobuf runtime in its global scope (cnd) the AOSP libraries
 * bind their protobuf symbols to the CAF runtime and abort with a protobuf
 * version mismatch.
 *
 * The argument order is the one AOSP has used since Android 6.0 and which is
 * still used in 8.1:
 *
 *     int android_setsocknetwork(net_handle_t network, int fd);
 *     int android_getaddrinfofornetwork(net_handle_t network, const char* node,
 *                                       const char* service,
 *                                       const struct addrinfo* hints,
 *                                       struct addrinfo** res);
 *
 * See notes/protobuf-runtime.md.
 */

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>

#include <NetdClient.h>

/* android/multinetwork.h is not part of the NDK headers, so the two types
 * it introduces are declared here.  They have to match the public header
 * exactly, because libwqe.so was compiled against it. */
typedef uint64_t net_handle_t;

#define NETWORK_UNSPECIFIED ((net_handle_t)0)

/*
 * bionic exports android_getaddrinfofornet() from libc.so (in the
 * LIBC_PRIVATE version).  It is declared weak so that this shim also links
 * against the NDK's libc stub; at run time the dynamic linker either binds
 * it to the copy in libc.so or leaves it NULL, in which case we fall back to
 * the default resolver below.
 */
extern int android_getaddrinfofornet(const char* node, const char* service,
                                      const struct addrinfo* hints,
                                      unsigned netid, unsigned flags,
                                      struct addrinfo** res) __attribute__((weak));

/*
 * Copy of getnetidfromhandle() from frameworks/base/native/android/net.c:
 * a valid network handle always carries kHandleMagic in its low 32 bits.
 */
static int getnetidfromhandle(net_handle_t handle, unsigned* netid) {
    static const uint32_t k32BitMask = 0xffffffff;
    /* This value MUST be kept in sync with the corresponding value in
     * the android.net.Network#getNetworkHandle() implementation. */
    static const uint32_t kHandleMagic = 0xfacade;

    /* Check for minimum acceptable version of the API in the low bits. */
    if (handle != NETWORK_UNSPECIFIED &&
            (handle & k32BitMask) != kHandleMagic) {
        return 0;
    }

    if (netid != NULL) {
        *netid = ((handle >> (CHAR_BIT * sizeof(k32BitMask))) & k32BitMask);
    }
    return 1;
}

int android_setsocknetwork(net_handle_t network, int fd) {
    unsigned netid;
    if (!getnetidfromhandle(network, &netid)) {
        errno = EINVAL;
        return -1;
    }

    int rval = setNetworkForSocket(netid, fd);
    if (rval < 0) {
        errno = -rval;
        rval = -1;
    }
    return rval;
}

int android_getaddrinfofornetwork(net_handle_t network, const char* node,
        const char* service, const struct addrinfo* hints, struct addrinfo** res) {
    unsigned netid;
    if (!getnetidfromhandle(network, &netid)) {
        errno = EINVAL;
        return EAI_SYSTEM;
    }

    if (netid != 0 && android_getaddrinfofornet != NULL) {
        return android_getaddrinfofornet(node, service, hints, netid, 0, res);
    }

    /* NETWORK_UNSPECIFIED resolves through the default network, which is
     * exactly what plain getaddrinfo() does. */
    return getaddrinfo(node, service, hints, res);
}
