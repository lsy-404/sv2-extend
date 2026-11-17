function getClientInfo(){return {name:'Extend JS Export Verification',author:'SV2 Extend',category:'SV2 Extend',versionNumber:1,minEditorVersion:0x020300};}
function main(){
    var E=eval(Duktape.enc('opensv2.module.v1','')),checks={};
    var output=typeof TEST_OUTPUT_PATH==='undefined'?null:TEST_OUTPUT_PATH;
    function done(){SV.print('EXTEND_JS_EXPORT_VERIFICATION '+JSON.stringify(checks));SV.finish();}
    if(!output){checks.error='TEST_OUTPUT_REQUIRED';done();return;}
    E.exports.snapshot(function(error,snapshot){
        checks.snapshot=!error&&typeof snapshot.guard==='string'&&!snapshot.busy;
        if(!checks.snapshot){checks.error=error?error.code:'EXPORT_BUSY';done();return;}
        E.exports.start({guard:snapshot.guard,outputPath:output,channels:1,sampleRate:44100,bitDepth:16},function(error,job){
            checks.started=!error&&typeof job.jobId==='string';
            if(!checks.started){checks.error=error?error.code:'INVALID_JOB';done();return;}
            var remaining=600;
            function poll(){E.exports.status(job.jobId,function(error,status){
                if(error){checks.error=error.code;done();return;}
                if(status.state==='running'){if(--remaining<=0){checks.error='TIMEOUT';done();return;}SV.setTimeout(100,poll);return;}
                checks.completed=status.state==='completed';checks.audio=!!(checks.completed&&status.audio&&status.audio.sampleRate===44100&&status.audio.channels===1&&status.audio.bitDepth===16);
                if(status.error)checks.error=status.error.code;done();
            });}poll();
        });
    });
}
