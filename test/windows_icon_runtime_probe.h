static void icon_test_tick(){
 using namespace native_category_registry;
 static unsigned stage=0,ticks=0;
 if(stage>=4||!prepared||bars.empty()||++ticks<8)return;
 ticks=0;
 auto found=native_panel::plugins.find("opensv2.lua-extend");
 if(found==native_panel::plugins.end())return;
 auto save=[&](const char *line){
  const char *profile=getenv("OPEN_SV2_LOADER_DATA_DIR");
  if(!profile)return;
  std::ofstream out(std::filesystem::u8path(profile)/L"outputs"/L"icon-test.log",std::ios::app);
  out<<line<<'\n';
 };
 if(stage==0){
  auto state=R"({"icon":"<svg xmlns='http://www.w3.org/2000/svg'><path fill='black' d='M0 0h8v8z'/></svg>","activeIcon":"<svg xmlns='http://www.w3.org/2000/svg'><path fill='red' d='M0 0h9v9z'/></svg>"})";
  bool ok=native_panel::update_ui("opensv2.lua-extend",state,found->second.owner)==LOADER_OK;
  save(ok?"update=ok":"update=failed");if(ok)stage=1;
 } else if(stage==1){
  bool ok=open_category("opensv2.lua-extend")==0;
  save(ok?"open=ok":"open=failed");if(ok)stage=2;
 } else if(stage==2){
  bool ok=false;
  for(const auto &b:button_icons)if(std::strcmp(b.second.category->id,"opensv2.lua-extend")==0){
   auto p=static_cast<unsigned char *>(b.first);
   float bounds[4]{};std::memcpy(bounds,p+0x380,sizeof bounds);
   ok=b.second.active&&b.second.copies[0]&&b.second.copies[1]&&b.second.copies[2]&&bounds[2]>0&&bounds[3]>0;
  }
  save(ok?"active-refreshed=ok":"active-refreshed=failed");if(ok)stage=3;
 } else {
  auto state=R"({"icon":"<svg xmlns='http://www.w3.org/2000/svg'><path fill='white' d='M0 0h7v7z'/></svg>","activeIcon":"<svg xmlns='http://www.w3.org/2000/svg'><path fill='blue' d='M0 0h6v6z'/></svg>"})";
  bool ok=native_panel::update_ui("opensv2.lua-extend",state,found->second.owner)==LOADER_OK;
  if(ok){
   tick();ok=false;
   for(const auto &b:button_icons)if(std::strcmp(b.second.category->id,"opensv2.lua-extend")==0)
    ok=b.second.active&&b.second.generation==b.second.category->icon_generation&&b.second.copies[0]&&b.second.copies[1]&&b.second.copies[2];
  }
  save(ok?"runtime-refreshed=ok":"runtime-refreshed=failed");if(ok)stage=4;
 }
}
