#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <vector>
#include <limits>
#include <cstddef>

namespace network_scope {
struct Job {
 bool child_scope=false,self_present=false;DWORD error=ERROR_SUCCESS,child_count=0;
};
struct Network {
 bool complete=false;DWORD tcp4_error=ERROR_SUCCESS,tcp6_error=ERROR_SUCCESS,udp4_error=ERROR_SUCCESS,udp6_error=ERROR_SUCCESS;
 uint32_t tcp4_active=0,tcp4_loopback=0,tcp6_active=0,tcp6_loopback=0,udp4_active=0,udp4_loopback=0,udp6_active=0,udp6_loopback=0;
};
inline Job job(){
 Job result;wchar_t text[32]{};DWORD length=GetEnvironmentVariableW(L"OPEN_SV2_HOST_JOB_HANDLE",text,32);
 if(!length||length>=32){result.error=length>=32?ERROR_INVALID_DATA:GetLastError();if(!result.error)result.error=ERROR_ENVVAR_NOT_FOUND;return result;}
 uintptr_t number=0;for(DWORD i=0;i<length;i++){if(text[i]<L'0'||text[i]>L'9'){result.error=ERROR_INVALID_DATA;return result;}auto digit=static_cast<uintptr_t>(text[i]-L'0');if(number>(std::numeric_limits<uintptr_t>::max()-digit)/10){result.error=ERROR_INVALID_DATA;return result;}number=number*10+digit;}if(!number){result.error=ERROR_INVALID_DATA;return result;}
 HANDLE handle=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(number));BOOL member=FALSE;
 if(!IsProcessInJob(GetCurrentProcess(),handle,&member)){result.error=GetLastError();return result;}if(!member){result.error=ERROR_INVALID_HANDLE;return result;}
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};if(!QueryInformationJobObject(handle,JobObjectExtendedLimitInformation,&limits,sizeof limits,nullptr)){result.error=GetLastError();return result;}
 if(limits.BasicLimitInformation.LimitFlags&(JOB_OBJECT_LIMIT_BREAKAWAY_OK|JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)){result.error=ERROR_NOT_SUPPORTED;return result;}
 size_t size=sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)+sizeof(ULONG_PTR)*63;
 for(int attempt=0;attempt<5;attempt++){
  std::vector<unsigned char> buffer(size);auto list=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());
  if(QueryInformationJobObject(handle,JobObjectBasicProcessIdList,list,static_cast<DWORD>(size),nullptr)){
   if(list->NumberOfProcessIdsInList!=list->NumberOfAssignedProcesses||list->NumberOfProcessIdsInList>(size-sizeof(DWORD)*2)/sizeof(ULONG_PTR)){result.error=ERROR_MORE_DATA;return result;}
   for(DWORD i=0;i<list->NumberOfProcessIdsInList;i++)if(list->ProcessIdList[i]==GetCurrentProcessId())result.self_present=true;
   if(!result.self_present){result.error=ERROR_INVALID_DATA;return result;}result.child_count=list->NumberOfAssignedProcesses-1;result.child_scope=result.child_count==0;return result;
  }
  DWORD error=GetLastError();if(error!=ERROR_MORE_DATA){result.error=error;return result;}if(size>=1048576){result.error=ERROR_INSUFFICIENT_BUFFER;return result;}size*=2;
 }
 result.error=ERROR_MORE_DATA;return result;
}
inline bool loopback4(DWORD address){return (ntohl(address)&0xff000000u)==0x7f000000u;}
inline bool loopback6(const UCHAR *address){static const UCHAR loop[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};static const UCHAR mapped[]={0,0,0,0,0,0,0,0,0,0,0xff,0xff};return !memcmp(address,loop,16)||(!memcmp(address,mapped,12)&&address[12]==127);}
inline bool tcp_active(DWORD state){return state!=MIB_TCP_STATE_LISTEN&&state!=MIB_TCP_STATE_TIME_WAIT;}
inline DWORD table(bool tcp,ULONG family,std::vector<unsigned char> &buffer){
 ULONG size=0;DWORD rc=tcp?GetExtendedTcpTable(nullptr,&size,FALSE,family,TCP_TABLE_OWNER_PID_ALL,0):GetExtendedUdpTable(nullptr,&size,FALSE,family,UDP_TABLE_OWNER_PID,0);
 if(rc!=ERROR_INSUFFICIENT_BUFFER)return rc==NO_ERROR?ERROR_INVALID_DATA:rc;
 for(int attempt=0;attempt<4;attempt++){
  if(!size||size>16777216)return ERROR_INSUFFICIENT_BUFFER;buffer.resize(size);
  rc=tcp?GetExtendedTcpTable(buffer.data(),&size,FALSE,family,TCP_TABLE_OWNER_PID_ALL,0):GetExtendedUdpTable(buffer.data(),&size,FALSE,family,UDP_TABLE_OWNER_PID,0);
  if(rc!=ERROR_INSUFFICIENT_BUFFER)return rc;
 }
 return ERROR_INSUFFICIENT_BUFFER;
}
template<class Table>inline bool valid_table(const std::vector<unsigned char> &data){
 constexpr size_t header=offsetof(Table,table);if(data.size()<header)return false;auto value=reinterpret_cast<const Table*>(data.data());return value->dwNumEntries<=(data.size()-header)/sizeof(value->table[0]);
}
inline Network connections(DWORD self){
 Network out;if(self!=GetCurrentProcessId()){out.tcp4_error=out.tcp6_error=out.udp4_error=out.udp6_error=ERROR_INVALID_PARAMETER;return out;}
 std::vector<unsigned char> data;out.tcp4_error=table(true,AF_INET,data);
 if(!out.tcp4_error&&!valid_table<MIB_TCPTABLE_OWNER_PID>(data))out.tcp4_error=ERROR_INVALID_DATA;
 if(!out.tcp4_error){auto p=reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(data.data());for(DWORD i=0;i<p->dwNumEntries;i++){auto &r=p->table[i];if(r.dwOwningPid==self&&tcp_active(r.dwState)){out.tcp4_active++;if(loopback4(r.dwRemoteAddr))out.tcp4_loopback++;}}}
 out.tcp6_error=table(true,AF_INET6,data);
 if(!out.tcp6_error&&!valid_table<MIB_TCP6TABLE_OWNER_PID>(data))out.tcp6_error=ERROR_INVALID_DATA;
 if(!out.tcp6_error){auto p=reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(data.data());for(DWORD i=0;i<p->dwNumEntries;i++){auto &r=p->table[i];if(r.dwOwningPid==self&&tcp_active(r.dwState)){out.tcp6_active++;if(loopback6(r.ucRemoteAddr))out.tcp6_loopback++;}}}
 out.udp4_error=table(false,AF_INET,data);
 if(!out.udp4_error&&!valid_table<MIB_UDPTABLE_OWNER_PID>(data))out.udp4_error=ERROR_INVALID_DATA;
 if(!out.udp4_error){auto p=reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(data.data());for(DWORD i=0;i<p->dwNumEntries;i++){auto &r=p->table[i];if(r.dwOwningPid==self){out.udp4_active++;if(loopback4(r.dwLocalAddr))out.udp4_loopback++;}}}
 out.udp6_error=table(false,AF_INET6,data);
 if(!out.udp6_error&&!valid_table<MIB_UDP6TABLE_OWNER_PID>(data))out.udp6_error=ERROR_INVALID_DATA;
 if(!out.udp6_error){auto p=reinterpret_cast<const MIB_UDP6TABLE_OWNER_PID*>(data.data());for(DWORD i=0;i<p->dwNumEntries;i++){auto &r=p->table[i];if(r.dwOwningPid==self){out.udp6_active++;if(loopback6(r.ucLocalAddr))out.udp6_loopback++;}}}
 out.complete=!out.tcp4_error&&!out.tcp6_error&&!out.udp4_error&&!out.udp6_error;return out;
}
}
