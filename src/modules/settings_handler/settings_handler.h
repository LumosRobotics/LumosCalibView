#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

class PlatformPath;

class SettingsHandler {
private:
    std::unique_ptr<PlatformPath> platformPath;
    std::string applicationName;
    nlohmann::json settings;
    std::string settingsFilePath;
    
    void ensureSettingsDirectory();
    bool loadSettingsFromFile();
    
public:
    SettingsHandler(const std::string& appName);
    ~SettingsHandler();
    
    // Core functionality
    bool loadSettings();
    bool saveSettings();
    
    // Generic getters and setters
    template<typename T>
    T getSetting(const std::string& key, const T& defaultValue = T{}) const;
    
    template<typename T>
    void setSetting(const std::string& key, const T& value);
    
    // Specialized getters for common types
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    // Specialized setters for common types
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
    
    // Settings management
    bool hasSetting(const std::string& key) const;
    void removeSetting(const std::string& key);
    void clearAllSettings();
    
    // File path information
    std::string getSettingsFilePath() const;
    
    // Export/Import functionality
    bool exportSettings(const std::string& filePath) const;
    bool importSettings(const std::string& filePath);
};

// Abstract base class for platform-specific path handling
class PlatformPath {
public:
    virtual ~PlatformPath() = default;
    virtual std::string getSettingsDirectory(const std::string& appName) const = 0;
    virtual std::string getConfigDirectory(const std::string& appName) const = 0;
    virtual bool createDirectoryRecursive(const std::string& path) const = 0;
    virtual bool fileExists(const std::string& path) const = 0;
    
    // Factory method to create platform-specific implementation
    static std::unique_ptr<PlatformPath> create();
};

// Template implementations must be in header
template<typename T>
T SettingsHandler::getSetting(const std::string& key, const T& defaultValue) const {
    if (settings.contains(key)) {
        try {
            return settings[key].get<T>();
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    return defaultValue;
}

template<typename T>
void SettingsHandler::setSetting(const std::string& key, const T& value) {
    settings[key] = value;
}