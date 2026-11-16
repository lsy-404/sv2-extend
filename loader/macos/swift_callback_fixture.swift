// Source-only resident diagnostic; callbacks capture no owning NWConnection.
import Foundation
import Network
import Darwin
private typealias Count = @convention(c)()->UInt32
private typealias Policy = @convention(c)(Int32)->Int32
private typealias State = @convention(c)(UnsafeMutablePointer<loader_network_state_v1>)->Int32
private func symbol<T>(_ name:String,_ type:T.Type)->T {unsafeBitCast(dlsym(UnsafeMutableRawPointer(bitPattern:-2),name)!,to:type)}
private let queue=DispatchQueue(label:"extend.swift.callback.fixture")
private let key=DispatchSpecificKey<Int>()
private final class Slot {var connection:NWConnection?;weak var witness:NWConnection?}
private func policy(_ blocked:Int32)->Bool {symbol("network_gate_set",Policy.self)(blocked)==0}
private func snapshot()->loader_network_state_v1 {var state=loader_network_state_v1();state.abi_version=1;state.struct_size=UInt32(MemoryLayout<loader_network_state_v1>.size);precondition(symbol("network_gate_state",State.self)(&state)==0);return state}
private func make(_ slot:Slot)->Bool {
 let port=NWEndpoint.Port(rawValue:UInt16(ProcessInfo.processInfo.environment["EXTEND_CALLBACK_PORT"]!)!)!
 let connection=NWConnection(host:"127.0.0.1",port:port,using:.udp)
 slot.connection=connection;slot.witness=connection
 let ready=DispatchSemaphore(value:0)
 connection.stateUpdateHandler={state in if case .ready=state {ready.signal()}}
 connection.start(queue:queue);let result=ready.wait(timeout:.now()+2) == .success
 queue.sync{};connection.stateUpdateHandler=nil;return result
}
private func submit(_ slot:Slot,_ mode:String,_ completion:@escaping (NWError?,Data?)->Void){
 if mode=="receive" {
  slot.connection!.receiveMessage{data,_,_,error in completion(error,data)}
  slot.connection!.send(content:Data("swift-callback-receive".utf8),completion:.idempotent)
 }else{slot.connection!.send(content:Data((mode=="queued" ? "swift-callback-queued":"swift-callback-held").utf8),completion:.contentProcessed{error in completion(error,nil)})}
}
private func runCase(_ mode:String)->[String:Any] {
 let count=symbol("loader_swift_network_outstanding",Count.self)
 let slot=Slot();let baseline=count();let ready=make(slot)
 let entered=DispatchSemaphore(value:0),release=DispatchSemaphore(value:0),finished=DispatchSemaphore(value:0)
 var replies=0,onQueue=false,success=false,bodyValid=false
 if mode=="queued" {queue.suspend()}
 submit(slot,mode){error,data in replies+=1;onQueue=DispatchQueue.getSpecific(key:key)==1;success=error==nil;bodyValid=data==Data("swift-callback-response".utf8);entered.signal();_ = release.wait(timeout:.now()+6);finished.signal()}
 var gotCallback=false,queuedCount:UInt32=0
 if mode=="queued" {queuedCount=count()}else{gotCallback=entered.wait(timeout:.now()+3) == .success}
 let guardedBeforeBlock=swift_probe_guarded_inet_count();let revoked=policy(1);slot.connection?.cancel();slot.connection=nil
 for _ in 0..<100 {if slot.witness==nil {break};usleep(10000)}
 let aliveWhileHeld=slot.witness != nil,heldCount=count();let heldState=snapshot()
 if mode=="queued" {queue.resume();gotCallback=entered.wait(timeout:.now()+3) == .success}
 release.signal();let returned=finished.wait(timeout:.now()+2) == .success;queue.sync{}
 for _ in 0..<100 {if count()==baseline {break};usleep(10000)}
 let restored=policy(0)
 return ["mode":mode,"guardedBeforeBlock":guardedBeforeBlock,"rawCoverageUnverified":(heldState.unverified_paths & UInt64(LOADER_NET_RAW_SOCKET)) != 0,"policyRevoked":revoked,"policyRestored":restored,"publicBlocked":heldState.requested_blocked,"publicDrainedWhileHeld":heldState.tracked_tasks_drained,"wholeHostVerified":heldState.whole_host_verified,"swiftUnverified":(heldState.unverified_paths & UInt64(LOADER_NET_SWIFT_NETWORK)) != 0,"ready":ready,"gotCallback":gotCallback,"success":success,"bodyValid":bodyValid,"correctQueue":onQueue,"replies":replies,"returned":returned,"baseline":baseline,"weakConnectionAliveWhileHeld":aliveWhileHeld,"outstandingWhileHeld":heldCount,"outstandingWhileQueuePaused":queuedCount,"outstandingAfterReturn":count()]
}
@_cdecl("swift_probe_start") public func runSwiftCallbackFixture(){
 queue.setSpecific(key:key,value:1)
 DispatchQueue.global().async {
  let result:[String:Any]=["pid":getpid(),"cases":[runCase("send"),runCase("receive"),runCase("queued")]]
  try! JSONSerialization.data(withJSONObject:result,options:.prettyPrinted).write(to:URL(fileURLWithPath:ProcessInfo.processInfo.environment["EXTEND_CALLBACK_RESULT"]!),options:.atomic)
 }
}
