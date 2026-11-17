#import <AppKit/AppKit.h>
extern "C" int loader_category_test_retire_layout(void);
extern "C" const char *loader_host_data_directory(void){return getenv("TEST_HOST_DATA");}
extern "C" bool loader_native_panel_owns(void*,void*){return false;}
extern "C" void loader_native_panel_remove(void*){}
extern "C" void loader_native_panel_capture(void*,NSString*,bool,intptr_t){}
extern "C" NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*){return nil;}
int main(int argc,char **argv){@autoreleasepool{if(argc!=3)return 2;setenv("OPEN_SV2_LOADER_DATA_DIR",argv[1],1);setenv("TEST_HOST_DATA",argv[2],1);printf("%d",loader_category_test_retire_layout());return 0;}}
