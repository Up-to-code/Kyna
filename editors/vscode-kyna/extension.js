const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const childProcess = require('child_process');

const languageSelector = { language: 'kyna', scheme: 'file' };
const manifestSelector = { language: 'kyna-manifest', scheme: 'file' };
const validationTimers = new Map();
const validationProcesses = new Map();
let missingExecutableReported = false;
let inspectionOutput;

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
    'listDirectory', 'processEnv', 'sleep', 'wait', 'httpGet', 'fetch', 'fetchResult', 'build',
    'collectGarbage', 'gcStats',
    'log', 'logColor', 'console', 'error', 'filter', 'sort', 'bubbleSort', 'map', 'reduce',
    'find', 'any', 'all', 'unique', 'call', 'jsonParse', 'jsonStringify',
    'tomlParse', 'tomlStringify', 'xmlParse', 'xmlStringify', 'process', 'createApiStore',
    'osName', 'osArchitecture', 'osWorkingDirectory', 'terminalIsInteractive',
    'terminalSupportsColor',
    'textContains', 'textFind', 'textSlice',
    'textReplace', 'textSplit', 'textTrim', 'textLower', 'textUpper']
    .map(word => [word, vscode.CompletionItemKind.Function, 'Kyna standard-library function']),
  ['fs', vscode.CompletionItemKind.Module, 'Kyna filesystem namespace'],
  ['http', vscode.CompletionItemKind.Module, 'Kyna HTTP namespace'],
  ['json', vscode.CompletionItemKind.Module, 'Kyna JSON namespace'],
  ['toml', vscode.CompletionItemKind.Module, 'Kyna TOML namespace'],
  ['xml', vscode.CompletionItemKind.Module, 'Kyna XML namespace'],
  ['os', vscode.CompletionItemKind.Module, 'Injected operating-system information'],
  ['terminal', vscode.CompletionItemKind.Module, 'Injected terminal information'],
  ['db', vscode.CompletionItemKind.Module, 'Kyna parameterized SQL namespace'],
  ['collections', vscode.CompletionItemKind.Module, 'Kyna collection algorithms namespace']
];

const namespaceMembers = {
  console: ['log'],
  process: ['json', 'stringify', 'run', 'env'],
  fs: ['read', 'write', 'readJson', 'writeJson', 'createDirectory', 'exists', 'remove', 'list'],
  http: ['fetch', 'tryFetch', 'server', 'response', 'json', 'redirect'],
  json: ['parse', 'stringify'],
  toml: ['parse', 'stringify'],
  xml: ['parse', 'stringify'],
  os: ['name', 'architecture', 'cwd'],
  terminal: ['interactive', 'supportsColor'],
  db: ['query', 'execute'],
  collections: ['map', 'reduce', 'find', 'any', 'all', 'unique']
};

function executable(document) {
  const configured = vscode.workspace.getConfiguration('kyna').get('executable', '');
  if (configured) return configured;
  const folder = document ? vscode.workspace.getWorkspaceFolder(document.uri) : undefined;
  const candidates = [];
  const executableNames = process.platform === 'win32'
    ? ['ky.exe', 'kyna.exe']
    : ['ky', 'kyna'];
  if (folder) {
    for (const buildName of ['build-debug', 'build-release', 'build-sanitizers', 'build-kyna-v1'])
      for (const name of executableNames)
        candidates.push(path.join(folder.uri.fsPath, buildName, 'bin', name));
    for (const name of executableNames) {
      candidates.push(path.join(folder.uri.fsPath, 'build', 'bin', name));
      candidates.push(path.join(folder.uri.fsPath, 'build', 'tools', 'kyna_cli', name));
    }
  }
  if (document) {
    let directory = path.dirname(document.fileName);
    for (;;) {
      for (const buildName of ['build-debug', 'build-release', 'build-sanitizers', 'build-kyna-v1'])
        for (const name of executableNames)
          candidates.push(path.join(directory, buildName, 'bin', name));
      for (const name of executableNames) {
        candidates.push(path.join(directory, 'build', 'bin', name));
        candidates.push(path.join(directory, 'build', 'tools', 'kyna_cli', name));
      }
      const parent = path.dirname(directory);
      if (parent === directory) break;
      directory = parent;
    }
  }
  return candidates.find(candidate => fs.existsSync(candidate)) || 'ky';
}

function formatDocument(document, token) {
  return new Promise((resolve, reject) => {
    const process = childProcess.spawn(executable(document),
      ['fmt', '-', '--source-name', document.fileName, '--no-color', '--no-interactive'],
      { cwd: path.dirname(document.fileName), stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';
    process.stdout.on('data', chunk => { stdout += chunk.toString(); });
    process.stderr.on('data', chunk => { stderr += chunk.toString(); });
    const cancellation = token.onCancellationRequested(() => process.kill());
    process.on('error', error => { cancellation.dispose(); reject(error); });
    process.on('close', code => {
      cancellation.dispose();
      if (token.isCancellationRequested) return resolve([]);
      if (code !== 0) return reject(new Error(stderr.trim() || `ky fmt exited with ${code}`));
      const fullDocument = new vscode.Range(
        new vscode.Position(0, 0),
        document.positionAt(document.getText().length)
      );
      resolve(stdout === document.getText() ? [] : [vscode.TextEdit.replace(fullDocument, stdout)]);
    });
    process.stdin.end(document.getText());
  });
}

const formattingProvider = {
  async provideDocumentFormattingEdits(document, _options, token) {
    try {
      return await formatDocument(document, token);
    } catch (error) {
      vscode.window.showErrorMessage(`Kyna formatting failed: ${error.message}. Set kyna.executable if ky is not on PATH.`);
      return [];
    }
  }
};

async function runActive(command) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'kyna') return;
  if (!await editor.document.save()) {
    vscode.window.showErrorMessage(`Kyna ${command} stopped because the active file could not be saved.`);
    return;
  }
  const program = executable(editor.document);
  const arguments = [command, editor.document.fileName, '--color', 'always'];
  const name = command === 'run' ? 'Kyna Run' : 'Kyna Check';
  const staleNames = command === 'run'
    ? ['Kyna Run', 'Kyna Project', 'Kyna Dev', 'Kyna Watch']
    : ['Kyna Check'];
  for (const existing of vscode.window.terminals.filter(terminal =>
    staleNames.some(stale => terminal.name.includes(stale))))
    existing.dispose();
  const folder = vscode.workspace.getWorkspaceFolder(editor.document.uri);
  const task = new vscode.Task(
    { type: 'kyna', command },
    folder || vscode.TaskScope.Workspace,
    name,
    'Kyna',
    new vscode.ProcessExecution(program, arguments, {
      cwd: path.dirname(editor.document.fileName)
    })
  );
  task.presentationOptions = {
    reveal: vscode.TaskRevealKind.Always,
    panel: vscode.TaskPanelKind.Dedicated,
    clear: true,
    focus: true,
    showReuseMessage: false
  };
  await vscode.tasks.executeTask(task);
}

async function inspectActive(command, title) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== 'kyna') return;
  await editor.document.save();
  const document = editor.document;
  childProcess.execFile(executable(document),
    [command, document.fileName, '--no-color'],
    { cwd: path.dirname(document.fileName), maxBuffer: 8 * 1024 * 1024 },
    (error, stdout, stderr) => {
      inspectionOutput.clear();
      inspectionOutput.appendLine(`${title}: ${document.fileName}`);
      inspectionOutput.append(stdout);
      inspectionOutput.append(stderr);
      if (error && !stdout && !stderr) inspectionOutput.appendLine(error.message);
      inspectionOutput.show(true);
    });
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
  const source = document.getText();
  const escapedReceiver = receiver.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const serverBinding = new RegExp(
    `\\b(?:let|set)\\s+${escapedReceiver}(?:\\s*:[^=;]+)?\\s*=\\s*(?:http\\.server|[A-Za-z_][A-Za-z0-9_]*\\.createApp)\\s*\\(`
  );
  if (serverBinding.test(source))
    return ['get', 'post', 'put', 'patch', 'delete', 'use', 'listen'].map(name =>
      completionItem([name, vscode.CompletionItemKind.Method, `Kyna HTTP application method`])
    );
  if (namespaceMembers[receiver])
    return namespaceMembers[receiver].map(name => completionItem([
      name, vscode.CompletionItemKind.Method, `Kyna ${receiver} operation`
    ]));
  const responseBinding = new RegExp(
    `\\b(?:let|set)\\s+${escapedReceiver}(?:\\s*:[^=;]+)?\\s*=\\s*(?:fetch|http\\.fetch)\\s*\\(`
  );
  if (responseBinding.test(source))
    return ['ok', 'status', 'url', 'method', 'headers', 'text', 'json'].map(name =>
      completionItem([name, vscode.CompletionItemKind.Property, 'Kyna HTTP response member'])
    );
  const resultBinding = new RegExp(
    `\\b(?:let|set)\\s+${escapedReceiver}(?:\\s*:[^=;]+)?\\s*=\\s*(?:fetchResult|http\\.tryFetch)\\s*\\(`
  );
  if (resultBinding.test(source))
    return ['ok', 'response', 'error'].map(name =>
      completionItem([name, vscode.CompletionItemKind.Property, 'Kyna non-throwing HTTP result member'])
    );
  const declaration = source.match(new RegExp(`\\bimport\\s+"([^"]+)"\\s+as\\s+${escapedReceiver}\\s*;`));
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

function declarationAt(document, name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const expression = new RegExp(`\\b(?:class|intf|func|let|set)\\s+${escaped}\\b`);
  for (let line = 0; line < document.lineCount; line += 1) {
    const match = document.lineAt(line).text.match(expression);
    if (match) {
      const column = document.lineAt(line).text.indexOf(name, match.index);
      return new vscode.Location(document.uri,
        new vscode.Position(line, Math.max(0, column)));
    }
  }
  return undefined;
}

const definitionProvider = {
  async provideDefinition(document, position) {
    const range = document.getWordRangeAtPosition(position);
    if (!range) return undefined;
    const name = document.getText(range);
    const local = declarationAt(document, name);
    if (local) return local;
    const linePrefix = document.lineAt(position.line).text.slice(0, range.start.character);
    const receiver = linePrefix.match(/([A-Za-z_][A-Za-z0-9_]*)\.$/)?.[1];
    if (!receiver) return undefined;
    const escaped = receiver.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const imported = document.getText().match(
      new RegExp(`\\bimport\\s+"([^"]+)"\\s+as\\s+${escaped}\\s*;`)
    );
    if (!imported) return undefined;
    try {
      const target = await vscode.workspace.openTextDocument(
        vscode.Uri.file(path.resolve(path.dirname(document.fileName), imported[1]))
      );
      return declarationAt(target, name);
    } catch (_) {
      return undefined;
    }
  }
};

const hoverDetails = new Map(wordCompletions.map(([word, , detail]) => [word, detail]));
const hoverProvider = {
  provideHover(document, position) {
    const range = document.getWordRangeAtPosition(position);
    if (!range) return undefined;
    const word = document.getText(range);
    const detail = hoverDetails.get(word);
    if (!detail) return undefined;
    return new vscode.Hover(new vscode.MarkdownString(`**${word}** — ${detail}`), range);
  }
};

const symbolProvider = {
  provideDocumentSymbols(document) {
    const symbols = [];
    const expression = /\b(class|intf|func|let|set)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
    for (let line = 0; line < document.lineCount; line += 1) {
      const text = document.lineAt(line).text;
      for (const match of text.matchAll(expression)) {
        const kinds = {
          class: vscode.SymbolKind.Class,
          intf: vscode.SymbolKind.Interface,
          func: vscode.SymbolKind.Function,
          let: vscode.SymbolKind.Variable,
          set: vscode.SymbolKind.Constant
        };
        const start = new vscode.Position(line, match.index);
        const end = new vscode.Position(line, match.index + match[0].length);
        symbols.push(new vscode.DocumentSymbol(match[2], match[1], kinds[match[1]],
          new vscode.Range(start, end), new vscode.Range(start, end)));
      }
    }
    return symbols;
  }
};

const codeLensProvider = {
  provideCodeLenses(document) {
    const range = new vscode.Range(0, 0, 0, 0);
    const lenses = [
      new vscode.CodeLens(range, { command: 'kyna.runFile', title: '$(play) Run Kyna' }),
      new vscode.CodeLens(range, { command: 'kyna.checkFile', title: '$(check) Check Kyna' })
    ];
    if (findProjectRoot(document.fileName))
      lenses.push(new vscode.CodeLens(range,
        { command: 'kyna.devProject', title: '$(sync) Watch Project & Restart', arguments: [document.uri] }));
    return lenses;
  }
};

const manifestCodeLensProvider = {
  provideCodeLenses(document) {
    const range = new vscode.Range(0, 0, 0, 0);
    const argument = [document.uri];
    return [
      new vscode.CodeLens(range, { command: 'kyna.runProject', title: '$(play) Run Project · port from kyna.toml', arguments: argument }),
      new vscode.CodeLens(range, { command: 'kyna.devProject', title: '$(sync) Watch & Restart', arguments: argument }),
      new vscode.CodeLens(range, { command: 'kyna.installDependencies', title: '$(cloud-download) Install Dependencies', arguments: argument }),
      new vscode.CodeLens(range, { command: 'kyna.generateRoute', title: '$(git-branch-create) Generate Route', arguments: argument }),
      new vscode.CodeLens(range, { command: 'kyna.configureProject', title: '$(settings-gear) Configure Host & Port', arguments: argument })
    ];
  }
};

function findProjectRoot(startPath) {
  let directory = startPath;
  try {
    if (fs.statSync(directory).isFile()) directory = path.dirname(directory);
  } catch (_) {
    directory = path.dirname(directory);
  }
  for (;;) {
    if (fs.existsSync(path.join(directory, 'kyna.toml'))) return directory;
    const parent = path.dirname(directory);
    if (parent === directory) return undefined;
    directory = parent;
  }
}

async function resolveProjectRoot(resource, allowSelection = true) {
  const candidate = resource?.fsPath || vscode.window.activeTextEditor?.document.fileName;
  const direct = candidate ? findProjectRoot(candidate) : undefined;
  if (direct) return direct;
  const manifests = await vscode.workspace.findFiles('**/kyna.toml', '**/{.git,node_modules,build,build-*}/**', 50);
  if (manifests.length === 0) {
    if (allowSelection)
      vscode.window.showWarningMessage('No kyna.toml project was found. Run “ky new” or “ky init” first.');
    return undefined;
  }
  if (manifests.length === 1) return path.dirname(manifests[0].fsPath);
  if (!allowSelection) return path.dirname(manifests[0].fsPath);
  const selected = await vscode.window.showQuickPick(
    manifests.map(uri => ({ label: path.basename(path.dirname(uri.fsPath)), description: path.dirname(uri.fsPath), uri })),
    { placeHolder: 'Select a Kyna project' }
  );
  return selected ? path.dirname(selected.uri.fsPath) : undefined;
}

function projectDocument(root) {
  const uri = vscode.Uri.file(path.join(root, 'kyna.toml'));
  return { fileName: uri.fsPath, uri };
}

async function runProjectTask(command, resource) {
  const root = await resolveProjectRoot(resource);
  if (!root) return;
  await vscode.workspace.saveAll(false);
  const labels = {
    run: 'Kyna Project · Run',
    dev: 'Kyna Project · Watch & Restart',
    install: 'Kyna Project · Install Dependencies'
  };
  const name = labels[command] || `Kyna ${command}`;
  const staleNames = command === 'install'
    ? ['Kyna Install', 'Install Dependencies']
    : ['Kyna Run', 'Kyna Project', 'Kyna Dev', 'Kyna Watch'];
  for (const terminal of vscode.window.terminals.filter(item =>
    staleNames.some(stale => item.name.includes(stale))))
    terminal.dispose();
  const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(root));
  const task = new vscode.Task(
    { type: 'kyna', command, project: root },
    folder || vscode.TaskScope.Workspace,
    name,
    'Kyna',
    new vscode.ProcessExecution(executable(projectDocument(root)),
      [command, '--color', 'always'], { cwd: root })
  );
  task.presentationOptions = {
    reveal: vscode.TaskRevealKind.Always,
    panel: vscode.TaskPanelKind.Dedicated,
    clear: command !== 'dev',
    focus: true,
    showReuseMessage: false
  };
  await vscode.tasks.executeTask(task);
}

async function openManifest(resource) {
  const root = await resolveProjectRoot(resource);
  if (!root) return;
  await vscode.window.showTextDocument(vscode.Uri.file(path.join(root, 'kyna.toml')));
}

async function generateRoute(resource) {
  const root = await resolveProjectRoot(resource);
  if (!root) return;
  const method = await vscode.window.showQuickPick(
    ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'],
    { title: 'Generate Kyna Route', placeHolder: 'Select the HTTP method' }
  );
  if (!method) return;
  const name = await vscode.window.showInputBox({
    title: 'Generate Kyna Route',
    prompt: 'Route name (creates GET /<name>)',
    placeHolder: 'users',
    validateInput: value => /^[A-Za-z0-9_-]+$/.test(value)
      ? undefined : 'Use letters, numbers, hyphens, or underscores.'
  });
  if (!name) return;
  const routePath = await vscode.window.showInputBox({
    title: 'Route URL', value: `/${name}`,
    validateInput: value => value.startsWith('/') ? undefined : 'The URL path must begin with /.'
  });
  if (!routePath) return;
  await vscode.window.withProgress({
    location: vscode.ProgressLocation.Notification,
    title: `Generating ${method} ${routePath}`,
    cancellable: false
  }, () => new Promise((resolve, reject) => {
    childProcess.execFile(executable(projectDocument(root)),
      ['generate', 'route', name, '--method', method.toLowerCase(), '--path', routePath,
        '--no-color', '--no-interactive'],
      { cwd: root }, (error, stdout, stderr) => {
        if (error) reject(new Error(stderr.trim() || stdout.trim() || error.message));
        else resolve();
      });
  })).then(async () => {
    const uri = vscode.Uri.file(path.join(root, 'src', 'routes', `${name}.kyna`));
    await vscode.window.showTextDocument(uri);
    vscode.window.showInformationMessage(`Generated and registered ${method} ${routePath}.`);
  }, error => vscode.window.showErrorMessage(`Kyna route generation failed: ${error.message}`));
}

async function configureProject(resource) {
  const root = await resolveProjectRoot(resource);
  if (!root) return;
  const uri = vscode.Uri.file(path.join(root, 'kyna.toml'));
  const document = await vscode.workspace.openTextDocument(uri);
  const source = document.getText();
  const currentHost = source.match(/^host\s*=\s*"([^"]+)"/m)?.[1] || '127.0.0.1';
  const currentPort = source.match(/^port\s*=\s*(\d+)/m)?.[1] || '3000';
  const host = await vscode.window.showInputBox({ title: 'Kyna Backend Host', value: currentHost });
  if (!host) return;
  const port = await vscode.window.showInputBox({
    title: 'Kyna Backend Port', value: currentPort,
    validateInput: value => /^\d+$/.test(value) && Number(value) > 0 && Number(value) <= 65535
      ? undefined : 'Enter a port from 1 to 65535.'
  });
  if (!port) return;
  let updated = source;
  if (/^\[server\]/m.test(updated)) {
    updated = updated.replace(/^host\s*=\s*"[^"]*"/m, `host = "${host.replace(/"/g, '')}"`);
    updated = updated.replace(/^port\s*=\s*\d+/m, `port = ${port}`);
  } else {
    updated += `\n[server]\nhost = "${host.replace(/"/g, '')}"\nport = ${port}\n`;
  }
  const edit = new vscode.WorkspaceEdit();
  edit.replace(uri, new vscode.Range(document.positionAt(0), document.positionAt(source.length)), updated);
  await vscode.workspace.applyEdit(edit);
  await document.save();
  vscode.window.showInformationMessage(
    `Kyna backend configured for http://${host}:${port}. kyna.toml now controls Run, Serve, and Dev.`);
}

const manifestCompletionProvider = {
  provideCompletionItems() {
    const definitions = [
      ['project', '[project]\nname = "${1:app}"\nversion = "${2:0.1.0}"\nentry = "${3:src/main.kyna}"\ntemplate = "${4:backend}"'],
      ['server', '[server]\nhost = "${1:127.0.0.1}"\nport = ${2:3000}'],
      ['scripts', '[scripts]\ncheck = "ky check"\ntest = "ky check tests"'],
      ['dependency-git', '${1:name} = { git = "${2:https://github.com/example/project.git}", rev = "${3:commit}" }'],
      ['dependency-path', '${1:name} = { path = "${2:../shared}" }']
    ];
    return definitions.map(([label, snippet]) => {
      const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.Snippet);
      item.insertText = new vscode.SnippetString(snippet);
      item.detail = 'Kyna manifest block';
      return item;
    });
  }
};

class KynaProjectProvider {
  constructor() { this.changed = new vscode.EventEmitter(); this.onDidChangeTreeData = this.changed.event; }
  refresh() { this.changed.fire(); }
  getTreeItem(item) { return item; }
  async getChildren() {
    const root = await resolveProjectRoot(undefined, false);
    if (!root) return [];
    let source = '';
    try { source = fs.readFileSync(path.join(root, 'kyna.toml'), 'utf8'); } catch (_) { return []; }
    const name = source.match(/^name\s*=\s*"([^"]+)"/m)?.[1] || path.basename(root);
    const version = source.match(/^version\s*=\s*"([^"]+)"/m)?.[1] || '0.0.0';
    const header = new vscode.TreeItem(`${name} · ${version}`, vscode.TreeItemCollapsibleState.None);
    header.description = source.match(/^template\s*=\s*"([^"]+)"/m)?.[1] || 'custom';
    header.iconPath = new vscode.ThemeIcon('package');
    const action = (label, icon, command) => {
      const item = new vscode.TreeItem(label, vscode.TreeItemCollapsibleState.None);
      item.iconPath = new vscode.ThemeIcon(icon);
      item.command = { command, title: label, arguments: [vscode.Uri.file(root)] };
      return item;
    };
    return [header,
      action('Run project · uses kyna.toml port', 'run', 'kyna.runProject'),
      action('Watch files · check & restart', 'sync', 'kyna.devProject'),
      action('Install dependencies', 'cloud-download', 'kyna.installDependencies'),
      action('Generate route', 'git-branch-create', 'kyna.generateRoute'),
      action('Configure backend', 'settings-gear', 'kyna.configureProject'),
      action('Open kyna.toml', 'settings', 'kyna.openManifest')];
  }
}

const projectDecorations = {
  provideFileDecoration(uri) {
    const name = path.basename(uri.fsPath);
    if (name === 'health.kyna') return { badge: '♥', tooltip: 'Health route', color: new vscode.ThemeColor('testing.iconPassed') };
    return undefined;
  }
};

function activate(context) {
  const diagnostics = vscode.languages.createDiagnosticCollection('kyna');
  inspectionOutput = vscode.window.createOutputChannel('Kyna Compiler');
  const runButton = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
  runButton.text = '$(play) Run Kyna';
  runButton.command = 'kyna.runFile';
  runButton.tooltip = 'Run the active Kyna file';
  const projectStatus = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 99);
  projectStatus.command = 'kyna.openManifest';
  const projectProvider = new KynaProjectProvider();
  const manifestWatcher = vscode.workspace.createFileSystemWatcher('**/kyna.toml');

  const updateButton = editor => {
    if (editor && editor.document.languageId === 'kyna') runButton.show();
    else runButton.hide();
    const root = editor ? findProjectRoot(editor.document.fileName) : undefined;
    if (root) {
      try {
        const source = fs.readFileSync(path.join(root, 'kyna.toml'), 'utf8');
        const name = source.match(/^name\s*=\s*"([^"]+)"/m)?.[1] || path.basename(root);
        const template = source.match(/^template\s*=\s*"([^"]+)"/m)?.[1] || 'custom';
        projectStatus.text = `$(package) ${name}`;
        projectStatus.tooltip = `Kyna ${template} project · Open kyna.toml`;
        projectStatus.show();
      } catch (_) { projectStatus.hide(); }
    } else projectStatus.hide();
  };

  context.subscriptions.push(
    diagnostics,
    inspectionOutput,
    runButton,
    projectStatus,
    manifestWatcher,
    vscode.window.registerTreeDataProvider('kyna.project', projectProvider),
    vscode.window.registerFileDecorationProvider(projectDecorations),
    vscode.commands.registerCommand('kyna.runFile', () => runActive('run')),
    vscode.commands.registerCommand('kyna.checkFile', () => runActive('check')),
    vscode.commands.registerCommand('kyna.showTokens', () => inspectActive('tokens', 'Tokens')),
    vscode.commands.registerCommand('kyna.showAst', () => inspectActive('ast', 'Syntax tree')),
    vscode.commands.registerCommand('kyna.showHir', () => inspectActive('hir', 'HIR')),
    vscode.commands.registerCommand('kyna.showMir', () => inspectActive('mir', 'MIR')),
    vscode.commands.registerCommand('kyna.showBytecode', () => inspectActive('bytecode', 'Bytecode')),
    vscode.commands.registerCommand('kyna.runProject', resource => runProjectTask('run', resource)),
    vscode.commands.registerCommand('kyna.devProject', resource => runProjectTask('dev', resource)),
    vscode.commands.registerCommand('kyna.installDependencies', resource => runProjectTask('install', resource)),
    vscode.commands.registerCommand('kyna.generateRoute', resource => generateRoute(resource).then(() => projectProvider.refresh())),
    vscode.commands.registerCommand('kyna.configureProject', resource => configureProject(resource).then(() => projectProvider.refresh())),
    vscode.commands.registerCommand('kyna.openManifest', resource => openManifest(resource)),
    vscode.languages.registerCompletionItemProvider(languageSelector, completionProvider, '.', '"', '/'),
    vscode.languages.registerCompletionItemProvider(manifestSelector, manifestCompletionProvider, '[', '=', ' '),
    vscode.languages.registerDefinitionProvider(languageSelector, definitionProvider),
    vscode.languages.registerHoverProvider(languageSelector, hoverProvider),
    vscode.languages.registerDocumentSymbolProvider(languageSelector, symbolProvider),
    vscode.languages.registerCodeLensProvider(languageSelector, codeLensProvider),
    vscode.languages.registerCodeLensProvider(manifestSelector, manifestCodeLensProvider),
    vscode.languages.registerDocumentFormattingEditProvider(languageSelector, formattingProvider),
    vscode.workspace.onDidOpenTextDocument(document => scheduleValidation(document, diagnostics)),
    vscode.workspace.onDidChangeTextDocument(event => scheduleValidation(event.document, diagnostics)),
    vscode.workspace.onDidSaveTextDocument(document => {
      validate(document, diagnostics);
      if (path.basename(document.fileName) === 'kyna.toml') projectProvider.refresh();
    }),
    vscode.workspace.onDidCloseTextDocument(document => {
      const key = document.uri.toString();
      clearTimeout(validationTimers.get(key));
      validationTimers.delete(key);
      validationProcesses.get(key)?.kill();
      validationProcesses.delete(key);
      diagnostics.delete(document.uri);
    }),
    vscode.window.onDidChangeActiveTextEditor(updateButton),
    manifestWatcher.onDidCreate(() => projectProvider.refresh()),
    manifestWatcher.onDidChange(() => projectProvider.refresh()),
    manifestWatcher.onDidDelete(() => projectProvider.refresh())
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
