/* Open SV2 Loader JS Extend. ES5/Duktape, no external runtime or broker.
 * Load in an official script: var Extend = eval(Duktape.enc('opensv2.module.v1', ''));
 * Async callback signature is callback(error, result).
 */
(function () {
    'use strict';
    function asciiJSON(value) {
        return JSON.stringify(value).replace(/[\u007f-\uffff]/g, function (character) {
            return '\\u' + ('0000' + character.charCodeAt(0).toString(16)).slice(-4);
        });
    }
    function request(method, args) {
        try {
            var body = asciiJSON({method: method, arguments: args || {}});
            if (body.length > 1048576) return {ok: false, error: {code: 'REQUEST_TOO_LARGE'}};
            var response = JSON.parse(Duktape.enc('opensv2.v1', body));
            if (!response || typeof response.ok !== 'boolean') return {ok: false, error: {code: 'INVALID_NATIVE_RESPONSE'}};
            return response;
        } catch (error) {
            return {ok: false, error: {code: 'NATIVE_CALL_FAILED', message: String(error)}};
        }
    }
    function call(method, args, callback) {
        if (typeof callback !== 'function') throw new TypeError('A completion callback is required');
        var submitted = request('call.submit', {method: method, arguments: args || {}});
        if (!submitted.ok) { callback(submitted.error, null); return; }
        function poll() {
            var value = request('call.result', {ticket: submitted.result.ticket});
            if (!value.ok) { callback(value.error, null); return; }
            if (value.result.pending) { SV.setTimeout(15, poll); return; }
            var response = value.result.response;
            callback(response.ok ? null : response.error, response.ok ? response.result : null);
        }
        SV.setTimeout(0, poll);
    }
    function httpRequest(options, callback) {
        if (typeof callback !== 'function') throw new TypeError('A completion callback is required');
        var identifier = null, cancelRequested = false, finished = false;
        function complete(error, result) {
            if (finished) return;
            finished = true;
            if (cancelRequested) { callback({code: 'HTTP_CANCELLED'}, null); return; }
            callback(error, result);
        }
        function cancel() {
            if (finished || cancelRequested) return;
            cancelRequested = true;
            if (identifier) call('http.cancel', {requestId: identifier}, function () {});
        }
        call('http.start', options, function (error, result) {
            if (error) { complete(error, null); return; }
            identifier = result.requestId;
            if (cancelRequested) call('http.cancel', {requestId: identifier}, function () {});
            function poll() {
                call('http.status', {requestId: identifier}, function (error, status) {
                    if (error) { complete(error, null); return; }
                    if (status.pending) { SV.setTimeout(15, poll); return; }
                    var response = status.response;
                    complete(response.ok ? null : response.error, response.ok ? response.result : null);
                });
            }
            poll();
        });
        return {cancel: cancel};
    }
    return {
        apiVersion: 1,
        request: request,
        call: call,
        runtimeInfo: function () { return request('runtime.info'); },
        capabilities: function () { return request('runtime.capabilities'); },
        voicebanks: {
            current: function (callback) { return call('voicebanks.current', {}, callback); },
            list: function (callback) { return call('voicebanks.list', {}, callback); },
            select: function (identifier, guard, callback) { return call('voicebanks.select', {voicebankId: identifier, guard: guard}, callback); }
        },
        exports: {
            snapshot: function (callback) { return call('exports.snapshot', {}, callback); },
            start: function (options, callback) { return call('exports.start', options, callback); },
            status: function (jobId, callback) { return call('exports.status', {jobId: jobId}, callback); },
            cancel: function (jobId, callback) { return call('exports.cancel', {jobId: jobId}, callback); }
        },
        http: {
            get: function (url, callback) { return httpRequest({url: url}, callback); },
            request: httpRequest
        }
    };
}())
