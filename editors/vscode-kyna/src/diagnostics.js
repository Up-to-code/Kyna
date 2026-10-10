// Owns CLI-backed diagnostic validation and per-source Problems entries.
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const os = require('os');
const childProcess = require('child_process');

function diagnosticRange(entry, text) {
  const lines = text.split('\n');
  const position = point => {
    const line = Math.min(Math.max(0, (point?.line || 1) - 1), lines.length - 1);
    const bytes = Math.max(0, (point?.column || 1) - 1);
    const character = Buffer.from(lines[line], 'utf8').subarray(0, bytes).toString('utf8').length;
    return new vscode.Position(line, Math.min(character, lines[line].length));
  };
  const bytes = entry.range?.bytes;
  if (Number.isInteger(bytes?.start) && Number.isInteger(bytes?.end) && bytes.end > bytes.start) {
    const at = offset => {
      const prefix = Buffer.from(text, 'utf8').subarray(0, offset).toString('utf8').split('\n');
      return new vscode.Position(prefix.length - 1, prefix.at(-1).length);
    };
    return new vscode.Range(at(bytes.start), at(bytes.end));
  }
  const start = position(entry.range?.start);
  let end = position(entry.range?.end || entry.range?.start);
  if (end.line < start.line || (end.line === start.line && end.character < start.character)) end = start;
  return new vscode.Range(start, end);
}

function createDiagnostics(executable, spawn = childProcess.spawn) {
  const timers = new Map();
  const processes = new Map();
  const contributions = new Map();
  let reportedFailure = false;
  let generation = 0;

  function publish(collection) {
    const grouped = new Map();
    for (const files of contributions.values())
      for (const [key, entry] of files) {
        if (!grouped.has(key)) grouped.set(key, { uri: entry.uri, items: new Map() });
        for (const diagnostic of entry.items) {
          const id = JSON.stringify([diagnostic.code, diagnostic.message, diagnostic.range]);
          grouped.get(key).items.set(id, diagnostic);
        }
      }
    collection.clear();
    for (const entry of grouped.values()) collection.set(entry.uri, [...entry.items.values()]);
  }

  function failure(message) {
    if (reportedFailure) return;
    reportedFailure = true;
    vscode.window.showWarningMessage(`Kyna checking failed: ${message}`);
  }

  function sourceFor(file, document) {
    const name = !file || file.startsWith('<') ? document.fileName
      : path.isAbsolute(file) ? file : path.resolve(path.dirname(document.fileName), file);
    const uri = vscode.Uri.file(name);
    const canonical = file => { try { return fs.realpathSync(file); } catch { return path.resolve(file); } };
    const open = vscode.workspace.textDocuments.find(item => item.uri.toString() === uri.toString() || canonical(item.fileName) === canonical(name));
    return { uri: open ? open.uri : uri, text: open ? open.getText() : fs.readFileSync(name, 'utf8') };
  }

  function validate(document, collection) {
    if (document.languageId !== 'kyna' || document.isClosed) return;
    const key = document.uri.toString();
    processes.get(key)?.kynaCleanup?.();
    processes.get(key)?.kill();
    const version = document.version;
    const epoch = generation;
    const args = ['check', '-', '--source-name', document.fileName, '--diagnostic-format', 'json', '--no-color'];
    let overlayDirectory;
    const cleanup = () => { if (overlayDirectory) { fs.rmSync(overlayDirectory, { recursive: true, force: true }); overlayDirectory = undefined; } };
    let process;
    try {
      const dirty = vscode.workspace.textDocuments.filter(item => item.languageId === 'kyna' && item.isDirty && item.uri.scheme === 'file');
      if (dirty.length) {
        const contents = dirty.map(item => `[[source]]\npath = ${JSON.stringify(item.fileName)}\ncontent = ${JSON.stringify(item.getText())}\n`).join('\n');
        if (Buffer.byteLength(contents) > 8 * 1024 * 1024) throw new Error('unsaved source exceeds the 8 MiB overlay limit');
        overlayDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'kyna-overlay-'));
        const overlay = path.join(overlayDirectory, 'sources.toml'); fs.writeFileSync(overlay, contents, { mode: 0o600 });
        args.push('--source-overlay', overlay);
      }
      process = spawn(executable(document), args, { cwd: path.dirname(document.fileName), stdio: ['pipe', 'pipe', 'pipe'] });
    } catch (error) { cleanup(); failure(error.message); return; }
    process.kynaCleanup = cleanup;
    processes.set(key, process);
    process.stdout.setEncoding?.('utf8');
    process.stderr.setEncoding?.('utf8');
    let stdout = '', stderr = '', oversized = false;
    const append = stream => chunk => {
      if (stream === 'stdout') stdout += chunk.toString(); else stderr += chunk.toString();
      if (stdout.length + stderr.length > 8 * 1024 * 1024) {
        oversized = true; process.kill();
      }
    };
    process.stdout.on('data', append('stdout'));
    process.stderr.on('data', append('stderr'));
    process.stdin.on('error', () => {}); // Spawn errors may close stdin before the write completes.
    process.on('error', error => {
      cleanup();
      if (processes.get(key) !== process) return;
      processes.delete(key);
      failure(`${error.message}. Set kyna.executable.`);
    });
    process.on('close', code => {
      cleanup();
      if (processes.get(key) !== process) return;
      processes.delete(key);
      if (document.isClosed) return;
      if (document.version !== version || epoch !== generation) {
        scheduleValidation(document, collection); return;
      }
      if (oversized) { failure('diagnostic output exceeded the size limit'); return; }
      const raw = stderr.trim() || stdout.trim();
      if (!raw && code === 0) {
        contributions.set(key, new Map()); publish(collection); reportedFailure = false; return;
      }
      try {
        const payload = JSON.parse(raw);
        if (payload.schema !== 'kyna.diagnostic/v1' || !Array.isArray(payload.diagnostics))
          throw new Error('unsupported diagnostic response');
        const grouped = new Map();
        for (const entry of payload.diagnostics) {
          const source = sourceFor(entry.file, document);
          const diagnostic = new vscode.Diagnostic(diagnosticRange(entry, source.text), entry.message,
            entry.severity === 'warning' ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error);
          diagnostic.code = entry.code; diagnostic.source = 'kyna';
          diagnostic.relatedInformation = (entry.labels || []).map(label => {
            const target = sourceFor(label.file, document);
            return new vscode.DiagnosticRelatedInformation(
              new vscode.Location(target.uri, diagnosticRange(label, target.text)), label.message);
          });
          if (entry.help) diagnostic.message += `\n${entry.help}`;
          const fileKey = source.uri.toString();
          if (!grouped.has(fileKey)) grouped.set(fileKey, { uri: source.uri, items: [] });
          grouped.get(fileKey).items.push(diagnostic);
        }
        contributions.set(key, grouped); publish(collection); reportedFailure = false;
      } catch (error) {
        // Keep the last known diagnostics visible instead of turning a failed check green.
        failure(`invalid compiler response (${error.message})`);
      }
    });
    process.stdin.end(document.getText());
  }

  function scheduleValidation(document, collection) {
    if (document.languageId !== 'kyna' || document.isClosed) return;
    const key = document.uri.toString();
    clearTimeout(timers.get(key));
    timers.set(key, setTimeout(() => { timers.delete(key); validate(document, collection); }, 200));
  }

  function closeDocument(document, collection) {
    const key = document.uri.toString();
    clearTimeout(timers.get(key)); timers.delete(key);
    processes.get(key)?.kynaCleanup?.();
    processes.get(key)?.kill(); processes.delete(key);
    contributions.delete(key); publish(collection);
  }

  function disposeValidation() {
    for (const timer of timers.values()) clearTimeout(timer);
    for (const process of processes.values()) { process.kynaCleanup?.(); process.kill(); }
    timers.clear(); processes.clear(); contributions.clear();
  }

  function dependencyChanged(collection) {
    ++generation;
    for (const document of vscode.workspace.textDocuments) scheduleValidation(document, collection);
  }

  return { validate, scheduleValidation, closeDocument, disposeValidation, dependencyChanged };
}
module.exports = { createDiagnostics, diagnosticRange };
