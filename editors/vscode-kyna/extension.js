const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const childProcess = require('child_process');

const languageSelector = { language: 'kyna', scheme: 'file' };
const validationTimers = new Map();
const validationProcesses = new Map();
let missingExecutableReported = false;

const wordCompletions = [
  ['let', vscode.CompletionItemKind.Keyword, 'Mutable, type-locked binding', 'let ${1:name} = ${0:value};'],
  ['set', vscode.CompletionItemKind.Keyword, 'Immutable binding', 'set ${1:name} = ${0:value};'],
  ['func', vscode.CompletionItemKind.Keyword, 'Function declaration', 'func ${1:name}(${2:arg}: ${3:type}): ${4:void} {\n\t$0\n}'],
  ['class', vscode.CompletionItemKind.Class, 'Class declaration', 'class ${1:Name} {\n\t$0\n}'],
  ['intf', vscode.CompletionItemKind.Interface, 'Structural interface declaration', 'intf ${1:Name} {\n\t$0\n}'],
  ['import', vscode.CompletionItemKind.Keyword, 'Namespace import', 'import "${1:./module.kyna}" as ${2:module};'],
  ['export', vscode.CompletionItemKind.Keyword, 'Export a named declaration', 'export ${0}'],
  ['if', vscode.CompletionItemKind.Keyword, 'Conditional', 'if (${1:condition}) {\n\t$0\n}'],
  ['while', vscode.CompletionItemKind.Keyword, 'While loop', 'while (${1:condition}) {\n\t$0\n}'],
  ['else', vscode.CompletionItemKind.Keyword, 'Alternative branch'],
  ['break', vscode.CompletionItemKind.Keyword, 'Exit the current or named loop', 'break${1:};'],
  ['continue', vscode.CompletionItemKind.Keyword, 'Continue the current or named loop', 'continue${1:};'],
  ['loop', vscode.CompletionItemKind.Keyword, 'C-style loop', 'loop (let ${1:i} = 0; ${1:i} < ${2:count}; ${1:i} = ${1:i} + 1) {\n\t$0\n}'],
  ['match', vscode.CompletionItemKind.Keyword, 'Match expression', 'match (${1:value}) {\n\t${2:pattern} => ${3:result};\n\t_ => ${0:fallback};\n}'],
  ['try', vscode.CompletionItemKind.Keyword, 'Handle a typed Error', 'try {\n\t$1\n} catch (${2:error}) {\n\t$3\n} finally {\n\t$0\n}'],
  ['catch', vscode.CompletionItemKind.Keyword, 'Catch a typed Error value'],
  ['finally', vscode.CompletionItemKind.Keyword, 'Always execute cleanup code'],
  ['throw', vscode.CompletionItemKind.Keyword, 'Raise an Error or value', 'throw ${0:error};'],
  ['return', vscode.CompletionItemKind.Keyword, 'Return from a function', 'return ${0:value};'],
  ['new', vscode.CompletionItemKind.Constructor, 'Construct a class', 'new ${1:Class}($0)'],
  ['init', vscode.CompletionItemKind.Constructor, 'Class constructor', 'init(${1:arg}: ${2:type}) {\n\t$0\n}'],
  ['public', vscode.CompletionItemKind.Keyword, 'Public member visibility'],
  ['private', vscode.CompletionItemKind.Keyword, 'Private member visibility'],
  ['protected', vscode.CompletionItemKind.Keyword, 'Protected member visibility'],
  ['static', vscode.CompletionItemKind.Keyword, 'Class-level member'],
  ['override', vscode.CompletionItemKind.Keyword, 'Explicit method override'],
  ['final', vscode.CompletionItemKind.Keyword, 'Prevent extension or override'],
  ['abstract', vscode.CompletionItemKind.Keyword, 'Abstract class or method'],
  ['implements', vscode.CompletionItemKind.Keyword, 'Declare interface conformance'],
  ['extends', vscode.CompletionItemKind.Keyword, 'Declare a parent class'],
  ['self', vscode.CompletionItemKind.Variable, 'Current receiver'],
  ['super', vscode.CompletionItemKind.Variable, 'Parent receiver'],
  ['true', vscode.CompletionItemKind.Value, 'Boolean true'],
  ['false', vscode.CompletionItemKind.Value, 'Boolean false'],
  ['null', vscode.CompletionItemKind.Value, 'Null value'],
  ...['int', 'float', 'num', 'str', 'char', 'bool', 'void', 'any'].map(word =>
    [word, vscode.CompletionItemKind.TypeParameter, `Kyna ${word} type`]),
  ...['print', 'typeOf', 'len', 'push', 'pop', 'keys', 'readFile', 'writeFile', 'processRun',
    'readJsonFile', 'writeJsonFile', 'createDirectory', 'fileExists', 'removePath',
    'listDirectory', 'processEnv', 'sleep', 'wait', 'httpGet', 'fetch', 'build',
    'collectGarbage', 'gcStats',
    'log', 'logColor', 'console', 'error', 'filter', 'sort', 'bubbleSort', 'map', 'reduce',
    'find', 'any', 'all', 'unique', 'call', 'jsonParse',
    'jsonStringify', 'process', 'createApiStore', 'textContains', 'textFind', 'textSlice',
    'textReplace', 'textSplit', 'textTrim', 'textLower', 'textUpper']
    .map(word => [word, vscode.CompletionItemKind.Function, 'Kyna standard-library function']),
  ['fs', vscode.CompletionItemKind.Module, 'Kyna filesystem namespace'],
  ['db', vscode.CompletionItemKind.Module, 'Kyna parameterized SQL namespace'],
  ['collections', vscode.CompletionItemKind.Module, 'Kyna collection algorithms namespace']
];

const namespaceMembers = {
  console: ['log'],
  process: ['json', 'stringify', 'run', 'env'],
  fs: ['read', 'write', 'readJson', 'writeJson', 'createDirectory', 'exists', 'remove', 'list'],
  db: ['query', 'execute'],
  collections: ['map', 'reduce', 'find', 'any', 'all', 'unique']
};

function executable(document) {
  const configured = vscode.workspace.getConfiguration('kyna').get('executable', '');
  if (configured) return configured;
  const folder = document ? vscode.workspace.getWorkspaceFolder(document.uri) : undefined;
  const candidates = [];
  if (folder) {
    candidates.push(path.join(folder.uri.fsPath, 'build', 'bin', 'kyna'));
    candidates.push(path.join(folder.uri.fsPath, 'build', 'tools', 'kyna_cli', 'kyna'));
    for (const buildName of ['build-debug', 'build-release', 'build-sanitizers', 'build-kyna-v1'])
      candidates.push(path.join(folder.uri.fsPath, buildName, 'bin', 'kyna'));
  }
  if (document) {
    let directory = path.dirname(document.fileName);
    for (;;) {
      candidates.push(path.join(directory, 'build', 'bin', 'kyna'));
      candidates.push(path.join(directory, 'build', 'tools', 'kyna_cli', 'kyna'));
      for (const buildName of ['build-debug', 'build-release', 'build-sanitizers', 'build-kyna-v1'])
        candidates.push(path.join(directory, buildName, 'bin', 'kyna'));
      const parent = path.dirname(directory);
      if (parent === directory) break;
      directory = parent;
    }
  }
  return candidates.find(candidate => fs.existsSync(candidate)) || 'kyna';
}

function quoteShell(value) {
  return `'${value.replace(/'/g, `'"'"'`)}'`;
}

async function runActive(command) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'kyna') return;
  await editor.document.save();
  const terminal = vscode.window.createTerminal({
    name: command === 'run' ? 'Kyna Run' : 'Kyna Check',
    cwd: path.dirname(editor.document.fileName)
  });
  terminal.show(true);
  terminal.sendText(`${quoteShell(executable(editor.document))} ${command} ${quoteShell(editor.document.fileName)} --no-color`);
}

function toDiagnostic(entry, document) {
  const startLine = Math.max(0, (entry.range?.start?.line || 1) - 1);
  const startColumn = Math.max(0, (entry.range?.start?.column || 1) - 1);
  const endLine = Math.max(startLine, (entry.range?.end?.line || startLine + 1) - 1);
  const endColumn = Math.max(startColumn + 1, (entry.range?.end?.column || startColumn + 2) - 1);
  const range = new vscode.Range(
    new vscode.Position(Math.min(startLine, document.lineCount - 1), startColumn),
    new vscode.Position(Math.min(endLine, document.lineCount - 1), endColumn)
  );
  const severity = entry.severity === 'warning'
    ? vscode.DiagnosticSeverity.Warning
    : vscode.DiagnosticSeverity.Error;
  const diagnostic = new vscode.Diagnostic(range, entry.message, severity);
  diagnostic.code = entry.code;
  diagnostic.source = 'kyna';
  return diagnostic;
}

function validate(document, collection) {
  if (document.languageId !== 'kyna') return;
  const key = document.uri.toString();
  validationProcesses.get(key)?.kill();
  const documentVersion = document.version;
  const process = childProcess.spawn(executable(document),
    ['check', '-', '--source-name', document.fileName, '--diagnostic-format', 'json', '--no-color'],
    { cwd: path.dirname(document.fileName), stdio: ['pipe', 'pipe', 'pipe'] });
  validationProcesses.set(key, process);
  let output = '';
  process.stdout.on('data', chunk => { output += chunk.toString(); });
  process.stderr.on('data', chunk => { output += chunk.toString(); });
  process.on('error', error => {
    if (validationProcesses.get(key) !== process) return;
    validationProcesses.delete(key);
    collection.delete(document.uri);
    if (!missingExecutableReported) {
      missingExecutableReported = true;
      vscode.window.showWarningMessage(`Kyna diagnostics could not start: ${error.message}. Set kyna.executable.`);
    }
  });
  process.on('close', () => {
    if (validationProcesses.get(key) !== process) return;
    validationProcesses.delete(key);
    if (document.isClosed || document.version !== documentVersion) return;
    const schemaStart = output.indexOf('{"schema":"kyna.diagnostic/v1"');
    const legacyStart = output.indexOf('{"version"');
    const jsonStart = schemaStart >= 0 ? schemaStart : legacyStart;
    if (jsonStart < 0) {
      collection.delete(document.uri);
      return;
    }
    try {
      const payload = JSON.parse(output.slice(jsonStart).trim());
      collection.set(document.uri, (payload.diagnostics || []).map(item => toDiagnostic(item, document)));
    } catch (_) {
      collection.delete(document.uri);
    }
  });
  process.stdin.end(document.getText());
}

function scheduleValidation(document, collection) {
  const key = document.uri.toString();
  clearTimeout(validationTimers.get(key));
  validationTimers.set(key, setTimeout(() => validate(document, collection), 250));
}

function declaredSymbols(text) {
  const symbols = [];
  const expression = /\b(class|intf|func|let|set)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
  for (const match of text.matchAll(expression)) {
    const kinds = {
      class: vscode.CompletionItemKind.Class,
      intf: vscode.CompletionItemKind.Interface,
      func: vscode.CompletionItemKind.Function,
      let: vscode.CompletionItemKind.Variable,
      set: vscode.CompletionItemKind.Constant
    };
    symbols.push([match[2], kinds[match[1]], `Declared ${match[1]}`]);
  }
  return symbols;
}

function completionItem(definition) {
  const [label, kind, detail, snippet] = definition;
  const item = new vscode.CompletionItem(label, kind);
  item.detail = detail;
  if (snippet) item.insertText = new vscode.SnippetString(snippet);
  return item;
}

async function importPathCompletions(document, position) {
  const before = document.lineAt(position.line).text.slice(0, position.character);
  if (!/\bimport\s+"[^"]*$/.test(before)) return null;
  const files = await vscode.workspace.findFiles('**/*.kyna', '**/{node_modules,build,build-*}/**', 300);
  return files.map(uri => {
    let relative = path.relative(path.dirname(document.fileName), uri.fsPath).replace(/\\/g, '/');
    if (!relative.startsWith('.')) relative = `./${relative}`;
    const item = new vscode.CompletionItem(relative, vscode.CompletionItemKind.File);
    item.insertText = relative;
    item.detail = 'Kyna module';
    return item;
  });
}

async function importedMemberCompletions(document, position) {
  const before = document.lineAt(position.line).text.slice(0, position.character);
  const receiver = before.match(/([A-Za-z_][A-Za-z0-9_]*)\.$/)?.[1];
  if (!receiver) return null;
  if (namespaceMembers[receiver])
    return namespaceMembers[receiver].map(name => completionItem([
      name, vscode.CompletionItemKind.Method, `Kyna ${receiver} operation`
    ]));
  const escaped = receiver.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const declaration = document.getText().match(new RegExp(`\\bimport\\s+"([^"]+)"\\s+as\\s+${escaped}\\s*;`));
  if (!declaration) return null;
  try {
    const uri = vscode.Uri.file(path.resolve(path.dirname(document.fileName), declaration[1]));
    const text = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
    const exports = [];
    const expression = /\bexport\s+(?:public\s+)?(class|intf|func|let|set)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
    for (const match of text.matchAll(expression))
      exports.push(completionItem([match[2], match[1] === 'func' ? vscode.CompletionItemKind.Function : vscode.CompletionItemKind.Field, `Exported ${match[1]} from ${declaration[1]}`]));
    return exports;
  } catch (_) {
    return [];
  }
}

const completionProvider = {
  async provideCompletionItems(document, position) {
    const paths = await importPathCompletions(document, position);
    if (paths) return paths;
    const members = await importedMemberCompletions(document, position);
    if (members) return members;
    return [...wordCompletions, ...declaredSymbols(document.getText())].map(completionItem);
  }
};

function activate(context) {
  const diagnostics = vscode.languages.createDiagnosticCollection('kyna');
  const runButton = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
  runButton.text = '$(play) Run Kyna';
  runButton.command = 'kyna.runFile';
  runButton.tooltip = 'Run the active Kyna file';

  const updateButton = editor => {
    if (editor && editor.document.languageId === 'kyna') runButton.show();
    else runButton.hide();
  };

  context.subscriptions.push(
    diagnostics,
    runButton,
    vscode.commands.registerCommand('kyna.runFile', () => runActive('run')),
    vscode.commands.registerCommand('kyna.checkFile', () => runActive('check')),
    vscode.languages.registerCompletionItemProvider(languageSelector, completionProvider, '.', '"', '/'),
    vscode.workspace.onDidOpenTextDocument(document => scheduleValidation(document, diagnostics)),
    vscode.workspace.onDidChangeTextDocument(event => scheduleValidation(event.document, diagnostics)),
    vscode.workspace.onDidSaveTextDocument(document => validate(document, diagnostics)),
    vscode.workspace.onDidCloseTextDocument(document => {
      const key = document.uri.toString();
      clearTimeout(validationTimers.get(key));
      validationTimers.delete(key);
      validationProcesses.get(key)?.kill();
      validationProcesses.delete(key);
      diagnostics.delete(document.uri);
    }),
    vscode.window.onDidChangeActiveTextEditor(updateButton)
  );
  vscode.workspace.textDocuments.forEach(document => scheduleValidation(document, diagnostics));
  updateButton(vscode.window.activeTextEditor);
}

function deactivate() {
  for (const timer of validationTimers.values()) clearTimeout(timer);
  for (const process of validationProcesses.values()) process.kill();
  validationTimers.clear();
  validationProcesses.clear();
}

module.exports = { activate, deactivate };
