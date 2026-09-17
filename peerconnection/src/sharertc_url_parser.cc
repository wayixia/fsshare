// sharertc_url_parser.cpp
// ShareRTCUrl 实现：解析 https://signalserver:port/peerid[?key=value&...]
//
// 声明见 sharertc_url_parser.h，编译：
//   g++ -std=c++17 -c sharertc_url_parser.cpp

#include "sharertc_url_parser.h"

#include <cctype>
#include <utility>

ShareRTCUrl::ShareRTCUrl(std::string scheme, std::string host, int port, std::string peerid,
                         std::map<std::string, std::string> query)
    : scheme_(to_lower(std::move(scheme))),
      host_(to_lower(std::move(host))),  // 域名大小写不敏感，统一小写
      port_(port),
      peerid_(std::move(peerid)),
      query_(std::move(query)) {}

ShareRTCUrlParseResult ShareRTCUrl::parse(const std::string& url, int default_port) {
    ShareRTCUrlParseResult r;
    std::string s = trim(url);
    if (s.empty()) {
        r.error = "URL must not be empty";
        return r;
    }
    if (default_port < 1 || default_port > 65535) {
        r.error = "invalid default port: " + std::to_string(default_port) + " (valid range 1-65535)";
        return r;
    }

    const auto sep = s.find("://");
    if (sep == std::string::npos) {
        r.error = "missing '://' separator";
        return r;
    }
    std::string scheme = to_lower(s.substr(0, sep));
    if (scheme != kScheme) {
        r.error = "unsupported scheme: '" + scheme + "', only " + kScheme + " is supported";
        return r;
    }

    const std::string rest = s.substr(sep + 3);
    const auto slash = rest.find('/');
    const std::string authority =
        (slash == std::string::npos) ? rest : rest.substr(0, slash);
    const std::string path =
        (slash == std::string::npos) ? "" : rest.substr(slash + 1);

    // authority = host[:port]，host 支持 IPv6 中括号写法
    std::string host;
    std::string port_str;
    bool has_port = false;
    if (!authority.empty() && authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos) {
            r.error = "IPv6 address missing ']'";
            return r;
        }
        host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') {
                r.error = "invalid authority: '" + authority + "'";
                return r;
            }
            port_str = authority.substr(close + 2);
            has_port = true;
        }
    } else {
        const auto colon = authority.rfind(':');
        if (colon != std::string::npos) {
            host = authority.substr(0, colon);
            port_str = authority.substr(colon + 1);
            has_port = true;
        } else {
            host = authority;
        }
    }

    if (host.empty()) {
        r.error = "host must not be empty";
        return r;
    }

    int port = default_port;
    if (has_port && !parse_port(port_str, port, r.error)) {
        return r;
    }

    // path 中拆分 query: path?query
    const auto qmark = path.find('?');
    const std::string path_only =
        (qmark == std::string::npos) ? path : path.substr(0, qmark);
    const std::string query_str =
        (qmark == std::string::npos) ? "" : path.substr(qmark + 1);

    // peerid: 去除 path_only 首尾斜杠
    const auto b = path_only.find_first_not_of('/');
    const std::string peerid =
        (b == std::string::npos)
            ? ""
            : path_only.substr(b, path_only.find_last_not_of('/') - b + 1);
    if (peerid.empty()) {
        r.error = "missing peerid (expected format: https://signalserver:port/peerid)";
        return r;
    }

    r.ok = true;
    r.value = ShareRTCUrl(std::move(scheme), std::move(host), port, std::move(peerid),
                          parse_query(query_str));
    return r;
}

std::string ShareRTCUrl::to_url() const {
    std::string url = scheme_ + "://" + host_ + ":" + std::to_string(port_) + "/" + peerid_;
    if (!query_.empty()) {
        url += "?";
        bool first = true;
        for (const auto& kv : query_) {
            if (!first) {
                url += "&";
            }
            first = false;
            url += kv.first;
            url += "=";
            url += kv.second;
        }
    }
    return url;
}

bool ShareRTCUrl::parse_port(const std::string& s, int& port, std::string& error) {
    if (s.empty()) {
        error = "invalid port: missing port number";
        return false;
    }
    int value = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') {
            error = "invalid port: '" + s + "'";
            return false;
        }
        const int digit = c - '0';
        if (value > (65535 - digit) / 10) {  // 防溢出，同时保证 <= 65535
            error = "port out of range: '" + s + "' (valid range 1-65535)";
            return false;
        }
        value = value * 10 + digit;
    }
    if (value == 0) {
        error = "invalid port: '" + s + "' (valid range 1-65535)";
        return false;
    }
    port = value;
    return true;
}

std::map<std::string, std::string> ShareRTCUrl::parse_query(const std::string& qs) {
    std::map<std::string, std::string> result;
    if (qs.empty()) {
        return result;
    }
    std::size_t start = 0;
    while (start <= qs.size()) {
        const auto amp = qs.find('&', start);
        const std::string seg =
            (amp == std::string::npos) ? qs.substr(start) : qs.substr(start, amp - start);
        if (!seg.empty()) {  // 跳过空段（如 "a=1&&b=2" 中间的空段）
            const auto eq = seg.find('=');
            const std::string key = (eq == std::string::npos) ? seg : seg.substr(0, eq);
            const std::string value =
                (eq == std::string::npos) ? "" : seg.substr(eq + 1);
            if (!key.empty()) {  // 跳过空 key；重复 key 后者覆盖前者
                result[key] = value;
            }
        }
        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }
    return result;
}

std::string ShareRTCUrl::to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string ShareRTCUrl::trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return "";
    }
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
