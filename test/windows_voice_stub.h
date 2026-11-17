#pragma once
#include "../loader/windows/voice_adapter.h"
namespace voice_adapter {
static Observer stubObserver;static void *stubContext,*stubReference;static int stubCount;
inline int install(DWORD){return -1;}
inline bool begin(Observer observer,void *context){if(stubObserver)return false;stubObserver=observer;stubContext=context;stubCount=0;return true;}
inline bool finish(){bool valid=stubObserver&&stubCount==1;stubObserver=nullptr;stubContext=nullptr;stubReference=nullptr;return valid;}
inline bool is_observing(void *reference){return stubObserver&&reference&&reference==stubReference;}
inline void deliver(void *reference){++stubCount;stubReference=reference;stubObserver(reference,stubContext);stubReference=nullptr;}
}
