// Ferriot - HTTP Downloader for Firmware Updates
// Provides async HTTP/HTTPS file downloads with progress tracking

#pragma once

#include "lwm2m/result.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace lwm2m::transport {

/// Progress information for ongoing downloads
struct DownloadProgress {
    uint64_t bytes_downloaded{0};
    uint64_t total_bytes{0};
    double speed_mbps{0.0};
    std::chrono::steady_clock::time_point start_time;

    /// Get download completion percentage (0-100)
    [[nodiscard]] uint8_t percent() const noexcept {
        if (total_bytes == 0) return 0;
        return static_cast<uint8_t>(bytes_downloaded * 100 / total_bytes);
    }

    /// Estimate time remaining based on current speed
    [[nodiscard]] std::chrono::seconds eta() const noexcept {
        if (speed_mbps <= 0 || bytes_downloaded >= total_bytes) {
            return std::chrono::seconds{0};
        }
        auto remaining = static_cast<double>(total_bytes - bytes_downloaded);
        auto bytes_per_sec = speed_mbps * 1024.0 * 1024.0;
        return std::chrono::seconds{static_cast<int64_t>(remaining / bytes_per_sec)};
    }

    /// Get elapsed time since download started
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        );
    }
};

/// Callback for progress updates during download
using ProgressCallback = std::function<void(const DownloadProgress&)>;

/// Callback when download completes (success/failure)
using CompletionCallback = std::function<void(bool success, const std::string& error)>;

/// Asynchronous HTTP/HTTPS file downloader using libcurl
///
/// Supports:
/// - HTTP and HTTPS protocols
/// - Streaming to disk (low memory usage)
/// - Progress callbacks with speed metrics
/// - Download cancellation
/// - SSL certificate verification
class HttpDownloader {
public:
    HttpDownloader();
    ~HttpDownloader();

    // Non-copyable, non-movable (owns thread)
    HttpDownloader(const HttpDownloader&) = delete;
    HttpDownloader& operator=(const HttpDownloader&) = delete;
    HttpDownloader(HttpDownloader&&) = delete;
    HttpDownloader& operator=(HttpDownloader&&) = delete;

    /// Start asynchronous download to file
    /// @param url HTTP or HTTPS URL to download from
    /// @param dest_path Local file path to save to
    /// @param on_progress Called periodically with progress updates (may be nullptr)
    /// @param on_complete Called when download finishes (may be nullptr)
    /// @return Ok() if download started, Err if already downloading or invalid params
    Result<void> start_download(
        const std::string& url,
        const std::string& dest_path,
        ProgressCallback on_progress,
        CompletionCallback on_complete
    );

    /// Cancel ongoing download
    /// Safe to call even if no download is in progress
    void cancel();

    /// Get current download progress
    /// Thread-safe, can be called from any thread
    [[nodiscard]] DownloadProgress progress() const;

    /// Check if download is currently in progress
    [[nodiscard]] bool is_downloading() const noexcept;

    /// Wait for current download to complete
    /// @param timeout Maximum time to wait
    /// @return true if download completed, false if timed out
    bool wait_for_completion(std::chrono::milliseconds timeout);

private:
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> downloading_{false};
    std::thread download_thread_;
    DownloadProgress current_progress_;
    mutable std::mutex progress_mutex_;

    void cleanup_thread();

    // libcurl callbacks (static to be compatible with C API)
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progress_callback(void* clientp, long long dltotal, long long dlnow,
                                 long long ultotal, long long ulnow);
};

} // namespace lwm2m::transport
