#include <mach-o/dyld.h>
#include <CoreFoundation/CoreFoundation.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv) {
    (void)argc;
    char raw[PATH_MAX],path[PATH_MAX],host[PATH_MAX],loader[PATH_MAX],plugin[PATH_MAX],mcp[PATH_MAX];
    uint32_t size=sizeof(raw);
    if(_NSGetExecutablePath(raw,&size)!=0 || !realpath(raw,path)) return 70;
    char *slash=strrchr(path,'/'); if(!slash)return 70; *slash=0;
    if(snprintf(host,sizeof(host),"%s/synthv-studio-original",path)>=(int)sizeof(host) ||
       snprintf(loader,sizeof(loader),"%s/../Frameworks/OpenSV2Loader.dylib",path)>=(int)sizeof(loader) ||
       snprintf(plugin,sizeof(plugin),"%s/../PlugIns/network-plugin.dylib",path)>=(int)sizeof(plugin) ||
       snprintf(mcp,sizeof(mcp),"%s/../PlugIns/sv2-mcp.dylib",path)>=(int)sizeof(mcp))return 70;
    CFBundleRef bundle=CFBundleGetMainBundle();
    CFTypeRef rootValue=CFBundleGetValueForInfoDictionaryKey(bundle,CFSTR("OpenSV2LoaderDataDirectory"));
    if(rootValue) {
        char root[PATH_MAX];
        if(CFGetTypeID(rootValue)!=CFStringGetTypeID() || !CFStringGetCString((CFStringRef)rootValue,root,sizeof(root),kCFStringEncodingUTF8))return 78;
        const char *requested=getenv("OPEN_SV2_LOADER_DATA_DIR");if(requested && strcmp(requested,root)){fputs("LOADER_LAUNCHER storage-binding-mismatch\n",stderr);return 78;}
        if(setenv("OPEN_SV2_LOADER_DATA_DIR",root,1)!=0)return 70;
        CFTypeRef mode=CFBundleGetValueForInfoDictionaryKey(bundle,CFSTR("OpenSV2HostDataNamespace"));
        if(mode && CFGetTypeID(mode)==CFBooleanGetTypeID() && CFBooleanGetValue((CFBooleanRef)mode)) {
            char home[PATH_MAX];if(snprintf(home,sizeof(home),"%s/host-home",root)>=(int)sizeof(home))return 78;
            if(setenv("OPEN_SV2_HOST_DATA_HOME",home,1)!=0 || setenv("OPEN_SV2_ISOLATE_HOST_DATA","1",1)!=0)return 70;
        } else {unsetenv("OPEN_SV2_HOST_DATA_HOME");unsetenv("OPEN_SV2_ISOLATE_HOST_DATA");}
    }
    const CFStringRef keys[]={CFSTR("OpenSV2ScriptHTTPLocal"),CFSTR("OpenSV2ScriptHTTPOnline")};
    const char *names[]={"OPEN_SV2_SCRIPT_HTTP_LOCAL","OPEN_SV2_SCRIPT_HTTP_ONLINE"};
    for(unsigned i=0;i<2;i++) {
        CFTypeRef permission=CFBundleGetValueForInfoDictionaryKey(bundle,keys[i]);
        if(permission) {
            if(CFGetTypeID(permission)!=CFBooleanGetTypeID())return 78;
            if(setenv(names[i],CFBooleanGetValue((CFBooleanRef)permission)?"1":"0",1)!=0)return 70;
        }
    }
    if(setenv("DYLD_INSERT_LIBRARIES",loader,1)!=0 ||
       setenv("OPEN_SV2_MCP_PLUGIN",mcp,1)!=0 || setenv("OPEN_SV2_AUTO_EDITOR_PROVIDER","1",1)!=0 || setenv("OPEN_SV2_ENABLE_JS_NATIVE","1",1)!=0)return 70;
    const char *diagnostics=getenv("OPEN_SV2_NETWORK_DIAGNOSTICS");
    if(diagnostics && !strcmp(diagnostics,"1"))setenv("LOADER_PROBE_PLUGIN",plugin,1);
    else unsetenv("LOADER_PROBE_PLUGIN");
    fprintf(stderr,"LOADER_LAUNCHER pid=%d exec-host\n",getpid()); fflush(stderr);
    argv[0]=host;
    execv(host,argv); perror("exec host"); return 71;
}
