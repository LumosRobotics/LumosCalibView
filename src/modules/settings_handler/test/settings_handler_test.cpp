#include <gtest/gtest.h>
#include "../settings_handler.h"
#include <filesystem>
#include <fstream>
#include <memory>

class SettingsHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testAppName = "TestApp_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        settings = std::make_unique<SettingsHandler>(testAppName);
        
        // Clear any existing settings
        settings->clearAllSettings();
    }

    void TearDown() override {
        // Clean up test files
        if (settings) {
            std::string filePath = settings->getSettingsFilePath();
            settings.reset(); // Destroy settings handler first
            
            // Remove the test settings file and directory
            if (std::filesystem::exists(filePath)) {
                std::filesystem::remove(filePath);
                
                // Try to remove the directory (will only succeed if empty)
                std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
                std::filesystem::remove(dir);
            }
        }
    }

    std::unique_ptr<SettingsHandler> settings;
    std::string testAppName;
};

TEST_F(SettingsHandlerTest, BasicStringOperations) {
    EXPECT_FALSE(settings->hasSetting("test_string"));
    EXPECT_EQ(settings->getString("test_string", "default"), "default");
    
    settings->setString("test_string", "hello world");
    EXPECT_TRUE(settings->hasSetting("test_string"));
    EXPECT_EQ(settings->getString("test_string"), "hello world");
    EXPECT_EQ(settings->getString("test_string", "default"), "hello world");
}

TEST_F(SettingsHandlerTest, BasicIntegerOperations) {
    EXPECT_FALSE(settings->hasSetting("test_int"));
    EXPECT_EQ(settings->getInt("test_int", 42), 42);
    
    settings->setInt("test_int", 123);
    EXPECT_TRUE(settings->hasSetting("test_int"));
    EXPECT_EQ(settings->getInt("test_int"), 123);
    EXPECT_EQ(settings->getInt("test_int", 42), 123);
}

TEST_F(SettingsHandlerTest, BasicDoubleOperations) {
    EXPECT_FALSE(settings->hasSetting("test_double"));
    EXPECT_DOUBLE_EQ(settings->getDouble("test_double", 3.14), 3.14);
    
    settings->setDouble("test_double", 2.718);
    EXPECT_TRUE(settings->hasSetting("test_double"));
    EXPECT_DOUBLE_EQ(settings->getDouble("test_double"), 2.718);
}

TEST_F(SettingsHandlerTest, BasicBooleanOperations) {
    EXPECT_FALSE(settings->hasSetting("test_bool"));
    EXPECT_TRUE(settings->getBool("test_bool", true));
    EXPECT_FALSE(settings->getBool("test_bool", false));
    
    settings->setBool("test_bool", true);
    EXPECT_TRUE(settings->hasSetting("test_bool"));
    EXPECT_TRUE(settings->getBool("test_bool"));
    EXPECT_TRUE(settings->getBool("test_bool", false));
    
    settings->setBool("test_bool", false);
    EXPECT_FALSE(settings->getBool("test_bool"));
}

TEST_F(SettingsHandlerTest, GenericTemplateOperations) {
    // Test with custom types using the template interface
    std::vector<int> testVector = {1, 2, 3, 4, 5};
    
    settings->setSetting("test_vector", testVector);
    EXPECT_TRUE(settings->hasSetting("test_vector"));
    
    std::vector<int> retrievedVector = settings->getSetting<std::vector<int>>("test_vector");
    EXPECT_EQ(retrievedVector.size(), 5u);
    EXPECT_EQ(retrievedVector, testVector);
}

TEST_F(SettingsHandlerTest, SettingsRemoval) {
    settings->setString("temp_setting", "value");
    EXPECT_TRUE(settings->hasSetting("temp_setting"));
    
    settings->removeSetting("temp_setting");
    EXPECT_FALSE(settings->hasSetting("temp_setting"));
    EXPECT_EQ(settings->getString("temp_setting", "default"), "default");
}

TEST_F(SettingsHandlerTest, ClearAllSettings) {
    settings->setString("setting1", "value1");
    settings->setInt("setting2", 42);
    settings->setBool("setting3", true);
    
    EXPECT_TRUE(settings->hasSetting("setting1"));
    EXPECT_TRUE(settings->hasSetting("setting2"));
    EXPECT_TRUE(settings->hasSetting("setting3"));
    
    settings->clearAllSettings();
    
    EXPECT_FALSE(settings->hasSetting("setting1"));
    EXPECT_FALSE(settings->hasSetting("setting2"));
    EXPECT_FALSE(settings->hasSetting("setting3"));
}

TEST_F(SettingsHandlerTest, PersistenceAcrossInstances) {
    // Create settings in first instance
    settings->setString("persistent_string", "persistent_value");
    settings->setInt("persistent_int", 999);
    settings->setBool("persistent_bool", true);
    
    // Force save and destroy first instance
    EXPECT_TRUE(settings->saveSettings());
    settings.reset();
    
    // Create new instance with same app name
    settings = std::make_unique<SettingsHandler>(testAppName);
    
    // Verify settings persisted
    EXPECT_TRUE(settings->hasSetting("persistent_string"));
    EXPECT_TRUE(settings->hasSetting("persistent_int"));
    EXPECT_TRUE(settings->hasSetting("persistent_bool"));
    
    EXPECT_EQ(settings->getString("persistent_string"), "persistent_value");
    EXPECT_EQ(settings->getInt("persistent_int"), 999);
    EXPECT_TRUE(settings->getBool("persistent_bool"));
}

TEST_F(SettingsHandlerTest, FilePathAccessible) {
    std::string filePath = settings->getSettingsFilePath();
    EXPECT_FALSE(filePath.empty());
    EXPECT_TRUE(filePath.find(".json") != std::string::npos);
    EXPECT_TRUE(filePath.find(testAppName) != std::string::npos);
}

TEST_F(SettingsHandlerTest, ExportImportSettings) {
    // Setup some test settings
    settings->setString("export_string", "exported_value");
    settings->setInt("export_int", 777);
    settings->setBool("export_bool", false);
    
    // Export to temporary file
    std::string exportPath = "/tmp/test_export_" + testAppName + ".json";
    EXPECT_TRUE(settings->exportSettings(exportPath));
    EXPECT_TRUE(std::filesystem::exists(exportPath));
    
    // Clear current settings
    settings->clearAllSettings();
    EXPECT_FALSE(settings->hasSetting("export_string"));
    
    // Import settings back
    EXPECT_TRUE(settings->importSettings(exportPath));
    
    // Verify imported settings
    EXPECT_TRUE(settings->hasSetting("export_string"));
    EXPECT_TRUE(settings->hasSetting("export_int"));
    EXPECT_TRUE(settings->hasSetting("export_bool"));
    
    EXPECT_EQ(settings->getString("export_string"), "exported_value");
    EXPECT_EQ(settings->getInt("export_int"), 777);
    EXPECT_FALSE(settings->getBool("export_bool"));
    
    // Cleanup
    std::filesystem::remove(exportPath);
}

TEST_F(SettingsHandlerTest, InvalidJsonHandling) {
    // Create a malformed JSON file
    std::string settingsPath = settings->getSettingsFilePath();
    
    // Ensure directory exists
    std::filesystem::create_directories(std::filesystem::path(settingsPath).parent_path());
    
    // Write invalid JSON
    std::ofstream file(settingsPath);
    file << "{ invalid json content }";
    file.close();
    
    // Create new settings handler - should handle invalid JSON gracefully
    auto newSettings = std::make_unique<SettingsHandler>(testAppName);
    
    // Should start with empty settings due to invalid JSON
    EXPECT_FALSE(newSettings->hasSetting("any_key"));
    
    // Should be able to set and save new settings
    newSettings->setString("recovery_test", "recovered");
    EXPECT_TRUE(newSettings->saveSettings());
    
    // Cleanup
    newSettings.reset();
}

TEST_F(SettingsHandlerTest, MultipleDataTypes) {
    // Test storing and retrieving various data types
    settings->setString("string_val", "test string with special chars: åäö");
    settings->setInt("negative_int", -42);
    settings->setDouble("scientific_double", 1.23e-10);
    settings->setBool("true_bool", true);
    settings->setBool("false_bool", false);
    
    // Save and reload
    EXPECT_TRUE(settings->saveSettings());
    settings = std::make_unique<SettingsHandler>(testAppName);
    
    // Verify all types
    EXPECT_EQ(settings->getString("string_val"), "test string with special chars: åäö");
    EXPECT_EQ(settings->getInt("negative_int"), -42);
    EXPECT_DOUBLE_EQ(settings->getDouble("scientific_double"), 1.23e-10);
    EXPECT_TRUE(settings->getBool("true_bool"));
    EXPECT_FALSE(settings->getBool("false_bool"));
}

TEST_F(SettingsHandlerTest, OverwriteSettings) {
    settings->setString("overwrite_test", "original");
    EXPECT_EQ(settings->getString("overwrite_test"), "original");
    
    settings->setString("overwrite_test", "modified");
    EXPECT_EQ(settings->getString("overwrite_test"), "modified");
    
    // Change type (JSON allows this)
    settings->setInt("overwrite_test", 123);
    EXPECT_EQ(settings->getInt("overwrite_test"), 123);
    
    // Original string getter should return default now due to type mismatch
    EXPECT_EQ(settings->getString("overwrite_test", "default"), "default");
}