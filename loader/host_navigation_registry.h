#ifndef HOST_NAVIGATION_REGISTRY_H
#define HOST_NAVIGATION_REGISTRY_H
#include "../sdk/include/loader_api.h"
#include <string.h>

typedef struct { char owner[129]; loader_host_navigation_handler handler; void *context; uint64_t serial; } navigation_entry;
typedef struct { navigation_entry entries[16]; uint64_t next_serial; } navigation_registry;

static int navigation_register(navigation_registry *registry,const char *owner,loader_host_navigation_handler handler,void *context){
 if(!owner||!*owner||strlen(owner)>128||!handler)return LOADER_ERR_INVALID;
 int free_slot=-1;for(int i=0;i<16;++i){if(registry->entries[i].handler&&!strcmp(registry->entries[i].owner,owner))return LOADER_ERR_INVALID;if(!registry->entries[i].handler&&free_slot<0)free_slot=i;}
 if(free_slot<0)return LOADER_ERR_INVALID;
 navigation_entry *entry=&registry->entries[free_slot];strcpy(entry->owner,owner);entry->handler=handler;entry->context=context;entry->serial=++registry->next_serial;return LOADER_OK;
}
static int navigation_unregister(navigation_registry *registry,const char *owner,loader_host_navigation_handler handler,void *context){
 if(!owner||!handler)return LOADER_ERR_INVALID;
 for(int i=0;i<16;++i){navigation_entry *entry=&registry->entries[i];if(entry->handler==handler&&entry->context==context&&!strcmp(entry->owner,owner)){memset(entry,0,sizeof *entry);return LOADER_OK;}}
 return LOADER_ERR_NOT_FOUND;
}
static int navigation_dispatch(navigation_registry *registry,const char *url){
 navigation_entry snapshot[16];memcpy(snapshot,registry->entries,sizeof snapshot);
 for(int i=0;i<16;++i){navigation_entry *entry=&snapshot[i];if(!entry->handler)continue;int active=0;for(int j=0;j<16;++j)if(registry->entries[j].serial==entry->serial&&registry->entries[j].handler){active=1;break;}if(!active)continue;int result=entry->handler(url,entry->context);if(result)return result==1?1:-1;}
 return 0;
}
#endif
