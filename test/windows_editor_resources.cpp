#define _WIN32_WINNT 0x0602
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "../loader/windows/host_sandbox.h"
int wmain(int argc,wchar_t **argv){
 if(argc!=2)return 64;namespace fs=std::filesystem;fs::path root=fs::absolute(argv[1]);fs::create_directories(root/L"native"/L"EditorLua");auto source=root/L"native"/L"EditorLua";
 for(auto name:{L"sv2_extend.lua",L"sv2_extend_json.lua",L"editor_provider.lua"}){std::ofstream file(source/name);file<<"return {}\n";}
 {std::ofstream file(source/L"windows_runtime.lua");file<<"function getClientInfo() return {name='SV2 Extend Runtime'} end\n";}
 host_sandbox::editor_resources(root/L"native",root/L"data");
 auto modules=root/L"data"/L"Local"/L"SV2Extend"/L"EditorLua";
 {std::ofstream file(modules/L"owner.txt");file<<"sv2-extend\n";}
 host_sandbox::editor_resources(root/L"native",root/L"data");
 std::cout<<"PASS: editor resources reinstall with Windows text marker\n";return 0;
}
