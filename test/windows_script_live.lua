function getClientInfo()
  return {name='Extend Runtime Verification',author='SV2 Extend',category='SV2 Extend',versionNumber=1,minEditorVersion=0x020300}
end

function main()
  local root = os.getenv('OPEN_SV2_HOST_DATA_DIR')
  assert(type(root)=='string' and root~='', 'Managed host data directory is unavailable')
  package.path = root:gsub('\\','/') .. '/Local/SV2Extend/EditorLua/?.lua;' .. package.path
  local extend = require('sv2_extend')
  local json = require('sv2_extend_json')
  local checks = {}
  local function finish()
    SV:print('EXTEND_LUA_VERIFICATION ' .. json.encode(checks))
    SV:finish()
  end
  local info, infoError = extend.runtimeInfo()
  checks.runtimeInfo = info ~= nil and info.apiVersion == 1 and info.mainThread == true and info.usesFilesOrPipes == false
  checks.runtimeInfoError = infoError and infoError.code or nil
  local capabilities, capabilityError = extend.capabilities()
  if not capabilities then
    checks.disabled = capabilityError and capabilityError.code == 'EXTENSION_DISABLED' or false
    local _, denial = extend.request('call.submit', {method='selection.read', arguments=json.object({})})
    checks.disabledSubmit = denial and denial.code == 'EXTENSION_DISABLED' or false
    finish()
    return
  end
  checks.capabilities = type(capabilities.methods) == 'table'
  local found = false
  for _, method in ipairs(capabilities.methods or {}) do if method == 'selection.read' then found = true end end
  checks.selectionAdvertised = found
  local submitted, failure = extend.request('call.submit', {method='selection.read', arguments=json.object({})})
  checks.submitted = submitted ~= nil and type(submitted.ticket) == 'number'
  checks.submitError = failure and failure.code or nil
  if not checks.submitted then finish(); return end
  local remaining = 400
  local function poll()
    local value, err = extend.request('call.result', {ticket=submitted.ticket})
    if not value then checks.replyError = err and err.code or 'UNKNOWN'; finish(); return end
    if value.pending then
      remaining = remaining - 1
      if remaining <= 0 then checks.replyError='TIMEOUT'; finish(); return end
      SV:setTimeout(15, poll)
      return
    end
    checks.reply = type(value.response)=='table' and value.response.ok == true
    checks.replyError = value.response and value.response.error and value.response.error.code or nil
    local _, expired = extend.request('call.result', {ticket=submitted.ticket})
    checks.consumed = expired and expired.code == 'CALL_TICKET_EXPIRED' or false
    finish()
  end
  SV:setTimeout(0, poll)
end
