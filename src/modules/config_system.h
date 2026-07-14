#pragma once
#include <Windows.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "../console.h"

namespace W101Hook {

    class ConfigSystem {
    public:
        struct ConfigEntry {
            std::string key;
            std::string value;
            std::string section;
        };

    private:
        static inline std::unordered_map<std::string, std::string> values;
        static inline std::string configPath;
        static inline bool active = false;
        static inline bool dirty = false;

        static std::string MakeKey(const std::string& section, const std::string& key) {
            return section + "." + key;
        }

        static std::string Trim(const std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            return s.substr(start, end - start + 1);
        }

    public:
        static bool Init(const std::string& path = "") {
            if (path.empty()) {
                char modulePath[MAX_PATH];
                GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
                std::string dir(modulePath);
                size_t lastSlash = dir.find_last_of("\\/");
                if (lastSlash != std::string::npos) dir = dir.substr(0, lastSlash + 1);
                configPath = dir + "w101_suite.cfg";
            } else {
                configPath = path;
            }

            active = true;
            return Load();
        }

        static void Shutdown() {
            if (dirty) Save();
            active = false;
        }

        static bool IsActive() { return active; }

        // --- Load/Save (INI-like format) ---

        static bool Load() {
            std::ifstream file(configPath);
            if (!file.is_open()) return false;

            values.clear();
            std::string line, currentSection = "general";

            while (std::getline(file, line)) {
                line = Trim(line);
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;

                // Section header
                if (line[0] == '[' && line.back() == ']') {
                    currentSection = Trim(line.substr(1, line.size() - 2));
                    continue;
                }

                // Key=Value
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string key = Trim(line.substr(0, eq));
                    std::string val = Trim(line.substr(eq + 1));
                    values[MakeKey(currentSection, key)] = val;
                }
            }

            file.close();
            Console::Info("Config loaded: %s (%d entries)", configPath.c_str(),
                static_cast<int>(values.size()));
            return true;
        }

        static bool Save() {
            std::ofstream file(configPath);
            if (!file.is_open()) return false;

            file << "# W101 Intelligence Suite Configuration\n";
            file << "# Auto-saved on ejection\n\n";

            // Group by section
            std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> sections;

            for (auto& kv : values) {
                size_t dot = kv.first.find('.');
                std::string section = (dot != std::string::npos) ?
                    kv.first.substr(0, dot) : "general";
                std::string key = (dot != std::string::npos) ?
                    kv.first.substr(dot + 1) : kv.first;
                sections[section].push_back({ key, kv.second });
            }

            for (auto& sec : sections) {
                file << "[" << sec.first << "]\n";
                for (auto& kv : sec.second) {
                    file << kv.first << " = " << kv.second << "\n";
                }
                file << "\n";
            }

            file.close();
            dirty = false;
            Console::Info("Config saved: %s", configPath.c_str());
            return true;
        }

        // --- Getters ---

        static std::string GetString(const std::string& section, const std::string& key,
            const std::string& defaultVal = "") {
            auto it = values.find(MakeKey(section, key));
            if (it != values.end()) return it->second;
            return defaultVal;
        }

        static int GetInt(const std::string& section, const std::string& key, int defaultVal = 0) {
            auto it = values.find(MakeKey(section, key));
            if (it != values.end()) {
                try { return std::stoi(it->second); }
                catch (...) { return defaultVal; }
            }
            return defaultVal;
        }

        static float GetFloat(const std::string& section, const std::string& key,
            float defaultVal = 0.0f) {
            auto it = values.find(MakeKey(section, key));
            if (it != values.end()) {
                try { return std::stof(it->second); }
                catch (...) { return defaultVal; }
            }
            return defaultVal;
        }

        static bool GetBool(const std::string& section, const std::string& key,
            bool defaultVal = false) {
            auto it = values.find(MakeKey(section, key));
            if (it != values.end()) {
                std::string v = it->second;
                std::transform(v.begin(), v.end(), v.begin(), ::tolower);
                return v == "true" || v == "1" || v == "yes" || v == "on";
            }
            return defaultVal;
        }

        // --- Setters ---

        static void SetString(const std::string& section, const std::string& key,
            const std::string& value) {
            values[MakeKey(section, key)] = value;
            dirty = true;
        }

        static void SetInt(const std::string& section, const std::string& key, int value) {
            values[MakeKey(section, key)] = std::to_string(value);
            dirty = true;
        }

        static void SetFloat(const std::string& section, const std::string& key, float value) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", value);
            values[MakeKey(section, key)] = buf;
            dirty = true;
        }

        static void SetBool(const std::string& section, const std::string& key, bool value) {
            values[MakeKey(section, key)] = value ? "true" : "false";
            dirty = true;
        }

        // --- Utility ---

        static bool HasKey(const std::string& section, const std::string& key) {
            return values.count(MakeKey(section, key)) > 0;
        }

        static void RemoveKey(const std::string& section, const std::string& key) {
            values.erase(MakeKey(section, key));
            dirty = true;
        }

        static int GetEntryCount() { return static_cast<int>(values.size()); }
        static bool IsDirty() { return dirty; }
        static const std::string& GetPath() { return configPath; }

        // Bulk save/load for module state
        static void SaveModuleState(const std::string& module,
            const std::vector<std::pair<std::string, std::string>>& state) {
            for (auto& kv : state) {
                values[MakeKey(module, kv.first)] = kv.second;
            }
            dirty = true;
        }

        static std::vector<ConfigEntry> GetAllEntries() {
            std::vector<ConfigEntry> result;
            for (auto& kv : values) {
                ConfigEntry entry;
                size_t dot = kv.first.find('.');
                entry.section = (dot != std::string::npos) ? kv.first.substr(0, dot) : "general";
                entry.key = (dot != std::string::npos) ? kv.first.substr(dot + 1) : kv.first;
                entry.value = kv.second;
                result.push_back(entry);
            }
            std::sort(result.begin(), result.end(),
                [](const ConfigEntry& a, const ConfigEntry& b) {
                    return a.section < b.section || (a.section == b.section && a.key < b.key);
                });
            return result;
        }
    };

} // namespace W101Hook
