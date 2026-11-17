package.path='plugins/lua-extend/lua/?.lua;'..package.path
local function run(code,recover)
 local queued=true
 local nextCalls,replyCalls,timerCalls=0,0,0
 local timer
 local extend={request=function(method,args)
  if method=='editor.attach' then return {epoch='epoch'} end
  if method=='editor.next' then
   assert(args.epoch=='epoch');nextCalls=nextCalls+1
   if queued then queued=false;return {request={id='1',epoch='epoch',method='unsupported',arguments={},expires=os.time()+4}} end
   return {}
  end
  if method=='editor.reply' then assert(args.epoch=='epoch' and args.id=='1');replyCalls=replyCalls+1;return nil,{code=code} end
  return nil
 end}
 package.loaded.sv2_extend=nil
 package.preload.sv2_extend=function() return extend end
 SV={getHostInfo=function() return {hostVersionNumber=0x020300} end,setTimeout=function(_,_,fn) timerCalls=timerCalls+1;timer=fn end}
 local ok,err=pcall(assert(loadfile('plugins/lua-extend/lua/editor_provider.lua')))
 if recover then
  assert(ok and nextCalls==2 and replyCalls==1 and timerCalls==1 and timer)
  timer();assert(nextCalls==3 and replyCalls==1 and timerCalls==2)
 else
  assert(not ok and tostring(err):find(code,1,true) and nextCalls==1 and replyCalls==1 and timerCalls==0)
 end
end
run('EDITOR_REQUEST_EXPIRED',true)
run('EDITOR_PROVIDER_OFFLINE',true)
run('EDITOR_EPOCH_MISMATCH',false)
run('INVALID_ARGUMENT',false)
