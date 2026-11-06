#include "Transport.h"
#include <charconv>
#include <chrono>
#include <cwctype>
#include <thread>
#include <winhttp.h>

namespace daymark::update::win {
namespace {
struct Internet {
    HINTERNET value = nullptr;
    ~Internet() {
        if (value)
            WinHttpCloseHandle(value);
    }
};
std::string header(HINTERNET request, DWORD id) {
    DWORD length = 0;
    WinHttpQueryHeaders(request, id, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &length,
                        WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length > 16384)
        return {};
    std::wstring text(length / sizeof(wchar_t), 0);
    if (!WinHttpQueryHeaders(request, id, WINHTTP_HEADER_NAME_BY_INDEX, text.data(), &length,
                             WINHTTP_NO_HEADER_INDEX))
        return {};
    text.resize(length / sizeof(wchar_t));
    return utf8(text);
}
[[noreturn]] void networkFailure(const Cancellation &cancel) {
    cancel.check();
    if (GetLastError() == ERROR_WINHTTP_TIMEOUT)
        fail("update.timeout", 10);
    fail("update.network_unavailable", 10);
}
std::string namedHeader(HINTERNET request, const wchar_t *name) {
    wchar_t buffer[128]{};
    DWORD bytes = sizeof(buffer);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, buffer, &bytes,
                             WINHTTP_NO_HEADER_INDEX))
        return {};
    return utf8(std::wstring_view(buffer, bytes / sizeof(wchar_t)));
}
std::int64_t retryDeadline(HINTERNET request, const std::string &retryAfter) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    auto decimal = [](const std::string &value) -> std::int64_t {
        std::int64_t n = 0;
        auto parsed = std::from_chars(value.data(), value.data() + value.size(), n);
        return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && n >= 0 ? n
                                                                                               : 0;
    };
    auto deadline = now + 3600;
    const auto seconds = decimal(retryAfter);
    if (seconds > 0)
        deadline = std::max(deadline, now + std::min<std::int64_t>(seconds, 604800));
    SYSTEMTIME date{};
    FILETIME file{};
    if (!retryAfter.empty() && WinHttpTimeToSystemTime(wide(retryAfter).c_str(), &date) &&
        SystemTimeToFileTime(&date, &file)) {
        const auto ticks = (std::uint64_t(file.dwHighDateTime) << 32) | file.dwLowDateTime;
        if (ticks >= 116444736000000000ULL)
            deadline = std::max(
                deadline, static_cast<std::int64_t>((ticks - 116444736000000000ULL) / 10000000));
    }
    deadline = std::max(deadline, decimal(namedHeader(request, L"X-RateLimit-Reset")));
    return std::min<std::int64_t>(deadline, now + 604800);
}
} // namespace
HttpResponse get(std::string url, std::uint64_t maximum, Cancellation &cancellation,
                 const NetworkPolicy &policy, const std::map<std::string, std::string> &headers,
                 Receive receive, BeginReceive begin) {
    const auto started = std::chrono::steady_clock::now();
    for (unsigned redirect = 0; redirect <= 5; ++redirect) {
        cancellation.check();
        auto target = wide(url);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
            parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
        if (!WinHttpCrackUrl(target.c_str(), static_cast<DWORD>(target.size()), 0, &parts))
            fail("update.invalid_url");
        auto host = std::wstring(parts.lpszHostName, parts.dwHostNameLength);
        for (auto &c : host)
            c = std::towlower(c);
        const bool fixture =
            policy.loopbackFixture && host == L"127.0.0.1" && parts.nScheme == INTERNET_SCHEME_HTTP;
        if (parts.dwUserNameLength || parts.dwPasswordLength ||
            (!fixture && (parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 ||
                          (host != L"api.github.com" && host != L"github.com" &&
                           host != L"release-assets.githubusercontent.com" &&
                           host != L"objects.githubusercontent.com"))))
            fail("update.redirect_rejected");
        Internet session{WinHttpOpen(L"Daymark-Updater/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
        if (!session.value)
            networkFailure(cancellation);
        WinHttpSetTimeouts(session.value, 15000, 15000, 15000, receive ? 30000 : 15000);
        Internet connection{WinHttpConnect(session.value, host.c_str(), parts.nPort, 0)};
        if (!connection.value)
            networkFailure(cancellation);
        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength)
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Internet request{
            WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                               parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)};
        if (!request.value)
            networkFailure(cancellation);
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects,
                         sizeof(redirects));
        std::atomic_bool closed = false;
        std::jthread cancelWatch([&](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (cancellation.requested.load()) {
                    if (!closed.exchange(true))
                        WinHttpCloseHandle(request.value);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        });
        struct RequestCleanup {
            Internet &request;
            std::atomic_bool &closed;
            std::jthread &thread;
            ~RequestCleanup() {
                thread.request_stop();
                thread.join();
                if (closed.load())
                    request.value = nullptr;
            }
        } cleanup{request, closed, cancelWatch};
        for (const auto &[key, value] : headers) {
            auto text = wide(key + ": " + value + "\r\n");
            if (!WinHttpAddRequestHeaders(request.value, text.c_str(),
                                          static_cast<DWORD>(text.size()),
                                          WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
                networkFailure(cancellation);
        }
        if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.value, nullptr))
            networkFailure(cancellation);
        HttpResponse response;
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(
                request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
            networkFailure(cancellation);
        response.status = status;
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            url = header(request.value, WINHTTP_QUERY_LOCATION);
            if (url.empty() || redirect == 5)
                fail("update.redirect_rejected");
            continue;
        }
        response.etag = header(request.value, WINHTTP_QUERY_ETAG);
        response.range = header(request.value, WINHTTP_QUERY_CONTENT_RANGE);
        response.retryAfter = header(request.value, WINHTTP_QUERY_RETRY_AFTER);
        if (status == 403 || status == 429) {
            Failure error("update.rate_limited", 11);
            error.retryAtUtcSeconds = retryDeadline(request.value, response.retryAfter);
            throw error;
        }
        if (status == 304 || status == 416)
            return response;
        if (status != 200 && status != 206)
            fail(status == 404 ? "update.assets_pending" : "update.network_unavailable", 10);
        if (begin)
            begin(response);
        const auto declared = header(request.value, WINHTTP_QUERY_CONTENT_LENGTH);
        if (!declared.empty()) {
            std::uint64_t n = 0;
            auto parsed = std::from_chars(declared.data(), declared.data() + declared.size(), n);
            if (parsed.ec != std::errc() || parsed.ptr != declared.data() + declared.size() ||
                n > maximum)
                fail("update.download_size");
        }
        std::array<char, 65536> buffer;
        std::uint64_t received = 0;
        for (;;) {
            cancellation.check();
            DWORD count = 0;
            if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()),
                                 &count))
                networkFailure(cancellation);
            if (!count)
                break;
            received += count;
            if (received > maximum)
                fail("update.download_size");
            if (receive)
                receive(buffer.data(), count);
            else
                response.body.append(buffer.data(), count);
            auto elapsed = std::chrono::steady_clock::now() - started;
            if (elapsed > (receive ? std::chrono::minutes(30) : std::chrono::minutes(2)))
                fail("update.timeout", 10);
        }
        return response;
    }
    fail("update.redirect_rejected");
}
} // namespace daymark::update::win
