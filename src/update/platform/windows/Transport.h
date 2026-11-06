#pragma once
#include "Windows.h"
#include <map>

namespace daymark::update::win {
struct HttpResponse {
    unsigned status = 0;
    std::string body, etag, range, retryAfter;
};
struct NetworkPolicy {
    bool loopbackFixture = false;
};
using Receive = std::function<void(const char *, std::size_t)>;
using BeginReceive = std::function<void(const HttpResponse &)>;
HttpResponse get(std::string url, std::uint64_t maximum, Cancellation &, const NetworkPolicy & = {},
                 const std::map<std::string, std::string> &headers = {}, Receive receive = {},
                 BeginReceive begin = {});
} // namespace daymark::update::win
