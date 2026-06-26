const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

let client;

function findServerPath(context) {
  const config = vscode.workspace.getConfiguration('fluxscript');
  const configured = config.get('lspPath');
  if (configured && fs.existsSync(configured)) return configured;

  const extDir = context.extensionPath;
  const candidates = [
    path.join(extDir, '..', '..', 'build', 'flux-lsp'),
    path.join(extDir, '..', '..', '..', 'build', 'flux-lsp'),
  ];
  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }

  const folders = vscode.workspace.workspaceFolders || [];
  for (const f of folders) {
    const p = path.join(f.uri.fsPath, 'build', 'flux-lsp');
    if (fs.existsSync(p)) return p;
  }

  return candidates[0];
}

function findFluxPath(context) {
  const config = vscode.workspace.getConfiguration('fluxscript');
  const configured = config.get('fluxPath');
  if (configured && fs.existsSync(configured)) return configured;

  const extDir = context.extensionPath;
  const candidates = [
    path.join(extDir, '..', '..', 'build', 'flux'),
    path.join(extDir, '..', '..', '..', 'build', 'flux'),
  ];
  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }

  const folders = vscode.workspace.workspaceFolders || [];
  for (const f of folders) {
    const p = path.join(f.uri.fsPath, 'build', 'flux');
    if (fs.existsSync(p)) return p;
  }

  return candidates[0];
}

function runProcess(fluxPath, args) {
  return new Promise((resolve, reject) => {
    const proc = spawn(fluxPath, args, { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';
    proc.stdout.on('data', d => { stdout += d; });
    proc.stderr.on('data', d => { stderr += d; });
    proc.on('close', code => resolve({ stdout, stderr, code }));
    proc.on('error', reject);
  });
}

function cleanMessage(msg) {
  return msg.replace(/\\+$/, '').trim();
}

function parseCheckErrors(stderr) {
  const diagnostics = [];
  const lineColRe = /<flux>:(\d+):(\d+):\s*error:\s*(.*)/gi;
  let match;
  while ((match = lineColRe.exec(stderr)) !== null) {
    const line = parseInt(match[1], 10) - 1;
    const col = parseInt(match[2], 10) - 1;
    const message = cleanMessage(match[3]);
    if (!message) continue;
    const range = new vscode.Range(line, col, line, col + 1);
    diagnostics.push(new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error));
  }

  if (diagnostics.length === 0) {
    const checkErrRe = /Parse Error:\s*Error at line (\d+),\s*column (\d+):\s*(.*)/i;
    const checkMatch = stderr.match(checkErrRe);
    if (checkMatch) {
      const line = parseInt(checkMatch[1], 10) - 1;
      const col = parseInt(checkMatch[2], 10) - 1;
      const message = cleanMessage(checkMatch[3]);
      const range = new vscode.Range(line, col, line, col + 1);
      diagnostics.push(new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error));
    }
  }

  return diagnostics;
}

async function lintDocument(doc, fluxPath) {
  if (doc.languageId !== 'fluxscript') return;
  const filePath = doc.fileName;
  if (doc.isDirty) await doc.save();
  try {
    const { stderr, code } = await runProcess(fluxPath, ['--cache=0', '--emit=parse-only', filePath]);
    if (code === 0) {
      diagnosticCollection.delete(doc.uri);
    } else {
      const diagnostics = parseCheckErrors(stderr);
      diagnosticCollection.set(doc.uri, diagnostics);
    }
  } catch (err) {
    diagnosticCollection.delete(doc.uri);
  }
}

const diagnosticCollection = vscode.languages.createDiagnosticCollection('fluxscript');

async function activate(context) {
  const serverPath = findServerPath(context);
  const outputChannel = vscode.window.createOutputChannel('FluxScript LSP');
  context.subscriptions.push(outputChannel);
  outputChannel.appendLine(`flux-lsp path: ${serverPath}`);

  if (fs.existsSync(serverPath)) {
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
    } catch (err) {
      outputChannel.appendLine(`ERROR: ${err.message}`);
    }
  }

  context.subscriptions.push(diagnosticCollection);
  const fluxPath = findFluxPath(context);

  // Lint on save
  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument(async (doc) => {
      if (!fs.existsSync(fluxPath)) return;
      const config = vscode.workspace.getConfiguration('fluxscript');
      if (!config.get('lintOnSave', true)) return;
      await lintDocument(doc, fluxPath);
    })
  );

  // --- Run File ---
  context.subscriptions.push(vscode.commands.registerCommand('fluxscript.runFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'fluxscript') return;
    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage('flux binary not found. Set fluxscript.fluxPath in settings.');
      return;
    }
    if (editor.document.isDirty) await editor.document.save();
    const terminal = vscode.window.createTerminal({ name: 'FluxScript', iconPath: new vscode.ThemeIcon('play') });
    terminal.show();
    terminal.sendText(`'${fluxPath}' --cache=0 '${editor.document.fileName}'`);
  }));

  // --- Run Selection ---
  context.subscriptions.push(vscode.commands.registerCommand('fluxscript.runSelection', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'fluxscript') return;
    const selection = editor.selection;
    if (selection.isEmpty) return;
    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage('flux binary not found.');
      return;
    }
    const text = editor.document.getText(selection);
    const terminal = vscode.window.createTerminal({ name: 'FluxScript Selection', iconPath: new vscode.ThemeIcon('play') });
    terminal.show();
    const escaped = text.replace(/'/g, "'\\''");
    terminal.sendText(`echo '${escaped}' | '${fluxPath}' --cache=0 -`);
  }));

  // --- Check File ---
  context.subscriptions.push(vscode.commands.registerCommand('fluxscript.checkFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'fluxscript') return;
    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage('flux binary not found.');
      return;
    }
    if (editor.document.isDirty) await editor.document.save();
    const { stdout, stderr, code } = await runProcess(fluxPath, ['--cache=0', '--emit=check', editor.document.fileName]);
    const channel = vscode.window.createOutputChannel('FluxScript Check');
    channel.clear();
    if (code === 0) {
      channel.appendLine('OK — file compiles successfully');
    } else {
      if (stdout.trim()) channel.appendLine(stdout.trim());
      if (stderr.trim()) channel.appendLine(stderr.trim());
    }
    channel.show();
  }));

  // --- Lint File ---
  context.subscriptions.push(vscode.commands.registerCommand('fluxscript.lintFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'fluxscript') return;
    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage('flux binary not found.');
      return;
    }
    await lintDocument(editor.document, fluxPath);
    const diags = diagnosticCollection.get(editor.document.uri);
    if (!diags || diags.length === 0) {
      vscode.window.showInformationMessage('FluxScript: No errors found');
    } else {
      vscode.window.showErrorMessage(`FluxScript: ${diags.length} error(s) found`);
    }
  }));
}

function deactivate() {
  if (client) client.stop();
  diagnosticCollection.dispose();
}

module.exports = { activate, deactivate };
