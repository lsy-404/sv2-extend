#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
__attribute__((constructor))static void hostConstructor(void){
 const char *path=getenv("EXTEND_TEST_BOOTSTRAP_LIBRARY");void *module=path?dlopen(path,RTLD_NOLOAD|RTLD_NOW):NULL;
 void(*observe)(void)=module?(void(*)(void))dlsym(module,"extend_test_executable_constructor"):NULL;
 if(observe)observe();else{puts("HOST_CONSTRUCTOR_NO_BOOTSTRAP");fflush(stdout);}
 if(module)dlclose(module); // Release only this fixture's NOLOAD reference.
}
int main(void){@autoreleasepool{puts("HOST_MAIN");fflush(stdout);[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.6]];}return 0;}
