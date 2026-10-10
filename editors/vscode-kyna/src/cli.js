// Locate the configured or workspace Kyna executable.
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
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


module.exports = { executable };
