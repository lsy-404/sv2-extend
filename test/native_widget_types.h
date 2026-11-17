#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace native_widget_types {
struct Type { std::string name; uint32_t vtable_rva; unsigned count; };
struct Result { bool ok=false, truncated=false; unsigned visited=0; std::vector<Type> types; };
static bool readable(const void *address,size_t size) {
 if(!address||!size)return false;
 auto start=reinterpret_cast<uintptr_t>(address);if(start>UINTPTR_MAX-size)return false;
 auto end=start+size;
 while(start<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(start),&page,sizeof page)||page.State!=MEM_COMMIT||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;DWORD protection=page.Protect&0xff;if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
  auto base=reinterpret_cast<uintptr_t>(page.BaseAddress);if(base>start||page.RegionSize>UINTPTR_MAX-base||base+page.RegionSize<=start)return false;start=(std::min)(end,base+page.RegionSize);}
 return true;
}
static bool image_range(unsigned char *base,size_t image_size,size_t offset,size_t length) {
 if(offset>image_size||length>image_size-offset||!readable(base+offset,length))return false;
 auto cursor=reinterpret_cast<uintptr_t>(base+offset),end=cursor+length;
 while(cursor<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(cursor),&page,sizeof page)||page.Type!=MEM_IMAGE||page.AllocationBase!=base)return false;auto start=reinterpret_cast<uintptr_t>(page.BaseAddress);if(start>cursor||page.RegionSize>UINTPTR_MAX-start||start+page.RegionSize<=cursor)return false;cursor=(std::min)(end,start+page.RegionSize);}
 return true;
}
static Result scan(void *pane,unsigned char *base,DWORD thread) {
 Result result;if(!pane||!base||!thread||GetCurrentThreadId()!=thread)return result;
 if(!readable(base,sizeof(IMAGE_DOS_HEADER)))return result;auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x100000)return result;
 auto nt=base+dos->e_lfanew;if(!readable(nt,sizeof(IMAGE_NT_HEADERS64)))return result;auto headers=reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
 if(headers->Signature!=IMAGE_NT_SIGNATURE||headers->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return result;
 size_t image_size=headers->OptionalHeader.SizeOfImage;if(image_size<0x1000||!image_range(base,image_size,0,sizeof(IMAGE_DOS_HEADER))||!image_range(base,image_size,(size_t)dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64)))return result;
 void *root=pane;std::set<void*> ancestors;
 for(unsigned depth=0;;++depth){if(depth>=128||!ancestors.insert(root).second||!readable(root,0x38))return result;void *parent=nullptr;std::memcpy(&parent,static_cast<unsigned char*>(root)+0x30,sizeof parent);if(!parent)break;root=parent;}
 struct Node {void *object;unsigned depth;};std::vector<Node> pending{{root,0}};std::set<void*> seen;std::map<std::pair<std::string,uint32_t>,unsigned> counts;
 while(!pending.empty()){
  auto node=pending.back();pending.pop_back();if(!seen.insert(node.object).second)continue;if(++result.visited>4000){result.visited=4000;result.truncated=true;break;}
  if(node.depth>=128){result.truncated=true;continue;}if(!readable(node.object,0x68)){result.truncated=true;continue;}
  auto object=static_cast<unsigned char*>(node.object);void *table=nullptr;std::memcpy(&table,object,sizeof table);
  auto table_addr=reinterpret_cast<uintptr_t>(table),base_addr=reinterpret_cast<uintptr_t>(base);
  if(table_addr>=base_addr+8&&table_addr-base_addr<image_size){size_t table_offset=table_addr-base_addr;
   if(image_range(base,image_size,table_offset-8,16)){void *locator=nullptr;std::memcpy(&locator,base+table_offset-8,sizeof locator);auto loc=reinterpret_cast<uintptr_t>(locator);
    if(loc>=base_addr&&loc-base_addr<image_size){size_t col=loc-base_addr;
     if(image_range(base,image_size,col,24)){uint32_t signature=0,type=0,self=0;std::memcpy(&signature,base+col,4);std::memcpy(&type,base+col+12,4);std::memcpy(&self,base+col+20,4);
      if(signature==1&&self==col&&type<image_size&&image_range(base,image_size,type,17)){std::string name;size_t at=(size_t)type+16;bool complete=false;
       for(size_t i=0;i<192&&at+i<image_size;++i){if(!image_range(base,image_size,at+i,1))break;char c=static_cast<char>(base[at+i]);if(!c){complete=true;break;}if(c<32||c>126)break;name.push_back(c);}
       if(complete&&(name.find("Label")!=std::string::npos||name.find("Viewport")!=std::string::npos||name.find("ComboBox")!=std::string::npos||name.find("TextEditor")!=std::string::npos))++counts[{name,(uint32_t)table_offset}];
      }
     }
    }
   }
  }
  void **children=nullptr;int count=0;std::memcpy(&children,object+0x58,sizeof children);std::memcpy(&count,object+0x64,sizeof count);
  if(count<0||count>4000||(count&&!readable(children,(size_t)count*sizeof(void*)))){result.truncated=true;continue;}
  for(int i=0;i<count;++i){void *child=nullptr;std::memcpy(&child,children+i,sizeof child);if(child&&!seen.count(child)){if(pending.size()+result.visited>=4000){result.truncated=true;break;}pending.push_back({child,node.depth+1});}}
 }
 for(const auto &entry:counts)result.types.push_back({entry.first.first,entry.first.second,entry.second});result.ok=true;return result;
}
}
