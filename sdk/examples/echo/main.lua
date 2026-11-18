local extend = ...
local handlers = {}

-- Answers every request to the package's own listener.
function handlers.serve(request, reply)
  reply({
    status = 200,
    headers = {['Content-Type'] = 'text/plain; charset=utf-8'},
    body = request.method .. ' ' .. request.path .. ' ' .. (request.body or ''),
  })
end

function handlers.start(_, reply)
  extend.request('server.listen', {handler = 'serve'}, function(server, failure)
    reply(server, failure)
  end)
end

function handlers.stop(_, reply)
  extend.request('server.close', {}, function(_, failure) reply({}, failure) end)
end

function handlers.fetch(arguments, reply)
  extend.request('http.request', arguments, function(response, failure)
    reply(response, failure)
  end)
end

return handlers
