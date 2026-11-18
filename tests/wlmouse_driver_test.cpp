#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>

// Standalone on Windows: g++ -std=c++17 tests/wlmouse_driver_test.cpp src/drivers/wlmouse.cpp -o build/wlmouse_driver_test.exe

namespace {
int opens = 0;
USHORT lastUsage = 0;
std::vector<std::vector<BYTE>> replies;

HidColl Collection(USHORT pid, USHORT page = 0xFFFF, USHORT usage = 0) {
    HidColl c;
    c.path = L"test";
    c.vid = 0x36A7;
    c.pid = pid;
    c.caps.UsagePage = page;
    c.caps.Usage = usage;
    c.caps.FeatureReportByteLength = 65;
    return c;
}
}  // namespace

bool HidDev::OpenFeatureOnly(const HidColl& c) {
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    ++opens;
    lastUsage = c.usage();
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
bool HidDev::SetFeature(const std::vector<BYTE>& data) {
    assert(data.size() == 65 && data[3] == 2 && data[4] == 2 && data[6] == 0x83);
    return true;
}
bool HidDev::GetFeature(BYTE, std::vector<BYTE>& out) {
    if (replies.empty()) return false;
    out = replies.front();
    replies.erase(replies.begin());
    return true;
}
std::wstring HidProduct(const HidColl&) { return L"Test device"; }
DeviceKind GuessKind(const std::wstring&) { return DeviceKind::Other; }
DeviceKind KindFromCollections(const std::vector<HidColl>&, USHORT, USHORT) { return DeviceKind::Other; }
std::wstring Hex4(USHORT v) {
    wchar_t text[8];
    swprintf(text, 8, L"%04X", v);
    return text;
}
int Clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

std::vector<BYTE> Reply(BYTE status, BYTE pct = 42, BYTE charging = 0) {
    return { 0, status, 0, 2, 2, 0, 0x83, charging, pct };
}

int main() {
    PollContext ctx;
    ctx.colls = { Collection(0xA870, 0xFF00), Collection(0xA870), Collection(0xA871), Collection(0xA882) };
    replies = { Reply(0xA1) };
    PollWlmouse(ctx);
    assert(opens == 1 && ctx.out.size() == 1);
    assert(ctx.out[0].key == L"wlmouse:A870" && ctx.out[0].name == L"WLMouse Beast X Pro");
    assert(ctx.out[0].percent == 42 && !ctx.out[0].charging && !ctx.out[0].asleep);
    assert(ctx.out[0].via == L"2.4 GHz receiver" && ctx.out[0].kind == DeviceKind::Mouse);
    assert(ctx.out[0].graceSec == 1800);
    assert(ctx.IsClaimed(0x36A7, 0xA870) && !ctx.IsClaimed(0x36A7, 0xA871));

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { Reply(0xA0) };
    PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].asleep && ctx.out[0].percent == -1);

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { Reply(0xA2, 37, 1) };
    PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 37 && ctx.out[0].charging);

    // Ignore echoed requests and stale commands until a matching battery reply.
    ctx = {};
    ctx.colls = { Collection(0xA870) };
    auto stale = Reply(0xA1); stale[4] = 3;
    replies = { Reply(0), stale, Reply(0xA1, 36) };
    PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 36);

    // Reject out-of-range values and malformed responses without claiming the device.
    for (auto reply : { Reply(0xA1, 101), std::vector<BYTE>{ 0, 0xA1 } }) {
        ctx = {};
        ctx.colls = { Collection(0xA870) };
        replies = { reply };
        PollWlmouse(ctx);
        assert(ctx.out.empty() && !ctx.IsClaimed(0x36A7, 0xA870));
    }

    // Prefer usage zero even if it was enumerated later.
    ctx = {};
    ctx.colls = { Collection(0xA870, 0xFFFF, 1), Collection(0xA870) };
    opens = 0;
    replies = { Reply(0xA1, 50) };
    PollWlmouse(ctx);
    assert(opens == 1 && ctx.out.size() == 1 && lastUsage == 0);

    // Cable claims its paired receiver, preventing duplicate entries.
    ctx = {};
    ctx.colls = { Collection(0xA883), Collection(0xA884) };
    opens = 0;
    replies = { Reply(0xA1, 90, 1) };
    PollWlmouse(ctx);
    assert(opens == 1 && ctx.out.size() == 1);
    assert(ctx.out[0].key == L"wlmouse:A884" && ctx.out[0].via == L"USB cable");
    assert(ctx.IsClaimed(0x36A7, 0xA883) && ctx.IsClaimed(0x36A7, 0xA884));
    std::puts("Upstream WLMouse driver tests passed.");
}
