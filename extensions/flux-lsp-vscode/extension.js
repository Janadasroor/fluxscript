const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

let client;
const diagnosticCollection = vscode.languages.createDiagnosticCollection('fluxscript');

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

function parseCheckErrors(stderr) {
  const diagnostics = [];
  // Match: <flux>:LINE:COL: error: MESSAGE
  const lineColRe = /<flux>:(\d+):(\d+):\s*error:\s*(.*)/gi;
  let match;
  while ((match = lineColRe.exec(stderr)) !== null) {
    const line = parseInt(match[1], 10) - 1; // 0-indexed
    const col = parseInt(match[2], 10) - 1;
    const message = match[3].trim();
    if (!message) continue;
    const range = new vscode.Range(line, col, line, col + 1);
    diagnostics.push(new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error));
  }

  // Match "Parse Error: Error at line N, column N: message"
  if (diagnostics.length === 0) {
    const checkErrRe = /Parse Error:\s*Error at line (\d+),\s*column (\d+):\s*(.*)/i;
    const checkMatch = stderr.match(checkErrRe);
    if (checkMatch) {
      const line = parseInt(checkMatch[1], 10) - 1;
      const col = parseInt(checkMatch[2], 10) - 1;
      const message = checkMatch[3].trim();
      const range = new vscode.Range(line, col, line, col + 1);
      diagnostics.push(new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error));
    }
  }

  return diagnostics;
}

async function lintDocument(doc, fluxPath) {
  if (doc.languageId !== 'fluxscript') return;

  const filePath = doc.fileName;

  // Save unsaved changes so the compiler reads the latest
  if (doc.isDirty) {
    await doc.save();
  }

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

async function activate(context) {
  const serverPath = findServerPath(context);

  const outputChannel = vscode.window.createOutputChannel('FluxScript LSP');
  context.subscriptions.push(outputChannel);
  outputChannel.appendLine(`flux-lsp path: ${serverPath}`);

  if (!fs.existsSync(serverPath)) {
    outputChannel.appendLine('ERROR: flux-lsp binary not found');
    vscode.window.showErrorMessage(
      'flux-lsp not found. Build the project first, or set fluxscript.lspPath in settings.'
    );
  } else {
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
      outputChannel.appendLine(err.stack || '');
    }
  }

  context.subscriptions.push(diagnosticCollection);

  const fluxPath = findFluxPath(context);
  const hasFlux = fs.existsSync(fluxPath);

  // --- Lint on save ---
  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument(async (doc) => {
      if (!hasFlux) return;
      const config = vscode.workspace.getConfiguration('fluxscript');
      if (!config.get('lintOnSave', true)) return;
      await lintDocument(doc, fluxPath);
    })
  );

  // --- Lint on open ---
  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument(async (doc) => {
      if (!hasFlux) return;
      const config = vscode.workspace.getConfiguration('fluxscript');
      if (!config.get('lintOnSave', true)) return;
      await lintDocument(doc, fluxPath);
    })
  );

  // --- Lint active editor on activation ---
  if (hasFlux && vscode.window.activeTextEditor) {
    const config = vscode.workspace.getConfiguration('fluxscript');
    if (config.get('lintOnSave', true)) {
      lintDocument(vscode.window.activeTextEditor.document, fluxPath);
    }
  }

  // --- Lint File command (explicit) ---
  const lintFileCmd = vscode.commands.registerCommand('fluxscript.lintFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('No active editor');
      return;
    }

    if (editor.document.languageId !== 'fluxscript') {
      vscode.window.showErrorMessage('Not a FluxScript file');
      return;
    }

    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage(
        'flux binary not found. Build the project first, or set fluxscript.fluxPath in settings.'
      );
      return;
    }

    await lintDocument(editor.document, fluxPath);

    const diags = diagnosticCollection.get(editor.document.uri);
    if (!diags || diags.length === 0) {
      vscode.window.showInformationMessage('FluxScript: No errors found');
    } else {
      vscode.window.showErrorMessage(`FluxScript: ${diags.length} error(s) found`);
    }
  });
  context.subscriptions.push(lintFileCmd);

  // --- Run File command ---
  const runFileCmd = vscode.commands.registerCommand('fluxscript.runFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('No active editor');
      return;
    }

    if (editor.document.languageId !== 'fluxscript') {
      vscode.window.showErrorMessage('Not a FluxScript file');
      return;
    }

    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage(
        'flux binary not found. Build the project first, or set fluxscript.fluxPath in settings.'
      );
      return;
    }

    const filePath = editor.document.fileName;
    if (editor.document.isDirty) await editor.document.save();

    const terminal = vscode.window.createTerminal({ name: 'FluxScript', iconPath: new vscode.ThemeIcon('play') });
    terminal.show();
    terminal.sendText(`'${fluxPath}' --cache=0 '${filePath}'`);
  });
  context.subscriptions.push(runFileCmd);

  // --- Run Selection command ---
  const runSelectionCmd = vscode.commands.registerCommand('fluxscript.runSelection', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('No active editor');
      return;
    }

    if (editor.document.languageId !== 'fluxscript') {
      vscode.window.showErrorMessage('Not a FluxScript file');
      return;
    }

    const selection = editor.selection;
    if (selection.isEmpty) {
      vscode.window.showErrorMessage('No text selected');
      return;
    }

    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage(
        'flux binary not found. Build the project first, or set fluxscript.fluxPath in settings.'
      );
      return;
    }

    const text = editor.document.getText(selection);
    const terminal = vscode.window.createTerminal({ name: 'FluxScript Selection', iconPath: new vscode.ThemeIcon('play') });
    terminal.show();
    const escaped = text.replace(/'/g, "'\\''");
    terminal.sendText(`echo '${escaped}' | '${fluxPath}' --cache=0 -`);
  });
  context.subscriptions.push(runSelectionCmd);

  // --- Check File command ---
  const checkFileCmd = vscode.commands.registerCommand('fluxscript.checkFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('No active editor');
      return;
    }

    if (editor.document.languageId !== 'fluxscript') {
      vscode.window.showErrorMessage('Not a FluxScript file');
      return;
    }

    if (!fs.existsSync(fluxPath)) {
      vscode.window.showErrorMessage(
        'flux binary not found. Build the project first, or set fluxscript.fluxPath in settings.'
      );
      return;
    }

    const filePath = editor.document.fileName;
    if (editor.document.isDirty) await editor.document.save();

    const { stdout, stderr, code } = await runProcess(fluxPath, ['--cache=0', '--emit=check', filePath]);

    const channel = vscode.window.createOutputChannel('FluxScript Check');
    channel.clear();
    if (code === 0) {
      channel.appendLine('OK — file compiles successfully');
    } else {
      if (stdout.trim()) channel.appendLine(stdout.trim());
      if (stderr.trim()) channel.appendLine(stderr.trim());
    }
    channel.show();
  });
  context.subscriptions.push(checkFileCmd);
}

function deactivate() {
  if (client) client.stop();
  diagnosticCollection.dispose();
}

module.exports = { activate, deactivate };
