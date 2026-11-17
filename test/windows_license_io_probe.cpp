#include <cassert>
#include <filesystem>
#include <fstream>
#include "windows_license_io_probe.h"
int wmain(int argc,wchar_t **argv){
 assert(argc==2);namespace fs=std::filesystem;fs::path root=fs::absolute(argv[1]),dir=root/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"license";fs::create_directories(dir);
 {std::ofstream output(dir/L"session",std::ios::binary);output<<"fixture";}{std::ofstream output(root/L"outside",std::ios::binary);output<<"fixture";}
 assert(MH_Initialize()==MH_OK&&license_io_probe::install()&&license_io_probe::arm(root.wstring(),GetCurrentThreadId()));
 HANDLE file=CreateFileW((dir/L"session").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(file!=INVALID_HANDLE_VALUE&&CloseHandle(file));
 assert(GetFileAttributesW((dir/L"session").c_str())!=INVALID_FILE_ATTRIBUTES);WIN32_FILE_ATTRIBUTE_DATA attributes{};assert(GetFileAttributesExW((dir/L"session").c_str(),GetFileExInfoStandard,&attributes));
 WIN32_FIND_DATAW found{};HANDLE search=FindFirstFileExW((dir/L"*").c_str(),FindExInfoStandard,&found,FindExSearchNameMatch,nullptr,0);assert(search!=INVALID_HANDLE_VALUE&&FindClose(search));search=FindFirstFileW((dir/L"*").c_str(),&found);assert(search!=INVALID_HANDLE_VALUE&&FindClose(search));
 file=CreateFileW((root/L"outside").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(file!=INVALID_HANDLE_VALUE&&CloseHandle(file));
 file=CreateFileW((dir/L"next").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);assert(file!=INVALID_HANDLE_VALUE&&CloseHandle(file));
 assert(MoveFileW((dir/L"next").c_str(),(dir/L"staged").c_str()));assert(MoveFileExW((dir/L"staged").c_str(),(dir/L"next").c_str(),0));
 assert(ReplaceFileW((dir/L"session").c_str(),(dir/L"next").c_str(),(dir/L"backup").c_str(),0,nullptr,nullptr));assert(DeleteFileW((dir/L"backup").c_str()));
 auto report=license_io_probe::finish();assert(report["ok"]&&report["dropped"]==0&&report["openFileHandles"]==0&&report["openFindHandles"]==0);
 bool opened=false,closed=false,enumerated=false,closedSearch=false,attrs=false,moved=false,movedEx=false,replaced=false,deleted=false;
 for(auto &event:report["events"]){auto op=event["operation"].get<std::string>();assert(event.value("path",std::string())!="outside");if(op=="CreateFileW"&&event.value("path",std::string())=="session")opened=true;if(op=="CloseHandle"&&event.value("path",std::string())=="session")closed=true;if(op=="FindFirstFileExW")enumerated=true;if(op=="FindClose")closedSearch=true;if(op=="GetFileAttributesExW")attrs=true;if(op=="MoveFileW")moved=true;if(op=="MoveFileExW")movedEx=true;if(op=="ReplaceFileW")replaced=true;if(op=="DeleteFileW")deleted=true;}
 assert(opened&&closed&&enumerated&&closedSearch&&attrs&&moved&&movedEx&&replaced&&deleted);return 0;
}
