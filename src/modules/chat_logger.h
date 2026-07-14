#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <ctime>
#include "../overlay.h"

namespace W101Hook {

    class ChatLogger {
    public:
        struct ChatMessage {
            DWORD       timestamp;
            std::string channel;
            std::string content;
            std::string raw;
        };

    private:
        static inline std::vector<ChatMessage> messages;
        static inline std::mutex               chatMtx;
        static inline bool                     active = false;
        static inline bool                     logging = true;
        static inline bool                     fileLogging = false;
        static inline std::ofstream            logFile;
        static inline std::string              logFilePath;
        static inline uint32_t                 totalMessages = 0;
        static inline size_t                   maxMessages = 5000;

        // Channel stats
        static inline uint32_t chatCount = 0;
        static inline uint32_t whisperCount = 0;
        static inline uint32_t systemCount = 0;
        static inline uint32_t otherCount = 0;

        static std::string DetectChannel(const std::string& cmd, const std::string& args) {
            if (cmd.find("sendChat") != std::string::npos) return "CHAT";
            if (cmd.find("chatInput") != std::string::npos) return "INPUT";
            if (cmd.find("cycleChat") != std::string::npos) return "CYCLE";
            if (cmd.find("whisper") != std::string::npos) return "WHISPER";
            if (cmd.find("Whisper") != std::string::npos) return "WHISPER";
            if (cmd.find("friend") != std::string::npos) return "SOCIAL";
            if (cmd.find("trade") != std::string::npos) return "TRADE";
            if (cmd.find("team") != std::string::npos) return "TEAM";
            if (cmd.find("guild") != std::string::npos) return "GUILD";

            // Check args for channel indicators
            if (args.find("say") != std::string::npos) return "SAY";
            if (args.find("yell") != std::string::npos) return "YELL";
            if (args.find("emote") != std::string::npos) return "EMOTE";
            if (args.find("open") != std::string::npos) return "OPEN";
            if (args.find("menu") != std::string::npos) return "MENU";

            return "OTHER";
        }

        static void WriteToFile(const ChatMessage& msg) {
            if (!fileLogging || !logFile.is_open()) return;

            time_t rawTime = static_cast<time_t>(msg.timestamp / 1000);
            struct tm timeInfo;
            localtime_s(&timeInfo, &rawTime);
            char timeBuf[32];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeInfo);

            logFile << "[" << timeBuf << "] [" << msg.channel << "] "
                    << msg.content << " | RAW: " << msg.raw << "\n";
            logFile.flush();
        }

    public:
        static bool Init() {
            active = true;
            return true;
        }

        static void Shutdown() {
            active = false;
            if (logFile.is_open()) logFile.close();
        }

        static bool IsActive() { return active; }
        static bool IsLogging() { return logging; }
        static bool IsFileLogging() { return fileLogging; }

        static bool StartFileLogging(const std::string& path = "") {
            if (logFile.is_open()) logFile.close();

            if (path.empty()) {
                // Default: next to the DLL
                char modulePath[MAX_PATH];
                GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
                std::string dir(modulePath);
                size_t lastSlash = dir.find_last_of("\\/");
                if (lastSlash != std::string::npos) dir = dir.substr(0, lastSlash + 1);

                time_t now = time(nullptr);
                struct tm timeInfo;
                localtime_s(&timeInfo, &now);
                char dateBuf[32];
                strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &timeInfo);

                logFilePath = dir + "w101_chat_" + dateBuf + ".log";
            } else {
                logFilePath = path;
            }

            logFile.open(logFilePath, std::ios::out | std::ios::app);
            if (logFile.is_open()) {
                logFile << "=== W101 Chat Log Started ===\n";
                logFile.flush();
                fileLogging = true;
                return true;
            }
            return false;
        }

        static void StopFileLogging() {
            if (logFile.is_open()) {
                logFile << "=== W101 Chat Log Ended ===\n";
                logFile.close();
            }
            fileLogging = false;
        }

        // Called from FSCommand processor — filters chat-related commands
        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!active || !logging) return;

            // Filter for chat-related FSCommands
            bool isChatRelated =
                cmd.find("chat") != std::string::npos ||
                cmd.find("Chat") != std::string::npos ||
                cmd.find("whisper") != std::string::npos ||
                cmd.find("Whisper") != std::string::npos ||
                cmd.find("sendChat") != std::string::npos ||
                cmd.find("cycleChat") != std::string::npos ||
                cmd.find("chatInput") != std::string::npos ||
                cmd.find("say") != std::string::npos ||
                cmd.find("emote") != std::string::npos ||
                cmd.find("friend") != std::string::npos ||
                cmd.find("trade") != std::string::npos ||
                cmd.find("team") != std::string::npos ||
                cmd.find("guild") != std::string::npos;

            if (!isChatRelated) return;

            ChatMessage msg;
            msg.timestamp = GetTickCount();
            msg.channel = DetectChannel(cmd, args);
            msg.content = args;
            msg.raw = cmd + "(" + args + ")";

            // Update stats
            if (msg.channel == "CHAT" || msg.channel == "SAY") chatCount++;
            else if (msg.channel == "WHISPER") whisperCount++;
            else if (msg.channel == "INPUT" || msg.channel == "CYCLE") {} // not real messages
            else otherCount++;

            totalMessages++;

            WriteToFile(msg);

            std::lock_guard<std::mutex> lock(chatMtx);
            messages.push_back(msg);
            if (messages.size() > maxMessages) {
                messages.erase(messages.begin(), messages.begin() + (maxMessages / 2));
            }
        }

        // Also process game log messages that might contain chat
        static void ProcessLogMessage(const std::string& msg, bool isError) {
            if (!active || !logging) return;

            // Look for chat-related log messages
            if (msg.find("chat") == std::string::npos &&
                msg.find("Chat") == std::string::npos &&
                msg.find("message") == std::string::npos) return;

            ChatMessage chatMsg;
            chatMsg.timestamp = GetTickCount();
            chatMsg.channel = "LOG";
            chatMsg.content = msg;
            chatMsg.raw = msg;
            systemCount++;
            totalMessages++;

            WriteToFile(chatMsg);

            std::lock_guard<std::mutex> lock(chatMtx);
            messages.push_back(chatMsg);
            if (messages.size() > maxMessages) {
                messages.erase(messages.begin(), messages.begin() + (maxMessages / 2));
            }
        }

        // --- Controls ---
        static void ToggleLogging() { logging = !logging; }

        static void ClearMessages() {
            std::lock_guard<std::mutex> lock(chatMtx);
            messages.clear();
        }

        // --- Data ---
        static std::vector<ChatMessage> GetRecentMessages(int count = 10) {
            std::lock_guard<std::mutex> lock(chatMtx);
            int total = static_cast<int>(messages.size());
            int start = (std::max)(0, total - count);
            return std::vector<ChatMessage>(messages.begin() + start, messages.end());
        }

        static uint32_t GetTotalMessages() { return totalMessages; }
        static uint32_t GetChatCount() { return chatCount; }
        static uint32_t GetWhisperCount() { return whisperCount; }
        static uint32_t GetSystemCount() { return systemCount; }
        static const std::string& GetLogFilePath() { return logFilePath; }

        static D3DCOLOR GetChannelColor(const std::string& channel) {
            if (channel == "CHAT" || channel == "SAY") return D3DCOLOR_ARGB(255, 255, 255, 255);
            if (channel == "WHISPER") return D3DCOLOR_ARGB(255, 200, 100, 255);
            if (channel == "TEAM") return D3DCOLOR_ARGB(255, 100, 200, 255);
            if (channel == "GUILD") return D3DCOLOR_ARGB(255, 100, 255, 100);
            if (channel == "TRADE") return D3DCOLOR_ARGB(255, 255, 200, 60);
            if (channel == "EMOTE") return D3DCOLOR_ARGB(255, 255, 165, 0);
            if (channel == "SOCIAL") return D3DCOLOR_ARGB(255, 255, 150, 200);
            if (channel == "LOG") return D3DCOLOR_ARGB(255, 160, 160, 160);
            if (channel == "INPUT") return D3DCOLOR_ARGB(255, 100, 100, 100);
            return D3DCOLOR_ARGB(255, 180, 180, 180);
        }

        // --- Overlay ---
        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            auto recent = GetRecentMessages(8);
            int msgCount = static_cast<int>(recent.size());
            int h = 52 + msgCount * 13;

            Overlay::DrawFilledRect(dev, x, y, 420, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 420, h, D3DCOLOR_ARGB(255, 200, 160, 255), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 200, 160, 255),
                "CHAT LOGGER %s%s",
                logging ? "[LIVE]" : "[PAUSED]",
                fileLogging ? " [FILE]" : ""); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Total: %u  Chat: %u  Whisper: %u  System: %u",
                totalMessages, chatCount, whisperCount, systemCount); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                "--- Recent Messages ---"); ty += 14;

            for (auto& msg : recent) {
                D3DCOLOR c = GetChannelColor(msg.channel);
                std::string line = msg.content;
                if (line.length() > 45) line = line.substr(0, 45) + "...";
                Overlay::DrawText(x + 5, ty, c,
                    "[%s] %s", msg.channel.c_str(), line.c_str());
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook
