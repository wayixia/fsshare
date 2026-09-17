// sharertc_url_parser_test.cpp
// ShareRTCUrl 解析器验证用例：编译运行
//   g++ -std=c++17 -Wall -Wextra -o sharertc_url_parser_test sharertc_url_parser.cpp sharertc_url_parser_test.cpp && ./sharertc_url_parser_test

#include "sharertc_url_parser.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

static int g_fail = 0;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
    if (!ok) ++g_fail;
}

// 从 map 取 query 值；缺失返回 "<missing>"（不依赖会抛异常的 at()）
static std::string qget(const std::map<std::string, std::string>& q, const std::string& k) {
    const auto it = q.find(k);
    return it == q.end() ? std::string("<missing>") : it->second;
}

int main() {
    // —— 正常解析 ——
    {
        const auto r = ShareRTCUrl::parse("https://signal.example.com:8443/peer_001");
        check(r.ok, "parse 成功");
        const ShareRTCUrl& u = r.value;
        check(u.scheme() == "https", "scheme");
        check(u.host() == "signal.example.com", "host");
        check(u.port() == 8443, "port");
        check(u.peerid() == "peer_001", "peerid");
        check(u.to_url() == "https://signal.example.com:8443/peer_001", "to_url");
    }
    {
        const auto r = ShareRTCUrl::parse("https://192.168.1.10:8080/abc123");
        check(r.ok && r.value.host() == "192.168.1.10" && r.value.peerid() == "abc123",
              "IPv4 地址");
    }
    {
        const auto r = ShareRTCUrl::parse("https://localhost:443/peer-xyz");
        check(r.ok && r.value.host() == "localhost" && r.value.port() == 443 &&
                  r.value.peerid() == "peer-xyz",
              "localhost");
    }
    {
        // scheme/host 大小写不敏感，scheme 归一为小写
        const auto r = ShareRTCUrl::parse("HTTPS://SIGNAL.EXAMPLE.COM:9000/Peer_1");
        check(r.ok && r.value.scheme() == "https" && r.value.host() == "signal.example.com" &&
                  r.value.port() == 9000 && r.value.peerid() == "Peer_1",
              "scheme/host 大小写");
    }
    {
        // 端口缺省 -> 默认 443
        const auto r = ShareRTCUrl::parse("https://localhost/peer_no_port");
        check(r.ok && r.value.port() == 443, "端口缺省 -> 443");
        check(r.value.to_url() == "https://localhost:443/peer_no_port", "缺省端口 to_url");
    }
    {
        // 自定义默认端口
        const auto r = ShareRTCUrl::parse("https://localhost/peer", 9000);
        check(r.ok && r.value.port() == 9000, "自定义 default_port");
    }
    {
        // IPv6
        const auto r = ShareRTCUrl::parse("https://[::1]:8080/peer6");
        check(r.ok && r.value.host() == "::1" && r.value.port() == 8080, "IPv6");
    }
    {
        // 首尾空白
        const auto r = ShareRTCUrl::parse("  https://host:1234/peer  ");
        check(r.ok && r.value.host() == "host" && r.value.port() == 1234 &&
                  r.value.peerid() == "peer",
              "首尾空白");
    }
    {
        // path 内的空格按原样保留
        const auto r = ShareRTCUrl::parse("https://host:8080/peer extra");
        check(r.ok && r.value.peerid() == "peer extra", "path 内空格保留");
    }

    // —— query 字段 ——
    {
        const auto r = ShareRTCUrl::parse("https://host:8443/peer?room=42&mode=sfu");
        check(r.ok && r.value.peerid() == "peer", "query 不影响 peerid");
        check(r.value.query().size() == 2 && qget(r.value.query(), "room") == "42" &&
                  qget(r.value.query(), "mode") == "sfu",
              "query 解析为 map");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer?flag");
        check(r.ok && r.value.query().size() == 1 && qget(r.value.query(), "flag") == "",
              "无 '=' 的 key -> 空值");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer?a=1&b=");
        check(r.ok && qget(r.value.query(), "a") == "1" && qget(r.value.query(), "b") == "",
              "空 value 保留");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer");
        check(r.ok && r.value.query().empty(), "无 query -> 空 map");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer?k=1&k=2");
        check(r.ok && qget(r.value.query(), "k") == "2", "重复 key 后者覆盖");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer?=v&a=1&&b=2");
        check(r.ok && r.value.query().size() == 2 && qget(r.value.query(), "a") == "1" &&
                  qget(r.value.query(), "b") == "2",
              "空 key / 空段跳过");
    }
    {
        const auto r = ShareRTCUrl::parse("https://host:8080/peer?room=1&mode=sfu");
        check(r.ok && r.value.to_url() == "https://host:8080/peer?mode=sfu&room=1",
              "to_url 含 query（按 key 排序）");
    }

    // —— 非法输入：ok=false 且 error 非空 ——
    const std::vector<std::string> bad = {
        "http://x.com:80/y",            // scheme 错误
        "https://:8080/peer",         // 缺 host
        "https://host:abc/peer",      // 端口非数字
        "https://host:99999/peer",    // 端口越界
        "https://host:0/peer",        // 端口为 0
        "https://host:/peer",         // 端口缺数字
        "https://host:8080/",         // 缺 peerid
        "https://host:8080",          // 缺 path
        "https://host:8080/?a=1",     // query 不能代替 peerid
        "",                             // 空串
    };
    for (const auto& s : bad) {
        const auto r = ShareRTCUrl::parse(s);
        check(!r.ok && !r.error.empty(), "正确拒绝: \"" + s + "\" -> " + r.error);
    }

    // —— 往返一致性（逐字段比较）——
    {
        const std::string url = "https://signal.example.com:8443/peer_001";
        const auto a = ShareRTCUrl::parse(url);
        const auto b = ShareRTCUrl::parse(a.value.to_url());
        check(a.ok && b.ok && a.value.scheme() == b.value.scheme() &&
                  a.value.host() == b.value.host() && a.value.port() == b.value.port() &&
                  a.value.peerid() == b.value.peerid() && a.value.query() == b.value.query(),
              "round-trip 一致性");
    }
    {
        // 往返一致性（含 query）
        const auto u = ShareRTCUrl::parse("https://host:8080/peer?a=1&b=2");
        const auto v = ShareRTCUrl::parse(u.value.to_url());
        check(u.ok && v.ok && u.value.scheme() == v.value.scheme() &&
                  u.value.host() == v.value.host() && u.value.port() == v.value.port() &&
                  u.value.peerid() == v.value.peerid() && u.value.query() == v.value.query(),
              "round-trip 含 query");
    }

    std::cout << "\n" << (g_fail == 0 ? "全部通过" : "存在失败") << "\n";
    return g_fail == 0 ? 0 : 1;
}
