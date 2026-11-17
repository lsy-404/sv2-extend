function getClientInfo()
  return {name='Extend Lua Voice Verification',author='SV2 Extend',category='SV2 Extend',versionNumber=1,minEditorVersion=0x020300}
end
function main()
  local host = os.getenv('OPEN_SV2_HOST_DATA_DIR')
  local root = os.getenv('OPEN_SV2_LOADER_DATA_DIR')
  if host and host~='' then
    package.path=host:gsub('\\','/')..'/Local/SV2Extend/EditorLua/?.lua;'..package.path
  elseif root and root~='' then
    package.path=root..'/plugins/lua-extend/lua/?.lua;'..package.path
  end
  local E=require('sv2_extend')
  local J=require('sv2_extend_json')
  local checks={}
  local function done() SV:print('EXTEND_LUA_VOICE_VERIFICATION '..J.encode(checks));SV:finish() end
  E.voicebanks.list(function(list,error)
    checks.list=not error and type(list.voicebanks)=='table'
    if not checks.list then checks.error=error and error.code or 'INVALID_CATALOG';done();return end
    E.voicebanks.current(function(current,error)
      checks.current=not error and type(current.guard)=='string' and type(current.voicebank)=='table'
      if not checks.current then checks.error=error and error.code or 'INVALID_CURRENT';done();return end
      local id=current.voicebank.identityKey;local match=false
      for _,voice in ipairs(list.voicebanks) do if voice.voicebankId==id then match=true;break end end
      checks.currentInCatalog=match
      if not match then checks.error='CURRENT_VOICE_NOT_IN_CATALOG';done();return end
      E.voicebanks.select(id,current.guard,function(selected,error)
        checks.select=not error and selected.voicebank.identityKey==id and selected.undoRecordCreated==false
        if not checks.select then checks.error=error and error.code or 'INVALID_SELECT';done();return end
        E.voicebanks.current(function(after,error)
          checks.readback=not error and after.voicebank.identityKey==id
          if error then checks.error=error.code end
          done()
        end)
      end)
    end)
  end)
end
