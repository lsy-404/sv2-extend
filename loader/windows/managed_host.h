#pragma once
#include <shlobj.h>
#include <fstream>
#include <random>
#include <stdexcept>
#include "network_bootstrap.h"
#include "executable_integrity.h"

namespace managed_host {
namespace fs=std::filesystem;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static bool safe(const fs::path &path){DWORD a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static void inspect(const fs::path &directory){require(safe(directory),"Linked or inaccessible source directory");for(const auto &item:fs::recursive_directory_iterator(directory))require(safe(item.path()),"Linked or inaccessible source entry");}
static bool same_file(const fs::path &left,const fs::path &right){WIN32_FILE_ATTRIBUTE_DATA a{},b{};return GetFileAttributesExW(left.c_str(),GetFileExInfoStandard,&a)&&GetFileAttributesExW(right.c_str(),GetFileExInfoStandard,&b)&&a.nFileSizeHigh==b.nFileSizeHigh&&a.nFileSizeLow==b.nFileSizeLow&&CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime)==0;}
static void stamp_file(const fs::path &source,const fs::path &target){WIN32_FILE_ATTRIBUTE_DATA info{};require(GetFileAttributesExW(source.c_str(),GetFileExInfoStandard,&info)!=FALSE,"Cannot inspect source timestamp");HANDLE file=CreateFileW(target.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);require(file!=INVALID_HANDLE_VALUE,"Cannot preserve file timestamp");BOOL stamped=SetFileTime(file,nullptr,nullptr,&info.ftLastWriteTime);CloseHandle(file);require(stamped!=FALSE,"Cannot preserve file timestamp");}
static bool same_tree(const fs::path &source,const fs::path &target){
 if(!fs::is_directory(target))return false;
 auto matches=[](const fs::path &left,const fs::path &right){for(const auto &item:fs::recursive_directory_iterator(left)){auto other=right/item.path().lexically_relative(left);if(!safe(other))return false;if(item.is_directory()){if(!fs::is_directory(other))return false;}else if(!item.is_regular_file()||!fs::is_regular_file(other)||!same_file(item.path(),other))return false;}return true;};
 return matches(source,target)&&matches(target,source);
}
static std::wstring identifier(){std::random_device random;const wchar_t alphabet[]=L"0123456789abcdef";std::wstring value=L"SV2Extend.";for(int i=0;i<32;i++)value+=alphabet[random()&15];return value;}
static fs::path roaming(){PWSTR path=nullptr;require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&path)),"Cannot locate official user data");fs::path result(path);CoTaskMemFree(path);return result;}
static fs::path common_data(){PWSTR path=nullptr;require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData,0,nullptr,&path)),"Cannot locate shared application data");fs::path result(path);CoTaskMemFree(path);return result;}
static void common_resources(const fs::path &data){
 auto source=common_data()/L"Dreamtonics"/L"Synthesizer V Studio 2";auto destination=data/L"ProgramData"/L"Dreamtonics"/L"Synthesizer V Studio 2";
 if(fs::exists(source)&&!fs::exists(destination)){inspect(source);fs::create_directories(destination.parent_path());auto staging=destination.parent_path()/(L".shared-"+identifier());fs::copy(source,staging,fs::copy_options::recursive);fs::rename(staging,destination);}
}
static std::string contents(const fs::path &path){std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot read editor resource");return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
static bool modules_owned(const fs::path &directory){std::ifstream file(directory/L"owner.txt");std::string marker;std::getline(file,marker);return fs::is_regular_file(directory/L"owner.txt")&&marker=="sv2-extend"&&file.peek()==std::char_traits<char>::eof();}
static void editor_resources(const fs::path &native,const fs::path &data){
 auto resources=native/L"EditorLua";inspect(resources);auto scripts=data/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"scripts";fs::create_directories(scripts);auto local=data/L"Local"/L"SV2Extend";fs::create_directories(local);auto target=local/L"EditorLua";auto owner=target/L"owner.txt";
 auto previous=scripts/L".sv2-extend-lua";if(fs::exists(previous)){inspect(previous);require(modules_owned(previous),"Existing script module directory is not Loader-owned");fs::rename(previous,local/(L"EditorLua.previous."+identifier()));}
 if(fs::exists(target)){require(modules_owned(target),"Existing script module directory is not Loader-owned");}else{fs::create_directory(target);std::ofstream file(owner,std::ios::binary);file<<"sv2-extend\n";require(bool(file),"Cannot mark editor resource directory");}
 auto entry=scripts/L"opensv2-loader-runtime.lua";auto next=contents(resources/L"windows_runtime.lua");if(fs::exists(entry))require(contents(entry).find("name='SV2 Extend Runtime'")!=std::string::npos,"Existing script entry is not Loader-owned");
 for(const auto *name:{L"sv2_extend.lua",L"sv2_extend_json.lua",L"editor_provider.lua"})require(CopyFileW((resources/name).c_str(),(target/name).c_str(),FALSE)!=FALSE,"Cannot install editor module");
 require(CopyFileW((resources/L"windows_runtime.lua").c_str(),entry.c_str(),FALSE)!=FALSE,"Cannot install editor runtime entry");
}
struct State {
 HANDLE lock=INVALID_HANDLE_VALUE,job=nullptr,job_query=nullptr;LPPROC_THREAD_ATTRIBUTE_LIST attributes=nullptr;std::wstring name;fs::path host,data;bool copied=false;
 HANDLE inherited[3]{};windows_network_bootstrap::Handoff network_handoff;
 ~State(){if(attributes){DeleteProcThreadAttributeList(attributes);HeapFree(GetProcessHeap(),0,attributes);}if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);if(job_query)CloseHandle(job_query);if(job)CloseHandle(job);}
 void prepare(const fs::path &original,const fs::path &profile,const fs::path &core,bool official,bool clean){
  inspect(original.parent_path());require(safe(profile),"Profile cannot be a link");inspect(profile);
  auto name_file=profile/L"container-name.txt";
  if(clean)require(!fs::exists(name_file),"Clean profile has already been initialized");
  if(clean||!fs::exists(name_file))for(const auto &item:fs::directory_iterator(profile))require(item.path().filename()==L"plugins","Use an empty Loader profile or an existing Loader-owned profile");
  SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};lock=CreateFileW((profile/L".host.lock").c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,&security,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);require(lock!=INVALID_HANDLE_VALUE,"The Loader profile is already in use");
  if(clean){
   require(!fs::exists(name_file),"Clean profile has already been initialized");
   for(const auto &item:fs::directory_iterator(profile))require(item.path().filename()==L"plugins"||item.path().filename()==L".host.lock","Use an empty Loader profile");
  }
  if(fs::exists(name_file)){std::wifstream stream(name_file);stream>>name;}else{name=identifier();std::wofstream stream(name_file);stream<<name;require(bool(stream),"Cannot save profile identity");}
  require(name.size()==42&&name.substr(0,10)==L"SV2Extend."&&name.find_first_not_of(L"0123456789abcdef",10)==std::wstring::npos,"Invalid profile identity");
  data=profile/L"host-data";fs::create_directories(data/L"ProgramData");fs::create_directories(data/L"Roaming");fs::create_directories(data/L"Local");fs::create_directories(data/L"LocalLow");fs::create_directories(data/L"Temp");
  auto destination=data/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2";auto source=roaming()/L"Dreamtonics"/L"Synthesizer V Studio 2";
  if(official&&!clean&&fs::exists(source)&&!fs::exists(destination)){inspect(source);fs::create_directories(destination.parent_path());auto staging=destination.parent_path()/(L".migration-"+identifier());fs::copy(source,staging,fs::copy_options::recursive);fs::rename(staging,destination);copied=true;}
  if(official){common_resources(data);editor_resources(core.parent_path(),data);}
  auto runtime=profile/L"runtime"/L"Synthesizer V Studio 2 Pro";
  bool update=!same_tree(original.parent_path(),runtime);
  if(update){fs::create_directories(runtime.parent_path());auto staging=runtime.parent_path()/(L".host-"+identifier());fs::copy(original.parent_path(),staging,fs::copy_options::recursive);for(const auto &item:fs::recursive_directory_iterator(original.parent_path()))if(item.is_regular_file())stamp_file(item.path(),staging/item.path().lexically_relative(original.parent_path()));if(fs::exists(runtime))fs::rename(runtime,runtime.parent_path()/(L".previous-"+identifier()));fs::rename(staging,runtime);}
  inspect(runtime);host=runtime/original.filename();require(fs::is_regular_file(host),"Managed host missing");require(executable_integrity::medium(host.wstring()),"Cannot set managed host integrity");
  for(const auto *name:{L"outputs",L"preferences",L"storage"})fs::create_directories(profile/name);
  job=CreateJobObjectW(nullptr,nullptr);require(job!=nullptr,"Cannot create managed process job");require(DuplicateHandle(GetCurrentProcess(),job,GetCurrentProcess(),&job_query,JOB_OBJECT_QUERY,TRUE,0)!=FALSE,"Cannot create job query handle");require(network_handoff.create(),"Cannot create network startup handoff");inherited[0]=lock;inherited[1]=job_query;inherited[2]=network_handoff.read_handle();SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);attributes=(LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(),0,size);require(attributes&&InitializeProcThreadAttributeList(attributes,1,0,&size)&&UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr),"Cannot initialize managed startup");
 }
};
}
