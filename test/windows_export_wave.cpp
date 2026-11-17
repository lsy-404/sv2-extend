#include "../loader/windows/export_wave.h"
#include <cassert>
#include <fstream>
#include <iostream>
static void write_file(const std::filesystem::path &path,const std::vector<unsigned char> &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(f.good());}
int main(){
 wchar_t tmp[MAX_PATH];assert(GetTempPathW(MAX_PATH,tmp));auto root=std::filesystem::path(tmp)/(L"sv2-export-wave-test-"+std::to_wstring(GetCurrentProcessId()));assert(!std::filesystem::exists(root));assert(std::filesystem::create_directory(root));
 std::vector<unsigned char> wav={'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',4,0,0,0,1,0,2,0};
 auto stage=root/L"render.wav",output=root/L"output.wav";export_wave::Audio audio{};write_file(stage,wav);
 assert(export_wave::publish(stage,output,2,44100,16,audio)==export_wave::Result::format_mismatch);assert(std::filesystem::exists(stage));
 write_file(output,{9});assert(export_wave::publish(stage,output,1,44100,16,audio)==export_wave::Result::output_exists);assert(std::filesystem::file_size(output)==1&&std::filesystem::exists(stage));assert(DeleteFileW(output.c_str()));
 assert(export_wave::publish(stage,output,1,44100,16,audio)==export_wave::Result::ok);assert(audio.audio_bytes==4&&audio.channels==1&&audio.rate==44100&&audio.bits==16);assert(!std::filesystem::exists(stage));
 wav[40]=100;write_file(stage,wav);assert(export_wave::publish(stage,root/L"bad.wav",1,44100,16,audio)==export_wave::Result::invalid_output);assert(!std::filesystem::exists(root/L"bad.wav"));
 assert(DeleteFileW(stage.c_str()));assert(DeleteFileW(output.c_str()));assert(RemoveDirectoryW(root.c_str()));std::cout<<"WAV validation and publication passed\n";
}
