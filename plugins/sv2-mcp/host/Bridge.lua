-- SPDX-License-Identifier: Apache-2.0
-- Runs only in SV2's official Lua VM. Both pipe endpoints belong to the same host PID.
local root = assert(os.getenv('OPEN_SV2_LOADER_DATA_DIR'), 'Loader data directory unavailable')
local json = dofile(root .. '/plugins/sv2-mcp/host/json.lua')
local epoch = assert(os.getenv('OPEN_SV2_EDITOR_EPOCH'), 'Loader epoch unavailable')
if rawget(_G, "__OPEN_SV2_LOADER_PIPE_EPOCH") == epoch then return end
rawset(_G, "__OPEN_SV2_LOADER_PIPE_EPOCH", epoch)
local input = assert(io.open('/dev/fd/' .. assert(os.getenv('OPEN_SV2_EDITOR_REQUEST_FD')), 'r'))
local output = assert(io.open('/dev/fd/' .. assert(os.getenv('OPEN_SV2_EDITOR_RESPONSE_FD')), 'w'))
input:setvbuf('no'); output:setvbuf('no')
local alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
local function b64encode(data)
  local result = {}
  for i = 1, #data, 3 do
    local a, b, c = data:byte(i, i + 2); b = b or 0; c = c or 0
    local n = (a << 16) | (b << 8) | c
    result[#result + 1] = alphabet:sub(((n >> 18) & 63) + 1, ((n >> 18) & 63) + 1)
    result[#result + 1] = alphabet:sub(((n >> 12) & 63) + 1, ((n >> 12) & 63) + 1)
    result[#result + 1] = i + 1 <= #data and alphabet:sub(((n >> 6) & 63) + 1, ((n >> 6) & 63) + 1) or '='
    result[#result + 1] = i + 2 <= #data and alphabet:sub((n & 63) + 1, (n & 63) + 1) or '='
  end
  return table.concat(result)
end
local reverse = {}; for i = 1, #alphabet do reverse[alphabet:sub(i, i)] = i - 1 end
local function b64decode(data)
  local result = {}
  for i = 1, #data, 4 do
    local a,b,c,d=data:sub(i,i),data:sub(i+1,i+1),data:sub(i+2,i+2),data:sub(i+3,i+3)
    assert(reverse[a] and reverse[b], 'Invalid frame encoding')
    local n=(reverse[a]<<18)|(reverse[b]<<12)|((reverse[c] or 0)<<6)|(reverse[d] or 0)
    result[#result+1]=string.char((n>>16)&255)
    if c~='=' then result[#result+1]=string.char((n>>8)&255) end
    if d~='=' then result[#result+1]=string.char(n&255) end
  end
  return table.concat(result)
end
local outgoing, assemblies = {}, {}
local function send(identifier, value)
  local data=json.encode(value); assert(#data<=524288,'Response too large')
  local sequence=0
  for start=1,#data,192 do
    local chunk=data:sub(start,start+191)
    outgoing[#outgoing+1]=string.format('%s %d %d %s\n',identifier,sequence,start+#chunk>#data and 1 or 0,b64encode(chunk));sequence=sequence+1
  end
end
local methods={'selection.read','notes.set_pitch','pitch.read_range','pitch.replace_range','transport.status','transport.play','transport.stop'}
local function integer(value, low, high)
  assert(type(value)=='number' and value==math.floor(value) and value>=low and value<=high,'Integer out of range');return value
end
local function selected()
  local reference=SV:getMainEditor():getCurrentGroup();assert(reference,'No current note group')
  local group=reference:getTarget();assert(group,'No current note group target');return reference,group
end
local function guard(group)
  local notes={}
  for i=1,group:getNumNotes() do
    local note=group:getNote(i);notes[i]={note:getOnset(),note:getDuration(),note:getPitch()}
  end
  assert(#notes<=4096,'Group exceeds edit guard limit')
  return json.encode({epoch,SV:getProject():getFileName(),group:getUUID(),notes})
end
local function exclusive(group)
  local project=SV:getProject();local count=0
  for t=1,project:getNumTracks() do local track=project:getTrack(t)
    for g=1,track:getNumGroups() do if track:getGroupReference(g):getTarget():getUUID()==group:getUUID() then count=count+1 end end
  end
  assert(count==1,'Shared group content edits require an explicit shared-group policy')
end
local function invoke(method,args)
  if method=='transport.status' then local transport=SV:getPlayback();return {status=transport:getStatus(),playhead=transport:getPlayhead()} end
  if method=='transport.play' then SV:getPlayback():play();return {status=SV:getPlayback():getStatus()} end
  if method=='transport.stop' then SV:getPlayback():stop();return {status=SV:getPlayback():getStatus()} end
  local reference,group=selected()
  if method=='selection.read' then
    local notes={};for i=1,math.min(group:getNumNotes(),128) do local note=group:getNote(i);notes[#notes+1]={index=i,onsetBlick=note:getOnset(),durationBlick=note:getDuration(),midiPitch=note:getPitch()} end
    return {groupUUID=group:getUUID(),noteCount=group:getNumNotes(),notes=notes,guard=guard(group),voiceParameters=reference:getVoice(),voicebankIdentityReadable=false}
  end
  if method=='pitch.read_range' then
    local start=integer(args.startBlick,0,9007199254740991);local finish=integer(args.endBlick,start+1,9007199254740991)
    local automation=group:getParameter('pitchDelta');local points=automation:getPoints(start,finish);assert(#points<=4096,'Pitch range too large')
    return {points=points,curveGuard=json.encode(points),guard=guard(group),range=automation:getDefinition().range}
  end
  assert(type(args.guard)=='string' and args.guard==guard(group),'STALE_EDIT_GUARD')
  exclusive(group)
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
    SV:getProject():newUndoRecord();automation:remove(start,finish)
    for _,pair in ipairs(points) do automation:add(pair[1],pair[2]) end
    local after=automation:getPoints(start,finish);assert(json.encode(after)==json.encode(points),'Curve readback failed; undo required')
    return {before=before,after=after,guard=guard(group),curveGuard=json.encode(after),undoRecordCreated=true}
  end
  error('UNSUPPORTED_HOST_CAPABILITY')
end
local function handle(request)
  assert(request.epoch==epoch,'Host epoch changed');assert(type(request.expires)=='number' and os.time()<=request.expires,'REQUEST_EXPIRED')
  assert(type(request.method)=='string' and type(request.arguments)=='table','Invalid editor request')
  local allowed={['notes.set_pitch']={noteIndex=true,midiPitch=true,guard=true},['pitch.read_range']={startBlick=true,endBlick=true},['pitch.replace_range']={startBlick=true,endBlick=true,points=true,guard=true,curveGuard=true}}
  for key in pairs(request.arguments) do assert((allowed[request.method] or {})[key],'Unknown argument') end
  return {ok=true,result=invoke(request.method,request.arguments)}
end
local function flush()
  for _=1,32 do
    if #outgoing==0 then break end
    if not output:write(outgoing[1]) then break end
    table.remove(outgoing,1)
  end
end
local function poll()
  flush()
  for _=1,32 do
    local line=input:read('*l');if not line then break end
    local identifier,sequence,final,encoded=line:match('^(%x+) (%d+) ([01]) ([A-Za-z0-9+/=]+)$')
    if identifier then
      local assembly=assemblies[identifier] or {sequence=0,parts={},size=0};assemblies[identifier]=assembly
      if tonumber(sequence)==assembly.sequence then
        local ok,chunk=pcall(b64decode,encoded)
        if ok then assembly.parts[#assembly.parts+1]=chunk;assembly.size=assembly.size+#chunk;assembly.sequence=assembly.sequence+1 end
        if not ok or assembly.size>262144 then assemblies[identifier]=nil
        elseif final=='1' then
          assemblies[identifier]=nil
          local valid,request=pcall(json.decode,table.concat(assembly.parts));local success,result
          if valid then success,result=pcall(handle,request) end
          send(identifier,success and result or {ok=false,error={code='EDITOR_REJECTED',message=tostring(result or 'Invalid JSON request')}})
        end
      else assemblies[identifier]=nil end
    end
  end
  flush();SV:setTimeout(25,poll)
end
send(string.rep('0',32),{kind='ready',epoch=epoch,methods=methods,editorVersion=SV:getHostInfo().hostVersionNumber})
poll()
