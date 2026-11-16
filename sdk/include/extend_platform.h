#ifndef SV2_EXTEND_PLATFORM_H
#define SV2_EXTEND_PLATFORM_H
static inline const char *loader_platform_key(void) {
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
 return "windows-arm64";
#elif defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
 return "windows-x86_64";
#elif defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
 return "macos-arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
 return "macos-x86_64";
#else
 return "unsupported";
#endif
}
static inline const char *loader_native_library_extension(void) {
#ifdef _WIN32
 return "dll";
#else
 return "dylib";
#endif
}
#endif
