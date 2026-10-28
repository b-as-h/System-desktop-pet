#include "GpuSource.h"

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <map>

namespace {

// 从实例名里取出引擎类型。
// 实例名形如：pid_1234_luid_0x00000000_0x0000DC44_phys_0_eng_0_engtype_3D
// 注意引擎名是 "3D"（大写），另有 "Copy" / "VideoDecode" / "Compute 0" 等。
std::string engineOf(const std::wstring& path) {
    const size_t ep = path.find(L"engtype_");
    if (ep == std::wstring::npos) return {};
    const size_t start = ep + 8;
    const size_t end   = path.find(L')', start);
    std::wstring e = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
    return std::string(e.begin(), e.end());
}

constexpr wchar_t kPath[] = L"\\GPU Engine(*)\\Utilization Percentage";

} // namespace

GpuSource::~GpuSource() {
    if (query_) {
        PdhCloseQuery(static_cast<PDH_HQUERY>(query_));
        query_ = nullptr;
    }
}

bool GpuSource::init() {
    PDH_HQUERY q = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return false;

    // 第一次调用传 nullptr 拿所需长度
    DWORD size = 0;
    PDH_STATUS st = PdhExpandWildCardPathW(nullptr, kPath, nullptr, &size, 0);
    if ((st != PDH_MORE_DATA && st != ERROR_SUCCESS) || size == 0) {
        PdhCloseQuery(q);
        return false;
    }

    // 多留 4 个字符余量：返回的是多字符串（\0 分隔、双 \0 结尾），
    // 边界算错就会在读实例名时越界。
    std::wstring buf(size + 4, L'\0');
    DWORD cap = static_cast<DWORD>(buf.size());
    st = PdhExpandWildCardPathW(nullptr, kPath, &buf[0], &cap, 0);
    if (st != ERROR_SUCCESS) {
        PdhCloseQuery(q);
        return false;
    }

    // 有界遍历，不能用 wcslen 裸跑
    const wchar_t* p   = buf.data();
    const wchar_t* end = buf.data() + buf.size();
    while (p < end && *p) {
        const std::wstring path(p, wcsnlen(p, static_cast<size_t>(end - p)));
        const std::string eng = engineOf(path);
        if (!eng.empty()) {
            // 必须保存句柄。传 nullptr 给 PdhAddCounterW 之后再取值会直接崩。
            PDH_HCOUNTER h = nullptr;
            if (PdhAddCounterW(q, path.c_str(), 0, &h) == ERROR_SUCCESS && h) {
                counters_.emplace_back(h, eng);
            }
        }
        p += path.size() + 1;
    }

    if (counters_.empty()) {
        PdhCloseQuery(q);
        return false;
    }

    // 先采一次建立基线，否则第一次 poll 拿不到有效差值
    PdhCollectQueryData(q);
    query_ = q;
    return true;
}

void GpuSource::poll(Metrics& io) {
    if (!query_ || counters_.empty()) {
        io.gpuUsage = -1.f;   // 无数据，不是错误
        return;
    }

    PDH_HQUERY q = static_cast<PDH_HQUERY>(query_);
    if (PdhCollectQueryData(q) != ERROR_SUCCESS) return;

    // 同一引擎会有多个进程实例，逐个累加得到该引擎的总占用
    std::map<std::string, double> byEngine;
    for (const auto& c : counters_) {
        PDH_FMT_COUNTERVALUE v{};
        if (PdhGetFormattedCounterValue(static_cast<PDH_HCOUNTER>(c.first),
                                        PDH_FMT_DOUBLE, nullptr, &v) != ERROR_SUCCESS) {
            continue;
        }
        if (v.CStatus != ERROR_SUCCESS) continue;
        byEngine[c.second] += v.doubleValue;
    }

    if (byEngine.empty()) return;

    double g = 0.0;
    auto it = byEngine.find("3D");
    if (it != byEngine.end()) {
        g = it->second;
    } else {
        // 有些老卡不上报 3D 引擎，退化成"取最忙的那个引擎"
        for (const auto& kv : byEngine) g = std::max(g, kv.second);
    }

    io.gpuUsage = static_cast<float>(std::min(g, 100.0));
}
