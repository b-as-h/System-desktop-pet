// ============================================================================
// 包含顺序在 MinGW 下是硬约束，别调整：
//
//   GetIfTable2 / MIB_IF_ROW2 声明在 netioapi.h 里（不在 iphlpapi.h）。
//   而 netioapi.h 里这些声明被 #ifdef _WS2IPDEF_ 包着，
//   且它只在 __IPHLPAPI_H__ 未定义时才会去包含 ws2ipdef.h。
//
//   所以：先包含 iphlpapi.h 会让 netioapi.h 跳过 ws2ipdef.h，
//         _WS2IPDEF_ 没定义，整段声明消失，
//         报错 "PMIB_IF_TABLE2 was not declared in this scope"。
//
//   正确顺序：winsock2.h → ws2ipdef.h → windows.h → iphlpapi.h → netioapi.h
// ============================================================================
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include "NetSource.h"

namespace {

// 判断是不是"真正承载流量的物理网卡"
bool isRealNic(const MIB_IF_ROW2& r) {
    // 1. 只要以太网和无线
    if (r.Type != IF_TYPE_ETHERNET_CSMACD && r.Type != IF_TYPE_IEEE80211) return false;

    // 2. 必须已连接
    if (r.OperStatus != IfOperStatusUp) return false;
    if (r.MediaConnectState != MediaConnectStateConnected) return false;

    // 3. 关键：排除 NDIS 过滤驱动接口
    //    一块物理网卡会被 GetIfTable2 拆成 5 条：本体 + WFP Native MAC Layer
    //    LightWeight Filter + QoS Packet Scheduler + WFP 802.3 MAC Layer ...
    //    这些过滤接口 Type 也是 6、状态也是 Up，只看类型状态根本筛不掉。
    if (!r.InterfaceAndOperStatusFlags.HardwareInterface) return false;
    if (r.InterfaceAndOperStatusFlags.FilterInterface)    return false;

    // 4. 描述黑名单：WAN Miniport / 虚拟网卡 / 调试用适配器
    static const wchar_t* kBad[] = {
        L"Virtual", L"Hyper-V", L"VMware", L"VirtualBox", L"TAP",
        L"WireGuard", L"Loopback", L"VPN", L"WAN Miniport", L"Kernel Debug"
    };
    for (const wchar_t* b : kBad) {
        if (wcsstr(r.Description, b)) return false;
    }
    return true;
}

} // namespace

void NetSource::poll(Metrics& io) {
    PMIB_IF_TABLE2 tbl = nullptr;
    if (GetIfTable2(&tbl) != NO_ERROR || !tbl) return;

    uint64_t in = 0, out = 0;
    for (ULONG i = 0; i < tbl->NumEntries; ++i) {
        const MIB_IF_ROW2& r = tbl->Table[i];
        if (!isRealNic(r)) continue;
        in  += r.InOctets;
        out += r.OutOctets;
    }
    FreeMibTable(tbl);

    if (hasPrev_) {
        // 计数器是累计值，两次采样求差。溢出不担心：uint64 字节数够跑几百年。
        const uint64_t dIn  = (in  >= prevIn_)  ? (in  - prevIn_)  : 0;
        const uint64_t dOut = (out >= prevOut_) ? (out - prevOut_) : 0;
        io.netDownKBs = static_cast<float>(dIn)  / 1024.0f;
        io.netUpKBs   = static_cast<float>(dOut) / 1024.0f;
    }
    prevIn_  = in;
    prevOut_ = out;
    hasPrev_ = true;
}
