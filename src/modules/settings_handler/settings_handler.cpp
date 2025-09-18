#include "settings_handler.h"
#include <fstream>
#include <iostream>
#include <filesystem>

SettingsHandler::SettingsHandler(const std::string& appName) 
    : applicationName(appName) {
    platformPath = PlatformPath::create();
    
    // Determine settings file path
    std::string settingsDir = platformPath->getSettingsDirectory(applicationName);
    settingsFilePath = settingsDir + "/settings.json";
    
    // Load existing settings
    loadSettings();
}

SettingsHandler::~SettingsHandler() {
    // Auto-save settings on destruction
    saveSettings();
}

bool SettingsHandler::loadSettings() {
    ensureSettingsDirectory();
    return loadSettingsFromFile();
}

bool SettingsHandler::saveSettings() {
    ensureSettingsDirectory();
    
    try {
        std::ofstream file(settingsFilePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open settings file for writing: " << settingsFilePath << std::endl;
            return false;
        }
        
        file << settings.dump(4); // Pretty print with 4-space indentation
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save settings: " << e.what() << std::endl;
        return false;
    }
}

void SettingsHandler::ensureSettingsDirectory() {
    std::string settingsDir = platformPath->getSettingsDirectory(applicationName);
    if (!platformPath->createDirectoryRecursive(settingsDir)) {
        std::cerr << "Failed to create settings directory: " << settingsDir << std::endl;
    }
}

bool SettingsHandler::loadSettingsFromFile() {
    if (!platformPath->fileExists(settingsFilePath)) {
        // File doesn't exist, start with empty settings
        settings = nlohmann::json::object();
        return true;
    }
    
    try {
        std::ifstream file(settingsFilePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open settings file: " << settingsFilePath << std::endl;
            return false;
        }
        
        file >> settings;
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load settings: " << e.what() << std::endl;
        settings = nlohmann::json::object();
        return false;
    }
}

// Specialized getters
std::string SettingsHandler::getString(const std::string& key, const std::string& defaultValue) const {
    return getSetting<std::string>(key, defaultValue);
}

int SettingsHandler::getInt(const std::string& key, int defaultValue) const {
    return getSetting<int>(key, defaultValue);
}

double SettingsHandler::getDouble(const std::string& key, double defaultValue) const {
    return getSetting<double>(key, defaultValue);
}

bool SettingsHandler::getBool(const std::string& key, bool defaultValue) const {
    return getSetting<bool>(key, defaultValue);
}

// Specialized setters
void SettingsHandler::setString(const std::string& key, const std::string& value) {
    setSetting<std::string>(key, value);
}

void SettingsHandler::setInt(const std::string& key, int value) {
    setSetting<int>(key, value);
}

void SettingsHandler::setDouble(const std::string& key, double value) {
    setSetting<double>(key, value);
}

void SettingsHandler::setBool(const std::string& key, bool value) {
    setSetting<bool>(key, value);
}

// Settings management
bool SettingsHandler::hasSetting(const std::string& key) const {
    return settings.contains(key);
}

void SettingsHandler::removeSetting(const std::string& key) {
    if (settings.contains(key)) {
        settings.erase(key);
    }
}

void SettingsHandler::clearAllSettings() {
    settings.clear();
}

std::string SettingsHandler::getSettingsFilePath() const {
    return settingsFilePath;
}

// Export/Import functionality
bool SettingsHandler::exportSettings(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open export file: " << filePath << std::endl;
            return false;
        }
        
        file << settings.dump(4);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to export settings: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsHandler::importSettings(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open import file: " << filePath << std::endl;
            return false;
        }
        
        nlohmann::json importedSettings;
        file >> importedSettings;
        file.close();
        
        // Merge imported settings with existing ones
        settings.update(importedSettings);
        
        // Save the merged settings
        return saveSettings();
    } catch (const std::exception& e) {
        std::cerr << "Failed to import settings: " << e.what() << std::endl;
        return false;
    }
}