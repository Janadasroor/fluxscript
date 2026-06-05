const vscode = require('vscode');
const path = require('path');
const fs = require('fs');

let client;

function findServerPath(context) {
  const config = vscode.workspace.getConfiguration('fluxscript');
  const configured = config.get('lspPath');
  if (configured && fs.existsSync(configured)) return configured;

  // Try relative to extension
  const candidates = [
    path.join(context.extensionPath, '..', '..', 'build', 'flux-lsp'),
    path.join(context.extensionPath, '..', '..', '..', 'build', 'flux-lsp'),
    '/home/jnd/qt_projects/fluxscript/build/flux-lsp',
  ];
  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }
  // Try workspace folders
  const folders = vscode.workspace.workspaceFolders || [];
  for (const f of folders) {
    const p = path.join(f.uri.fsPath, 'build', 'flux-lsp');
    if (fs.existsSync(p)) return p;
  }
  return candidates[0]; // return first candidate even if it doesn't exist
}

async function activate(context) {
  const serverPath = findServerPath(context);

  const outputChannel = vscode.window.createOutputChannel('FluxScript LSP');
  context.subscriptions.push(outputChannel);
  outputChannel.appendLine(`flux-lsp path: ${serverPath}`);

  if (!fs.existsSync(serverPath)) {
    outputChannel.appendLine('ERROR: binary not found');
    vscode.window.showErrorMessage(
      `flux-lsp not found. Build the project first, or set fluxscript.lspPath in settings.`
    );
    return;
  }

  outputChannel.appendLine('Starting server...');

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

  try {
    await client.start();
    outputChannel.appendLine('FluxScript LSP ready');
    await client.onReady();
  } catch (err) {
    outputChannel.appendLine(`ERROR: ${err.message}`);
    outputChannel.appendLine(err.stack || '');
  }
}

function deactivate() {
  return client && client.stop();
}

module.exports = { activate, deactivate };
