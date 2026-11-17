function getClientInfo() return {name='Extend Lua Export Verification',author='SV2 Extend',category='SV2 Extend',versionNumber=1,minEditorVersion=0x020300} end
function main()
  local host=os.getenv('OPEN_SV2_HOST_DATA_DIR');local root=os.getenv('OPEN_SV2_LOADER_DATA_DIR')
  if host and host~='' then package.path=host:gsub('\\','/')..'/Local/SV2Extend/EditorLua/?.lua;'..package.path
  elseif root and root~='' then package.path=root..'/plugins/lua-extend/lua/?.lua;'..package.path end
  local E=require('sv2_extend');local J=require('sv2_extend_json');local checks={}
  local output=os.getenv('OPEN_SV2_TEST_EXPORT_PATH')
  local function done() SV:print('EXTEND_LUA_EXPORT_VERIFICATION '..J.encode(checks));SV:finish() end
  if not output or output=='' then checks.error='TEST_OUTPUT_REQUIRED';done();return end
  E.exports.snapshot(function(snapshot,error)
    checks.snapshot=not error and type(snapshot.guard)=='string' and not snapshot.busy
    if not checks.snapshot then checks.error=error and error.code or 'EXPORT_BUSY';done();return end
    E.exports.start({guard=snapshot.guard,outputPath=output,channels=1,sampleRate=44100,bitDepth=16},function(job,error)
      checks.started=not error and type(job.jobId)=='string'
      if not checks.started then checks.error=error and error.code or 'INVALID_JOB';done();return end
      local remaining=600
      local function poll()
        E.exports.status(job.jobId,function(status,error)
          if error then checks.error=error.code;done();return end
          if status.state=='running' then remaining=remaining-1;if remaining<=0 then checks.error='TIMEOUT';done();return end;SV:setTimeout(100,poll);return end
          checks.completed=status.state=='completed';checks.audio=checks.completed and status.audio and status.audio.sampleRate==44100 and status.audio.channels==1 and status.audio.bitDepth==16 or false
          if status.error then checks.error=status.error.code end;done()
        end)
      end
      poll()
    end)
  end)
end
