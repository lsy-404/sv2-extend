import AppKit
import Foundation
let app = URL(fileURLWithPath:CommandLine.arguments[1])
let environment = try JSONDecoder().decode([String:String].self,from:Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[2])))
let configuration = NSWorkspace.OpenConfiguration()
configuration.environment = environment
configuration.createsNewApplicationInstance = true
configuration.activates = false
configuration.addsToRecentItems = false
var completed = false
var status:Int32 = 1
NSWorkspace.shared.openApplication(at:app,configuration:configuration) { application,error in
    if let application = application {
        print("{\"pid\":\(application.processIdentifier)}")
        status=0
    } else {
        fputs("Fixture launch failed: \(error?.localizedDescription ?? "unknown")\n",stderr)
    }
    completed=true
}
while !completed { RunLoop.current.run(until:Date(timeIntervalSinceNow:0.05)) }
exit(status)
