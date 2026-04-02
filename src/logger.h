#pragma once

#include <filesystem>
#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// Creates and registers an "app" logger backed by two sinks:
//
//   1. Coloured stdout       – instant feedback during development.
//   2. Rotating file sink    – persists log history across runs.
//                              Files rotate once max_size bytes is reached.
//                              At most max_files files are kept; the oldest is
//                              deleted automatically when the limit is exceeded.
//
// Log-entry format (both sinks):
//   [2026-04-02 09:00:00.123] [INFO ]  your message here
//
// The level tokens (DEBUG / INFO / WARN / ERROR / CRITICAL) are colour-coded
// on the stdout sink when the terminal supports ANSI colours.
//
// After calling this function the logger is also available globally via
// spdlog::get("app") from any translation unit that includes <spdlog/spdlog.h>.
//
// Usage:
//   auto log = setup_logger(fs::temp_directory_path() / "my_app");
//   log->info("server listening on port {}", 8080);
//   log->warn("retry {}/{}", attempt, max_retries);
//   log->error("connection refused: {}", endpoint);
inline std::shared_ptr<spdlog::logger> setup_logger(
    const std::filesystem::path& log_dir,
    std::size_t max_size  = 5 * 1024 * 1024,  // rotate at 5 MB
    std::size_t max_files = 3)                 // keep 3 rotated files
{
    std::filesystem::create_directories(log_dir);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink    = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (log_dir / "app.log").string(), max_size, max_files);

    auto logger = std::make_shared<spdlog::logger>(
        "app",
        spdlog::sinks_init_list{console_sink, file_sink});

    // [date time] [LEVEL]  message
    // %-5l left-pads the level token to 5 chars so columns line up.
    // %^ / %$ wrap the token in the sink's colour escape sequences.
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-5l%$]  %v");
    logger->set_level(spdlog::level::debug);

    spdlog::register_logger(logger);  // enables spdlog::get("app") globally
    return logger;
}
