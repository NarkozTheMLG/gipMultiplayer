#include "MultiplayerLog.h"

#include "znet/logger.h"

#include <atomic>
#include <iostream>
#include <mutex>

namespace gipmp {
namespace {

std::atomic<bool> g_verbose{false};

/*
 * Records can arrive from any znet thread, so the two halves of a line must
 * not interleave with another thread's.
 */
std::mutex& outputMutex() {
    static std::mutex mutex;
    return mutex;
}

const char* levelPrefix(znet::LogLevel level) {
    switch (level) {
        case znet::LogLevel::Debug: return "[debug]";
        case znet::LogLevel::Info:  return "[info ]";
        case znet::LogLevel::Warn:  return "[warn ]";
        case znet::LogLevel::Error: return "[error]";
    }
    return "[?????]";
}

/*
 * Debug and Info are chatter and answer to the flag. Warn and Error always
 * print: a dropped warning is a networking fault nobody sees.
 */
void writeZnetRecord(znet::LogLevel level, const char* function,
                     const char* message, void* user) {
    (void) user;

    bool chatter = level == znet::LogLevel::Debug || level == znet::LogLevel::Info;
    if (chatter && !g_verbose.load(std::memory_order_relaxed)) {
        return;
    }

    std::lock_guard<std::mutex> lock(outputMutex());
    std::cout << levelPrefix(level) << " " << (function != nullptr ? function : "")
              << ": " << (message != nullptr ? message : "") << std::endl;
}

/*
 * Static storage, because znet keeps the pointer and reads it on every log
 * call for the rest of the process.
 */
const znet::LogSink& znetSink() {
    static znet::LogSink sink{&writeZnetRecord, nullptr};
    return sink;
}

/*
 * znet logs from its own constructors, so the sink has to be in place before
 * the first network object exists rather than at the first setVerboseLogging()
 * call. Installing it from a namespace-scope initializer covers that; the sink
 * only ever consults g_verbose, so it costs nothing while quiet.
 */
struct SinkInstaller {
    SinkInstaller() { znet::SetLogSink(&znetSink()); }
};

SinkInstaller g_sinkInstaller;

}  // namespace

void mpLogInfo(const std::string& message) {
    if (!g_verbose.load(std::memory_order_relaxed)) {
        return;
    }
    std::lock_guard<std::mutex> lock(outputMutex());
    std::cout << message << std::endl;
}

void mpLogError(const std::string& message) {
    std::lock_guard<std::mutex> lock(outputMutex());
    std::cout << message << std::endl;
}

void setVerboseLogging(bool verbose) {
    g_verbose.store(verbose, std::memory_order_relaxed);
}

bool isVerboseLogging() {
    return g_verbose.load(std::memory_order_relaxed);
}

}  // namespace gipmp
