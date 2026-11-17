#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>

namespace {
int opens = 0;
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

int main() {
    PollContext ctx;
    ctx.colls = { Collection(0xA870, 0xFF00), Collection(0xA870), Collection(0xA871), Collection(0xA882) };
    replies = { { 0, 0xA1, 0, 2, 2, 0, 0x83, 0, 42 } };
    PollGlorious(ctx);
    assert(opens == 1 && ctx.out.size() == 1);
    assert(ctx.out[0].key == L"wlmouse:A870" && ctx.out[0].name == L"WLMouse Beast X Pro");
    assert(ctx.out[0].percent == 42 && !ctx.out[0].charging && !ctx.out[0].asleep);
    assert(ctx.out[0].via == L"2.4 GHz receiver" && ctx.out[0].kind == DeviceKind::Mouse);
    assert(ctx.IsClaimed(0x36A7, 0xA870) && !ctx.IsClaimed(0x36A7, 0xA871));

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { { 0, 0xA4, 0, 2, 2, 0, 0x83, 0, 42 } };
    PollGlorious(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].asleep && ctx.out[0].percent == -1);

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { { 0, 0xA1, 0, 2, 2, 0, 0, 0, 42 } };
    PollGlorious(ctx);
    assert(ctx.out.empty() && !ctx.IsClaimed(0x36A7, 0xA870));

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { { 0, 0xA0, 0, 2, 2, 0, 0x83, 0, 0 },
                { 0, 0xA1, 0, 2, 2, 1, 0x83, 0, 99 },
                { 0, 0xA1, 0, 2, 2, 0, 0x83, 0, 37 } };
    PollGlorious(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 37 && !ctx.out[0].asleep);

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { { 0, 0xA3, 0, 2, 2, 0, 0x83, 0, 0 },
                { 0, 0xA1, 0, 2, 2, 0, 0x83, 0, 36 } };
    PollGlorious(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 36);

    ctx = {};
    ctx.colls = { Collection(0xA870) };
    replies = { { 0, 0xA2, 0, 2, 2, 0, 0x83, 0, 0 } };
    PollGlorious(ctx);
    assert(ctx.out.empty() && !ctx.IsClaimed(0x36A7, 0xA870));
    std::puts("WLMouse Beast X Pro driver tests passed.");
}
