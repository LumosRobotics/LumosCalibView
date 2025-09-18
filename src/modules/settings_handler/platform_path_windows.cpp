#include "settings_handler.h"
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

class WindowsPlatformPath : public PlatformPath {
public:
    std::string getSettingsDirectory(const std::string& appName) const override {
#ifdef _WIN32
        return getAppDataDirectory() + "\\" + appName;
#else
        // Fallback for non-Windows builds
        return "/tmp/" + appName;
#endif
    }
    
    std::string getConfigDirectory(const std::string& appName) const override {
        return getSettingsDirectory(appName);
    }
    
    bool createDirectoryRecursive(const std::string& path) const override {
        try {
            std::filesystem::create_directories(path);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create directory: " << path << " - " << e.what() << std::endl;
            return false;
        }
    }
    
    bool fileExists(const std::string& path) const override {
        return std::filesystem::exists(path);
    }
    
private:
#ifdef _WIN32
    std::string getAppDataDirectory() const {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            return std::string(path);
        }
        
        // Fallback to environment variable
        const char* appdata = getenv("APPDATA");
        if (appdata) {
            return std::string(appdata);
        }
        
        return "C:\\temp";
    }
#endif
};