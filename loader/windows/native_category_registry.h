#pragma once

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "native_string.h"
#include "../packages/builtin_icons.h"
#include "../packages/panel_icon.h"
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor_gate { bool version_matches(); }
namespace native_category_registry {
constexpr unsigned capacity = 33;
struct Category {
 void *registry[6]{};
 void *model[25]{};
 void **object = nullptr;
 char id[97]{};
 char title[513]{};
 char icon[4096]{};
 char active_icon[4096]{};
 bool match_icon_colors = true;
 bool present = false;
 bool visible = false;
 uint64_t icon_generation = 0;
};
static std::array<Category, capacity> categories;
static unsigned char *image;
static DWORD thread_id;
static bool prepared;
static const char *prepare_result="not_attempted";
struct Entry { const char *id; const char *title; const char *icon; const char *active_icon; bool match_icon_colors; bool visible; };
using DockUpdate = void (*)(void *, const char *, HWND, const RECT &, bool);
using DockRemove = void (*)(void *);
using OwnsChild = bool (*)(void *, void *);
static DockUpdate dock_update;
static DockRemove dock_remove;
static OwnsChild owns_child;
static void (*scripts_update)(void *,const RECT &,bool);
static void (*scripts_remove)(void *);
[[maybe_unused]] static void set_scripts_callbacks(void (*update)(void *,const RECT &,bool),void (*remove)(void *)){scripts_update=update;scripts_remove=remove;}
static HWND host_window;
static std::set<void *> panes;
static std::set<void *> bars;
struct ButtonIconState {
 Category *category;
 uint64_t generation;
 bool active;
 std::array<void *,3> copies;
};
static std::unordered_map<void *,ButtonIconState> button_icons;

static bool readable(const void *address, size_t length) {
 if (!address || !length) return false;
 auto cursor = reinterpret_cast<uintptr_t>(address);
 if (cursor > UINTPTR_MAX - length) return false;
 const auto end = cursor + length;
 while (cursor < end) {
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(reinterpret_cast<void *>(cursor), &info, sizeof info) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
  auto start = reinterpret_cast<uintptr_t>(info.BaseAddress);
  if (start > cursor || info.RegionSize > UINTPTR_MAX - start || start + info.RegionSize <= cursor) return false;
  cursor = (std::min)(end, start + info.RegionSize);
 }
 return true;
}

static Category *registry_category(const void *object) {
 for (auto &category : categories) if (object == &category.object) return &category;
 return nullptr;
}
static Category *model_category(const void *object) {
 if (!object) return nullptr;
 void *table = *static_cast<void *const *>(object);
 for (auto &category : categories) if (table == category.model + 1) return &category;
 return nullptr;
}
static Category *pane_category(void *pane) {
 auto bytes = static_cast<unsigned char *>(pane);
 if (!readable(bytes, 0x148)) return nullptr;
 void *model = nullptr;
 std::memcpy(&model, bytes + 0x140, sizeof model);
 return readable(model, sizeof(void *)) ? model_category(model) : nullptr;
}
struct RectI { int x, y, width, height; };
#ifdef NATIVE_CATEGORY_TEST
static RectI *(*test_content)(void *, RectI *);
static void (*test_set_visible)(void *, bool);
#endif
static bool pane_bounds(void *pane, HWND host, RECT &result, bool &showing) {
 auto node = static_cast<unsigned char *>(pane);
 unsigned char *root = nullptr;
 for (unsigned depth = 0; depth < 128; ++depth) {
  if (!readable(node, 0xe9)) return false;
  showing = showing && (node[0xe8] & 2);
  unsigned char *parent = nullptr;
  std::memcpy(&parent, node + 0x30, sizeof parent);
  if (!parent) { root = node; break; }
  node = parent;
 }
 if (!root || !readable(image + 0x17f0f38, sizeof(void *))) return false;
 void *desktop = nullptr; std::memcpy(&desktop, image + 0x17f0f38, sizeof desktop);
 if (!readable(desktop, 0xe0)) return false;
 auto desktop_bytes = static_cast<unsigned char *>(desktop);
 void **peers = nullptr; int count = 0;
 std::memcpy(&peers, desktop_bytes + 0xd0, sizeof peers);
 std::memcpy(&count, desktop_bytes + 0xdc, 4);
 if (count < 1 || count > 256 || !readable(peers, count * sizeof(void *))) return false;
 unsigned matches = 0;
 for (int i = 0; i < count; ++i) {
  auto peer = static_cast<unsigned char *>(peers[i]);
  if (!readable(peer, 0x100)) return false;
  void *table = nullptr, *component = nullptr; HWND peer_window = nullptr;
  std::memcpy(&table, peer, sizeof table); std::memcpy(&component, peer + 8, sizeof component);
  std::memcpy(&peer_window, peer + 0xf8, sizeof peer_window);
  if (component == root) {
   if (table != image + 0x14f8510 || peer_window != host) return false;
   ++matches;
  }
 }
 if (matches != 1) return false;
 DWORD pid = 0;
 if (GetWindowThreadProcessId(host, &pid) != thread_id || pid != GetCurrentProcessId()) return false;
 void *table = nullptr; std::memcpy(&table, pane, sizeof table);
 if (table != image + 0x14ec388 || !readable(static_cast<unsigned char *>(table) + 0x1e8, sizeof(void *))) return false;
#ifndef NATIVE_CATEGORY_TEST
 void *height_method = nullptr; std::memcpy(&height_method, static_cast<unsigned char *>(table) + 0x1e8, sizeof height_method);
 if (height_method != image + 0x8ec3f0) return false;
#endif
 RectI area{};
#ifdef NATIVE_CATEGORY_TEST
 if (test_content) test_content(pane, &area); else
#endif
 reinterpret_cast<RectI *(*)(void *, RectI *)>(image + 0xbcf2a0)(pane, &area);
 if (area.x < 0 || area.y < 0 || area.width <= 0 || area.width > 32768 || area.height <= 0 || area.height > 32768 || area.x > 32768 || area.y > 32768) return false;
 result = {area.x, area.y, area.x + area.width, area.y + area.height};
 return true;
}
static void update_pane(void *pane) {
 if (!prepared) return;
 auto category = pane_category(pane);
 if (!category) {
  auto bytes=static_cast<unsigned char*>(pane);void *model=nullptr,*table=nullptr;if(!readable(bytes,0x148))return;std::memcpy(&model,bytes+0x140,sizeof model);if(!readable(model,sizeof(void*)))return;std::memcpy(&table,model,sizeof table);if(table!=image+0x14eaca0||!scripts_update||!host_window||!IsWindow(host_window))return;
  bool showing=true;RECT rect{};if(pane_bounds(pane,host_window,rect,showing))scripts_update(pane,rect,showing);return;
 }
 panes.insert(pane);
 if (!dock_update || !host_window || !IsWindow(host_window)) return;
 auto bytes = static_cast<unsigned char *>(pane);
 void **children = nullptr; int count = 0;
 std::memcpy(&children, bytes + 0x58, sizeof children);
 std::memcpy(&count, bytes + 0x64, sizeof count);
 if (count < 0 || count > 128 || (count && !readable(children, count * sizeof(void *)))) return;
 using SetVisible = void (*)(void *, bool);
 for (int i = 0; i < count; ++i) {
  void *child = children[i];
  if (!readable(child, 0xe9)) return;
  if (owns_child && owns_child(pane, child)) continue;
#ifdef NATIVE_CATEGORY_TEST
  if (test_set_visible) test_set_visible(child, false); else
#endif
  reinterpret_cast<SetVisible>(image + 0xc4e5b0)(child, false);
  if (!panes.count(pane)) return;
 }
 bool visible = category->present && category->visible;
 RECT rect{};
 if (!pane_bounds(pane, host_window, rect, visible)) visible = false;
 dock_update(pane, category->id, host_window, rect, visible);
}
static void pane_paint(void *pane, void *graphics) {
 reinterpret_cast<void (*)(void *, void *)>(image + 0xbce780)(pane, graphics);
 update_pane(pane);
}
static void pane_resized(void *pane) {
 reinterpret_cast<void (*)(void *)>(image + 0xbd82e0)(pane);
 update_pane(pane);
}
static void *pane_destroy(void *pane, unsigned flags) {
 if (panes.erase(pane) && dock_remove) dock_remove(pane);
 if (scripts_remove) scripts_remove(pane);
 return reinterpret_cast<void *(*)(void *, unsigned)>(image + 0xbd8090)(pane, flags);
}
static bool release_string(void *value) {
 if (!value) return false;
 auto text = static_cast<unsigned char *>(value);
 if (text == image + 0x17903a0) return true;
 if (reinterpret_cast<uintptr_t>(text) < 0x10 || !readable(text - 0x10, sizeof(LONG))) return false;
 if (InterlockedExchangeAdd(reinterpret_cast<LONG *>(text - 0x10), -1) == 0)
  reinterpret_cast<void (*)(void *)>(image + 0x10baa40)(text - 0x10);
 return true;
}
static void *icon(void *object, void **output) {
 auto category = registry_category(object);
 // The Loader owns the root icon; every other category shows its declared icon, falling back to the generic plugin mark.
 const char *svg = category == &categories[0] ? OPENSV2_ICON_ROOT
  : category && category->icon[0] ? category->icon
  : OPENSV2_ICON_PLUGIN;
 const std::string resolved=category && category!=&categories[0]
  ? panel_icon::resolve(svg,category->active_icon,category->match_icon_colors,false) : svg;
 *output = nullptr;
 void *source = nullptr, *xml = nullptr;
 reinterpret_cast<void *(*)(void *, const char *)>(image + 0xe6fac0)(&source, resolved.c_str());
 if (!source) return output;
 reinterpret_cast<void *(*)(void *, void *)>(image + 0xe588e0)(&xml, &source);
 release_string(source);
 if (xml) {
  reinterpret_cast<void *(*)(void *, void *)>(image + 0xc68000)(output, xml);
  reinterpret_cast<void (*)(void *)>(image + 0xe5ce40)(xml);
  reinterpret_cast<void (*)(void *, size_t)>(image + 0x10baa40)(xml, 0x20);
 }
 return output;
}
static void destroy_drawable(void *drawable) {
 if (drawable) reinterpret_cast<void (*)(void *,unsigned)>((*static_cast<void ***>(drawable))[0])(drawable,1);
}
static void replace_drawable(void **slot,void *replacement) {
 void *previous=*slot;
 *slot=replacement;
 destroy_drawable(previous);
}
static bool refresh_icon(void *button,Category &category,bool active) {
 if (&category==&categories[0] || !readable(button,0x398)) return false;
 auto bytes=static_cast<unsigned char *>(button);
 void *table=nullptr;
 std::memcpy(&table,bytes,sizeof table);
 if(table!=image+0x14eaac0)return false;
 auto copies=reinterpret_cast<void **>(bytes+0x368);
 auto cached=button_icons.find(button);
 if(cached!=button_icons.end()&&cached->second.category==&category&&cached->second.generation==category.icon_generation&&
    cached->second.active==active&&cached->second.copies[0]==copies[0]&&cached->second.copies[1]==copies[1]&&cached->second.copies[2]==copies[2])return true;
 const char *source=category.icon[0]?category.icon:OPENSV2_ICON_PLUGIN;
 std::string svg=panel_icon::resolve(source,category.active_icon,category.match_icon_colors,active);
 auto parse=[&](void **out){
  *out=nullptr;
  void *string=nullptr,*xml=nullptr;
  reinterpret_cast<void *(*)(void *,const char *)>(image+0xe6fac0)(&string,svg.c_str());
  if(!string)return;
  reinterpret_cast<void *(*)(void *,void *)>(image+0xe588e0)(&xml,&string);
  release_string(string);
  if(xml){
   reinterpret_cast<void *(*)(void *,void *)>(image+0xc68000)(out,xml);
   reinterpret_cast<void (*)(void *)>(image+0xe5ce40)(xml);
   reinterpret_cast<void (*)(void *,size_t)>(image+0x10baa40)(xml,0x20);
  }
 };
 void *normal=nullptr,*raw[3]{};
 parse(&normal);
 const bool raw_colors=!category.match_icon_colors||(active&&category.active_icon[0]);
 if(raw_colors)for(auto &copy:raw)parse(&copy);
 if(!normal||(raw_colors&&(!raw[0]||!raw[1]||!raw[2]))){
  destroy_drawable(normal);for(void *copy:raw)destroy_drawable(copy);return false;
 }
 struct Bounds {float x,y,width,height;} bounds{};
 auto drawable_table=*static_cast<void ***>(normal);
 reinterpret_cast<Bounds *(*)(void *,Bounds *)>(drawable_table[0x1a0/sizeof(void *)])(normal,&bounds);
 std::memcpy(bytes+0x380,&bounds,sizeof bounds);
 replace_drawable(reinterpret_cast<void **>(bytes+0x360),normal);
 reinterpret_cast<void (*)(void *,bool)>(image+0xbcf8f0)(button,active);
 if(raw_colors)for(unsigned i=0;i<3;i++)replace_drawable(&copies[i],raw[i]);
 button_icons[button]={&category,category.icon_generation,active,{copies[0],copies[1],copies[2]}};
 return true;
}
static bool button_title(void *button, const char *title) {
 auto slot = reinterpret_cast<void **>(static_cast<unsigned char *>(button) + 0x358);
 if (!readable(slot, sizeof *slot) || !*slot || !readable(*slot, 1)) return false;
 auto old = *slot;
 if (reinterpret_cast<int (*)(const char *, const char *)>(image + 0xe71e00)(static_cast<const char *>(old), title) == 0) return true;
 void *replacement = nullptr;
 native_string::init(image,&replacement,title);
 if (!replacement) return false;
 *slot = replacement;
 if (release_string(old)) return true;
 *slot = old; release_string(replacement); return false;
}
struct BarItem { bool custom, visible; int y; };
static int extension_boundary(const BarItem *items,int count) {
 bool host_seen=false;
 for(int i=0;i<count;++i){if(!items[i].visible)continue;if(items[i].custom){if(host_seen)return i;}else host_seen=true;}
 return -1;
}
#ifdef NATIVE_CATEGORY_TRACE
static unsigned separator_draw_count;
static float separator_last_rect[4]{};
static unsigned separator_last_colour;
#endif
#ifdef NATIVE_CATEGORY_TEST
static void (*test_separator_draw)(void *,int,int,int,int);
#endif
static void draw_extension_separator(void *bar,void *graphics,int width,int y) {
 if(width<=12||y<6)return;
#ifdef NATIVE_CATEGORY_TEST
 if(test_separator_draw){test_separator_draw(graphics,6,y-6,width-12,1);return;}
#endif
 struct RectF {float x,y,width,height;};
 alignas(16) unsigned char transform[0x48]{};RectF source{6.0f,float(y-6),float(width-12),1.0f},scaled{};RectI pixels{};RectF mapped{};
 reinterpret_cast<void*(*)(void*,void*,uint64_t)>(image+0xc0c550)(transform,bar,0);
 reinterpret_cast<void*(*)(void*,void*,void*)>(image+0xc0cbd0)(transform,&scaled,&source);
 int left=(int)std::nearbyint(scaled.x),right=(int)std::nearbyint(scaled.x+scaled.width);
 pixels={left,(int)std::nearbyint(scaled.y),right-left,(std::max)(0,*reinterpret_cast<int*>(transform+4))};
 reinterpret_cast<void*(*)(void*,void*,void*)>(image+0xc0cd00)(transform,&mapped,&pixels);
 unsigned colour=0;std::memcpy(&colour,image+0x178f2d0,sizeof colour);colour=(colour&0x00ffffffu)|0x40000000u;
 reinterpret_cast<void(*)(void*,unsigned)>(image+0xd06280)(graphics,colour);
 void *context=nullptr;std::memcpy(&context,static_cast<unsigned char*>(graphics)+8,sizeof context);auto table=*static_cast<void***>(context);
 reinterpret_cast<void(*)(void*,const RectF*)>(table[0xa8/sizeof(void*)])(context,&mapped);
#ifdef NATIVE_CATEGORY_TRACE
 ++separator_draw_count;std::memcpy(separator_last_rect,&mapped,sizeof mapped);separator_last_colour=colour;
#endif
}
static void bar_paint(void *bar,void *graphics) {
 reinterpret_cast<void(*)(void*,void*)>(image+0xbd0560)(bar,graphics);
 if(!prepared||!bars.count(bar)||!readable(bar,0x250)||!readable(graphics,0x10))return;
 auto bytes=static_cast<unsigned char*>(bar);void **buttons=nullptr,**entries=nullptr;int count=0,entry_count=0,width=0;
 std::memcpy(&buttons,bytes+0x240,sizeof buttons);std::memcpy(&count,bytes+0x24c,4);std::memcpy(&entries,image+0x180aae0,sizeof entries);std::memcpy(&entry_count,image+0x180aaec,4);std::memcpy(&width,bytes+0x40,4);
 if(count!=entry_count||count<1||count>128||!readable(buttons,count*sizeof(void*))||!readable(entries,count*sizeof(void*)))return;
 BarItem items[128]{};for(int i=0;i<count;++i){if(!readable(buttons[i],0xe9))return;auto category=registry_category(entries[i]);items[i].custom=category!=nullptr;items[i].visible=!category||(category->present&&category->visible);std::memcpy(&items[i].y,static_cast<unsigned char*>(buttons[i])+0x3c,4);}
 int index=extension_boundary(items,count);if(index>=0)draw_extension_separator(bar,graphics,width,items[index].y);
}
static void layout_bar(void *bar) {
 auto bytes = static_cast<unsigned char *>(bar);
 if (!readable(bytes, 0x250)) return;
 void **buttons = nullptr, **entries = nullptr;
 int count = 0, entry_count = 0, width = 0, height = 0;
 std::memcpy(&buttons, bytes + 0x240, sizeof buttons);
 std::memcpy(&count, bytes + 0x24c, 4);
 std::memcpy(&entries, image + 0x180aae0, sizeof entries);
 std::memcpy(&entry_count, image + 0x180aaec, 4);
 std::memcpy(&width, bytes + 0x40, 4); std::memcpy(&height, bytes + 0x44, 4);
 if (count != entry_count || count < 1 || count > 128 || width <= 0 || height <= 0 || !readable(buttons, count * sizeof(void *)) || !readable(entries, count * sizeof(void *))) return;
 bars.insert(bar);
 BarItem items[128]{};int active = 0;
 for (int i = 0; i < count; ++i) {
  auto category = registry_category(entries[i]);
  items[i].custom=category!=nullptr;items[i].visible=!category||(category->present&&category->visible);if(items[i].visible)++active;
 }
 int boundary=extension_boundary(items,count);
 int row = (std::min)(48, (std::max)(12, (height - (boundary>=0?40:28)) / (std::max)(1, active))), y = 8;
 using SetVisible = void (*)(void *, bool);
 using SetBounds = void (*)(void *, int, int, int, int);
 auto set_visible = reinterpret_cast<SetVisible>(image + 0xc4e5b0);
 auto set_bounds = reinterpret_cast<SetBounds>(image + 0xc4f6f0);
 void *separator = reinterpret_cast<void *(*)(void *)>(image + 0xbcf460)(bar);
 for (int i = 0; i < count; ++i) {
  if (!readable(buttons[i], 0x360)) return;
  auto category = registry_category(entries[i]);
  bool visible = !category || (category->present && category->visible);
  set_visible(buttons[i], visible);
  if (!bars.count(bar)) return;
  if (!visible) { set_bounds(buttons[i], 0, 0, 0, 0); continue; }
  if (category && !button_title(buttons[i], category->title)) continue;
  if (category && category!=&categories[0]) {
   void *container=nullptr;
   std::memcpy(&container,bytes+0xf8,sizeof container);
   if (readable(container,0xa8)) {
    void *model=reinterpret_cast<void *(*)(void *,void *)>(image+0xbd1620)(container,&category->object);
    if (!model || (readable(model,0x35)&&model_category(model)==category))
     refresh_icon(buttons[i],*category,model&&!static_cast<unsigned char *>(model)[0x34]);
   }
  }
  if (buttons[i] == separator) y += 12;
  if (i == boundary) y += 12;
  set_bounds(buttons[i], 0, y, width, row); y += row;
  if (!bars.count(bar)) return;
 }
}
static void bar_resized(void *bar) {
 reinterpret_cast<void (*)(void *)>(image + 0xbd0af0)(bar);
 if (prepared) layout_bar(bar);
}
static void *bar_destroy(void *bar, unsigned flags) {
 if(readable(bar,0x250)){
  auto bytes=static_cast<unsigned char *>(bar);
  void **buttons=nullptr;int count=0;
  std::memcpy(&buttons,bytes+0x240,sizeof buttons);std::memcpy(&count,bytes+0x24c,4);
  if(count>0&&count<=128&&readable(buttons,count*sizeof(void *)))
   for(int i=0;i<count;i++)button_icons.erase(buttons[i]);
 }
 bars.erase(bar);
 return reinterpret_cast<void *(*)(void *, unsigned)>(image + 0xbd0330)(bar, flags);
}
struct Slot { void **slot; void *expected; void *replacement; };
#ifdef NATIVE_CATEGORY_TEST
static BOOL (*test_protect)(void *, SIZE_T, DWORD, DWORD *);
#endif
static BOOL protect(void *address, SIZE_T length, DWORD mode, DWORD *old) {
#ifdef NATIVE_CATEGORY_TEST
 if (test_protect) return test_protect(address, length, mode, old);
#endif
 return VirtualProtect(address, length, mode, old);
}
static bool patch_slots(Slot *slots, size_t slot_count) {
 for (size_t i = 0; i < slot_count; ++i) if (*slots[i].slot != slots[i].expected) return false;
 SYSTEM_INFO system{}; GetSystemInfo(&system);
 if (!system.dwPageSize) return false;
 struct Page { void *address; DWORD original; } pages[2]{};
 size_t page_count = 0;
 for (size_t s = 0; s < slot_count; ++s) {
  auto address = reinterpret_cast<uintptr_t>(slots[s].slot);
  auto page = reinterpret_cast<void *>(address - address % system.dwPageSize);
  bool found = false;
  for (size_t i = 0; i < page_count; ++i) if (pages[i].address == page) found = true;
  if (found) continue;
  if (page_count == 2) return false;
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(page, &info, sizeof info) || info.State != MEM_COMMIT || (info.Protect & 0xff) != PAGE_READONLY) return false;
#ifdef NATIVE_CATEGORY_TEST
  if (info.Type != MEM_IMAGE && info.Type != MEM_PRIVATE) return false;
#else
  if (info.Type != MEM_IMAGE) return false;
#endif
  pages[page_count++] = {page, info.Protect};
 }
 size_t opened = 0; DWORD previous = 0;
 for (; opened < page_count; ++opened) if (!protect(pages[opened].address, system.dwPageSize, PAGE_READWRITE, &previous)) break;
 if (opened != page_count) {
  while (opened) { --opened; protect(pages[opened].address, system.dwPageSize, pages[opened].original, &previous); }
  return false;
 }
 for (size_t i = 0; i < slot_count; ++i) *slots[i].slot = slots[i].replacement;
 bool restored = true;
 for (size_t i = 0; i < page_count; ++i) if (!protect(pages[i].address, system.dwPageSize, pages[i].original, &previous)) restored = false;
 if (!restored) {
  bool writable = true;
  for (size_t i = 0; i < page_count; ++i) if (!protect(pages[i].address, system.dwPageSize, PAGE_READWRITE, &previous)) writable = false;
  if (writable) for (size_t i = 0; i < slot_count; ++i) *slots[i].slot = slots[i].expected;
  for (size_t i = 0; i < page_count; ++i) protect(pages[i].address, system.dwPageSize, pages[i].original, &previous);
  return false;
 }
 return true;
}
static bool install_lifecycle(unsigned char *base) {
 auto pane = reinterpret_cast<void **>(base + 0x14ec388);
 auto bar = reinterpret_cast<void **>(base + 0x14ea908);
 Slot slots[] = {
  {pane, base + 0xbd8090, reinterpret_cast<void *>(pane_destroy)},
  {pane + 0xf0 / 8, base + 0xbce780, reinterpret_cast<void *>(pane_paint)},
  {pane + 0x138 / 8, base + 0xbd82e0, reinterpret_cast<void *>(pane_resized)},
  {bar, base + 0xbd0330, reinterpret_cast<void *>(bar_destroy)},
  {bar + 0xf0 / 8, base + 0xbd0560, reinterpret_cast<void *>(bar_paint)},
  {bar + 0x138 / 8, base + 0xbd0af0, reinterpret_cast<void *>(bar_resized)}
 };
 return patch_slots(slots, sizeof slots / sizeof slots[0]);
}
static void *name(void *object, void *output) {
 auto category = registry_category(object);
 return native_string::init(image,output,category ? category->title : "Extend");
}
static int position(void *object) {
 auto category = registry_category(object);
 return category ? 1000 + static_cast<int>(category - categories.data()) : 1000;
}
static void *type_name(void *, void *output) {
 using StringInit = void *(*)(void *, const char *);
 return reinterpret_cast<StringInit>(image + 0xe6fac0)(output, "OpenSV2ExtendPanel");
}
static void *registry_for_model(void *object) {
 auto category = model_category(object);
 return category ? &category->object : nullptr;
}
static void *make_model(void *object) {
 auto category = registry_category(object);
 if (!category) return nullptr;
 using Factory = void *(*)(void *);
 void *model = reinterpret_cast<Factory>(image + 0xbd7b50)(image + 0x178f5e8);
 if (model) *static_cast<void ***>(model) = category->model + 1;
 return model;
}
static void *no_destroy(void *object, unsigned) { return object; }

static bool prepare(unsigned char *base, DWORD main_thread, DockUpdate update, DockRemove remove, OwnsChild owns) {
 if (prepared) return base == image && main_thread == thread_id && GetCurrentThreadId() == main_thread && update == dock_update && remove == dock_remove && owns == owns_child;
 prepare_result="unsupported_host";
 if (!base || !main_thread || !update || !remove || !owns || GetCurrentThreadId() != main_thread || !editor_gate::version_matches()) return false;
 auto registry = reinterpret_cast<void **>(base + 0x14ec5b0);
 auto model = reinterpret_cast<void **>(base + 0x14eaca0);
 const size_t expected[] = {0x926a50, 0xbd7870, 0xbd79b0, 0xbd78a0, 0xbd7b50};
 prepare_result="registry_type";if (*reinterpret_cast<void ***>(base + 0x178f5e8) != registry) return false;
 prepare_result="registry_vtable";
 for (size_t i = 0; i < 5; ++i) if (registry[i] != base + expected[i]) return false;
 prepare_result="descriptor_vtable";if (model[0xa0 / 8] != base + 0xbd1cf0 || model[0xb8 / 8] != base + 0xbd7bb0) return false;
 constexpr unsigned char factory_signature[] = {0x40,0x53,0x48,0x83,0xec,0x20,0xb9,0x50,0x00,0x00,0x00};
 prepare_result="factory_signature";
 if (std::memcmp(base + 0xbd7b50, factory_signature, sizeof factory_signature)) return false;
 constexpr unsigned char content_signature[] = {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
 constexpr unsigned char height_signature[] = {0xb8,0x2c,0,0,0,0xc3};
 constexpr unsigned char transform_signature[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x4c,0x89,0x40,0x18};
 constexpr unsigned char scale_signature[] = {0x48,0x83,0xec,0x58,0xf3,0x0f,0x10,0x49,0x14};
 constexpr unsigned char unscale_signature[] = {0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x61,0x24};
 constexpr unsigned char colour_signature[] = {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x50};
 prepare_result="separator_signature";if(std::memcmp(base+0xc0c550,transform_signature,sizeof transform_signature)||std::memcmp(base+0xc0cbd0,scale_signature,sizeof scale_signature)||std::memcmp(base+0xc0cd00,unscale_signature,sizeof unscale_signature)||std::memcmp(base+0xd06280,colour_signature,sizeof colour_signature))return false;
 prepare_result="geometry_signature";
 if (std::memcmp(base + 0xbcf2a0, content_signature, sizeof content_signature) || std::memcmp(base + 0x8ec3f0, height_signature, sizeof height_signature)) return false;
 constexpr unsigned char parse_signature[] = {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10};
 prepare_result="drawable_signature";if (std::memcmp(base + 0xe588e0, parse_signature, sizeof parse_signature) || std::memcmp(base + 0xc68000, parse_signature, sizeof parse_signature)) return false;
 constexpr unsigned char icon_refresh_signature[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x56,0x57,0x41,0x56};
 prepare_result="icon_refresh_signature";if(std::memcmp(base+0xbcf8f0,icon_refresh_signature,sizeof icon_refresh_signature))return false;
 image = base;
 thread_id = main_thread;
 dock_update = update;
 dock_remove = remove;
 owns_child = owns;
 prepare_result="lifecycle_slots";if (!install_lifecycle(base)) return false;
 for (unsigned i = 0; i < capacity; ++i) {
  auto &category = categories[i];
  std::memcpy(category.registry, registry - 1, sizeof category.registry);
  std::memcpy(category.model, model - 1, sizeof category.model);
  category.registry[1] = reinterpret_cast<void *>(no_destroy);
  category.registry[2] = reinterpret_cast<void *>(name);
  category.registry[3] = reinterpret_cast<void *>(icon);
  category.registry[4] = reinterpret_cast<void *>(position);
  category.registry[5] = reinterpret_cast<void *>(make_model);
  category.model[1 + 0xa0 / 8] = reinterpret_cast<void *>(type_name);
  category.model[1 + 0xb8 / 8] = reinterpret_cast<void *>(registry_for_model);
  category.object = category.registry + 1;
 }
 std::strcpy(categories[0].title, "Extend");
 std::strcpy(categories[1].id, "sv2.mcp");
 categories[0].present = categories[0].visible = true;
 using Insert = void (*)(void *, void *, void *);
 auto insert = reinterpret_cast<Insert>(base + 0xbd24b0);
 for (auto &category : categories) insert(base + 0x180aae0, nullptr, &category.object);
 prepared = true;
 prepare_result="ready";
 return true;
}
static bool set_open(Category &category, bool want_open) {
 using Find = void *(*)(void *, void *);
 using Toggle = void (*)(void *, void *, void *);
 for (void *bar : bars) {
  auto bytes = static_cast<unsigned char *>(bar);
  if (!readable(bytes, 0x100)) continue;
  void *table = nullptr; std::memcpy(&table, bytes, sizeof table);
  if (table != image + 0x14ea908 || !(bytes[0xe8] & 2)) continue;
  void *container = nullptr;
  std::memcpy(&container, bytes + 0xf8, sizeof container);
  if (!readable(container, 0xa8)) continue;
  auto find = reinterpret_cast<Find>(image + 0xbd1620);
  void *model = find(container, &category.object);
  if (model && (!readable(model, 0x35) || model_category(model) != &category)) return false;
  bool is_open = model && !static_cast<unsigned char *>(model)[0x34];
  if (is_open == want_open) return true;
  void *reason = nullptr;
  reinterpret_cast<void *(*)(void *, const char *)>(image + 0xe6fac0)(&reason, "user");
  if (!reason) return false;
  reinterpret_cast<Toggle>(image + 0xbd16f0)(container, &category.object, &reason);
  bool released = release_string(reason);
  if (!released || !bars.count(bar) || !readable(container, 0xa8)) return false;
  model = find(container, &category.object);
  if (model && (!readable(model, 0x35) || model_category(model) != &category)) return false;
  return (model && !static_cast<unsigned char *>(model)[0x34]) == want_open;
 }
 return false;
}
static int open_category(const char *id) {
 if (!prepared || GetCurrentThreadId() != thread_id || !id) return -1;
 for (auto &category : categories) {
  if (category.present && category.visible && std::strcmp(category.id, id) == 0)
   return set_open(category, true) ? 0 : -1;
 }
 return -1;
}
static int close_category(const char *id) {
 if (!prepared || GetCurrentThreadId() != thread_id || !id) return -1;
 for (auto &category : categories) {
  if (category.present && std::strcmp(category.id, id) == 0)
   return set_open(category, false) ? 0 : -1;
 }
 return -1;
}
static int category_open(const char *id, bool *result) {
 if (!prepared || GetCurrentThreadId() != thread_id || !id || !result) return -1;
 Category *category = nullptr;
 for (auto &item : categories) if (item.present && std::strcmp(item.id, id) == 0) { category = &item; break; }
 if (!category) return -1;
 for (void *bar : bars) {
  auto bytes = static_cast<unsigned char *>(bar);
  if (!readable(bytes, 0x100)) continue;
  void *table = nullptr, *container = nullptr;
  std::memcpy(&table, bytes, sizeof table); std::memcpy(&container, bytes + 0xf8, sizeof container);
  if (table != image + 0x14ea908 || !readable(container, 0xa8)) continue;
  void *model = reinterpret_cast<void *(*)(void *, void *)>(image + 0xbd1620)(container, &category->object);
  if (model && (!readable(model, 0x35) || model_category(model) != category)) return -1;
  *result = model && !static_cast<unsigned char *>(model)[0x34]; return 0;
 }
 return -1;
}
static void tick();
static bool sync(const Entry *entries, size_t count, bool root_visible) {
 if (!prepared || GetCurrentThreadId() != thread_id || count >= capacity || (count && !entries)) return false;
 std::array<int, capacity> mapping{};
 std::array<bool, capacity> claimed{};
 claimed[0] = true;
 for (size_t i = 0; i < count; ++i) {
  auto &entry = entries[i];
  if (!entry.id || !*entry.id || !entry.title || !*entry.title || std::strlen(entry.id) >= sizeof categories[0].id || std::strlen(entry.title) >= sizeof categories[0].title) return false;
  for (size_t j = 0; j < i; ++j) if (!std::strcmp(entry.id, entries[j].id)) return false;
  for (unsigned j = 1; j < capacity; ++j) if (!std::strcmp(entry.id, categories[j].id)) { mapping[i] = static_cast<int>(j); claimed[j] = true; break; }
 }
 for (size_t i = 0; i < count; ++i) if (!mapping[i]) {
  for (unsigned j = 1; j < capacity; ++j) {
   if (claimed[j]) continue;
   bool live = false;
   for (void *pane : panes) if (pane_category(pane) == &categories[j]) { live = true; break; }
   bool retained = false;
   for (size_t k = 0; k < count; ++k) if (!std::strcmp(entries[k].id, categories[j].id)) { retained = true; break; }
   if (!live && !retained) { mapping[i] = static_cast<int>(j); claimed[j] = true; break; }
  }
  if (!mapping[i]) return false;
 }
 if (categories[0].visible && !root_visible && !bars.empty() && close_category(categories[0].id)!=0) return false;
 for (unsigned j = 1; j < capacity; ++j) {
  auto &category = categories[j];
  bool remain_visible = false;
  for (size_t i = 0; i < count; ++i) if (mapping[i] == static_cast<int>(j) && !std::strcmp(category.id, entries[i].id)) { remain_visible = entries[i].visible; break; }
  if (category.present && category.visible && !remain_visible && !bars.empty() && close_category(category.id)!=0) return false;
 }
 for (size_t i = 0; i < count; ++i) {
  auto &category = categories[mapping[i]];
  if (!std::strcmp(category.id, entries[i].id)) continue;
  for (void *pane : panes) if (pane_category(pane) == &category) return false;
 }
 categories[0].visible = root_visible;
 for (unsigned j = 1; j < capacity; ++j) categories[j].present = false;
 for (size_t i = 0; i < count; ++i) {
  auto &category = categories[mapping[i]];
  if(std::strcmp(category.id,entries[i].id)||std::strcmp(category.icon,entries[i].icon?entries[i].icon:"")||
     std::strcmp(category.active_icon,entries[i].active_icon?entries[i].active_icon:"")||
     category.match_icon_colors!=entries[i].match_icon_colors)++category.icon_generation;
  std::strcpy(category.id, entries[i].id); std::strcpy(category.title, entries[i].title);
  std::snprintf(category.icon, sizeof category.icon, "%s", entries[i].icon ? entries[i].icon : "");
  std::snprintf(category.active_icon, sizeof category.active_icon, "%s", entries[i].active_icon ? entries[i].active_icon : "");
  category.match_icon_colors=entries[i].match_icon_colors;
  category.present = true; category.visible = entries[i].visible;
 }
 tick();
 if (host_window) RedrawWindow(host_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
 return true;
}
struct Status { bool ready; unsigned toolbar_count; unsigned pane_count; };
static Status status() {
 if (!prepared || GetCurrentThreadId() != thread_id) return {false, 0, 0};
 return {true, static_cast<unsigned>(bars.size()), static_cast<unsigned>(panes.size())};
}
static void set_host(HWND window) {
 if (!prepared || GetCurrentThreadId() != thread_id || (window && !IsWindow(window))) return;
 if (!window) {
  host_window = nullptr;
  std::vector<void *> snapshot(panes.begin(), panes.end());
  for (void *pane : snapshot) if (panes.count(pane) && dock_remove) dock_remove(pane);
  panes.clear(); bars.clear();
 } else host_window = window;
}
static void tick() {
 if (!prepared || GetCurrentThreadId() != thread_id) return;
 std::vector<void *> bar_snapshot(bars.begin(), bars.end());
 std::vector<void *> pane_snapshot(panes.begin(), panes.end());
 for (void *bar : bar_snapshot) if (bars.count(bar)) layout_bar(bar);
 for (void *pane : pane_snapshot) if (panes.count(pane)) update_pane(pane);
}
}
