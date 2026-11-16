// Official SV2 script. Start the editor through Extend Loader first.
function getClientInfo() {
    return {name: 'JS Extend HTTPS Example', author: 'Open SV2 Loader',
        category: 'Open SV2 Loader', versionNumber: 1, minEditorVersion: 131840};
}
function main() {
    var Extend;
    try { Extend = eval(Duktape.enc('opensv2.module.v1', '')); }
    catch (error) {
        SV.showMessageBoxAsync('JS Extend', 'Start this supported SV2 build through Extend Loader.', function () { SV.finish(); });
        return;
    }
    Extend.http.get('https://example.com/', function (error, response) {
        SV.showMessageBoxAsync('JS Extend', error ? error.code :
            'HTTP ' + response.status + '\n' + response.body.substring(0, 500), function () { SV.finish(); });
    });
}
