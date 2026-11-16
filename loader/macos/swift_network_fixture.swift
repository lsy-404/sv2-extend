import Foundation
import Network
import Darwin
private final class WeakGroup {weak var value:NWConnectionGroup?;init(_ v:NWConnectionGroup){value=v}}
private var observed:[WeakGroup]=[]
private typealias Policy = @convention(c)(Int32)->Void
private typealias Scope = @convention(c)(Int32)->Int32
private typealias Count = @convention(c)()->UInt32
private func symbol<T>(_ name:String, _ type:T.Type)->T {unsafeBitCast(dlsym(UnsafeMutableRawPointer(bitPattern:-2),name)!,to:type)}
private func policy(_ value:Int32) {symbol("socket_gate_set_blocked",Policy.self)(value);symbol("network_framework_gate_set_blocked",Policy.self)(value)}
private func make(_ port:UInt16)->NWConnectionGroup {
 let parameters=NWParameters.udp
 (parameters.defaultProtocolStack.internetProtocol as? NWProtocolIP.Options)?.hopLimit=0
 parameters.allowLocalEndpointReuse=true
 return NWConnectionGroup(with:try! NWMulticastGroup(for:[.hostPort(host:"239.255.18.42",port:NWEndpoint.Port(rawValue:port)!)]),using:parameters)
}
private func transport(_ port:UInt16,_ tag:String,_ provided:NWConnectionGroup? = nil)->Bool {
 let group=provided ?? make(port);observed.append(WeakGroup(group));let queue=DispatchQueue(label:"swift.host."+tag);let done=DispatchSemaphore(value:0);var sent=false;var success=false
 group.setReceiveHandler(maximumMessageSize:4096,rejectOversizedMessages:true){[weak group] message,_,_ in
  guard !sent else{return};sent=true
  group?.send(content:Data(tag.utf8),to:message.remoteEndpoint){error in success=error==nil;done.signal()}
 }
 group.start(queue:queue);let completed=done.wait(timeout:.now()+3) == .success;group.cancel();queue.sync{group.setReceiveHandler(maximumMessageSize:4096,rejectOversizedMessages:true,handler:nil)};return completed && success
}
@_cdecl("swift_probe_start")
public func start() {
 DispatchQueue.global().async {
  let port=UInt16(ProcessInfo.processInfo.environment["LOADER_GROUP_PORT"]!)!
  let other=UInt16(ProcessInfo.processInfo.environment["LOADER_GROUP_SECOND_PORT"]!)!
  let count=symbol("loader_swift_network_outstanding",Count.self)
  let initialCount=count()
  let before=transport(port,"swift-before")
  var dormant:NWConnectionGroup?=make(other)
  observed.append(WeakGroup(dormant!))
  let tracked=count()>0
  policy(1)
  var duringBlocked:NWConnectionGroup?=make(other)
  let scope=symbol("socket_gate_scope",Scope.self);let previous=scope(2)
  let countBeforePlugin=count()
  var plugin:NWConnectionGroup?=make(UInt16(ProcessInfo.processInfo.environment["LOADER_GROUP_THIRD_PORT"]!)!)
  _=scope(previous)
  let pluginExcludedFromHostCount=count()==countBeforePlugin
  policy(0)
  let queue=DispatchQueue(label:"swift.host.dormant");var ready=false
  dormant!.stateUpdateHandler={state in if case .ready=state {ready=true}}
  dormant!.setReceiveHandler(maximumMessageSize:4096,rejectOversizedMessages:true){message,_,_ in dormant?.send(content:Data("swift-old-forbidden".utf8),to:message.remoteEndpoint){_ in}}
  dormant!.start(queue:queue);Thread.sleep(forTimeInterval:0.7);queue.sync{}
  let oldReady=ready;let conservative=count()>0
  var duringReady=false
  duringBlocked!.stateUpdateHandler={state in if case .ready=state {duringReady=true}}
  duringBlocked!.start(queue:queue);Thread.sleep(forTimeInterval:0.3);queue.sync{}
  duringBlocked!.stateUpdateHandler=nil;duringBlocked=nil
  let pluginTransport=transport(UInt16(ProcessInfo.processInfo.environment["LOADER_GROUP_THIRD_PORT"]!)!,"swift-plugin",plugin)
  plugin=nil
  dormant!.setReceiveHandler(maximumMessageSize:4096,rejectOversizedMessages:true,handler:nil);dormant!.stateUpdateHandler=nil;dormant=nil
  let restored=transport(port,"swift-restored");Thread.sleep(forTimeInterval:0.3)
  let result:[String:Any]=["before":before,"restored":restored,"oldReadyAfterAllow":oldReady,"tracked":tracked,"retainedObjectCounted":conservative,"outstandingAfterRelease":count(),"pid":getpid(),"createdWhileBlockedNotReady":!duringReady,"pluginScopeExcluded":pluginExcludedFromHostCount,"pluginScopeTransport":pluginTransport]
  DispatchQueue.global().asyncAfter(deadline:.now()+0.7) {
   var final=result;final["outstandingAfterRelease"]=count();final["initialCount"]=initialCount;final["testObjectsStillAlive"]=observed.map{$0.value != nil}
   let data=try! JSONSerialization.data(withJSONObject:final,options:.prettyPrinted)
   try! data.write(to:URL(fileURLWithPath:ProcessInfo.processInfo.environment["LOADER_GROUP_RESULT"]!),options:.atomic)
  }
 }
}
