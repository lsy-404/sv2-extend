#include "../loader/host_navigation_registry.h"
#include <assert.h>
#include <stdio.h>
static navigation_registry registry;
static int first_calls,second_calls;
static int second(const char *url,void *context){(void)url;(void)context;++second_calls;return 1;}
static int first(const char *url,void *context){(void)url;(void)context;++first_calls;assert(navigation_unregister(&registry,"second",second,0)==0);assert(navigation_register(&registry,"second",second,0)==0);return 0;}
static int reject(const char *url,void *context){(void)url;(void)context;return -4;}
int main(void){
 assert(navigation_register(&registry,"first",first,0)==0);assert(navigation_register(&registry,"second",second,0)==0);
 assert(navigation_register(&registry,"first",second,0)==LOADER_ERR_INVALID);
 assert(navigation_dispatch(&registry,"example:test")==0&&first_calls==1&&second_calls==0);
 assert(navigation_unregister(&registry,"first",first,0)==0);assert(navigation_dispatch(&registry,"example:test")==1&&second_calls==1);
 assert(navigation_unregister(&registry,"second",second,0)==0);assert(navigation_register(&registry,"reject",reject,0)==0);assert(navigation_dispatch(&registry,"example:test")<0);
 assert(navigation_unregister(&registry,"reject",reject,0)==0);assert(navigation_dispatch(&registry,"example:test")==0);
 char owner[32];for(int i=0;i<16;++i){snprintf(owner,sizeof owner,"owner%d",i);assert(navigation_register(&registry,owner,second,0)==0);}assert(navigation_register(&registry,"overflow",second,0)==LOADER_ERR_INVALID);
 puts("navigation registry contract passed");
}
