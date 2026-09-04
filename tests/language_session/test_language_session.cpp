#include "kyna/language/language_session.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class RecordingFiles final : public kyna::FileSystemPort {
public:
  std::string contents;
  int reads{0};
  int writes{0};
  int directories{0};
  int removals{0};
  std::optional<std::string> read(const std::filesystem::path &, std::string &) override {
    ++reads;
    return contents;
  }
  bool write(const std::filesystem::path &, const std::string &value, std::string &) override {
    ++writes;
    contents = value;
    return true;
  }
  bool createDirectories(const std::filesystem::path &, std::string &) override {
    ++directories;
    return true;
  }
  bool exists(const std::filesystem::path &, std::string &) override { return true; }
  bool remove(const std::filesystem::path &, std::string &) override {
    ++removals;
    return true;
  }
  std::optional<std::vector<std::string>> list(const std::filesystem::path &,
                                               std::string &) override {
    return std::vector<std::string>{"products.json"};
  }
};

class RecordingProcesses final : public kyna::ProcessPort {
public:
  int runs{0};
  std::vector<std::string> environmentNames;
  int run(const std::string &) override {
    ++runs;
    return 0;
  }
  std::optional<std::string> environment(const std::string &name) override {
    environmentNames.push_back(name);
    if (name == "MISSING")
      return std::nullopt;
    return std::string("test");
  }
};

class RecordingNetwork final : public kyna::NetworkPort {
public:
  int requests{0};
  std::string lastMethod;
  std::optional<std::string> lastBody;
  std::map<std::string, std::string> lastHeaders;
  std::chrono::milliseconds lastTimeout{0};
  std::optional<kyna::NetworkResponse> send(const kyna::NetworkRequest &request,
                                            kyna::NetworkFailure &failure) override {
    ++requests;
    lastMethod = request.method;
    lastBody = request.body;
    lastHeaders = request.headers;
    lastTimeout = request.timeout;
    if (request.url == "https://failure.test") {
      failure = {kyna::NetworkFailurePhase::Tls, 35, "certificate handshake rejected", false};
      return std::nullopt;
    }
    if (request.url == "https://timeout.test") {
      failure = {kyna::NetworkFailurePhase::Timeout, 28, "request deadline exceeded", true};
      return std::nullopt;
    }
    if (request.url == "https://unauthorized.test")
      return kyna::NetworkResponse{401, "{\"error\":\"unauthorized\"}", request.url,
                                   {{"content-type", "application/json"}}};
    if (request.url == "https://invalid-json.test")
      return kyna::NetworkResponse{200, "{broken json}", request.url,
                                   {{"content-type", "application/json"}}};
    if (request.method == "POST")
      return kyna::NetworkResponse{201, "{\"id\":31,\"title\":\"created\"}", request.url,
                                   {{"content-type", "application/json"}}};
    return kyna::NetworkResponse{200, "[3,1,2]", request.url,
                                 {{"content-type", "application/json"}}};
  }
};

class RecordingClock final : public kyna::ClockPort {
public:
  int sleeps{0};
  void sleep(std::chrono::milliseconds) override { ++sleeps; }
};

class RecordingHostInfo final : public kyna::HostInfoPort {
public:
  std::string operatingSystem() const override { return "test-os"; }
  std::string architecture() const override { return "test-architecture"; }
  std::optional<std::string> workingDirectory(std::string &) const override {
    return "/virtual/workspace";
  }
  bool standardOutputIsTerminal() const override { return true; }
  bool supportsColor() const override { return false; }
};

class RecordingDatabase final : public kyna::DatabasePort {
public:
  int queries{0};
  kyna::DatabaseRequest lastRequest;
  std::optional<kyna::DatabaseResult> execute(const kyna::DatabaseRequest &request,
                                              kyna::DatabaseFailure &failure) override {
    ++queries;
    lastRequest = request;
    if (request.statement == "FAIL") {
      failure = {kyna::DatabaseFailurePhase::Execute, "23505",
                 "duplicate key violates unique constraint", false};
      return std::nullopt;
    }
    kyna::DatabaseRow row;
    row["id"] = std::int64_t{7};
    row["name"] = std::string("Ada");
    row["active"] = true;
    row["nickname"] = nullptr;
    return kyna::DatabaseResult{{std::move(row)}, 1, "SELECT 1"};
  }
};

void writeSource(const std::filesystem::path &path, const std::string &source) {
  std::ofstream file(path, std::ios::binary);
  assert(file);
  file << source;
  assert(file);
}

bool hasCode(const std::vector<kyna::Diagnostic> &diagnostics, const std::string &code) {
  for (const auto &diagnostic : diagnostics)
    if (diagnostic.code == code)
      return true;
  return false;
}

} // namespace

int main() {
  const kyna::SourceFile comments{7, "comments.kyna", "# π\nvar value: int = 1;"};
  auto lexed = kyna::tokenize(comments);
  assert(lexed.ok());
  assert(lexed.tokens.front().kind == kyna::TokenKind::Var);
  assert(lexed.tokens.front().location.startByte == 5);
  assert(lexed.tokens.front().location.line == 2);
  assert(lexed.tokens.front().location.column == 1);

  const kyna::SourceFile malformed{8, "malformed.kyna", "var first = ; var second = ;"};
  auto malformedTokens = kyna::tokenize(malformed);
  auto recovered = kyna::parseModule(malformed, std::move(malformedTokens.tokens));
  assert(!recovered.ok());
  assert(recovered.diagnostics.size() == 2);

  kyna::LanguageSession session;
  assert(session
             .checkSource("interface-ok.kyna", "intf Named { name: str; } "
                                             "class User implements Named { public name: str; }")
             .ok());
  assert(!session
              .checkSource("interface-bad.kyna",
                           "intf Named { name: str; } class User implements Named { }")
              .ok());

  auto unprotected = session.checkSource("unprotected.kyna",
                                         "const response = fetch(\"http://example.test/products\");");
  assert(unprotected.ok());
  assert(hasCode(unprotected.diagnostics, "K2601"));
  assert(hasCode(unprotected.diagnostics, "K2603"));
  const auto protectedFetch = session.checkSource(
      "protected.kyna", "try { const response = fetch(\"https://example.test/products\"); } "
                      "catch (message) { print(message); }");
  assert(protectedFetch.ok());
  assert(!hasCode(protectedFetch.diagnostics, "K2601"));
  const auto emptyCatch =
      session.checkSource("empty-catch.kyna", "try { error(\"failure\"); } catch (message) { }");
  assert(emptyCatch.ok());
  assert(hasCode(emptyCatch.diagnostics, "K2602"));
  const auto unprotectedDatabase = session.checkSource(
      "database-warning.kyna",
      "const sql = \"SELECT * FROM users\"; const result = db.query(\"postgres://local\", sql);");
  assert(unprotectedDatabase.ok());
  assert(hasCode(unprotectedDatabase.diagnostics, "K2601"));
  assert(hasCode(unprotectedDatabase.diagnostics, "K2610"));
  const auto nullMember = session.checkSource(
      "null-member.kyna", "const missing = null; print(missing.name);");
  assert(!nullMember.ok());
  assert(hasCode(nullMember.diagnostics, "KSEM2401"));

  auto files = std::make_shared<RecordingFiles>();
  auto processes = std::make_shared<RecordingProcesses>();
  auto network = std::make_shared<RecordingNetwork>();
  auto clock = std::make_shared<RecordingClock>();
  auto database = std::make_shared<RecordingDatabase>();
  auto host = std::make_shared<RecordingHostInfo>();
  kyna::LanguageSessionOptions deterministicOptions;
  deterministicOptions.capabilities = {files, processes, network, clock, database, host, nullptr};
  kyna::LanguageSession deterministicSession(std::move(deterministicOptions));
  assert(deterministicSession
             .runSource("capabilities.kyna",
                        "writeFile(\"memory\", \"value\"); readFile(\"memory\"); "
                        "processRun(\"command\"); build(\"command\"); "
                        "processEnv(\"NAME\"); sleep(1); wait(1); "
                        "httpGet(\"http://example.test\"); "
                        "const response = fetch(\"http://example.test\"); "
                        "const values = response.json(); const ordered = sort(values); "
                        "if (ordered[0] != 1) { error(\"sort failed\"); } "
                        "const metadata = process.json(\"{\\\"ready\\\":true}\"); "
                        "if (metadata.ready != true) { error(\"json failed\"); } "
                        "const createdResponse = fetch(\"https://example.test/products\", "
                        "{ method: \"POST\", body: \"{\\\"title\\\":\\\"created\\\"}\", "
                        "headers: { Authorization: \"Bearer test\" } }); "
                        "const created = createdResponse.json(); "
                        "if (createdResponse.status != 201 || created.id != 31 || "
                        "createdResponse.headers[\"content-type\"] != \"application/json\") { error(\"POST failed\"); } "
                        "try { const query = db.query(\"postgres://test\", "
                        "\"SELECT id, name, active, nickname FROM users WHERE id = $1\", [7]); "
                        "if (query.affectedRows != 1 || query.command != \"SELECT 1\" || "
                        "query.rows[0].id != 7 || query.rows[0].active != true || "
                        "query.rows[0].nickname != null) { error(\"database mapping failed\"); } "
                        "} catch (message) { error(message); } "
                        "try { createDirectory(\"cache\"); "
                        "writeJsonFile(\"cache/products.json\", created); "
                        "const saved = readJsonFile(\"cache/products.json\"); "
                        "if (saved.id != 31 || !fileExists(\"cache/products.json\")) { "
                        "error(\"JSON file failed\"); } "
                        "const entries = listDirectory(\"cache\"); "
                        "if (entries[0] != \"products.json\") { error(\"list failed\"); } "
                        "removePath(\"cache/products.json\"); "
                        "} catch (message) { error(message); }")
             .ok());
  assert(files->writes == 2 && files->reads == 2);
  assert(files->contents == "{\"id\":31,\"title\":\"created\"}");
  assert(files->directories == 1 && files->removals == 1);
  assert(processes->runs == 2);
  assert(network->requests == 3);
  assert(network->lastMethod == "POST");
  assert(network->lastBody == "{\"title\":\"created\"}");
  assert(network->lastHeaders.at("Authorization") == "Bearer test");
  assert(clock->sleeps == 2);
  assert(database->queries == 1);
  assert(database->lastRequest.parameters.size() == 1);
  assert(std::get<std::int64_t>(database->lastRequest.parameters[0]) == 7);
  const auto hostInformation = deterministicSession.runSource(
      "host-information.kyna",
      "if (os.name() != \"test-os\" || osArchitecture() != \"test-architecture\" || "
      "os.cwd() != \"/virtual/workspace\" || !terminal.interactive() || "
      "terminalSupportsColor()) { error(\"host information mapping\"); }");
  assert(hostInformation.ok());
  const auto bytecodeReads = files->reads;
  const auto bytecodeWrites = files->writes;
  const auto bytecodeDirectories = files->directories;
  const auto bytecodeRequests = network->requests;
  const auto bytecodeSleeps = clock->sleeps;
  const auto bytecodeCapabilities = deterministicSession.runSource(
      "bytecode-capabilities.kyna",
      "writeFile(\"vm.txt\", \"draft\"); "
      "var contents = readFile(\"vm.txt\"); "
      "contents = textReplace(contents, \"draft\", \"published\"); "
      "writeFile(\"vm.txt\", contents); "
      "const edited = readFile(\"vm.txt\"); "
      "if (edited != \"published\" || !fileExists(\"vm.txt\")) { error(\"file edit\"); } "
      "createDirectory(\"vm-cache\"); "
      "const present: str? = processEnv(\"NAME\"); "
      "const missing: str? = process.env(\"MISSING\"); "
      "if (present != \"test\" || missing != null) { error(\"environment\"); } "
      "sleep(1); "
      "if (httpGet(\"http://example.test\") != \"[3,1,2]\") { error(\"network\"); }");
  assert(bytecodeCapabilities.ok());
  assert(files->reads == bytecodeReads + 2);
  assert(files->writes == bytecodeWrites + 2);
  assert(files->contents == "published");
  assert(files->directories == bytecodeDirectories + 1);
  assert(processes->environmentNames.size() >= 2);
  assert(processes->environmentNames[processes->environmentNames.size() - 2] == "NAME");
  assert(processes->environmentNames.back() == "MISSING");
  assert(network->requests == bytecodeRequests + 1);
  assert(clock->sleeps == bytecodeSleeps + 1);
  const auto bytecodeFetchRequests = network->requests;
  const auto bytecodeFetch = deterministicSession.runSource(
      "bytecode-fetch.kyna",
      "try { "
      "const response = fetch(\"https://example.test/products\", "
      "{ method: \"POST\", body: \"{\\\"title\\\":\\\"created\\\"}\", "
      "timeout: 1200, headers: { Authorization: \"Bearer bytecode\", "
      "\"X-API-Key\": \"test-key\", Cookie: \"session=test-session\", "
      "\"Content-Type\": \"application/json\" } }); "
      "const product = response.json(); "
      "if (!response.ok || response.status != 201 || product.id != 31 || "
      "response.text() != \"{\\\"id\\\":31,\\\"title\\\":\\\"created\\\"}\" || "
      "response.headers[\"content-type\"] != \"application/json\") { "
      "error(\"fetch response\"); } "
      "} catch (failure) { throw failure; }");
  assert(bytecodeFetch.ok());
  assert(network->requests == bytecodeFetchRequests + 1);
  assert(network->lastMethod == "POST");
  assert(network->lastBody == "{\"title\":\"created\"}");
  assert(network->lastHeaders.at("Authorization") == "Bearer bytecode");
  assert(network->lastHeaders.at("X-API-Key") == "test-key");
  assert(network->lastHeaders.at("Cookie") == "session=test-session");
  assert(network->lastHeaders.at("Content-Type") == "application/json");
  assert(network->lastTimeout == std::chrono::milliseconds(1200));
  const auto statusResponse = deterministicSession.runSource(
      "bytecode-http-status.kyna",
      "const response = fetch(\"https://unauthorized.test\", { timeout: 800 }); "
      "const body = response.json(); "
      "if (response.ok || response.status != 401 || body.error != \"unauthorized\") { "
      "error(\"HTTP status handling\"); }");
  assert(statusResponse.ok());
  const auto invalidResponseJson = deterministicSession.runSource(
      "bytecode-invalid-response-json.kyna",
      "try { const response = fetch(\"https://invalid-json.test\", { timeout: 800 }); "
      "response.json(); error(\"expected invalid JSON\"); "
      "} catch (failure) { if (failure.code != \"K5100\") { throw failure; } }");
  assert(invalidResponseJson.ok());
  const auto caughtTimeout = deterministicSession.runSource(
      "bytecode-timeout.kyna",
      "try { fetch(\"https://timeout.test\", { timeout: 25 }); "
      "error(\"expected timeout\"); } catch (failure) { "
      "if (failure.code != \"KNET2001\" || !textContains(failure.message, \"timeout\")) { "
      "throw failure; } }");
  assert(caughtTimeout.ok());
  const auto safeFetchSuccess = deterministicSession.runSource(
      "bytecode-safe-fetch-success.kyna",
      "const result = http.tryFetch(\"https://example.test/products\", { timeout: 50 }); "
      "if (!result.ok || result.error != null || result.response.status != 200) { "
      "error(\"safe fetch success\"); }");
  assert(safeFetchSuccess.ok());
  const auto safeFetchHttpFailure = deterministicSession.runSource(
      "bytecode-safe-fetch-http-status.kyna",
      "const result = fetchResult(\"https://unauthorized.test\", { timeout: 50 }); "
      "if (result.ok || result.error != null || result.response.status != 401) { "
      "error(\"safe fetch HTTP status\"); }");
  assert(safeFetchHttpFailure.ok());
  const auto safeFetchTransportFailure = deterministicSession.runSource(
      "bytecode-safe-fetch-transport.kyna",
      "const result = http.tryFetch(\"https://failure.test\", { timeout: 50 }); "
      "if (result.ok || result.response != null || result.error.code != \"KNET2001\" || "
      "!textContains(result.error.message, \"request failed\")) { error(\"safe fetch transport\"); }");
  assert(safeFetchTransportFailure.ok());
  const auto bytecodeJson = deterministicSession.runSource(
      "bytecode-json.kyna",
      "const decoded = jsonParse(\"{\\\"items\\\":[20,22],\\\"ready\\\":true}\"); "
      "if (decoded.items[1] != 22 || decoded.ready != true) { error(\"decode\"); } "
      "const encoded = jsonStringify(decoded); "
      "if (encoded != \"{\\\"items\\\":[20,22],\\\"ready\\\":true}\") { error(\"encode\"); }");
  assert(bytecodeJson.ok());
  const auto caughtJsonFailure = deterministicSession.runSource(
      "bytecode-json-failure.kyna",
      "try { jsonParse(\"{broken}\"); } catch (failure) { "
      "if (failure.code != \"K5100\") { throw failure; } }");
  assert(caughtJsonFailure.ok());
  const auto bytecodeCollections = deterministicSession.runSource(
      "bytecode-collections.kyna",
      "var values = [3,1,2,2]; push(values, 4); "
      "if (pop(values) != 4) { error(\"pop\"); } "
      "const ordered = sort(values); const distinct = unique(ordered); "
      "const names = keys({ beta: 2, alpha: 1 }); "
      "if (ordered[0] != 1 || ordered[3] != 3 || len(distinct) != 3 || "
      "names[0] != \"alpha\" || names[1] != \"beta\") { error(\"collections\"); }");
  assert(bytecodeCollections.ok());
  const auto bytecodeText = deterministicSession.runSource(
      "bytecode-text.kyna",
      "const greeting = \"  Héllo 世界  \"; "
      "if (len(\"世界\") != 2 || textFind(greeting, \"世界\") != 8 || "
      "textSlice(greeting, 2, 7) != \"Héllo\" || !textContains(greeting, \"éll\") || "
      "textReplace(\"a世界a\", \"世界\", \"Kyna\") != \"aKynaa\" || "
      "textTrim(greeting) != \"Héllo 世界\" || textLower(\"ÄBC\") != \"äbc\" || "
      "textUpper(\"kyna\") != \"KYNA\") { error(\"unicode text\"); } "
      "const pieces = textSplit(\"one::two::three\", \"::\"); "
      "if (len(pieces) != 3 || pieces[1] != \"two\") { error(\"text split\"); }");
  assert(bytecodeText.ok());
  const auto caughtTextFailure = deterministicSession.runSource(
      "bytecode-text-failure.kyna",
      "try { textSlice(\"Kyna\", 0, 9); } catch (failure) { "
      "if (failure.code != \"KTEXT2002\") { throw failure; } }");
  assert(caughtTextFailure.ok());
  const auto bytecodeJsonFileWrites = files->writes;
  const auto bytecodeJsonFileReads = files->reads;
  const auto bytecodeRemovals = files->removals;
  const auto bytecodeProcessRuns = processes->runs;
  const auto bytecodeHostUtilities = deterministicSession.runSource(
      "bytecode-host-utilities.kyna",
      "writeJsonFile(\"products.json\", { id: 42, revision: 1 }); "
      "const initial = readJsonFile(\"products.json\"); "
      "writeJsonFile(\"products.json\", { id: initial.id, revision: initial.revision + 1 }); "
      "const saved = readJsonFile(\"products.json\"); "
      "const entries = listDirectory(\".\"); "
      "if (saved.id != 42 || saved.revision != 2 || entries[0] != \"products.json\" || "
      "processRun(\"deterministic\") != 0 || !removePath(\"products.json\")) { "
      "error(\"host utilities\"); }");
  assert(bytecodeHostUtilities.ok());
  assert(files->writes == bytecodeJsonFileWrites + 2);
  assert(files->reads == bytecodeJsonFileReads + 2);
  assert(files->removals == bytecodeRemovals + 1);
  assert(processes->runs == bytecodeProcessRuns + 1);
  auto collectionsResult = deterministicSession.runSource(
      "collections.kyna",
      "fn kynaDouble(value: int): int { return value * 2; } "
      "fn kynaSum(total: int, value: int): int { return total + value; } "
      "fn kynaEven(value: int): bool { return value % 2 == 0; } "
      "const collectionInput = [1, 2, 2, 3]; "
      "const mappedValues = map(collectionInput, kynaDouble); "
      "const totalValue = reduce(collectionInput, kynaSum, 0); "
      "const firstEven = find(collectionInput, kynaEven); "
      "const distinctValues = unique(collectionInput); "
      "const keyedValue = process.json(\"{\\\"content-type\\\":\\\"application/json\\\"}\"); "
      "if (mappedValues[3] != 6 || totalValue != 8 || firstEven != 2 || "
      "distinctValues[2] != 3 || !any(collectionInput, kynaEven) || "
      "all(collectionInput, kynaEven) || keyedValue[\"content-type\"] != \"application/json\") { "
      "error(\"collection algorithms failed\"); }");
  assert(collectionsResult.ok());
  const auto writesBeforeCallbackFailure = files->writes;
  const auto requestsBeforeCallbackFailure = network->requests;
  const auto processesBeforeCallbackFailure = processes->runs;
  const auto callbackFailure = deterministicSession.runSource(
      "callback-effects-once.kyna",
      "fn reject(value: int): int { throw \"callback failed\"; } "
      "writeFile(\"effect.txt\", \"once\"); "
      "fetch(\"https://example.test\"); processRun(\"deterministic\"); "
      "map([1, 2], reject);");
  assert(!callbackFailure.ok());
  assert(files->writes == writesBeforeCallbackFailure + 1);
  assert(network->requests == requestsBeforeCallbackFailure + 1);
  assert(processes->runs == processesBeforeCallbackFailure + 1);
  kyna::LanguageSessionOptions measuredOptions;
  measuredOptions.capabilities = {files, processes, network, clock, database, host, nullptr};
  measuredOptions.collectMetrics = true;
  kyna::LanguageSession measuredSession(measuredOptions);
  const auto measuredWrites = files->writes;
  const auto measuredFailure = measuredSession.runSource("vm-callback-failure.kyna",
      "fn reject(x: int): int { throw \"failed\"; } "
      "writeFile(\"effect.txt\", \"once\"); map([1], reject);");
  assert(!measuredFailure.ok());
  assert(files->writes == measuredWrites + 1);
  bool failedInVm = false;
  for (const auto &metric : measuredFailure.metrics) {
    assert(metric.phase != "tree_execute");
    failedInVm = failedInVm || metric.phase == "vm_execute";
  }
  assert(failedInVm);
  assert(!measuredFailure.diagnostics.back().callFrames.empty());
  assert(measuredFailure.diagnostics.back().callFrames.front().function == "reject");
  const auto measuredCallbacks = measuredSession.runSource(
      "nested-callbacks.kyna",
      "fn double(x: int): int { return x * 2; } "
      "fn nested(x: int): int { const mapped = map([x], double); collectGarbage(); return mapped[0]; } "
      "fn sum(a: int, b: int): int { return a + b; } "
      "var values = []; loop (var i = 0; i < 1000; i = i + 1) { push(values, i); } "
      "const mapped = map(values, nested); "
      "if (reduce(mapped, sum, 0) != 999000) { error(\"nested callbacks\"); } "
      "fn reject(x: int): int { throw \"caught callback\"; } "
      "var caught = false; try { map([1], reject); } catch (failure) { caught = true; } "
      "if (!caught) { error(\"callback exception lost\"); }");
  assert(measuredCallbacks.ok());
  bool usedVm = false;
  for (const auto &metric : measuredCallbacks.metrics) {
    assert(metric.phase != "tree_execute");
    usedVm = usedVm || metric.phase == "vm_execute";
  }
  assert(usedVm);
  const auto sortedCallbacks = measuredSession.runSource("sort-callback.kyna",
      "fn swap(a: int, b: int): bool { collectGarbage(); return a > b; } "
      "const input = [3, 1, 2]; const output = sort(input, swap); "
      "if (input[0] != 3 || output[0] != 1 || output[2] != 3) { error(\"sort copy\"); }");
  assert(sortedCallbacks.ok());
  const auto emptyCallbacks = measuredSession.runSource("empty-callbacks.kyna",
      "fn yes(x: int): bool { return true; } "
      "fn sum(a: int, b: int): int { return a + b; } "
      "if (any([], yes) || !all([], yes) || find([], yes) != null || "
      "len(filter([], yes)) != 0 || reduce([], sum, 42) != 42) { error(\"empty\"); } "
      "fn indexed(x: int, index: int): int { return x + index; } "
      "const values = map([10, 10], indexed); if (values[1] != 11) { error(\"index\"); }");
  assert(emptyCallbacks.ok());
  auto failedNetwork = deterministicSession.runSource(
      "network-failure.kyna",
      "const response = fetch(\"https://failure.test\", { timeout: 1000 });");
  assert(!failedNetwork.ok());
  assert(hasCode(failedNetwork.diagnostics, "KNET2001"));
  assert(failedNetwork.diagnostics.back().location.line == 1);
  auto failedDatabase = deterministicSession.runSource(
      "database-failure.kyna",
      "const result = db.query(\"postgres://test\", \"FAIL\");");
  assert(!failedDatabase.ok());
  assert(hasCode(failedDatabase.diagnostics, "KDB2001"));
  assert(failedDatabase.diagnostics.back().location.line == 1);
  auto invalidIndex = deterministicSession.runSource(
      "invalid-index.kyna", "const boundsProbe = [1]; print(boundsProbe[2]);");
  assert(!invalidIndex.ok());
  assert(hasCode(invalidIndex.diagnostics, "KRT2104"));
  auto zeroRemainder = deterministicSession.runSource(
      "zero-remainder.kyna", "print(5 % 0);");
  assert(!zeroRemainder.ok());
  assert(hasCode(zeroRemainder.diagnostics, "KRT2201"));
  assert(!session
              .checkSource("private.kyna", "class Secret { value: int; } var secret = new Secret(); "
                                         "var leaked = secret.value;")
              .ok());
  assert(
      !session.checkSource("final.kyna", "final class Base { } class Derived extends Base { }").ok());
  assert(!session
              .checkSource("override.kyna",
                           "class Base { public fn value(): int { return 1; } } "
                           "class Derived extends Base { public fn value(): int { return 2; } }")
              .ok());
  assert(!session
              .checkSource("abstract.kyna",
                           "abstract class Base { public abstract fn value(): int; } "
                           "class Derived extends Base { }")
              .ok());

  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory =
      std::filesystem::temp_directory_path() / ("kyna-v03-language-test-" + std::to_string(nonce));
  std::filesystem::create_directories(directory);
  writeSource(directory / "math.kyna", "export fn add(a: int, b: int): int { return a + b; } "
                                     "fn hidden(): int { return 9; }");
  writeSource(directory / "main.kyna",
              "import \"./math.kyna\" as math; var answer: int = math.add(20, 22);");
  kyna::LanguageSession moduleSession;
  assert(moduleSession.check(directory / "main.kyna").ok());
  assert(moduleSession.run(directory / "main.kyna").ok());

  auto moduleFiles = std::make_shared<RecordingFiles>();
  kyna::LanguageSessionOptions cachedModuleOptions;
  cachedModuleOptions.capabilities = {moduleFiles, processes, network, clock, database, host,
                                      nullptr};
  kyna::LanguageSession cachedModuleSession(std::move(cachedModuleOptions));
  writeSource(directory / "side-effect.kyna",
              "export const value = 1; writeFile(\"initialization\", \"once\");");
  writeSource(directory / "cached-main.kyna", "import \"./side-effect.kyna\" as side;");
  assert(cachedModuleSession.run(directory / "cached-main.kyna").ok());
  assert(cachedModuleSession.run(directory / "cached-main.kyna").ok());
  assert(moduleFiles->writes == 1);

  auto overlay = moduleSession.checkSourceAtPath(
      directory / "main.kyna", "import \"./math.kyna\" as math; var answer: int = math.add(1, 2);");
  assert(overlay.ok());

  writeSource(directory / "private-import.kyna",
              "import \"./math.kyna\" as math; var answer = math.hidden();");
  auto privateImport = moduleSession.check(directory / "private-import.kyna");
  assert(!privateImport.ok());
  assert(privateImport.diagnostics.front().message.find("no exported member") != std::string::npos);

  writeSource(directory / "cycle-a.kyna", "import \"./cycle-b.kyna\" as b;");
  writeSource(directory / "cycle-b.kyna", "import \"./cycle-a.kyna\" as a;");
  auto cycle = moduleSession.check(directory / "cycle-a.kyna");
  assert(!cycle.ok());
  assert(hasCode(cycle.diagnostics, "K4002"));

  const auto persistedDirectory = directory / "persisted";
  const auto persistedTextFile = persistedDirectory / "message.txt";
  const auto persistedFile = persistedDirectory / "products.json";
  const auto pathText = [](const std::filesystem::path &path) { return path.generic_string(); };
  const auto persistenceSource =
      "try { createDirectory(\"" + pathText(persistedDirectory) + "\"); writeFile(\"" +
      pathText(persistedTextFile) + "\", \"draft\"); var text = readFile(\"" +
      pathText(persistedTextFile) + "\"); text = textReplace(text, \"draft\", \"published\"); "
      "writeFile(\"" + pathText(persistedTextFile) + "\", text); writeJsonFile(\"" +
      pathText(persistedFile) + "\", { id: 1, revision: 1 }); const initial = readJsonFile(\"" +
      pathText(persistedFile) + "\"); writeJsonFile(\"" + pathText(persistedFile) +
      "\", { id: initial.id, revision: initial.revision + 1 }); const saved = readJsonFile(\"" +
      pathText(persistedFile) + "\"); if (readFile(\"" + pathText(persistedTextFile) +
      "\") != \"published\" || saved.id != 1 || saved.revision != 2 || !fileExists(\"" +
      pathText(persistedFile) + "\")) { error(\"persistence edit failed\"); } "
      "const entries = listDirectory(\"" + pathText(persistedDirectory) +
      "\"); if (len(entries) != 2) { error(\"listing failed\"); } removePath(\"" +
      pathText(persistedTextFile) + "\"); removePath(\"" + pathText(persistedFile) +
      "\"); removePath(\"" + pathText(persistedDirectory) + "\"); } "
      "catch (message) { error(message); }";
  kyna::LanguageSession productionFileSession;
  assert(productionFileSession.runSource("persistence.kyna", persistenceSource).ok());
  assert(!std::filesystem::exists(persistedDirectory));

  const auto packageDir = directory / "pkg";
  std::filesystem::create_directories(packageDir);
  {
    std::ofstream helpers(packageDir / "helpers.kyna");
    helpers << "fn add(a: int, b: int): int { return a + b; }\n";
  }
  {
    std::ofstream entry(packageDir / "main.kyna");
    entry << "print(add(20, 22));\n";
  }
  kyna::LanguageSession packageSession;
  const auto packageCheck = packageSession.check(packageDir);
  assert(packageCheck.ok());
  const auto packageRun = packageSession.run(packageDir);
  assert(packageRun.ok());

  const auto importedPkg = directory / "libpkg";
  std::filesystem::create_directories(importedPkg);
  {
    std::ofstream helpers(importedPkg / "math.kyna");
    helpers << "export fn add(a: int, b: int): int { return a + b; }\n";
  }
  writeSource(directory / "import-dir.kyna",
              "import \"./libpkg\" as lib; var n: int = lib.add(20, 22);");
  assert(moduleSession.check(directory / "import-dir.kyna").ok());

  std::filesystem::remove_all(directory);
}
