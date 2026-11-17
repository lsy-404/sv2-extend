-- Official Lua editor provider for Loader's resident native API. No file/pipe requests.
local extend = require('sv2_extend')
local json = require('sv2_extend_json')
local methods={'selection.read','notes.set_pitch','pitch.read_range','pitch.replace_range','transport.status','transport.play','transport.stop'}
local noteStatus=extend.request('notes.native_status', {})
if noteStatus and noteStatus.addable then methods[#methods+1]='notes.add' end
if noteStatus and noteStatus.removable then methods[#methods+1]='notes.remove' end
local groupStatus=extend.request('groups.native_status', {})
if groupStatus and groupStatus.available then methods[#methods+1]='groups.create';methods[#methods+1]='groups.remove' end
local voiceStatus=extend.request('voice.native_status', {})
local nativeVoice=voiceStatus and voiceStatus.identityReadable
if nativeVoice then methods[#methods+1]='voicebanks.current' end
if voiceStatus and voiceStatus.catalogReadable then methods[#methods+1]='voicebanks.list' end
if voiceStatus and voiceStatus.selectable then methods[#methods+1]='voicebanks.select' end
local exportStatus=extend.request('export.native_status', {})
if exportStatus and exportStatus.available then
  for _,name in ipairs({'exports.snapshot','exports.start','exports.status','exports.cancel'}) do methods[#methods+1]=name end
end
local attached, failure = extend.request('editor.attach', {methods=methods, editorVersion=SV:getHostInfo().hostVersionNumber})
-- Preserve the existing provider if a delayed auto-start races its owner.
if not attached and failure and failure.code=='EDITOR_PROVIDER_ALREADY_ATTACHED' then SV:finish(); return end
assert(attached, failure and failure.code or 'Editor registration failed')
local epoch = attached.epoch
local function integer(value, low, high)
  assert(type(value)=='number' and value==math.floor(value) and value>=low and value<=high,'Integer out of range');return value
end
local function selected(identifier)
  if identifier~=nil then
    assert(type(identifier)=='string' and #identifier>0 and #identifier<=128,'Invalid group UUID')
    local project=SV:getProject();local group=project:getNoteGroup(identifier)
    if not group then error({code='GROUP_NOT_FOUND'}) end
    local match,count=nil,0
    for t=1,project:getNumTracks() do local track=project:getTrack(t)
      for g=1,track:getNumGroups() do local ref=track:getGroupReference(g);local target=ref:getTarget()
        if target and target:getUUID()==identifier then if not match then match=ref end;count=count+1 end
      end
    end
    if count==0 then error({code='GROUP_REFERENCE_NOT_FOUND'}) end
    return match,group
  end
  local reference=SV:getMainEditor():getCurrentGroup();assert(reference,'No current note group')
  local group=reference:getTarget();assert(group,'No current note group target');return reference,group
end
local function guard(group)
  local notes={}
  for i=1,group:getNumNotes() do
    local note=group:getNote(i);notes[i]={note:getOnset(),note:getDuration(),note:getPitch(),note:getLyrics()}
  end
  assert(#notes<=4096,'Group exceeds edit guard limit')
  local track=SV:getMainEditor():getCurrentTrack()
  return json.encode({epoch,SV:getProject():getFileName(),track and track:getIndexInParent() or 0,group:getUUID(),group:getName(),notes})
end
local function exclusive(group)
  local project=SV:getProject();local count=0
  for t=1,project:getNumTracks() do local track=project:getTrack(t)
    for g=1,track:getNumGroups() do local target=track:getGroupReference(g):getTarget();if target and target:getUUID()==group:getUUID() then count=count+1 end end
  end
  assert(count==1,'Shared group content edits require an explicit shared-group policy')
end
local function voiceSnapshot(reference)
  if not nativeVoice then return reference:getVoice(), nil end
  local scope, why=extend.request('voice.capture.begin', {})
  assert(scope, why and why.code or 'Voice capture unavailable')
  local ok, parameters=pcall(function() return reference:getVoice() end)
  local identity, failure=extend.request('voice.capture.end', {token=scope.token})
  assert(ok, parameters)
  assert(identity, failure and failure.code or 'Voice identity unavailable')
  return parameters, identity
end
local function currentVoice(reference,group)
  local _, raw=voiceSnapshot(reference)
  assert(raw,'VOICE_NATIVE_ADAPTER_UNAVAILABLE')
  local identity=raw;local inherited=false
  if not identity.assigned then
    local track=SV:getMainEditor():getCurrentTrack()
    if track then local _, main=voiceSnapshot(track:getGroupReference(1));if main.assigned then identity=main;inherited=true end end
  end
  return {voicebank=identity,inherited=inherited,groupUUID=group:getUUID(),guard=json.encode({epoch,SV:getProject():getFileName(),group:getUUID(),raw.referenceKey,raw.identityKey,identity.identityKey,inherited})},raw
end
local function nativeResult(method,args)
  local value,why=extend.request(method,args)
  if not value then error(why or {code='VOICE_NATIVE_CALL_FAILED'}) end
  return value
end
local function invoke(method,args)
  if method:sub(1,8)=='exports.' then
    local project=SV:getProject()
    local mainUUID=''
    if project:getNumTracks()>0 then mainUUID=project:getTrack(1):getGroupReference(1):getTarget():getUUID() end
    local projectContext=json.encode({project:getFileName(),mainUUID})
    local scope=nativeResult('export.capture.begin',{operation=method:sub(9),arguments=args,projectContext=projectContext})
    local ok,why=pcall(function() SV:getProject() end)
    local result,failure=extend.request('export.capture.end',{token=scope.token})
    if not ok then error({code='PROJECT_GETTER_FAILED',message=tostring(why)}) end
    if not result then error(failure or {code='EXPORT_NATIVE_CALL_FAILED'}) end
    return result
  end
  if method=='voicebanks.list' then return nativeResult('voice.catalog',{}) end
  if method=='transport.status' then local transport=SV:getPlayback();return {status=transport:getStatus(),playhead=transport:getPlayhead()} end
  if method=='transport.play' then SV:getPlayback():play();return {status=SV:getPlayback():getStatus()} end
  if method=='transport.stop' then SV:getPlayback():stop();return {status=SV:getPlayback():getStatus()} end
  if method=='groups.create' then
    local reference,current=selected();local track=SV:getMainEditor():getCurrentTrack();assert(track,'No current track')
    assert(type(args.guard)=='string' and args.guard==guard(current),'STALE_EDIT_GUARD')
    local name=args.name or 'Group';assert(type(name)=='string' and #name<=256 and not name:find('\0',1,true),'Invalid group name')
    local project=SV:getProject();local group=SV:create('NoteGroup');group:setName(name)
    local ref=SV:create('NoteGroupReference');local index
    project:newUndoRecord()
    local ok,why=pcall(function() index=project:addNoteGroup(group);ref:setTarget(group);track:addGroupReference(ref) end)
    if not ok then if index then pcall(function() project:removeNoteGroup(index) end) end;error(why) end
    local uuid=group:getUUID();local actual=project:getNoteGroup(uuid);assert(actual,'Group readback failed')
    return {groupUUID=uuid,guard=guard(actual),undoRecordCreated=true}
  end
  if args.groupUUID and (method=='notes.add' or method=='notes.remove' or method=='groups.remove') then
    local project=SV:getProject()
    for i=1,project:getNumTracks() do local track=project:getTrack(i);local main=track:getGroupReference(1);local target=main and main:getTarget()
      if target and target:getUUID()==args.groupUUID then error({code='MAIN_GROUP_EDIT_UNSUPPORTED'}) end
    end
  end
  local reference,group=selected(args.groupUUID)
  if method=='voicebanks.current' then return currentVoice(reference,group) end
  if method=='voicebanks.select' then
    local before,raw=currentVoice(reference,group)
    if type(args.guard)~='string' or args.guard~=before.guard then error({code='STALE_VOICE_GUARD'}) end
    local target=nativeResult('voice.select.validate',{voicebankId=args.voicebankId})
    if target.identityKey==before.voicebank.identityKey then before.undoRecordCreated=false;return before end
    local scope=nativeResult('voice.select.begin',{voicebankId=args.voicebankId,expectedIdentityKey=raw.identityKey,expectedReferenceKey=raw.referenceKey})
    SV:getProject():newUndoRecord()
    local ok,why=pcall(function() reference:getVoice() end)
    local completed,failed=extend.request('voice.select.end',{token=scope.token})
    if not ok then error({code='VOICE_GETTER_FAILED',message=tostring(why)}) end
    if not completed then error(failed or {code='VOICE_NATIVE_CALL_FAILED'}) end
    local after=currentVoice(reference,group)
    if after.voicebank.identityKey~=target.identityKey then error({code='VOICE_CONTEXT_CHANGED_AFTER_SELECTION'}) end
    after.undoRecordCreated=completed.undoRecordCreated;return after
  end
  if method=='selection.read' then
    local notes={};for i=1,math.min(group:getNumNotes(),128) do local note=group:getNote(i);notes[#notes+1]={index=i,onsetBlick=note:getOnset(),durationBlick=note:getDuration(),midiPitch=note:getPitch()} end
    local parameters, identity=voiceSnapshot(reference)
    return {groupUUID=group:getUUID(),isMain=reference:isMain(),noteCount=group:getNumNotes(),notes=notes,guard=guard(group),voiceParameters=parameters,voicebankIdentityReadable=identity~=nil,voicebank=identity}
  end
  if method=='pitch.read_range' then
    local start=integer(args.startBlick,0,9007199254740991);local finish=integer(args.endBlick,start+1,9007199254740991)
    local automation=group:getParameter('pitchDelta');local points=automation:getPoints(start,finish);assert(#points<=4096,'Pitch range too large')
    return {points=points,curveGuard=json.encode(points),guard=guard(group),range=automation:getDefinition().range}
  end
  assert(type(args.guard)=='string' and args.guard==guard(group),'STALE_EDIT_GUARD')
  exclusive(group)
  if (method=='notes.add' or method=='notes.remove' or method=='groups.remove') and reference:isMain() then error({code='MAIN_GROUP_EDIT_UNSUPPORTED'}) end
  if method=='groups.remove' then
    assert(type(args.groupUUID)=='string','Group UUID required')
    local project=SV:getProject();local index
    for i=1,project:getNumNoteGroupsInLibrary() do if project:getNoteGroup(i):getUUID()==args.groupUUID then index=i;break end end
    if not index then error({code='GROUP_NOT_FOUND'}) end
    project:newUndoRecord();project:removeNoteGroup(index)
    assert(not project:getNoteGroup(args.groupUUID),'Group removal readback failed')
    return {groupUUID=args.groupUUID,removed=true,undoRecordCreated=true}
  end
  if method=='notes.add' then
    assert(group:getNumNotes()<4096,'Group exceeds edit limit')
    local onset=integer(args.onsetBlick,0,9007199254740991)
    local duration=integer(args.durationBlick,1,9007199254740991-onset)
    local pitch=integer(args.midiPitch,0,127);local lyrics=args.lyrics or 'la'
    assert(type(lyrics)=='string' and #lyrics<=1024 and not lyrics:find('\0',1,true),'Invalid lyrics')
    local note=SV:create('Note');note:setOnset(onset);note:setDuration(duration);note:setPitch(pitch);note:setLyrics(lyrics)
    SV:getProject():newUndoRecord();group:addNote(note)
    return {noteCount=group:getNumNotes(),guard=guard(group),undoRecordCreated=true}
  end
  if method=='notes.remove' then
    local index=integer(args.noteIndex,1,group:getNumNotes())
    SV:getProject():newUndoRecord();group:removeNote(index)
    return {noteCount=group:getNumNotes(),guard=guard(group),undoRecordCreated=true}
  end
  if method=='notes.set_pitch' then
    local index=integer(args.noteIndex,1,group:getNumNotes());local pitch=integer(args.midiPitch,0,127);local note=group:getNote(index)
    local before=note:getPitch();if before==pitch then return {noteIndex=index,before=before,after=before,guard=guard(group),undoRecordCreated=false} end
    SV:getProject():newUndoRecord();note:setPitch(pitch)
    assert(note:getPitch()==pitch,'Pitch readback failed');return {noteIndex=index,before=before,after=note:getPitch(),guard=guard(group),undoRecordCreated=true}
  end
  if method=='pitch.replace_range' then
    local start=integer(args.startBlick,0,9007199254740991);local finish=integer(args.endBlick,start+1,9007199254740991)
    assert(type(args.points)=='table' and #args.points<=4096,'Invalid points')
    local automation=group:getParameter('pitchDelta');local definition=automation:getDefinition();local points={};local previous=-1
    for i,pair in ipairs(args.points) do
      assert(type(pair)=='table' and #pair==2,'Invalid point');local time=integer(pair[1],start,finish)
      assert(time>previous and type(pair[2])=='number' and pair[2]==pair[2] and pair[2]>=definition.range[1] and pair[2]<=definition.range[2],'Pitch point outside host range')
      points[i]={time,pair[2]};previous=time
    end
    local before=automation:getPoints(start,finish);assert(type(args.curveGuard)=='string' and args.curveGuard==json.encode(before),'STALE_CURVE_GUARD')
    if json.encode(before)==json.encode(points) then return {before=before,after=before,guard=guard(group),curveGuard=json.encode(before),undoRecordCreated=false} end
    SV:getProject():newUndoRecord()
    -- Remove sampled points individually because range-remove retains the end boundary.
    for _,pair in ipairs(before) do automation:remove(pair[1]) end
    for _,pair in ipairs(points) do automation:add(pair[1],pair[2]) end
    local after=automation:getPoints(start,finish)
    if json.encode(after)~=json.encode(points) then
      for _,pair in ipairs(after) do automation:remove(pair[1]) end
      for _,pair in ipairs(before) do automation:add(pair[1],pair[2]) end
      local restored=automation:getPoints(start,finish)
      error(json.encode(restored)==json.encode(before) and 'CURVE_READBACK_FAILED_ROLLED_BACK' or 'CURVE_READBACK_FAILED_ROLLBACK_FAILED')
    end
    return {before=before,after=after,guard=guard(group),curveGuard=json.encode(after),undoRecordCreated=true}
  end
  error('UNSUPPORTED_HOST_CAPABILITY')
end
local function handle(request)
  assert(request.epoch==epoch,'Host epoch changed');assert(type(request.expires)=='number' and os.time()<=request.expires,'REQUEST_EXPIRED')
  assert(type(request.method)=='string' and type(request.arguments)=='table','Invalid editor request')
  local allowed={['exports.start']={guard=true,outputPath=true,channels=true,sampleRate=true,bitDepth=true},['exports.status']={jobId=true},['exports.cancel']={jobId=true},['voicebanks.select']={voicebankId=true,guard=true},['selection.read']={groupUUID=true},['groups.create']={name=true,guard=true},['groups.remove']={groupUUID=true,guard=true},['notes.set_pitch']={groupUUID=true,noteIndex=true,midiPitch=true,guard=true},['pitch.read_range']={groupUUID=true,startBlick=true,endBlick=true},['pitch.replace_range']={groupUUID=true,startBlick=true,endBlick=true,points=true,guard=true,curveGuard=true}}
  allowed['notes.add']={groupUUID=true,onsetBlick=true,durationBlick=true,midiPitch=true,lyrics=true,guard=true}
  allowed['notes.remove']={groupUUID=true,noteIndex=true,guard=true}
  for key in pairs(request.arguments) do assert((allowed[request.method] or {})[key],'Unknown argument') end
  return {ok=true,result=invoke(request.method,request.arguments)}
end
local function poll()
  for _=1,8 do
    local result, failure = extend.request('editor.next', {epoch=epoch})
    if not result then error(failure and failure.code or 'Native editor disconnected') end
    local request=result.request
    if not request then break end
    local ok, response=pcall(handle,request)
    if not ok then
      if type(response)=='table' and type(response.code)=='string' then response={ok=false,error=response}
      else response={ok=false,error={code='EDITOR_REJECTED',message=tostring(response)}} end
    end
    local completed, reason=extend.request('editor.reply',{epoch=epoch,id=request.id,response=response})
    if not completed and (not reason or (reason.code~='EDITOR_REQUEST_EXPIRED' and reason.code~='EDITOR_PROVIDER_OFFLINE')) then error(reason and reason.code or 'Native editor reply failed') end
  end
  SV:setTimeout(15,poll)
end
poll()
