local extend = ...
local handlers = {}

function handlers.increment(arguments, reply)
  extend.request('storage.read', {key = 'count'}, function(stored, failure)
    if failure then reply(nil, failure) return end
    local count = (stored.found and stored.value or 0) + (arguments.by or 1)
    extend.request('storage.write', {key = 'count', value = count}, function(_, writeFailure)
      if writeFailure then reply(nil, writeFailure) else reply({count = count}) end
    end)
  end)
end

return handlers
