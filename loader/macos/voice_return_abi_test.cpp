#include <cstdint>
#include <initializer_list>
#include <cstdio>
#include <stdexcept>
#include <cstring>
struct Arguments{void *self;const void *args;int count;};
static int constructed,copied,destroyed,observed;static bool throwOriginal,throwObserver;static void *expectedClosure;static const Arguments *expectedArguments;
struct Value {
 uint64_t words[2];
 Value(uint64_t a,uint64_t b):words{a,b}{constructed++;}
 Value(const Value &other):words{other.words[0],other.words[1]}{copied++;}
 ~Value(){destroyed++;}
};
static_assert(sizeof(Value)==16,"Fixture must model a nontrivial 16-byte indirect result");
extern "C" void loader_voice_return_trampoline();
extern "C" void loader_export_return_trampoline();
extern "C" {void (*loader_voice_original_target)()=nullptr;void (*loader_export_original_target)()=nullptr;}
extern "C" void loader_voice_observe_after(void *closure,const Arguments *arguments){if(closure!=expectedClosure || arguments!=expectedArguments)throw std::runtime_error("arguments changed");observed++;if(throwObserver)throw std::runtime_error("observer exception");}
extern "C" void loader_export_observe_after(void *closure,const Arguments *arguments){loader_voice_observe_after(closure,arguments);}
static Value getter(void *closure,const Arguments *arguments){if(closure!=expectedClosure || arguments!=expectedArguments)throw std::runtime_error("original arguments changed");if(throwOriginal)throw std::runtime_error("original exception");return Value(0x1122334455667788ULL,0x8877665544332211ULL);}
int main(){
 Arguments args{nullptr,nullptr,0};expectedArguments=&args;expectedClosure=&args;loader_voice_original_target=reinterpret_cast<void(*)()>(getter);loader_export_original_target=loader_voice_original_target;bool good=true;bool originalUnwound=false;
 for(auto entry:{loader_voice_return_trampoline,loader_export_return_trampoline}) {auto wrapped=reinterpret_cast<Value(*)(void*,const Arguments*)>(entry);
 for(int i=0;i<1000;i++){
  struct Guarded {uint64_t before;Value value;uint64_t after;};
  {Guarded result{0xcafebabedeadbeefULL,wrapped(expectedClosure,&args),0xabcdef0123456789ULL};good &= result.before==0xcafebabedeadbeefULL && result.after==0xabcdef0123456789ULL && result.value.words[0]==0x1122334455667788ULL && result.value.words[1]==0x8877665544332211ULL;}
 }

 originalUnwound=false;throwOriginal=true;try{Value value=wrapped(expectedClosure,&args);(void)value;}catch(const std::runtime_error &){originalUnwound=true;}throwOriginal=false;good &= originalUnwound;
 }
 good &= constructed==2000 && destroyed==2000 && copied==0 && observed==2000;
 // A post-observer throwing would leave a constructed result without normal caller cleanup.
 // Production observer must therefore contain exceptions; only original unwinding is supported.
 std::printf("{\"passed\":%s,\"iterations\":2000,\"constructed\":%d,\"destroyed\":%d,\"copies\":%d,\"observed\":%d,\"originalExceptionUnwound\":%s}\n",good&&originalUnwound?"true":"false",constructed,destroyed,copied,observed,originalUnwound?"true":"false");return good&&originalUnwound?0:1;
}
