// sharertc_url_parser.h
// https:// URL 解析器（C++17，无异常）：类声明
//
// 实现见 sharertc_url_parser.cpp，链接时需与 .cpp 一起编译：
//   g++ -std=c++17 sharertc_url_parser.cpp your_main.cpp -o app
//
// 支持格式:
//   https://signalserver:port/peerid
//   https://signalserver:port/peerid?key1=value1&key2=value2
//   例如: https://signal.example.com:8443/peer_001?room=1&mode=sfu
//
// 用法:
//   const auto r = ShareRTCUrl::parse("https://signal.example.com:8443/peer_001?room=1&mode=sfu");
//   if (!r.ok) { /* r.error 为失败原因；不抛异常 */ }
//   r.value.scheme()  // "https"
//   r.value.host()    // "signal.example.com"
//   r.value.port()    // 8443
//   r.value.peerid()  // "peer_001"
//   r.value.query()   // std::map<std::string,std::string>{ {"room","1"}, {"mode","sfu"} }
//   r.value.to_url()  // "https://signal.example.com:8443/peer_001?mode=sfu&room=1"
//
// 本实现不使用异常：解析失败通过 ShareRTCUrlParseResult.ok / .error 返回。

#pragma once

#include <map>
#include <string>

// parse() 的返回值类型，完整定义见类定义之后
struct ShareRTCUrlParseResult;

class ShareRTCUrl {
public:
    // 端口缺省时的默认值（https 惯例 443），可在 parse 时按需覆盖
    static constexpr int kDefaultPort = 443;

    // 允许的 scheme（小写比较）
    static constexpr const char* kScheme = "https";

    // 默认构造，字段为空（供 ShareRTCUrlParseResult 使用）
    ShareRTCUrl() = default;

    // 直接构造（仅存储，不做校验；请优先使用 parse() 解析）
    ShareRTCUrl(std::string scheme, std::string host, int port, std::string peerid,
                std::map<std::string, std::string> query = {});

    // 解析 https://signalserver:port/peerid[?key=value&...]。
    // - port 缺省时使用 default_port；
    // - peerid 取 path 中去除首尾斜杠后的部分；
    // - query 按 '&' 拆分、每个键值对按首个 '=' 拆分存入 std::map；
    //   无 '=' 的 key 值为空串；重复 key 后者覆盖；空 key/空段跳过；
    // - 支持 IPv6 写法，如 https://[::1]:8080/peer。
    // 失败不抛异常：返回结果的 ok=false，error 含失败原因。
    static ShareRTCUrlParseResult parse(const std::string& url, int default_port = kDefaultPort);

    const std::string& scheme() const { return scheme_; }
    const std::string& host() const { return host_; }
    int port() const { return port_; }
    const std::string& peerid() const { return peerid_; }
    const std::map<std::string, std::string>& query() const { return query_; }

    // 序列化回标准 https:// 格式（query 按 key 字典序输出）
    std::string to_url() const;

private:
    static bool parse_port(const std::string& s, int& port, std::string& error);
    static std::map<std::string, std::string> parse_query(const std::string& qs);
    static std::string to_lower(std::string s);
    static std::string trim(const std::string& s);

    std::string scheme_;
    std::string host_;
    int port_ = 0;
    std::string peerid_;
    std::map<std::string, std::string> query_;
};

// parse() 的返回值：ok=true 时 value 有效；ok=false 时 error 说明原因
struct ShareRTCUrlParseResult {
    bool ok = false;
    std::string error;
    ShareRTCUrl value;
};
