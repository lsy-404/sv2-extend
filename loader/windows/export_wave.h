#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace export_wave {
struct Audio {uint16_t channels=0,bits=0;uint32_t rate=0;uint64_t audio_bytes=0,file_bytes=0;};
inline uint16_t u16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline uint32_t u32(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline bool exact(HANDLE file,void *data,DWORD n){DWORD got=0;return ReadFile(file,data,n,&got,nullptr)&&got==n;}
inline bool seek(HANDLE file,uint64_t offset){if(offset>INT64_MAX)return false;LARGE_INTEGER n{};n.QuadPart=static_cast<LONGLONG>(offset);return SetFilePointerEx(file,n,nullptr,FILE_BEGIN)!=FALSE;}
inline bool inspect(HANDLE file,Audio &audio){
 BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(file,&info)||info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)||info.nNumberOfLinks!=1||GetFileType(file)!=FILE_TYPE_DISK)return false;
 uint64_t size=uint64_t(info.nFileSizeHigh)<<32|info.nFileSizeLow;if(size<12||!seek(file,0))return false;std::array<uint8_t,12> head{};if(!exact(file,head.data(),12)||memcmp(head.data(),"RIFF",4)||memcmp(head.data()+8,"WAVE",4))return false;
 uint64_t declared=uint64_t(u32(head.data()+4))+8;if(declared>size||declared<12)return false;uint64_t pos=12;bool format=false,data=false;Audio result{};
 for(unsigned i=0;i<256&&pos+8<=declared;++i){std::array<uint8_t,8> chunk{};if(!seek(file,pos)||!exact(file,chunk.data(),8))return false;uint32_t length=u32(chunk.data()+4);uint64_t start=pos+8,end=start+length;if(end>declared||end<start)return false;
  if(!memcmp(chunk.data(),"fmt ",4)){if(length<16||length>4096||format)return false;std::vector<uint8_t> fmt(length);if(!exact(file,fmt.data(),length))return false;auto type=u16(fmt.data());result.channels=u16(fmt.data()+2);result.rate=u32(fmt.data()+4);result.bits=u16(fmt.data()+14);if(type==0xfffe){const uint8_t tail[]={0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};if(length<40||u16(fmt.data()+16)<22||u16(fmt.data()+18)!=result.bits||memcmp(fmt.data()+28,tail,sizeof tail))return false;auto subtype=u32(fmt.data()+24);if(subtype!=1&&subtype!=3)return false;type=static_cast<uint16_t>(subtype);}if((type!=1&&type!=3)||!result.channels||result.channels>2||!result.rate||!result.bits||result.bits%8||u16(fmt.data()+12)!=result.channels*(result.bits/8))return false;format=true;}
  else if(!memcmp(chunk.data(),"data",4)){if(data||!length)return false;result.audio_bytes=length;data=true;}
  if(format&&data){auto frame=uint64_t(result.channels)*(result.bits/8);if(!frame||result.audio_bytes%frame)return false;result.file_bytes=size;audio=result;return true;}pos=end+(length&1);if(pos>end&&pos>declared)return false;
 }return false;
}
enum class Result {ok,invalid_output,format_mismatch,output_exists,publish_failed};
inline Result publish(const std::filesystem::path &staging,const std::filesystem::path &destination,uint16_t channels,uint32_t rate,uint16_t bits,Audio &audio){
 HANDLE h=CreateFileW(staging.c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h==INVALID_HANDLE_VALUE)return Result::invalid_output;
 Audio value{};Result result=Result::invalid_output;if(inspect(h,value)){if(value.channels!=channels||value.rate!=rate||value.bits!=bits)result=Result::format_mismatch;else{auto path=destination.wstring();size_t bytes=sizeof(FILE_RENAME_INFO)+path.size()*sizeof(wchar_t);std::vector<uint8_t> buffer(bytes);auto rename=reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());rename->ReplaceIfExists=FALSE;rename->RootDirectory=nullptr;rename->FileNameLength=static_cast<DWORD>(path.size()*sizeof(wchar_t));memcpy(rename->FileName,path.data(),rename->FileNameLength);if(SetFileInformationByHandle(h,FileRenameInfo,rename,static_cast<DWORD>(bytes))){audio=value;result=Result::ok;}else {auto error=GetLastError();result=error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS?Result::output_exists:Result::publish_failed;}}}
 CloseHandle(h);return result;
}
}
