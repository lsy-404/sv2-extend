#pragma once
#include <memory>
#include <string>
#include "../../../loader/windows/vendor/json.hpp"
#include "../../../sdk/include/loader_api.h"

namespace windows_mcp {
using json = nlohmann::json;
struct Request {
 const loader_api_v1 *api;
 loader_json_completion complete;
 void *context;
 json id, params;
 bool list;
};
inline void finish(Request *request, const json &body) {
 auto encoded=body.dump(); request->complete(encoded.data(),encoded.size(),request->context); delete request;
}
inline void error(Request *request,int code,const char *message) {
 finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"error",{{"code",code},{"message",message}}}});
}
inline void called(const char *body,size_t length,void *opaque) {
 auto request=static_cast<Request*>(opaque);
 auto value=body?json::parse(body,body+length,nullptr,false):json();
 if(!value.is_object() || !value.contains("ok") || !value["ok"].is_boolean()) {error(request,-32603,"Invalid provider response");return;}
 json result={{"content",json::array({{{"type","text"},{"text",value.dump()}}})},{"structuredContent",value},{"isError",!value["ok"].get<bool>()}};
 finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",result}});
}
inline void capabilities(const char *body,size_t length,void *opaque) {
 auto request=static_cast<Request*>(opaque);
 auto value=body?json::parse(body,body+length,nullptr,false):json();
 if(!value.is_object() || !value.contains("ok") || !value["ok"].is_boolean() || !value["ok"].get<bool>() || !value.contains("result") || !value["result"].is_object() || !value["result"].contains("providers") || !value["result"]["providers"].is_array()) {error(request,-32603,"Capability registry unavailable");return;}
 json tools=json::array();std::string method;
 for(const auto &provider:value["result"]["providers"]) {
  if(!provider.is_object() || !provider.contains("capabilities") || !provider["capabilities"].is_object()) continue;
  const auto &caps=provider["capabilities"];
  if(!caps.contains("methods") || !caps["methods"].is_array() || !caps.contains("tools") || !caps["tools"].is_array()) continue;
  for(const auto &definition:caps["tools"]) {
   if(!definition.is_object() || !definition.contains("name") || !definition["name"].is_string() || !definition.contains("method") || !definition["method"].is_string() || !definition.contains("description") || !definition["description"].is_string() || !definition.contains("inputSchema") || !definition["inputSchema"].is_object()) continue;
   bool available=false;for(const auto &entry:caps["methods"])if(entry==definition["method"])available=true;
   if(!available)continue;
   if(request->list){auto tool=definition;tool.erase("method");tools.push_back(tool);}
   else if(definition["name"]==request->params["name"])method=definition["method"].get<std::string>();
  }
 }
 if(request->list){finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",{{"tools",tools}}}});return;}
 if(method.empty()){error(request,-32602,"Unknown or unavailable tool");return;}
 auto arguments=request->params.value("arguments",json::object()).dump();
 request->api->request_capability_json(method.c_str(),arguments.c_str(),called,request);
}
inline void handle(const loader_api_v1 *api,const char *body,size_t length,loader_json_completion complete,void *context) {
 if(!complete)return;
 auto request=new Request{api,complete,context,nullptr,json::object(),false};
 if(!body || length>1048576){error(request,-32600,"Invalid request");return;}
 auto value=json::parse(body,body+length,nullptr,false);
 if(!value.is_object() || !value.contains("jsonrpc") || value["jsonrpc"]!="2.0" || !value.contains("method") || !value["method"].is_string()){error(request,-32600,"Invalid request");return;}
 if(!value.contains("id")){complete(nullptr,0,context);delete request;return;}
 request->id=value["id"];
 if(!request->id.is_string() && !request->id.is_number_integer()){request->id=nullptr;error(request,-32600,"Invalid request id");return;}
 if(value.contains("params") && !value["params"].is_object()){error(request,-32602,"Invalid parameters");return;}
 request->params=value.value("params",json::object());auto method=value["method"].get<std::string>();
 if(method=="initialize") {
  auto version=request->params.value("protocolVersion",json("2025-11-25"));
  if(!version.is_string() || (version!="2025-11-25" && version!="2025-06-18" && version!="2025-03-26"))version="2025-11-25";
  finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",{{"protocolVersion",version},{"capabilities",{{"tools",{{"listChanged",false}}}}},{"serverInfo",{{"name","sv2-mcp"},{"version","0.3.0"}}}}}});return;
 }
 if(method=="ping"){finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",json::object()}});return;}
 if(method!="tools/list" && method!="tools/call"){error(request,-32601,"Method not found");return;}
 request->list=method=="tools/list";
 if(!request->list && (!request->params.contains("name") || !request->params["name"].is_string() || (request->params.contains("arguments") && !request->params["arguments"].is_object()))){error(request,-32602,"Invalid tool parameters");return;}
 if(!api || !api->request_capability_json){error(request,-32603,"Capability registry unavailable");return;}
 api->request_capability_json("editor.capabilities","{}",capabilities,request);
}
}
