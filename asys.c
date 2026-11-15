/*
 * Compatibility shim that provides AOSP's android::AudioSystem to LG's
 * CAF radio blobs.
 *
 * Why this exists
 * ---------------
 * LG's CAF radio implementation (vendor/lge/v521/proprietary/vendor/lib/
 * libril-qc-qmi-1.so) was built against LG's Android 8.0 platform.  In
 * Android 8.0 the AudioSystem class lived inside libmedia.so, so the CAF
 * blob gets
 *
 *     android::AudioSystem::getParameters(int, const android::String8&)
 *     android::AudioSystem::setParameters(int, const android::String8&)
 *     android::AudioSystem::setErrorCallback(void (*)(int), int)
 *
 * from its DT_NEEDED entry for libmedia.so.
 *
 * LineageOS 15.1 is Android 8.1, where AudioSystem moved to
 * libaudioclient.so and libmedia.so no longer exports it.  Linking the CAF
 * blob against the 8.1 libmedia.so therefore fails with
 *
 *     cannot locate symbol "_ZN7android11AudioSystem13getParametersEiRKNS_7String8E"
 *
 * The obvious fix - keeping the DT_NEEDED entry for libmedia.so - is not
 * possible, because the 8.1 libmedia.so pulls in libmediadrm.so, and the
 * 8.1 libmediadrm.so is the first consumer of AOSP's protobuf runtime
 * (framework/av/drm/libmediadrm/protos/plugin_metrics.proto was added in
 * Android 8.1).  That runtime has the same symbol names as the CAF runtime
 * that libsettings.so needs, so the linker would bind libmediadrm.so's
 * google::protobuf symbols to the CAF runtime and abort with a protobuf
 * version mismatch.  See notes/protobuf-runtime.md.
 *
 * So the edge is cut: libril-qc-qmi-1.so's libmedia.so is repointed at this
 * library instead.  libaudioclient.so itself has no protobuf dependency
 * (liblog, libcutils, libutils, libbinder, libaudioutils, libaudiomanager,
 * libc++, libc, libm, libdl), so pulling it in keeps AOSP's protobuf runtime
 * out of the radio process.
 *
 * The name has to be no longer than "libmedia.so" (11 characters), because
 * the DT_NEEDED string is patched in place inside libril-qc-qmi-1.so's
 * .dynstr.
 */

#include <stddef.h>

/*
 * Hard references to the three AudioSystem entry points the CAF blob needs.
 * The declarations carry the exact mangled names of the Android 8.1
 * libaudioclient.so, so that the linker has to resolve them - which keeps
 * libaudioclient.so in this library's DT_NEEDED list even if the linker is
 * invoked with --as-needed.
 *
 * These functions are never called; they only exist to create the
 * relocations.
 */
extern int v521_asys_AudioSystem_getParameters(int audio_system,
                                               const void *key)
    __asm__("_ZN7android11AudioSystem13getParametersEiRKNS_7String8E");
extern int v521_asys_AudioSystem_setParameters(int audio_system,
                                               const void *key)
    __asm__("_ZN7android11AudioSystem13setParametersEiRKNS_7String8E");
extern int v521_asys_AudioSystem_setErrorCallback(void (*callback)(int),
                                                  int audio_system)
    __asm__("_ZN7android11AudioSystem16setErrorCallbackEPFviE");

/*
 * Referencing them from an exported, "used" function keeps the relocations
 * alive even when the linker garbage collects unused sections.
 */
__attribute__((used, visibility("default")))
int v521_asys_probe(int audio_system, const void *key) {
    return v521_asys_AudioSystem_getParameters(audio_system, key) +
           v521_asys_AudioSystem_setParameters(audio_system, key) +
           v521_asys_AudioSystem_setErrorCallback(NULL, audio_system);
}
