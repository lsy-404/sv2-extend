// Open SV2 Loader JS API discovery; read-only, no network requests.
function getClientInfo() { return {name:"JS Extend Runtime Probe",author:"Open SV2 Loader",category:"Open SV2 Loader",versionNumber:1,minEditorVersion:131330}; }
function main() {
 var result={fetch:typeof fetch,XMLHttpRequest:typeof XMLHttpRequest,require:typeof require,process:typeof process,os:typeof os,std:typeof std,Duktape:typeof Duktape,console:typeof console,globalThis:typeof globalThis,module:typeof module,SV:typeof SV};
 result.duktapeVersion=Duktape.version; result.duktapeEnv=Duktape.env;
 try {result.duktapeMethods=Object.getOwnPropertyNames(Duktape); if(typeof Duktape.info==="function") result.encoderInfo=Duktape.info(Duktape.enc);}catch(error){result.inspectError=String(error);}
 try { SV.print("OPEN_SV2_JS_DISCOVERY "+JSON.stringify({types:result,svMethods:Object.getOwnPropertyNames(SV)})); } catch(error) {result.discoveryError=String(error);}
 SV.showMessageBox("JS Extend Runtime Probe",JSON.stringify(result,null,2));
 SV.finish();
}
