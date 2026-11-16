// SDK entry-point round trip inside an owned isolated host.
#include "loader_api.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int loader_plugin_init_v1(const loader_api_v1 *api){
 if(!api||api->struct_size<offsetof(loader_api_v1,plugin_storage_read_scoped)+sizeof api->plugin_storage_read_scoped||!(api->capabilities&LOADER_CAP_SCOPED_STORAGE)||!api->plugin_storage_write_scoped||!api->plugin_storage_read_scoped)return -1;
 const uint8_t first[]={0,1,0,255},second[]={9,8};uint8_t output[16]={0};size_t length=sizeof output;
 int a=api->plugin_storage_write_scoped("test.storage.a","value",first,sizeof first),b=api->plugin_storage_write_scoped("test.storage.b","value",second,sizeof second);
 int readA=api->plugin_storage_read_scoped("test.storage.a","value",output,&length);int exactA=readA==0&&length==sizeof first&&!memcmp(output,first,sizeof first);
 length=sizeof output;int readB=api->plugin_storage_read_scoped("test.storage.b","value",output,&length);int exactB=readB==0&&length==sizeof second&&!memcmp(output,second,sizeof second);
 output[0]=77;length=1;int small=api->plugin_storage_read_scoped("test.storage.a","value",output,&length);int smallSafe=small==LOADER_ERR_BUFFER_TOO_SMALL&&length==sizeof first&&output[0]==77;
 length=0;int query=api->plugin_storage_read_scoped("test.storage.a","value",NULL,&length);int queryOK=query==0&&length==sizeof first;
 const char *path=getenv("EXTEND_STORAGE_RESULT");FILE *file=path?fopen(path,"w"):NULL;if(!file)return -1;
 fprintf(file,"{\"pid\":%d,\"apiSize\":%u,\"writeA\":%d,\"writeB\":%d,\"readA\":%d,\"readB\":%d,\"smallBufferUnchanged\":%d,\"sizeQuery\":%d}",getpid(),api->struct_size,a,b,exactA,exactB,smallSafe,queryOK);fclose(file);return 0;
}
