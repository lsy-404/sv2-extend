#pragma once
#include <windows.h>
#include <msxml2.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

namespace transient_layout {
namespace fs=std::filesystem;
template<class T>struct Com {T *p=nullptr;~Com(){if(p)p->Release();}T *operator->()const{return p;}T **out(){return &p;}};
struct Text {BSTR p=nullptr;explicit Text(const wchar_t *s):p(SysAllocString(s)){}Text()=default;~Text(){SysFreeString(p);}Text(const Text&)=delete;};
struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);~Apartment(){if(SUCCEEDED(result))CoUninitialize();}};
inline void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
inline void write_new(const fs::path &file,const std::string &bytes){
 HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 require(handle!=INVALID_HANDLE_VALUE,"Cannot create layout backup or staging file");DWORD written=0;
 bool ok=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(handle);CloseHandle(handle);
 if(!ok){DeleteFileW(file.c_str());throw std::runtime_error("Cannot write layout backup or staging file");}
}
inline std::string unique_name(){GUID id{};require(SUCCEEDED(CoCreateGuid(&id)),"Cannot create layout backup name");wchar_t wide[40]{};require(StringFromGUID2(id,wide,40)>0,"Cannot format layout backup name");std::string value;for(wchar_t c:wide){if(!c)break;if(c!=L'{'&&c!=L'}')value.push_back(static_cast<char>(c));}return value;}
inline unsigned retire(const fs::path &settings,const fs::path &profile){
 std::error_code error;bool exists=fs::exists(settings,error);require(!error,"Cannot inspect managed settings");if(!exists)return 0;
 DWORD attributes=GetFileAttributesW(settings.c_str());require(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)),"Managed settings must be a regular file");
 auto length=fs::file_size(settings);require(length<=4*1024*1024,"Managed settings exceeds layout limit");
 std::ifstream input(settings,std::ios::binary);std::string before(static_cast<size_t>(length),'\0');require(bool(input)&&bool(input.read(before.data(),static_cast<std::streamsize>(length))),"Cannot read managed settings");input.close();
 if(before.find("OpenSV2ExtendPanel")==std::string::npos)return 0;
 int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,before.data(),static_cast<int>(before.size()),nullptr,0);require(size>0,"Managed settings must be UTF-8");
 std::wstring wide(static_cast<size_t>(size),L'\0');require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,before.data(),static_cast<int>(before.size()),wide.data(),size)==size,"Cannot decode managed settings");
 Apartment apartment;require(SUCCEEDED(apartment.result)||apartment.result==RPC_E_CHANGED_MODE,"Cannot initialize layout XML parser");
 Com<IXMLDOMDocument2> document;require(SUCCEEDED(CoCreateInstance(__uuidof(DOMDocument60),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IXMLDOMDocument2),reinterpret_cast<void**>(document.out()))),"Cannot create layout XML parser");
 require(SUCCEEDED(document->put_async(VARIANT_FALSE))&&SUCCEEDED(document->put_validateOnParse(VARIANT_FALSE))&&SUCCEEDED(document->put_resolveExternals(VARIANT_FALSE))&&SUCCEEDED(document->put_preserveWhiteSpace(VARIANT_TRUE)),"Cannot configure layout XML parser");
 VARIANT prohibit{};prohibit.vt=VT_BOOL;prohibit.boolVal=VARIANT_TRUE;Text property(L"ProhibitDTD");require(property.p&&SUCCEEDED(document->setProperty(property.p,prohibit)),"Cannot disable layout DTD parsing");
 Text xml;xml.p=SysAllocStringLen(wide.data(),static_cast<UINT>(wide.size()));VARIANT_BOOL loaded=VARIANT_FALSE;require(xml.p&&SUCCEEDED(document->loadXML(xml.p,&loaded))&&loaded==VARIANT_TRUE,"Cannot parse managed layout");
 Com<IXMLDOMElement> root;Text root_name;require(SUCCEEDED(document->get_documentElement(root.out()))&&root.p&&SUCCEEDED(root->get_tagName(&root_name.p))&&root_name.p&&wcscmp(root_name.p,L"ApplicationSettings")==0,"Unexpected managed settings root");
 Text query(L"/ApplicationSettings/UILayout//OpenSV2ExtendPanel");Com<IXMLDOMNodeList> nodes;require(query.p&&SUCCEEDED(document->selectNodes(query.p,nodes.out()))&&nodes.p,"Cannot select transient layout nodes");
 long count=0;require(SUCCEEDED(nodes->get_length(&count)),"Cannot count transient layout nodes");if(!count)return 0;
 for(long i=count-1;i>=0;--i){Com<IXMLDOMNode> node,parent,removed;require(SUCCEEDED(nodes->get_item(i,node.out()))&&node.p&&SUCCEEDED(node->get_parentNode(parent.out()))&&parent.p&&SUCCEEDED(parent->removeChild(node.p,removed.out())),"Cannot remove transient layout node");}
 Text output;require(SUCCEEDED(document->get_xml(&output.p))&&output.p,"Cannot serialize managed layout");int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,output.p,static_cast<int>(SysStringLen(output.p)),nullptr,0,nullptr,nullptr);require(bytes>0,"Cannot encode managed layout");
 std::string after(static_cast<size_t>(bytes),'\0');require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,output.p,static_cast<int>(SysStringLen(output.p)),after.data(),bytes,nullptr,nullptr)==bytes,"Cannot encode managed layout");
 auto backups=profile/L"preferences"/L"layout-backups";fs::create_directories(backups);auto name=unique_name();write_new(backups/fs::u8path(name+".xml"),before);
 auto staged=settings.parent_path()/fs::u8path(".layout-"+name+".tmp");write_new(staged,after);
 if(!ReplaceFileW(settings.c_str(),staged.c_str(),nullptr,0,nullptr,nullptr)){DeleteFileW(staged.c_str());throw std::runtime_error("Cannot replace managed layout");}
 return static_cast<unsigned>(count);
}
}
