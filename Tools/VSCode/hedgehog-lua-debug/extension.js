// The Hedgehog Lua debug type: the engine itself is the debug adapter (HedgehogLuaDebug's
// DebugServer), so VS Code only connects to it on the loopback address.
const vscode = require('vscode');

const DEFAULT_PORT = 4711;

function activate(context) {
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('hedgehog-lua', {
            createDebugAdapterDescriptor(session) {
                const port = session.configuration.port || DEFAULT_PORT;
                return new vscode.DebugAdapterServer(port, '127.0.0.1');
            },
        })
    );
}

function deactivate() {}

module.exports = { activate, deactivate };
