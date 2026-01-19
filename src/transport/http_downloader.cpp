// Ferriot - HTTP Downloader Implementation

#include "lwm2m/transport/http_downloader.hpp"

#include <curl/curl.h>
#include <cstdio>
#include <fstream>
#include <iostream>

namespace lwm2m::transport {

namespace {

// Context passed to curl callbacks
struct DownloadContext {
    std::ofstream file;
    HttpDownloader* downloader{nullptr};
    ProgressCallback on_progress;
    std::chrono::steady_clock::time_point last_progress_time;
    uint64_t last_bytes{0};
    double last_speed{0.0};
};

} // anonymous namespace

HttpDownloader::HttpDownloader() {
    // Initialize curl globally (thread-safe in modern curl)
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpDownloader::~HttpDownloader() {
    cancel();
    cleanup_thread();
    curl_global_cleanup();
}

void HttpDownloader::cleanup_thread() {
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
}

size_t HttpDownloader::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<DownloadContext*>(userp);
    size_t total = size * nmemb;

    ctx->file.write(static_cast<const char*>(contents), static_cast<std::streamsize>(total));

    if (!ctx->file.good()) {
        return 0;  // Signal error to curl
    }

    return total;
}

int HttpDownloader::progress_callback(void* clientp, long long dltotal, long long dlnow,
                                       long long /*ultotal*/, long long /*ulnow*/) {
    auto* ctx = static_cast<DownloadContext*>(clientp);

    // Check for cancellation
    if (ctx->downloader->cancelled_) {
        return 1;  // Non-zero aborts the transfer
    }

    // Calculate speed (update every 250ms for smoother display)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - ctx->last_progress_time
    );

    DownloadProgress progress;
    progress.bytes_downloaded = static_cast<uint64_t>(dlnow);
    progress.total_bytes = static_cast<uint64_t>(dltotal);
    progress.start_time = ctx->downloader->current_progress_.start_time;

    // Calculate speed if enough time has passed
    if (elapsed.count() >= 250 && dlnow > static_cast<long long>(ctx->last_bytes)) {
        auto bytes_delta = static_cast<double>(dlnow) - static_cast<double>(ctx->last_bytes);
        double seconds = static_cast<double>(elapsed.count()) / 1000.0;
        double bytes_per_sec = bytes_delta / seconds;
        progress.speed_mbps = bytes_per_sec / (1024.0 * 1024.0);

        // Smooth speed calculation (exponential moving average)
        if (ctx->last_speed > 0) {
            progress.speed_mbps = 0.7 * progress.speed_mbps + 0.3 * ctx->last_speed;
        }

        ctx->last_speed = progress.speed_mbps;
        ctx->last_bytes = static_cast<uint64_t>(dlnow);
        ctx->last_progress_time = now;
    } else {
        // Use last known speed
        progress.speed_mbps = ctx->last_speed;
    }

    // Update stored progress (thread-safe)
    {
        std::lock_guard<std::mutex> lock(ctx->downloader->progress_mutex_);
        ctx->downloader->current_progress_ = progress;
    }

    // Call user callback if provided
    if (ctx->on_progress && dltotal > 0) {
        ctx->on_progress(progress);
    }

    return 0;  // Continue download
}

Result<void> HttpDownloader::start_download(
    const std::string& url,
    const std::string& dest_path,
    ProgressCallback on_progress,
    CompletionCallback on_complete)
{
    // Check if already downloading
    if (downloading_.exchange(true)) {
        return Err<void>(ErrorCode::InvalidState, "Download already in progress");
    }

    // Validate URL
    if (url.empty()) {
        downloading_ = false;
        return Err<void>(ErrorCode::BadRequest, "URL cannot be empty");
    }

    // Clean up previous thread if any
    cleanup_thread();

    // Reset state
    cancelled_ = false;
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        current_progress_ = DownloadProgress{};
        current_progress_.start_time = std::chrono::steady_clock::now();
    }

    // Start download in background thread
    download_thread_ = std::thread([this, url, dest_path, on_progress, on_complete]() {
        DownloadContext ctx;
        ctx.downloader = this;
        ctx.on_progress = on_progress;
        ctx.last_progress_time = std::chrono::steady_clock::now();
        ctx.last_bytes = 0;
        ctx.last_speed = 0.0;

        // Open output file
        ctx.file.open(dest_path, std::ios::binary | std::ios::trunc);
        if (!ctx.file.is_open()) {
            downloading_ = false;
            if (on_complete) {
                on_complete(false, "Failed to open output file: " + dest_path);
            }
            return;
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            ctx.file.close();
            std::remove(dest_path.c_str());
            downloading_ = false;
            if (on_complete) {
                on_complete(false, "Failed to initialize curl");
            }
            return;
        }

        // Configure curl options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        // Follow redirects
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);

        // SSL/TLS settings
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Timeouts
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);  // 1 hour max for large files
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);  // 1KB/s minimum
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);     // For 60 seconds

        // User agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ferriot-client/1.0");

        CURLcode res = curl_easy_perform(curl);

        // Get final info
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        // Cleanup
        ctx.file.close();
        curl_easy_cleanup(curl);
        downloading_ = false;

        // Handle result
        if (cancelled_) {
            std::remove(dest_path.c_str());
            if (on_complete) {
                on_complete(false, "Download cancelled");
            }
        } else if (res != CURLE_OK) {
            std::remove(dest_path.c_str());
            if (on_complete) {
                std::string error = curl_easy_strerror(res);
                on_complete(false, "curl error: " + error);
            }
        } else if (http_code >= 400) {
            std::remove(dest_path.c_str());
            if (on_complete) {
                on_complete(false, "HTTP error: " + std::to_string(http_code));
            }
        } else {
            if (on_complete) {
                on_complete(true, "");
            }
        }
    });

    return Ok();
}

void HttpDownloader::cancel() {
    cancelled_ = true;
}

DownloadProgress HttpDownloader::progress() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return current_progress_;
}

bool HttpDownloader::is_downloading() const noexcept {
    return downloading_;
}

bool HttpDownloader::wait_for_completion(std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();

    while (downloading_) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    return true;
}

} // namespace lwm2m::transport
