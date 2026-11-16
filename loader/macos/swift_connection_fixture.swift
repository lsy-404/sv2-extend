// Diagnostic resident package, never bundled in the production application.
import Foundation
import Network
import Darwin
private typealias Policy = @convention(c)(Int32)->Void
private typealias Scope = @convention(c)(Int32)->Int32
private typealias Count = @convention(c)()->UInt32
private func symbol<T>(_ name:String,_ type:T.Type)->T {unsafeBitCast(dlsym(UnsafeMutableRawPointer(bitPattern:-2),name)!,to:type)}
private func policy(_ blocked:Int32) {symbol("socket_gate_set_blocked",Policy.self)(blocked);symbol("network_framework_gate_set_blocked",Policy.self)(blocked)}
private let queue=DispatchQueue(label:"swift.connection.fixture")
private let queueKey=DispatchSpecificKey<Int>()
private func make(_ hostInit:Bool)->NWConnection {
 let port=NWEndpoint.Port(rawValue:UInt16(ProcessInfo.processInfo.environment["LOADER_SWIFT_PEER_PORT"]!)!)!
 return hostInit ? NWConnection(host:"127.0.0.1",port:port,using:.udp) : NWConnection(to:.hostPort(host:"127.0.0.1",port:port),using:.udp)
}
private func start(_ connection:NWConnection)->Bool {
 let done=DispatchSemaphore(value:0);var ready=false
 connection.stateUpdateHandler={state in if case .ready=state {ready=true;done.signal()}}
 connection.start(queue:queue);_ = done.wait(timeout:.now()+0.5);queue.sync{};connection.stateUpdateHandler=nil
 return ready
}
private func send(_ connection:NWConnection,_ text:String)->[String:Any] {
 let done=DispatchSemaphore(value:0);var errorValue=0;var onQueue=false;var count=0
 connection.send(content:Data(text.utf8),completion:.contentProcessed{error in
  count+=1;onQueue=DispatchQueue.getSpecific(key:queueKey)==1
  if error != nil {errorValue=1};done.signal()
 })
 let completed=done.wait(timeout:.now()+2) == .success;queue.sync{}
 return ["completed":completed,"error":errorValue,"correctQueue":onQueue,"callbackCount":count]
}
@_cdecl("swift_probe_start")
public func runSwiftConnectionFixture() {
 queue.setSpecific(key:queueKey,value:1)
 DispatchQueue.global().async {
  let count=symbol("loader_swift_network_outstanding",Count.self)
  let initial=count()
  var active:NWConnection?=make(true)
  let activeReady=start(active!);let before=send(active!,"swift-connection-before")
  var dormant:NWConnection?=make(false)
  let dormantTracked=count()>=initial+2
  policy(1)
  var during:NWConnection?=make(true)
  let scope=symbol("socket_gate_scope",Scope.self);let previous=scope(2)
  let beforePlugin=count();var plugin:NWConnection?=make(false);_ = scope(previous)
  let pluginExcluded=count()==beforePlugin
  // A local-domain endpoint must remain outside Internet policy accounting.
  let beforeUnix=count();var local:NWConnection?=NWConnection(to:.unix(path:"/tmp/extend-fixture-not-opened"),using:.tcp)
  let unixExcluded=count()==beforeUnix;local?.cancel();local=nil
  policy(0)
  let dormantReady=start(dormant!);dormant!.restart()
  let duringReady=start(during!);during!.restart()
  let oldSend=send(active!,"swift-connection-old-forbidden")
  let pluginReady=start(plugin!);let pluginSend=send(plugin!,"swift-connection-plugin")
  active?.cancel();active=nil;dormant?.cancel();dormant=nil;during?.cancel();during=nil;plugin?.cancel();plugin=nil
  var fresh:NWConnection?=make(false);let freshReady=start(fresh!);let restored=send(fresh!,"swift-connection-restored");fresh?.cancel();fresh=nil
  let result:[String:Any]=["pid":getpid(),"initial":initial,"activeReady":activeReady,"before":before,"tracked":dormantTracked,"dormantReadyAfterAllow":dormantReady,"createdWhileBlockedReady":duringReady,"oldSend":oldSend,"pluginExcluded":pluginExcluded,"unixExcluded":unixExcluded,"pluginReady":pluginReady,"pluginSend":pluginSend,"freshReady":freshReady,"restored":restored]
  DispatchQueue.global().asyncAfter(deadline:.now()+0.7){
   var final=result;final["outstandingAfterRelease"]=count()
   let data=try! JSONSerialization.data(withJSONObject:final,options:.prettyPrinted)
   try! data.write(to:URL(fileURLWithPath:ProcessInfo.processInfo.environment["LOADER_GROUP_RESULT"]!),options:.atomic)
  }
 }
}
