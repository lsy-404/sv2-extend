#pragma once
#include "../loader/windows/voice_adapter.h"
namespace voice_adapter {
static Observer stubObserver;static void *stubContext,*stubReference;static int stubCount;static DWORD stubDelay;static bool stubOnce;
inline int install(DWORD){return -1;}
inline bool begin(Observer observer,void *context,bool once){if(stubObserver)return false;stubObserver=observer;stubContext=context;stubCount=0;stubOnce=once;return true;}
inline bool finish(){bool valid=stubObserver&&stubCount==1;stubObserver=nullptr;stubContext=nullptr;stubReference=nullptr;return valid;}
inline bool callback_active(){return stubReference!=nullptr;}
inline bool is_observing(void *reference){return stubObserver&&reference&&reference==stubReference;}
inline void deliver(void *reference){if(stubOnce&&stubCount)return;++stubCount;stubReference=reference;if(stubDelay)Sleep(stubDelay);stubObserver(reference,stubContext);stubReference=nullptr;}
}
