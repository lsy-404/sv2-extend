function getClientInfo() {
    return {name:'Extend JS Voice Verification',author:'SV2 Extend',category:'SV2 Extend',versionNumber:1,minEditorVersion:0x020300};
}
function main() {
    var E=eval(Duktape.enc('opensv2.module.v1','')), checks={};
    function done(){SV.print('EXTEND_JS_VOICE_VERIFICATION '+JSON.stringify(checks));SV.finish();}
    E.voicebanks.list(function(error,list){
        checks.list=!error && Array.isArray(list.voicebanks);
        if(!checks.list){checks.error=error?error.code:'INVALID_CATALOG';done();return;}
        E.voicebanks.current(function(error,current){
            checks.current=!error && typeof current.guard==='string' && !!current.voicebank;
            if(!checks.current){checks.error=error?error.code:'INVALID_CURRENT';done();return;}
            var id=current.voicebank.identityKey, match=false;
            for(var i=0;i<list.voicebanks.length;i++)if(list.voicebanks[i].voicebankId===id){match=true;break;}
            checks.currentInCatalog=match;
            if(!match){checks.error='CURRENT_VOICE_NOT_IN_CATALOG';done();return;}
            E.voicebanks.select(id,current.guard,function(error,selected){
                checks.select=!error && selected.voicebank.identityKey===id && selected.undoRecordCreated===false;
                if(!checks.select){checks.error=error?error.code:'INVALID_SELECT';done();return;}
                E.voicebanks.current(function(error,after){
                    checks.readback=!error && after.voicebank.identityKey===id;
                    if(error)checks.error=error.code;
                    done();
                });
            });
        });
    });
}
