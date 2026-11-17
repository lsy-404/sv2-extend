#include "../loader/windows/plugin_storage.h"
#include <cassert>
#include <iostream>
#include <thread>
int main(){
 size_t initial=0;assert(plugin_storage::read("Plugin","Key",nullptr,&initial)==LOADER_ERR_STORAGE_NOT_CONFIGURED);
 wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));auto root=std::filesystem::path(temp)/(L"sv2-storage-test-"+std::to_wstring(GetCurrentProcessId()));assert(!std::filesystem::exists(root));assert(std::filesystem::create_directory(root));assert(plugin_storage::install(root));
 const uint8_t a[]={1,2,3},b[]={9,8};size_t n=0;
 {std::filesystem::path dir;std::vector<plugin_storage::Handle> held;assert(plugin_storage::directories("Probe",dir,held));assert(held.size()==4);auto renamed=root;renamed+=L"-renamed";assert(!MoveFileExW(root.c_str(),renamed.c_str(),0));}
 auto root_link=root;root_link+=L"-link";if(CreateSymbolicLinkW(root_link.c_str(),root.c_str(),3)){assert(plugin_storage::install(root_link));assert(plugin_storage::write("Plugin","Key",a,3)==LOADER_ERR_STORAGE_IO);assert(RemoveDirectoryW(root_link.c_str()));assert(plugin_storage::install(root));}
 assert(plugin_storage::write("Plugin","Key",a,3)==LOADER_OK);assert(plugin_storage::write("plugin","key",b,2)==LOADER_OK);
 assert(plugin_storage::read("Plugin","Key",nullptr,&n)==LOADER_OK&&n==3);uint8_t out[]={7,7,7};n=2;assert(plugin_storage::read("Plugin","Key",out,&n)==LOADER_ERR_BUFFER_TOO_SMALL&&n==3&&out[0]==7);
 n=3;assert(plugin_storage::read("Plugin","Key",out,&n)==LOADER_OK&&out[0]==1);assert(plugin_storage::write("Plugin","Key",b,2)==LOADER_OK);n=3;assert(plugin_storage::read("Plugin","Key",out,&n)==LOADER_OK&&n==2&&out[0]==9);
 assert(plugin_storage::write("Plugin","case",a,3)==LOADER_OK);assert(plugin_storage::write("Plugin","case.",b,2)==LOADER_OK);n=3;assert(plugin_storage::read("Plugin","case",out,&n)==LOADER_OK&&n==3&&out[0]==1);
 n=0;assert(plugin_storage::read("Plugin","absent",nullptr,&n)==LOADER_ERR_NOT_FOUND);assert(plugin_storage::write(".bad","x",a,3)==LOADER_ERR_INVALID);assert(plugin_storage::write("valid","..",a,3)==LOADER_ERR_INVALID);assert(plugin_storage::write("valid","key",a,plugin_storage::limit+1)==LOADER_ERR_INVALID);
 std::string long_key(128,'Z');assert(plugin_storage::disk_name(long_key.c_str(),false).find(L".loader-key-")==0);assert(plugin_storage::write("Plugin",long_key.c_str(),a,3)==LOADER_OK);n=3;assert(plugin_storage::read("Plugin",long_key.c_str(),out,&n)==LOADER_OK&&out[0]==1);
 assert(plugin_storage::write("valid",".LoAdEr-private",a,3)==LOADER_ERR_INVALID);std::string long_id(97,'a');assert(plugin_storage::write(long_id.c_str(),"key",a,3)==LOADER_ERR_INVALID);
 auto worker=[&](const uint8_t *value){for(int i=0;i<20;++i)assert(plugin_storage::write("Plugin","parallel",value,2)==LOADER_OK);};std::thread first(worker,a),second(worker,b);first.join();second.join();n=3;assert(plugin_storage::read("Plugin","parallel",out,&n)==LOADER_OK&&n==2&&((out[0]==1&&out[1]==2)||(out[0]==9&&out[1]==8)));
 auto target=root/L"target";{auto h=CreateFileW(target.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);assert(h!=INVALID_HANDLE_VALUE);DWORD count=0;assert(WriteFile(h,a,3,&count,nullptr)&&count==3);CloseHandle(h);}auto symlink=root/L"storage"/L"plugin-data"/plugin_storage::disk_name("Plugin",true)/L"link";
 if(CreateSymbolicLinkW(symlink.c_str(),target.c_str(),2)){n=3;assert(plugin_storage::read("Plugin","link",out,&n)==LOADER_ERR_STORAGE_IO);assert(plugin_storage::write("Plugin","link",b,2)==LOADER_ERR_STORAGE_IO);assert(DeleteFileW(symlink.c_str()));assert(std::filesystem::file_size(target)==3);}
 auto p=root/L"storage"/L"plugin-data"/plugin_storage::disk_name("Plugin",true)/plugin_storage::disk_name("Key",false);auto link=root/L"hardlink";assert(CreateHardLinkW(link.c_str(),p.c_str(),nullptr));n=3;assert(plugin_storage::read("Plugin","Key",out,&n)==LOADER_ERR_STORAGE_IO);assert(plugin_storage::write("Plugin","Key",a,3)==LOADER_ERR_STORAGE_IO);assert(std::filesystem::file_size(link)==2);
 assert(DeleteFileW(plugin_storage::native_path(root/L"storage"/L"plugin-data"/plugin_storage::disk_name("Plugin",true)/L"case.").c_str()));
 std::filesystem::remove_all(root);std::cout<<"scoped storage tests passed\n";
}
