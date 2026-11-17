package.path='plugins/lua-extend/lua/?.lua;'..package.path
local queue,replies,timer,last,attached={},{},nil,nil,nil
local reads={}
local extend={request=function(method,args)
 if method=='voice.native_status' then return {identityReadable=not VOICE_DISABLED} end
 if method=='notes.native_status' or method=='groups.native_status' or method=='export.native_status' then return {} end
 if method=='voice.capture.begin' then return {token='capture'} end
 if method=='voice.capture.end' then return last end
 if method=='editor.attach' then attached=args.methods;return {epoch='epoch'} end
 if method=='editor.next' then return {request=table.remove(queue,1)} end
 if method=='editor.reply' then replies[args.id]=args.response;return {} end
 return nil
end}
package.preload.sv2_extend=function() return extend end
local function group(id) return {getUUID=function() return id end} end
local function ref(id,voice,assigned)
 local g=group(id)
 return {getTarget=function() return g end,getVoice=function() reads[id]=(reads[id] or 0)+1;last={identityKey=voice,referenceKey='private-'..id,assigned=assigned,name=voice};return {} end}
end
local first={refs={ref('a','A',true),ref('b','empty',false),ref('c','B',true)}}
local second={refs={ref('d','B',true),ref('e','empty',false),ref('c','A',true)}}
local third={refs={ref('f','empty',false),ref('g','empty',false)}}
local tracks={first,second,third}
for _,track in ipairs(tracks) do
 function track:getNumGroups() return #self.refs end
 function track:getGroupReference(index) return self.refs[index] end
end
local project={getFileName=function() return 'example.svp' end,getNumTracks=function() return #tracks end,getTrack=function(_,i) return tracks[i] end}
SV={getHostInfo=function() return {hostVersionNumber=0x020300} end,getProject=function() return project end,setTimeout=function(_,_,fn) timer=fn end}
require('editor_provider')
local advertised=false;for _,name in ipairs(attached) do if name=='voicebanks.project' then advertised=true end end;if VOICE_DISABLED then assert(not advertised);return end;assert(advertised)
queue[1]={id='1',epoch='epoch',method='voicebanks.project',arguments={},expires=os.time()+4};timer()
local response=assert(replies['1']);assert(response.ok)
local value=response.result;assert(value.referenceCount==8 and value.assignedReferenceCount==6 and #value.voices==2)
local by={};for _,item in ipairs(value.voices) do by[item.voicebank.identityKey]=item;assert(item.voicebank.referenceKey==nil) end
assert(by.A.referenceCount==3 and by.B.referenceCount==3)
assert(value.projectContext=='["example.svp","a"]')
assert(reads.a==1 and reads.d==1 and reads.f==1 and reads.b==1 and reads.c==2 and reads.e==1 and reads.g==1)
queue[1]={id='2',epoch='epoch',method='voicebanks.project',arguments={unknown=true},expires=os.time()+4};timer();assert(not replies['2'].ok)
tracks={};queue[1]={id='3',epoch='epoch',method='voicebanks.project',arguments={},expires=os.time()+4};timer();assert(replies['3'].ok and #replies['3'].result.voices==0 and replies['3'].result.referenceCount==0)
