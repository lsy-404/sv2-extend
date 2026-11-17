#define NATIVE_CATEGORY_TEST
#include "../loader/windows/native_category_registry.h"
#include <cstdio>
#include <cstring>

bool editor_gate::version_matches() { return false; }
static int updates, hidden_updates;
static float point_scale = 1;
static int client_width = 300, client_height = 600;
static RECT expected_bounds{23, 107, 173, 341};
static int protection_call, fail_protection_call;
static BOOL fail_protect(void *address, SIZE_T size, DWORD protection, DWORD *old) {
 if (++protection_call == fail_protection_call) return FALSE;
 return VirtualProtect(address, size, protection, old);
}
static void updated(void *, const char *id, HWND, const RECT &rect, bool visible) {
 if (!std::strcmp(id, "example") && visible && EqualRect(&rect, &expected_bounds)) ++updates;
 if (!std::strcmp(id, "example") && !visible) ++hidden_updates;
}
static void removed(void *) {}
static native_category_registry::RectI *content(void *, native_category_registry::RectI *out) { *out = {0, 44, 100, 156}; return out; }
static native_category_registry::PointF *point(native_category_registry::PointF *out, void *, void *component, uint64_t packed) {
 std::memcpy(out, &packed, sizeof packed);
 int x = 0, y = 0; auto p = static_cast<unsigned char *>(component);
 std::memcpy(&x, p + 0x38, 4); std::memcpy(&y, p + 0x3c, 4);
 out->x = out->x * point_scale + x; out->y = out->y * point_scale + y; return out;
}
static BOOL client(HWND, RECT *out) { *out = {0, 0, client_width, client_height}; return TRUE; }
template<class T> static void put(unsigned char *where, size_t offset, T value) { std::memcpy(where + offset, &value, sizeof value); }
int main() {
 using namespace native_category_registry;
 (void)&set_host; (void)&tick; (void)&open_category; (void)&close_category; (void)&category_open; (void)&status;
 if (prepare(reinterpret_cast<unsigned char *>(1), GetCurrentThreadId(), updated, removed)) return 3;
 SYSTEM_INFO system{}; GetSystemInfo(&system);
 auto memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, system.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
 if (!memory) return 7;
 auto first_slot = reinterpret_cast<void **>(memory), second_slot = reinterpret_cast<void **>(memory + system.dwPageSize);
 *first_slot = reinterpret_cast<void *>(1); *second_slot = reinterpret_cast<void *>(2);
 DWORD previous = 0;
 if (!VirtualProtect(memory, system.dwPageSize * 2, PAGE_READONLY, &previous)) return 8;
 Slot slots[] = {{first_slot, reinterpret_cast<void *>(1), reinterpret_cast<void *>(3)}, {second_slot, reinterpret_cast<void *>(2), reinterpret_cast<void *>(4)}};
 test_protect = fail_protect;
 image = reinterpret_cast<unsigned char *>(0x1234);
 for (int fail : {1, 2, 3, 4}) {
  protection_call = 0; fail_protection_call = fail;
  if (patch_slots(slots, 2) || *first_slot != reinterpret_cast<void *>(1) || *second_slot != reinterpret_cast<void *>(2) || image != reinterpret_cast<unsigned char *>(0x1234) || prepared) return 9;
  MEMORY_BASIC_INFORMATION a{}, b{};
  if (!VirtualQuery(first_slot, &a, sizeof a) || !VirtualQuery(second_slot, &b, sizeof b) || (a.Protect & 0xff) != PAGE_READONLY || (b.Protect & 0xff) != PAGE_READONLY) return 10;
 }
 test_protect = nullptr;
 VirtualFree(memory, 0, MEM_RELEASE);
 auto fake = static_cast<unsigned char *>(VirtualAlloc(nullptr, 0xbd0000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
 if (!fake) return 11;
 fake[0xbce780] = 0xc3;
 if (!VirtualProtect(fake, 0xbd0000, PAGE_EXECUTE_READ, &previous)) return 12;
 image = fake; pane_paint(nullptr, nullptr);
 if (updates || image != fake || prepared) return 13;
 VirtualFree(fake, 0, MEM_RELEASE); image = nullptr;
 HWND host = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
 if (!host) return 1;
 host_window = host; dock_update = updated; dock_remove = removed; prepared = true; thread_id = GetCurrentThreadId();
 image = static_cast<unsigned char *>(VirtualAlloc(nullptr, 0x1801000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
 if (!image) return 21;
 test_content = content; test_point = point; test_client = client;
 auto &category = categories[1]; std::strcpy(category.id, "example"); category.present = category.visible = true;
 unsigned char pane[0x148]{}, parent[0xe9]{}, model[8]{}, unrelated[8]{}, desktop[0xe0]{}, peer[0x100]{};
 void *peer_list[] = {peer};
 put(image, 0x17f0f38, static_cast<void *>(desktop));
 put(desktop, 0xd0, static_cast<void *>(peer_list)); put(desktop, 0xdc, 1);
 put(peer, 0, static_cast<void *>(image + 0x14f8510)); put(peer, 8, static_cast<void *>(parent)); put(peer, 0xf8, host);
 put(pane, 0, static_cast<void *>(image + 0x14ec388));
 put(model, 0, static_cast<void *>(category.model + 1));
 put(pane, 0x140, static_cast<void *>(model)); put(pane, 0x30, static_cast<void *>(parent));
 put(pane, 0x38, 15); put(pane, 0x3c, 27); put(pane, 0x40, 100); put(pane, 0x44, 200);
 put(parent, 0x40, 200); put(parent, 0x44, 400); pane[0xe8] = 2; parent[0xe8] = 3;
 update_pane(pane);
 point_scale = 2; expected_bounds = {23, 173, 323, 641}; update_pane(pane); point_scale = 1;
 put(parent, 0x40, 2560); put(parent, 0x44, 1370); client_width = 3840; client_height = 2054;
 expected_bounds = {23, 106, 173, 340}; update_pane(pane);
 put(peer, 0xf8, static_cast<HWND>(nullptr)); update_pane(pane); put(peer, 0xf8, host);
 put(parent, 0x50, static_cast<void *>(peer)); update_pane(pane); put(parent, 0x50, static_cast<void *>(nullptr));
 if (hidden_updates != 2) return 22;
 put(pane, 0x140, static_cast<void *>(unrelated)); update_pane(pane);
 if (updates != 3 || panes.size() != 1) return 2;
 thread_id = GetCurrentThreadId(); dock_update = nullptr;
 Entry first[] = {{"example", "Example", true}, {"second", "Second", false}};
 if (!sync(first, 2, true) || !categories[1].present || std::strcmp(categories[1].title, "Example")) return 4;
 Entry duplicate[] = {{"example", "Changed", true}, {"example", "Duplicate", true}};
 if (sync(duplicate, 2, true) || std::strcmp(categories[1].title, "Example")) return 5;
 if (!sync(first + 1, 1, true) || categories[1].present) return 6;
 panes.clear(); std::strcpy(categories[1].id, "sv2.mcp"); categories[1].present = false; categories[2].present = false;
 Entry next[] = {{"boxy.sv2", "Boxy", true}, {"sv2.mcp", "MCP", true}};
 if (!sync(next, 2, true) || std::strcmp(categories[1].id, "sv2.mcp") || std::strcmp(categories[2].id, "boxy.sv2")) return 14;
 char ids[32][32]{}; Entry full[32]{};
 std::strcpy(ids[0], "sv2.mcp");
 for (int i = 1; i < 32; ++i) std::snprintf(ids[i], sizeof ids[i], "plugin.%d", i);
 for (int i = 0; i < 32; ++i) full[i] = {ids[i], ids[i], true};
 if (!sync(full, 32, true)) return 15;
 std::strcpy(ids[31], "replacement");
 if (!sync(full, 32, true)) return 16;
 VirtualFree(image, 0, MEM_RELEASE); image = nullptr;
 DestroyWindow(host);
 std::puts("PASS: executable and protection failure guards, forwarding, pane identity and bounds, dynamic registration");
 return 0;
}
