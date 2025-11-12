#include "gm/core/Logger.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct LoggerFileGuard {
    std::filesystem::path path;
    LoggerFileGuard() = default;
    explicit LoggerFileGuard(std::filesystem::path p) : path(std::move(p)) {}
    ~LoggerFileGuard() {
        gm::core::Logger::SetLogFile({});
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
};

} // namespace

TEST_CASE("Logger writes formatted messages to file", "[logger]") {
    const std::filesystem::path tempFile =
        std::filesystem::temp_directory_path() / "gotmilked_logger_test.log";

    LoggerFileGuard guard(tempFile);

    gm::core::Logger::SetLogFile(tempFile);
    gm::core::Logger::Info("Test message {}", 42);

    std::ifstream file(tempFile);
    REQUIRE(file.is_open());

    std::string line;
    std::getline(file, line);
    file.close();

    REQUIRE(line.find("[Info] Test message 42") != std::string::npos);
}

TEST_CASE("Logger listeners receive log lines", "[logger]") {
    const std::filesystem::path tempFile =
        std::filesystem::temp_directory_path() / "gotmilked_logger_listener.log";

    LoggerFileGuard guard(tempFile);

    gm::core::Logger::SetLogFile(tempFile);

    std::vector<std::string> captured;
    const auto token = gm::core::Logger::RegisterListener(
        [&captured](gm::core::LogLevel level, const std::string& line) {
            if (level == gm::core::LogLevel::Warning) {
                captured.push_back(line);
            }
        });

    gm::core::Logger::Warning("Captured warning {}", 7);

    gm::core::Logger::UnregisterListener(token);

    REQUIRE(captured.size() == 1);
    REQUIRE(captured.front().find("[Warning] Captured warning 7") != std::string::npos);
}
