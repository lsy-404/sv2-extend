function getClientInfo() {
    return {name: 'Extend JavaScript Verification', author: 'SV2 Extend', category: 'SV2 Extend', versionNumber: 1, minEditorVersion: 0x020300};
}
function main() {
    var checks = {};
    function finish() { SV.print('EXTEND_JS_VERIFICATION ' + JSON.stringify(checks)); SV.finish(); }
    try {
        checks.originalEncoding = Duktape.enc('hex', 'abc') === '616263';
        var extend = eval(Duktape.enc('opensv2.module.v1', ''));
        var info = extend.runtimeInfo(), caps = extend.capabilities();
        checks.runtimeInfo = info.ok && info.result.apiVersion === 1 && info.result.mainThread === true && info.result.javascriptNativeAvailable === true;
        checks.capabilities = caps.ok && Array.isArray(caps.result.methods);
        checks.selectionAdvertised = checks.capabilities && caps.result.methods.indexOf('selection.read') >= 0;
        extend.call('selection.read', {}, function (error, result) {
            checks.reply = !error && !!result;
            checks.replyError = error && error.code;
            finish();
        });
    } catch (error) { checks.exception = String(error); finish(); }
}
