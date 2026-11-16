// Research only: proves official Lua -> native entry without depending on Lua ABI.
// Deliberately returns zero Lua results and never reads/writes lua_State internals.
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
typedef struct lua_State lua_State;
__attribute__((visibility("default"))) int luaopen_sv2_extend_probe(lua_State *state) {
    const char *names[]={"lua_gettop","lua_pushlstring","lua_pushcclosure","lua_pcallk","luaL_checklstring","luaL_newlib"};
    fprintf(stderr,"LUA_EXTEND_ENTRY pid=%d main=%d state_present=%d",getpid(),pthread_main_np(),state!=NULL);
    for(unsigned i=0;i<sizeof names/sizeof names[0];i++)fprintf(stderr," %s=%d",names[i],dlsym(RTLD_DEFAULT,names[i])!=NULL);
    fprintf(stderr,"\n");fflush(stderr);return 0;
}
