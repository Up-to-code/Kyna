// Verify Problems routing, Unicode offsets, cancellation, and failure retention.
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const Module = require('node:module');
const path = require('node:path');
const fs = require('node:fs');
class Position { constructor(line, character) { Object.assign(this, { line, character }); } }
class Range { constructor(start, end) { Object.assign(this, { start, end }); } }
class Diagnostic { constructor(range, message, severity) { Object.assign(this, { range, message, severity }); } }
class Location { constructor(uri, range) { Object.assign(this, { uri, range }); } }
class DiagnosticRelatedInformation { constructor(location, message) { Object.assign(this, { location, message }); } }
const warnings = [];
const vscode = { Position, Range, Diagnostic, Location, DiagnosticRelatedInformation,
  DiagnosticSeverity: { Warning: 1, Error: 0 }, Uri: { file: file => ({ fsPath: file, toString: () => `file:${file}` }) },
  workspace: { textDocuments: [] }, window: { showWarningMessage: text => warnings.push(text) } };
const original = Module._load;
Module._load = function(name, ...args) { return name === 'vscode' ? vscode : original.call(this, name, ...args); };
const { createDiagnostics, diagnosticRange } = require('../src/diagnostics');
Module._load = original;
const unicode = diagnosticRange({ range: { bytes: { start: 5, end: 6 } } }, '😀 x');
assert.equal(unicode.start.character, 3);
assert.equal(unicode.end.character, 4);
const documents = ['main.ky', 'library.ky'].map(file => ({ fileName: path.resolve(file), languageId: 'kyna',
  uri: vscode.Uri.file(path.resolve(file)), isClosed: false, version: 1, getText: () => '😀 x' }));
vscode.workspace.textDocuments = documents;
const processes = [];
const validation = createDiagnostics(() => 'ky', (_binary, args) => {
  const child = new EventEmitter();
  child.stdout = new EventEmitter(); child.stderr = new EventEmitter(); child.stdin = new EventEmitter();
  child.stdin.end = text => { child.input = text; }; child.kill = () => { child.killed = true; };
  const overlayIndex = args.indexOf('--source-overlay');
  child.overlay = overlayIndex >= 0 ? args[overlayIndex + 1] : '';
  processes.push(child); return child;
});
const entries = new Map();
const collection = { clear: () => entries.clear(), set: (uri, items) => entries.set(uri.toString(), items) };
function result(process, file, code = 'KSEM1202') {
  process.stderr.emit('data', JSON.stringify({ schema: 'kyna.diagnostic/v1', diagnostics: [{ file,
    code, severity: 'error', message: 'wrong type', range: { bytes: { start: 5, end: 6 } },
    labels: [{ file: documents[0].fileName, message: 'declared here', range: { start: { line: 1, column: 6 } } }] }] }));
  process.emit('close', 1);
}
documents[1].isDirty = true; documents[1].uri.scheme = 'file';
validation.validate(documents[0], collection);
assert.equal(fs.existsSync(processes.at(-1).overlay), true);
result(processes.at(-1), documents[1].fileName);
assert.equal(fs.existsSync(processes.at(-1).overlay), false);
assert.equal(entries.has(documents[0].uri.toString()), false);
assert.equal(entries.get(documents[1].uri.toString())[0].code, 'KSEM1202');
assert.equal(entries.get(documents[1].uri.toString())[0].range.start.character, 3);
assert.equal(entries.get(documents[1].uri.toString())[0].relatedInformation.length, 1);
validation.validate(documents[0], collection);
processes.at(-1).stderr.emit('data', 'malformed'); processes.at(-1).emit('close', 1);
assert.equal(entries.get(documents[1].uri.toString()).length, 1);
assert.equal(warnings.length, 1);
validation.validate(documents[0], collection);
const stale = processes.at(-1);
validation.validate(documents[0], collection);
assert.equal(stale.killed, true);
result(stale, documents[0].fileName, 'STALE');
assert.equal(entries.has(documents[0].uri.toString()), false);
processes.at(-1).emit('close', 0);
assert.equal(entries.size, 0);
validation.validate(documents[0], collection); result(processes.at(-1), documents[1].fileName);
validation.validate(documents[1], collection); result(processes.at(-1), documents[1].fileName);
assert.equal(entries.get(documents[1].uri.toString()).length, 1);
validation.closeDocument(documents[0], collection);
assert.equal(entries.get(documents[1].uri.toString()).length, 1);
validation.disposeValidation();
console.log('extension diagnostic tests passed');
