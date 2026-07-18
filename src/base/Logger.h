#pragma once

#include "base/NonCopyable.h"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace muduo
{

// 进程级异步日志系统。应在业务线程启动前 init，在业务线程退出后 shutdown。
class Logger final : NonCopyable
{
public:
    static Logger& instance();

    void init(const std::string& projectName = "muduo",
              const std::string& logDirectory = "logs");
    void shutdown() noexcept;

    std::shared_ptr<spdlog::logger> get() const noexcept;
    const std::string& logFilePath() const noexcept { return logFilePath_; }

    ~Logger();

private:
    Logger() = default;

    std::shared_ptr<spdlog::logger> logger_;
    std::string logFilePath_;
};

// main() 中最先创建该对象，可保证其在其他局部对象和工作线程之后析构。
class LogGuard final : NonCopyable
{
public:
    explicit LogGuard(const std::string& projectName = "muduo",
                      const std::string& logDirectory = "logs")
    {
        Logger::instance().init(projectName, logDirectory);
    }

    ~LogGuard() { Logger::instance().shutdown(); }
};

} // namespace muduo

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(::muduo::Logger::instance().get(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(::muduo::Logger::instance().get(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(::muduo::Logger::instance().get(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(::muduo::Logger::instance().get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::muduo::Logger::instance().get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::muduo::Logger::instance().get(), __VA_ARGS__)
