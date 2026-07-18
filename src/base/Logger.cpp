#include "base/Logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace muduo
{
namespace
{
constexpr std::size_t kQueueSize = 8192;
constexpr std::size_t kWorkerThreads = 1;
constexpr auto kFlushInterval = std::chrono::seconds(3);

std::mutex& loggerMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string safeName(std::string name)
{
    for (char& ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) && ch != '-' && ch != '_') {
            ch = '_';
        }
    }
    return name.empty() ? "muduo" : name;
}

std::string startupTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    return stream.str();
}

std::filesystem::path resolveLogDirectory(const std::string& logDirectory)
{
    std::filesystem::path directory(logDirectory);
    if (directory.is_absolute() || logDirectory != "logs") {
        return directory;
    }

    // 从仓库根目录或常见的 build/ 目录启动时，统一写入项目根目录 logs/。
    const auto current = std::filesystem::current_path();
    if (std::filesystem::exists(current / "CMakeLists.txt")) {
        return current / directory;
    }
    if (std::filesystem::exists(current.parent_path() / "CMakeLists.txt")) {
        return current.parent_path() / directory;
    }
    return current / directory;
}
} // namespace

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::init(const std::string& projectName, const std::string& logDirectory)
{
    std::lock_guard<std::mutex> lock(loggerMutex());
    if (logger_) {
        return;
    }

    const auto directory = resolveLogDirectory(logDirectory);
    std::filesystem::create_directories(directory);
    logFilePath_ = std::filesystem::absolute(directory /
                    (safeName(projectName) + "_" + startupTimestamp() + ".log"))
                       .string();

    spdlog::init_thread_pool(kQueueSize, kWorkerThreads);
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath_, true);
    fileSink->set_level(spdlog::level::info);
    auto consoleSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    consoleSink->set_level(spdlog::level::warn);

    spdlog::sinks_init_list sinks{fileSink, consoleSink};
    logger_ = std::make_shared<spdlog::async_logger>(
        "muduo_async",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    // logger 先过滤 info 以下内容，各 sink 再执行自己的级别过滤。
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] [%s:%#] %v");
    logger_->flush_on(spdlog::level::err);
    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
    spdlog::flush_every(kFlushInterval);
    logger_->info("logging initialized: file={}, queue_size={}, flush_interval={}s",
                  logFilePath_, kQueueSize, kFlushInterval.count());
}

std::shared_ptr<spdlog::logger> Logger::get() const noexcept
{
    std::lock_guard<std::mutex> lock(loggerMutex());
    return logger_ ? logger_ : spdlog::default_logger();
}

void Logger::shutdown() noexcept
{
    std::lock_guard<std::mutex> lock(loggerMutex());
    if (!logger_) {
        return;
    }
    try {
        logger_->info("logging shutdown");
        logger_->flush();
        spdlog::drop(logger_->name());
        logger_.reset();
        spdlog::shutdown();
    } catch (...) {
        // 析构路径不能抛异常；spdlog 的 flush/shutdown 已尽最大努力落盘。
        logger_.reset();
    }
}

Logger::~Logger()
{
    shutdown();
}

} // namespace muduo
