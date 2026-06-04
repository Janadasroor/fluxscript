const vscode = require('vscode');
const path = require('path');
const fs = require('fs');

let client;

function activate(context) {
  const config = vscode.workspace.getConfiguration('fluxscript');
  const serverPath = config.get('lspPath') ||
    path.join(context.extensionPath, '..', '..', 'build', 'flux-lsp');

  if (!fs.existsSync(serverPath)) {
    vscode.window.showErrorMessage(
      `flux-lsp not found at "${serverPath}". Build the project first or set fluxscript.lspPath.`
    );
    return;
  }

  const outputChannel = vscode.window.createOutputChannel('FluxScript LSP');
  context.subscriptions.push(outputChannel);
  outputChannel.appendLine(`Starting flux-lsp from: ${serverPath}`);

  const serverOptions = {
    run: { command: serverPath, args: ['--stdio'] },
    debug: { command: serverPath, args: ['--stdio'] },
  };

  const clientOptions = {
    documentSelector: [{ scheme: 'file', language: 'fluxscript' }],
    synchronize: { configurationSection: 'fluxscript' },
    outputChannel,
  };

  const lsp = require('vscode-languageclient/node');
  client = new lsp.LanguageClient('fluxscript', 'FluxScript LSP', serverOptions, clientOptions);
  context.subscriptions.push(client.start());
  client.onReady().then(() => {
    outputChannel.appendLine('FluxScript LSP ready');
  }).catch(err => {
    outputChannel.appendLine(`FluxScript LSP error: ${err}`);
  });
}

function deactivate() {
  return client && client.stop();
}

module.exports = { activate, deactivate };
