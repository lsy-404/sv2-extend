package.path='plugins/lua-extend/lua/?.lua;'..package.path
local queue,replies,timer,last={},{},nil,nil
local extend={request=function(method,args)
 if method=='voice.native_status' then return {identityReadable=true,catalogReadable=true,selectable=true} end
 if method=='notes.native_status' or method=='groups.native_status' or method=='export.native_status' then return {} end
 if method=='voice.capture.begin' then return {token='capture'} end
 if method=='voice.capture.end' then return last end
 if method=='voice.select.validate' then return {identityKey=args.voicebankId} end
 if method=='editor.attach' then return {epoch='epoch'} end
 if method=='editor.next' then return {request=table.remove(queue,1)} end
 if method=='editor.reply' then replies[args.id]=args.response;return {} end
 return nil
end}
package.preload.sv2_extend=function() return extend end
local function group(id) return {getUUID=function() return id end,getNumNotes=function() return 0 end,getName=function() return id end} end
local function ref(g,identity,assigned)
 return {getTarget=function() return g end,getVoice=function() last={identityKey=identity,referenceKey='ref-'..g:getUUID(),assigned=assigned};return {} end,isMain=function() return false end}
end
local a,b,c=group('a'),group('b'),group('c')
local first={refs={ref(a,'voice-a',true)}}
local second={refs={ref(b,'voice-b',true),ref(c,'empty',false)}}
for _,track in ipairs({first,second}) do
 function track:getIndexInParent() return 1 end
 function track:getNumGroups() return #self.refs end
 function track:getGroupReference(index) return self.refs[index] end
end
local project={getFileName=function() return '' end,getNumTracks=function() return 2 end,getTrack=function(_,index) return ({first,second})[index] end,getNoteGroup=function(_,id) return ({a=a,b=b,c=c})[id] end}
local editor={getCurrentGroup=function() return first.refs[1] end,getCurrentTrack=function() return first end}
SV={getHostInfo=function() return {hostVersionNumber=0x020300} end,getProject=function() return project end,getMainEditor=function() return editor end,setTimeout=function(_,_,fn) timer=fn end}
require('editor_provider')
local nextid=0
local function call(method,args)
 nextid=nextid+1;local id=tostring(nextid);queue[#queue+1]={id=id,epoch='epoch',method=method,arguments=args or {},expires=os.time()+4};timer();return assert(replies[id])
end
local current=call('voicebanks.current',{groupUUID='c'});assert(current.ok and current.result.voicebank.identityKey=='voice-b' and current.result.inherited)
local selected=call('voicebanks.select',{groupUUID='c',voicebankId='voice-b',guard=current.result.guard});assert(selected.ok and selected.result.voicebank.identityKey=='voice-b' and selected.result.undoRecordCreated==false)
local invalid=call('voicebanks.current',{groupUUID='c',unexpected=true});assert(not invalid.ok)
first.refs[2]=ref(c,'other',true)
local ambiguous=call('voicebanks.current',{groupUUID='c'});assert(not ambiguous.ok and ambiguous.error.code=='GROUP_REFERENCE_AMBIGUOUS')
local shared=call('selection.read',{groupUUID='c'});assert(shared.ok)
