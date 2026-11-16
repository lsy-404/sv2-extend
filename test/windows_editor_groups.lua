package.path='plugins/lua-extend/lua/?.lua;'..package.path
local queue,replies,timer={},{}
local extend={request=function(method,args)
 if method=='notes.native_status' then return {addable=true,removable=true} end
 if method=='groups.native_status' then return {available=true} end
 if method=='editor.attach' then return {epoch='epoch'} end
 if method=='editor.next' then return {request=table.remove(queue,1)} end
 if method=='editor.reply' then replies[args.id]=args.response;return {} end
 return nil
end}
package.preload.sv2_extend=function() return extend end
local function group(uuid,name)
 local g={uuid=uuid,name=name,notes={}}
 function g:getUUID() return self.uuid end
 function g:getName() return self.name end
 function g:setName(value) self.name=value end
 function g:getNumNotes() return #self.notes end
 function g:getNote(i) return self.notes[i] end
 function g:addNote(note) self.notes[#self.notes+1]=note;return #self.notes end
 function g:removeNote(i) table.remove(self.notes,i) end
 return g
end
local function reference(target,main)
 local r={target=target,main=main}
 function r:getTarget() return self.target end
 function r:setTarget(value) self.target=value end
 function r:isMain() return self.main or false end
 function r:getVoice() return {} end
 return r
end
local main=group('main','Main');local mainRef=reference(main,true)
local track={groups={mainRef}}
function track:getIndexInParent() return 1 end
function track:getNumGroups() return #self.groups end
function track:getGroupReference(i) return self.groups[i] end
function track:addGroupReference(ref) self.groups[#self.groups+1]=ref;return #self.groups end
local project={library={}}
function project:getFileName() return 'test' end
function project:getNumTracks() return 1 end
function project:getTrack() return track end
function project:newUndoRecord() end
function project:addNoteGroup(g) self.library[#self.library+1]=g;return #self.library end
function project:getNumNoteGroupsInLibrary() return #self.library end
function project:getNoteGroup(id) if type(id)=='number' then return self.library[id] end;for _,g in ipairs(self.library) do if g.uuid==id then return g end end end
function project:removeNoteGroup(index) local g=table.remove(self.library,index);for i=#track.groups,1,-1 do if track.groups[i].target==g then table.remove(track.groups,i) end end end
local editor={getCurrentGroup=function() return mainRef end,getCurrentTrack=function() return track end}
SV={}
function SV:getHostInfo() return {hostVersionNumber=0x020300} end
function SV:getProject() return project end
function SV:getMainEditor() return editor end
function SV:setTimeout(_,fn) timer=fn end
function SV:create(kind)
 if kind=='NoteGroup' then return group('created','') end
 if kind=='NoteGroupReference' then return reference(nil,false) end
 if kind=='Note' then local n={};function n:setOnset(v) self.onset=v end;function n:getOnset() return self.onset end;function n:setDuration(v) self.duration=v end;function n:getDuration() return self.duration end;function n:setPitch(v) self.pitch=v end;function n:getPitch() return self.pitch end;function n:setLyrics(v) self.lyrics=v end;function n:getLyrics() return self.lyrics end;return n end
 error('Unexpected type')
end
require('editor_provider')
local nextid=0
local function call(method,args)
 nextid=nextid+1;local id=tostring(nextid);queue[#queue+1]={id=id,epoch='epoch',method=method,arguments=args or {},expires=os.time()+4};timer();return assert(replies[id])
end
local mainRead=call('selection.read');assert(mainRead.ok and mainRead.result.isMain)
local denied=call('notes.add',{guard=mainRead.result.guard,onsetBlick=0,durationBlick=100,midiPitch=60});assert(not denied.ok and denied.error.code=='MAIN_GROUP_EDIT_UNSUPPORTED')
local deniedExplicit=call('notes.add',{groupUUID='main',guard=mainRead.result.guard,onsetBlick=0,durationBlick=100,midiPitch=60});assert(not deniedExplicit.ok and deniedExplicit.error.code=='MAIN_GROUP_EDIT_UNSUPPORTED')
local created=call('groups.create',{guard=mainRead.result.guard,name='Test'});assert(created.ok and created.result.groupUUID=='created' and #track.groups==2)
local id=created.result.groupUUID;local added=call('notes.add',{groupUUID=id,guard=created.result.guard,onsetBlick=0,durationBlick=100,midiPitch=60});assert(added.ok and added.result.noteCount==1)
local read=call('selection.read',{groupUUID=id});assert(read.ok and read.result.notes[1].midiPitch==60)
track.groups[#track.groups+1]=reference(project:getNoteGroup(id),false);assert(call('selection.read',{groupUUID=id}).ok)
local shared=call('notes.remove',{groupUUID=id,guard=read.result.guard,noteIndex=1});assert(not shared.ok and #project:getNoteGroup(id).notes==1);table.remove(track.groups)
local stale=call('notes.remove',{groupUUID=id,guard=created.result.guard,noteIndex=1});assert(not stale.ok)
local removed=call('notes.remove',{groupUUID=id,guard=read.result.guard,noteIndex=1});assert(removed.ok and removed.result.noteCount==0)
local deleted=call('groups.remove',{groupUUID=id,guard=removed.result.guard});assert(deleted.ok and deleted.result.removed and #track.groups==1 and #project.library==0)
