#include "HwinfoSource.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace {

// ---------------------------------------------------------------- 结构偏移
// HWiNFO 的共享内存是 C 结构体的原始字节，这里不用 struct 强转，
// 而是按固定偏移读取。原因是结构体里有 8 字节对齐的 double，
// 编译器可能插入不同的 padding，手算偏移反而更可靠。
//
// 头部（HWiNFO_SENSORS_SHARED_MEM2）：
//   0  dwSignature            "HWiS"
//   4  dwVersion
//   8  dwRevision
//   12 (padding，__time64_t 需要 8 字节对齐)
//   16 poll_time
//   24 dwOffsetOfSensorSection
//   28 dwSizeOfSensorElement
//   32 dwNumSensorElements
//   36 dwOffsetOfReadingSection
//   40 dwSizeOfReadingElement
//   44 dwNumReadingElements
constexpr uint32_t kSignature = 0x53695748;   // 'HWiS' 小端

constexpr size_t kOffSig        = 0;
constexpr size_t kOffRevision   = 8;
constexpr size_t kOffPollTime   = 16;
constexpr size_t kOffReadingSec = 36;
constexpr size_t kOffReadingSz  = 40;
constexpr size_t kOffReadingCnt = 44;
constexpr size_t kHeaderMinSize = 48;

// 读数元素（HWiNFO_SENSORS_READING_ELEMENT）：
//   0   dwReadingID
//   4   dwSensorID
//   8   dwReadingType
//   12  szLabelOrig[128]
//   140 szLabelUser[128]
//   268 szUnit[16]
//   284 (padding)
//   288 double Value
constexpr size_t kOffRType      = 8;
constexpr size_t kOffRLabelUser = 140;
constexpr size_t kOffRValue     = 288;
constexpr size_t kReadingMinSz  = 296;

constexpr uint32_t kTypeTemp  = 1;
constexpr uint32_t kTypeClock = 5;

// ---------------------------------------------------------------- 小工具
uint32_t rdU32(const uint8_t* p, size_t off) {
    uint32_t v = 0;
    std::memcpy(&v, p + off, sizeof(v));
    return v;
}

int64_t rdI64(const uint8_t* p, size_t off) {
    int64_t v = 0;
    std::memcpy(&v, p + off, sizeof(v));
    return v;
}

std::string rdStr(const uint8_t* p, size_t off, size_t cap) {
    char buf[160]{};
    const size_t n = (cap < sizeof(buf) - 1) ? cap : sizeof(buf) - 1;
    std::memcpy(buf, p + off, n);
    buf[n] = '\0';
    return std::string(buf);
}

bool has(const std::string& s, const char* k) {
    return s.find(k) != std::string::npos;
}

// ---------------------------------------------------------------- 打分选优
// HWiNFO 里同一类读数有很多条（每个核心一个、每个传感器一个），
// 用打分挑出最想要的那条，而不是简单取第一个。
int scoreCpuTemp(const std::string& l) {
    if (!has(l, "CPU")) return -1;
    int s = 10;
    if (has(l, "Package"))                       s += 50;   // Intel 封装温度
    if (has(l, "Tctl") || has(l, "Tdie"))        s += 60;   // AMD 核心温度
    if (has(l, "Core"))                          s += 15;
    if (has(l, "Max"))                           s -= 40;
    if (has(l, "Distance") || has(l, "to TjMax")) s -= 60;
    return s;
}

int scoreGpuTemp(const std::string& l) {
    if (!has(l, "GPU")) return -1;
    int s = 10;
    if (has(l, "Temperature"))  s += 50;
    if (has(l, "Hot Spot"))     s -= 35;
    if (has(l, "Memory"))       s -= 20;
    if (has(l, "Max"))          s -= 40;
    return s;
}

int scoreGpuClock(const std::string& l) {
    if (!has(l, "GPU")) return -1;
    int s = 10;
    if (has(l, "Clock"))   s += 50;
    if (has(l, "Memory"))  s -= 20;
    if (has(l, "Max"))     s -= 40;
    return s;
}

const wchar_t* kMapNames[] = {
    L"Global\\HWiNFO_SENS_SM2",
    L"Global\\HWiNFO_SENS_SM",
    // 少数环境（非管理员、或 HWiNFO 以普通权限运行）会落在会话本地命名空间
    L"HWiNFO_SENS_SM2",
    L"HWiNFO_SENS_SM",
};

} // namespace

HwinfoSource::~HwinfoSource() {
    detach();
}

bool HwinfoSource::init() {
    attach();     // 失败无所谓，poll 里会定期重试
    return view_ != nullptr;
}

void HwinfoSource::detach() {
    if (view_) {
        UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    viewSize_     = 0;
    numReadings_  = 0;
    cpuTempOff_   = -1;
    gpuTempOff_   = -1;
    gpuClockOff_  = -1;
    lastPollTime_ = -1;
    staleCount_   = 0;
}

bool HwinfoSource::attach() {
    for (const wchar_t* name : kMapNames) {
        HANDLE h = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
        if (!h) continue;

        // 映射成功后立刻关掉句柄：视图本身会让 section 保持存活
        void* v = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(h);
        if (!v) continue;

        // 映射区大小要自己问：MapViewOfFile 传 0 表示整块，但不告诉我们多大
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(v, &mbi, sizeof(mbi)) == 0) {
            UnmapViewOfFile(v);
            continue;
        }

        view_     = static_cast<const uint8_t*>(v);
        viewSize_ = mbi.RegionSize;

        if (viewSize_ < kHeaderMinSize || rdU32(view_, kOffSig) != kSignature) {
            detach();
            continue;
        }

        if (!rescan()) {
            detach();
            continue;
        }
        return true;
    }
    return false;
}

bool HwinfoSource::rescan() {
    if (!view_ || viewSize_ < kHeaderMinSize) return false;

    const uint32_t offSec  = rdU32(view_, kOffReadingSec);
    const uint32_t szElem  = rdU32(view_, kOffReadingSz);
    const uint32_t cntElem = rdU32(view_, kOffReadingCnt);

    if (szElem < kReadingMinSz || cntElem == 0) return false;
    if (static_cast<uint64_t>(offSec) + static_cast<uint64_t>(cntElem) * szElem > viewSize_) {
        return false;
    }

    offReadings_ = offSec;
    stride_      = szElem;
    numReadings_ = cntElem;

    cpuTempOff_  = -1;
    gpuTempOff_  = -1;
    gpuClockOff_ = -1;

    int bestCpu = -1, bestGpuT = -1, bestGpuC = -1;

    for (uint32_t i = 0; i < numReadings_; ++i) {
        const uint8_t* p = view_ + offReadings_ + static_cast<size_t>(i) * stride_;
        const uint32_t type = rdU32(p, kOffRType);

        if (type != kTypeTemp && type != kTypeClock) continue;

        std::string label = rdStr(p, kOffRLabelUser, 128);
        if (label.empty()) label = rdStr(p, 12, 128);   // 退回原始标签
        if (label.empty()) continue;

        const int32_t off = static_cast<int32_t>(offReadings_ + static_cast<size_t>(i) * stride_);

        if (type == kTypeTemp) {
            const int sc = scoreCpuTemp(label);
            if (sc > bestCpu) { bestCpu = sc; cpuTempOff_ = off; }

            const int sg = scoreGpuTemp(label);
            if (sg > bestGpuT) { bestGpuT = sg; gpuTempOff_ = off; }
        } else {
            const int sg = scoreGpuClock(label);
            if (sg > bestGpuC) { bestGpuC = sg; gpuClockOff_ = off; }
        }
    }

    return true;
}

double HwinfoSource::readValue(int32_t offset) const {
    if (offset < 0 || !view_) return -1.0;
    const size_t off = static_cast<size_t>(offset);
    if (off + kOffRValue + sizeof(double) > viewSize_) return -1.0;

    double v = 0.0;
    std::memcpy(&v, view_ + off + kOffRValue, sizeof(v));
    // HWiNFO 用极值表示无效
    if (v < -900.0 || v > 100000.0) return -1.0;
    return v;
}

void HwinfoSource::poll(Metrics& io) {
    // 没挂上就每 5 秒重试一次：用户可能中途才启动 HWiNFO
    const uint64_t now = GetTickCount64();
    if (!view_) {
        if (now - lastTryMs_ < 5000) return;
        lastTryMs_ = now;
        attach();
        if (!view_) return;
    }

    // 数据新鲜度：HWiNFO 免费版 12 小时后共享内存会冻结，poll_time 不再前进。
    // 连续 15 次没变化（约 15 秒）就认为已冻结，全部返回 -1，
    // 免得界面上一直挂着一个不再更新的温度。
    const int64_t pt = rdI64(view_, kOffPollTime);
    if (pt == lastPollTime_) {
        if (++staleCount_ > 15) {
            io.cpuTemp  = -1.f;
            io.gpuTemp  = -1.f;
            return;
        }
    } else {
        lastPollTime_ = pt;
        staleCount_   = 0;
    }

    const double ct = readValue(cpuTempOff_);
    if (ct > -1.0 && ct < 150.0) io.cpuTemp = static_cast<float>(ct);

    const double gt = readValue(gpuTempOff_);
    if (gt > -1.0 && gt < 150.0) io.gpuTemp = static_cast<float>(gt);

    const double gc = readValue(gpuClockOff_);
    if (gc > 0.0 && gc < 10000.0) io.gpuClockMHz = static_cast<float>(gc);
}
