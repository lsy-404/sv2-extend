#include "../loader/windows/transient_layout.h"
#include <cassert>
#include <iostream>

static std::string read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::filesystem::path &p,const std::string &s){std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<s;assert(f.good());}
int main(){
 namespace fs=std::filesystem;auto root=fs::temp_directory_path()/fs::u8path("sv2-layout-"+transient_layout::unique_name());fs::create_directories(root/L"settings");auto file=root/L"settings"/L"settings.xml";
 const std::string plain="<?xml version=\"1.0\" encoding=\"UTF-8\"?><ApplicationSettings><General interfaceLanguage=\"zh-cn\"/><UILayout><UserScriptPanel value=\"preserve\"/></UILayout></ApplicationSettings>";
 write(file,plain);assert(transient_layout::retire(file,root)==0&&read(file)==plain&&!fs::exists(root/L"preferences"));
 const std::string before="<?xml version=\"1.0\" encoding=\"UTF-8\"?><ApplicationSettings><General interfaceLanguage=\"zh-cn\" text=\"保留\"/><Other><OpenSV2ExtendPanel value=\"outside-layout\"/></Other><UILayout><Group><OpenSV2ExtendPanel id=\"boxy.sv2\"/><UserScriptPanel value=\"preserve\"/><OpenSV2ExtendPanel id=\"sv2.mcp\"/></Group></UILayout></ApplicationSettings>";
 write(file,before);assert(transient_layout::retire(file,root)==2);auto after=read(file);assert(after.find("boxy.sv2")==std::string::npos&&after.find("sv2.mcp")==std::string::npos&&after.find("outside-layout")!=std::string::npos&&after.find("UserScriptPanel")!=std::string::npos&&after.find("zh-cn")!=std::string::npos&&after.find("保留")!=std::string::npos);
 auto backups=root/L"preferences"/L"layout-backups";unsigned count=0;for(auto &p:fs::directory_iterator(backups)){assert(read(p.path())==before);++count;}assert(count==1);assert(transient_layout::retire(file,root)==0&&read(file)==after);
 for(const auto &invalid:{std::string("<ApplicationSettings><UILayout><OpenSV2ExtendPanel>"),std::string("<!DOCTYPE ApplicationSettings [<!ENTITY x SYSTEM 'file:///not-read'>]><ApplicationSettings><UILayout><OpenSV2ExtendPanel>&x;</OpenSV2ExtendPanel></UILayout></ApplicationSettings>")}){
  write(file,invalid);bool rejected=false;try{transient_layout::retire(file,root);}catch(const std::exception&){rejected=true;}assert(rejected&&read(file)==invalid);
 }
 std::cout<<"PASS: transient nodes only, exact backup, preserved settings, repeat no-op and invalid/DTD rejection\n";
}
