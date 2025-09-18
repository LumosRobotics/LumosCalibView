#include "settings_handler.h"
#include <filesystem>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <iostream>

class MacOSPlatformPath : public PlatformPath {
public:
    std::string getSettingsDirectory(const std::string& appName) const override {
        return getApplicationSupportDirectory() + "/" + appName;
    }
    
    std::string getConfigDirectory(const std::string& appName) const override {
        return getApplicationSupportDirectory() + "/" + appName;
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
    std::string getApplicationSupportDirectory() const {
        // Try to get home directory from environment
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + "/Library/Application Support";
        }
        
        // Fallback: get home directory from passwd
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            return std::string(pw->pw_dir) + "/Library/Application Support";
        }
        
        // Last resort fallback
        return "/tmp";
    }
};

// Factory method implementation
std::unique_ptr<PlatformPath> PlatformPath::create() {
#ifdef __APPLE__
    return std::make_unique<MacOSPlatformPath>();
#else
    // For now, we'll use the macOS implementation as fallback
    // In a real implementation, you'd have separate files for Windows and Linux
    return std::make_unique<MacOSPlatformPath>();
#endif
}