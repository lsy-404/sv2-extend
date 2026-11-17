#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace windows_network {
inline bool canonical_executable_path(const std::wstring &path,std::wstring &result){
 result.clear();if(path.empty())return false;
 HANDLE file=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(file,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){CloseHandle(file);return false;}
 DWORD length=GetFinalPathNameByHandleW(file,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);if(!length){CloseHandle(file);return false;}
 std::vector<wchar_t> buffer(size_t(length)+1);DWORD written=GetFinalPathNameByHandleW(file,buffer.data(),static_cast<DWORD>(buffer.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);CloseHandle(file);
 if(!written||written>=buffer.size())return false;std::wstring value(buffer.data(),written);
 if(value.rfind(L"\\\\?\\UNC\\",0)==0)value=L"\\\\"+value.substr(8);
 else if(value.rfind(L"\\\\?\\",0)==0)value=value.substr(4);
 else return false;
 result=std::move(value);return true;
}
}
