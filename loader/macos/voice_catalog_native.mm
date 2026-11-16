#import "voice_native_internal.h"
#include <CommonCrypto/CommonDigest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <string.h>
#if defined(__arm64__)
namespace {
static intptr_t slide;
static BOOL ready;
static BOOL readMemory(uintptr_t address,void *bytes,size_t length){mach_vm_size_t actual=0;return address>4096 && mach_vm_read_overwrite(mach_task_self(),address,length,(mach_vm_address_t)bytes,&actual)==KERN_SUCCESS && actual==length;}
static NSDictionary *failure(NSString *code){return @{@"ok":@NO,@"error":@{@"code":code}};}
static NSDictionary *success(id value){return @{@"ok":@YES,@"result":value};}
static NSString *readString(uintptr_t pointer){NSMutableData *data=[NSMutableData data];for(unsigned i=0;i<512;i++){char c;if(!readMemory(pointer+i,&c,1))return nil;if(!c)return [[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];[data appendBytes:&c length:1];}return nil;}
// Nontrivial return keeps this host's hidden-x8 C++ array ABI. Host cleanup owns
// each shared_ptr release and allocation; no foreign C++ stdlib objects cross ABI.
struct Catalog {void *data;int capacity,count;~Catalog(){((void(*)(void*))(slide+0x10097dec4ULL))(this);}};
struct Database {
    uintptr_t fields[5];
    Database(){for(auto &value:fields)value=slide+0x1015ec5c8ULL;}
    Database(const Database&)=delete;Database&operator=(const Database&)=delete;
    ~Database(){for(auto &value:fields)((void(*)(void*))(slide+0x100699298ULL))(&value);}
    void capture(void *reference){for(unsigned i=0;i<5;i++)((void(*)(void*,const void*))(slide+0x1006992c4ULL))(&fields[i],(char*)reference+0x80+i*8);}
    NSDictionary *identity(){NSMutableArray *strings=[NSMutableArray array];for(auto pointer:fields){NSString *value=readString(pointer);if(!value)return nil;[strings addObject:value];}return loader_voice_identity(strings);}
};
static BOOL knownVariant(uintptr_t variant){
    uintptr_t table=0;uint64_t language=0,backend=0,languages=0,available=0;
    return readMemory(variant,&table,8) && readMemory(table+0x10,&available,8) && available==(uintptr_t)(slide+0x100a1bdbcULL) && readMemory(table+0x18,&language,8) && language==(uintptr_t)(slide+0x100a1bdc4ULL) && readMemory(table+0x30,&backend,8) && backend==(uintptr_t)(slide+0x100a1bdecULL) && readMemory(table+0x68,&languages,8) && languages==(uintptr_t)(slide+0x100a1be44ULL);
}
static NSDictionary *rollback(void *reference,Database &before,NSDictionary *identity,NSString *reason){
    BOOL restored=NO;
    try{((void(*)(void*,const void*,bool))(slide+0x100e016b8ULL))(reference,&before,false);restored=[loader_voice_reference_identity(reference)[@"identityKey"] isEqual:identity[@"identityKey"]];}catch(...){restored=NO;}
    return @{@"ok":@NO,@"error":@{@"code":restored?@"VOICE_CHANGE_FAILED_ROLLED_BACK":@"VOICE_CHANGE_ROLLBACK_FAILED",@"cause":reason,@"rolledBack":@(restored),@"undoRecordMayExist":@YES}};
}
static NSDictionary *apply(void *reference,Database &target,NSDictionary *desired){
    NSDictionary *identity=loader_voice_reference_identity(reference);if(!identity)return failure(@"VOICE_REFERENCE_UNREADABLE");
    if([identity[@"identityKey"] isEqual:desired[@"identityKey"]])return success(@{@"voicebank":identity,@"undoRecordCreated":@NO});
    Database before;before.capture(reference);
    try{((void(*)(void*,const void*,bool))(slide+0x100e016b8ULL))(reference,&target,true);}catch(...){return rollback(reference,before,identity,@"NATIVE_EXCEPTION");}
    NSDictionary *after=loader_voice_reference_identity(reference);
#if defined(LOADER_TEST_VOICE_ROLLBACK)
    // Fault injection exists only in a separately compiled disposable test core.
    const char *fault=getenv("OPEN_SV2_TEST_FAIL_VOICE_READBACK");
    if(fault && [desired[@"name"] isEqualToString:[NSString stringWithUTF8String:fault]])after=nil;
#endif
    if(![after[@"identityKey"] isEqual:desired[@"identityKey"]])return rollback(reference,before,identity,@"READBACK_MISMATCH");
    return success(@{@"voicebank":after,@"undoRecordCreated":@YES});
}
static NSDictionary *walk(NSString *identifier,void *reference){
    uintptr_t manager=0;if(!readMemory(slide+0x10161e678ULL,&manager,8) || !manager)return failure(@"VOICE_CATALOG_NOT_READY");
    Catalog entries=((Catalog(*)(void*))(slide+0x100a1a1f8ULL))((void*)manager);
    if(entries.count<0 || entries.count>512 || entries.count>entries.capacity)return failure(@"VOICE_CATALOG_INVALID");
    NSMutableArray *voices=[NSMutableArray array];NSMutableSet *seen=[NSMutableSet set];
    for(int i=0;i<entries.count;i++){
        uintptr_t record=0;if(!readMemory((uintptr_t)entries.data+16*i,&record,8))return failure(@"VOICE_CATALOG_INVALID");
        uint8_t kind=0;if(!readMemory(record+0x140,&kind,1))return failure(@"VOICE_CATALOG_INVALID");
        uintptr_t begin=0,end=0;if(!readMemory(record+0x148,&begin,8) || !readMemory(record+0x150,&end,8) || end<begin || (end-begin)%16 || (end-begin)/16>64)return failure(@"VOICE_CATALOG_INVALID");
        // Synthetic online catalog records hold a vendor at +0x20 and no
        // installed native variants. Their menu action may open a product page;
        // the record-level can-select predicate is NOT an availability test.
        if(kind){if(begin!=end)return failure(@"VOICE_CATALOG_KIND_UNSUPPORTED");continue;}
        for(uintptr_t cursor=begin;cursor<end;cursor+=16){
            uintptr_t variant=0;if(!readMemory(cursor,&variant,8) || !knownVariant(variant))return failure(@"VOICE_CATALOG_VARIANT_UNSUPPORTED");
            // The host's own can-select predicate reads this flag on the selected
            // native variant. Reject unavailable variants rather than activating.
            uint8_t available=0;if(!readMemory(variant+0x34,&available,1))return failure(@"VOICE_CATALOG_INVALID");if(!available)continue;
            Database database;((void(*)(void*,void*))(slide+0x100a13980ULL))((void*)variant,&database);
            NSMutableDictionary *identity=[database.identity() mutableCopy];if(!identity || ![identity[@"assigned"] boolValue])return failure(@"VOICE_CATALOG_INVALID");
            NSString *key=identity[@"identityKey"];identity[@"voicebankId"]=key;
            if(identifier && [key isEqual:identifier])return reference?apply(reference,database,identity):success(identity);
            if(![seen containsObject:key]){[seen addObject:key];[voices addObject:identity];}
            if(voices.count>1024)return failure(@"VOICE_CATALOG_LIMIT");
        }
    }
    if(identifier)return failure(@"VOICEBANK_UNAVAILABLE");
    [voices sortUsingComparator:^NSComparisonResult(NSDictionary *a,NSDictionary *b){return [a[@"identityKey"] compare:b[@"identityKey"]];}];
    return success(@{@"voicebanks":voices,@"source":@"native-host-catalog",@"availableOnly":@YES});
}
}
BOOL loader_voice_catalog_initialize(intptr_t hostSlide){
    if(!NSThread.isMainThread)return NO;slide=hostSlide;
    struct Check {uintptr_t address;size_t length;const char *hash;};
    Check checks[]={{0x100a1a1f8ULL,436,"5cf9b7de8d45b6f0944f12fb95bc3f5d833908bfb5c2a2c7bcd7579aee102442"},{0x10097dec4ULL,160,"2ab61e5981fecc91d0db4fa41e56e81c91bc02385a6ff7296cb47a8ac23923bd"},{0x100a119acULL,344,"4b89c24a7d2e5b5014a1863601ed7ae19016f23417ed5508efdc0f01cd8119cb"},{0x100a13980ULL,600,"a1120aaa296bca4073dd80564d5aeda48619cd951e3b765f5a46fcfebf236d93"},{0x100699298ULL,44,"f79a7305b5ef0e3115ede6956943945e726c6ea451df8a01f2dc4312ac25af1c"},{0x1006992c4ULL,40,"20eca3681ba14e0915855eaeb454b96a250ea3f509343c54d7503441ded17272"},{0x100e016b8ULL,1452,"70931accffe5b275e31477aef2b3d560f3ab805838e6653ee93006c07f6beb6a"},{0x100a1bdbcULL,8,"3b00dbfe0e7baa06f400a59db8fa929c7a491ea3ae9e7d990cf39851847f7e7d"},{0x100a1bdc4ULL,40,"c338922bda6f3e1527d0464dfc3695bb588f36bb9e1dccd9d16f7521c023b93d"},{0x100a1bdecULL,72,"6ef7cc575e5be9787c2c7eb62b60e89b8d241b2b85d4e944a5b8dd769dddfe05"},{0x100a1be44ULL,68,"670819fed87775f236905203d43435c191eeb687205ea08a604a38caa296f7d2"}};
    for(const auto &check:checks){unsigned char digest[32];CC_SHA256((const void*)(slide+check.address),(CC_LONG)check.length,digest);char value[65];for(unsigned i=0;i<32;i++)snprintf(value+2*i,3,"%02x",digest[i]);if(strcmp(value,check.hash))return NO;}
    ready=YES;return YES;
}
NSDictionary *loader_voice_catalog_request(NSString *identifier,void *reference){
    if(!NSThread.isMainThread)return failure(@"MAIN_THREAD_REQUIRED");if(!ready)return failure(@"VOICE_CATALOG_UNAVAILABLE");
    try{return walk(identifier,reference);}catch(...){return failure(@"VOICE_CATALOG_NATIVE_EXCEPTION");}
}
#else
BOOL loader_voice_catalog_initialize(intptr_t hostSlide){(void)hostSlide;return NO;}
NSDictionary *loader_voice_catalog_request(NSString *identifier,void *reference){(void)identifier;(void)reference;return @{@"ok":@NO,@"error":@{@"code":@"VOICE_CATALOG_UNAVAILABLE"}};}
#endif
