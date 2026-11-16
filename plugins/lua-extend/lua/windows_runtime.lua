function getClientInfo()
  return {name='SV2 Extend Runtime',author='SV2 Extend',category='SV2 Extend',versionNumber=1,minEditorVersion=0x020300}
end

function main()
  local root = os.getenv('OPEN_SV2_HOST_DATA_DIR')
  assert(type(root)=='string' and root~='', 'Managed host data directory is unavailable')
  local modules = root:gsub('\\','/') .. '/Local/SV2Extend/EditorLua/'
  package.path = modules .. '?.lua;' .. package.path
  require('editor_provider')
end
