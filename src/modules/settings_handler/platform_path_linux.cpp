#include "settings_handler.h"
#include <filesystem>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <iostream>

class LinuxPlatformPath : public PlatformPath {
public:
    std::string getSettingsDirectory(const std::string& appName) const override {
        // Use XDG Base Directory Specification
        const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
        if (xdgConfigHome) {
            return std::string(xdgConfigHome) + "/" + appName;
        }
        
        return getHomeDirectory() + "/.config/" + appName;
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
    std::string getHomeDirectory() const {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home);
        }
        
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            return std::string(pw->pw_dir);
        }
        
        return "/tmp";
    }
};