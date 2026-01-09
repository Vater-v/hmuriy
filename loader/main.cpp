#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <android/log.h>

namespace fs = std::filesystem;

#define LOG_TAG "ValeraLoader"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* TARGET_SOURCE = "/storage/emulated/0/Misc/valera/hmuriy.so";
const char* TARGET_FILENAME = "hmuriy.so";

std::string get_package_name() {
    std::ifstream cmdline("/proc/self/cmdline");
    std::string packageName;
    if (cmdline) {
        std::getline(cmdline, packageName, '\0');
    }
    return packageName.empty() ? "unknown" : packageName;
}

fs::path get_internal_dir() {
    std::string pkg = get_package_name();
    if (pkg == "unknown") return "/data/local/tmp";
    return fs::path("/data/data") / pkg / "files";
}

void __attribute__((constructor)) init_loader() {
    try {
        fs::path workDir = get_internal_dir();
        fs::path destPath = workDir / TARGET_FILENAME;
        std::error_code ec;

        if (!fs::exists(workDir, ec)) {
            fs::create_directories(workDir, ec);
        }

        if (fs::copy_file(TARGET_SOURCE, destPath, fs::copy_options::overwrite_existing, ec)) {
            fs::permissions(destPath, fs::perms::owner_all, ec);
            
            void* handle = dlopen(destPath.c_str(), RTLD_NOW);
            if (handle) {
                LOGD("Library loaded successfully: %s", destPath.c_str());
            } else {
                LOGE("dlopen failed: %s", dlerror());
            }
        } else {
            LOGE("Copy failed: %s", ec.message().c_str());
        }
    } catch (const std::exception& e) {
        LOGE("Loader exception: %s", e.what());
    }
}