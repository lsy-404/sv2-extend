-- Lua Extend: public Lua facade over Loader's resident native API.
-- The private call trampoline does not read/write environment variables.
local json = require('sv2_extend_json')
local native_call = os.getenv
local prefix = '__OPEN_SV2_NATIVE_CALL_V1__:'
local M = {apiVersion = 1}
local function objectArguments(value)
  if value == nil then return json.object({}) end
  if type(value) ~= 'table' then return nil end
  local result = {}
  for key, item in pairs(value) do
    if type(key) ~= 'string' then return nil end
    result[key] = item
  end
  return json.object(result)
end
function M.request(method, arguments)
  if type(method) ~= 'string' or method == '' then
    return nil, {code = 'INVALID_ARGUMENT', message = 'A method name is required.'}
  end
  local args = objectArguments(arguments)
  if not args then return nil, {code='INVALID_ARGUMENT'} end
  local valid, encoded = pcall(json.encode, {method = method, arguments = args})
  if not valid then return nil, {code='INVALID_ARGUMENT'} end
  if #encoded > (method == 'editor.reply' and 1048576 or 262144) then return nil, {code = 'REQUEST_TOO_LARGE'} end
  local response = native_call(prefix .. encoded)
  if not response then return nil, {code = 'LOADER_UNAVAILABLE'} end
  local ok, value = pcall(json.decode, response)
  if not ok or type(value) ~= 'table' or type(value.ok) ~= 'boolean' then
    return nil, {code = 'INVALID_NATIVE_RESPONSE'}
  end
  if not value.ok then return nil, value.error or {code = 'NATIVE_CALL_FAILED'} end
  return value.result
end
-- Async public capabilities share Loader's routing with MCP. A callback receives
-- (result, error). It never waits on the host UI thread or uses a file broker.
function M.call(method, arguments, callback)
  assert(type(callback) == 'function', 'A completion callback is required')
  local args = objectArguments(arguments)
  if not args then callback(nil, {code='INVALID_ARGUMENT'}); return end
  local submitted, failure = M.request('call.submit', {method=method, arguments=args})
  if not submitted then callback(nil, failure); return end
  local function poll()
    local value, why = M.request('call.result', {ticket=submitted.ticket})
    if not value then callback(nil, why); return end
    if value.pending then SV:setTimeout(15, poll); return end
    local response=value.response
    if response.ok then callback(response.result, nil) else callback(nil, response.error) end
  end
  SV:setTimeout(0, poll)
end
M.http = {}
function M.http.request(options, callback)
  assert(type(callback) == 'function', 'A completion callback is required')
  local identifier, cancelRequested, finished = nil, false, false
  local function complete(result, failure)
    if finished then return end
    finished = true
    if cancelRequested then callback(nil, {code='HTTP_CANCELLED'}); return end
    callback(result, failure)
  end
  local handle = {}
  function handle.cancel()
    if finished or cancelRequested then return end
    cancelRequested = true
    if identifier then M.call('http.cancel', {requestId=identifier}, function() end) end
  end
  M.call('http.start', options, function(started, failure)
    if not started then complete(nil, failure); return end
    identifier = started.requestId
    if cancelRequested then M.call('http.cancel', {requestId=identifier}, function() end) end
    local function poll()
      M.call('http.status', {requestId=identifier}, function(status, failure)
        if not status then complete(nil, failure); return end
        if status.pending then SV:setTimeout(15, poll); return end
        local response = status.response
        if response.ok then complete(response.result, nil) else complete(nil, response.error) end
      end)
    end
    poll()
  end)
  return handle
end
function M.http.get(url, callback) return M.http.request({url=url}, callback) end
M.voicebanks = {}
function M.voicebanks.current(callback) return M.call('voicebanks.current', {}, callback) end
function M.voicebanks.list(callback) return M.call('voicebanks.list', {}, callback) end
function M.voicebanks.select(identifier, guard, callback) return M.call('voicebanks.select', {voicebankId=identifier,guard=guard}, callback) end
M.exports = {}
function M.exports.snapshot(callback) return M.call('exports.snapshot', {}, callback) end
function M.exports.start(options, callback) return M.call('exports.start', options, callback) end
function M.exports.status(jobId, callback) return M.call('exports.status', {jobId=jobId}, callback) end
function M.exports.cancel(jobId, callback) return M.call('exports.cancel', {jobId=jobId}, callback) end
function M.runtimeInfo() return M.request('runtime.info') end
function M.capabilities() return M.request('runtime.capabilities') end
-- Voicebank and export APIs will only be advertised after native adapters pass
-- real-host validation. No directory/catalog fallback impersonates those APIs.
return M
