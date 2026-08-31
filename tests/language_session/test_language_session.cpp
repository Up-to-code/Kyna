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
  int run(const std::string &) override {
    ++runs;
    return 0;
  }
  std::optional<std::string> environment(const std::string &) override {
    return std::string("test");
  }
};

class RecordingNetwork final : public kyna::NetworkPort {
public:
  int requests{0};
  std::string lastMethod;
  std::optional<std::string> lastBody;
  std::map<std::string, std::string> lastHeaders;
  std::optional<kyna::NetworkResponse> send(const kyna::NetworkRequest &request,
                                            kyna::NetworkFailure &failure) override {
    ++requests;
    lastMethod = request.method;
    lastBody = request.body;
    lastHeaders = request.headers;
    if (request.url == "https://failure.test") {
      failure = {kyna::NetworkFailurePhase::Tls, 35, "certificate handshake rejected", false};
      return std::nullopt;
    }
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
  const kyna::SourceFile comments{7, "comments.kyna", "# π\nlet value: int = 1;"};
  auto lexed = kyna::tokenize(comments);
  assert(lexed.ok());
  assert(lexed.tokens.front().kind == kyna::TokenKind::Let);
  assert(lexed.tokens.front().location.startByte == 5);
  assert(lexed.tokens.front().location.line == 2);
  assert(lexed.tokens.front().location.column == 1);

  const kyna::SourceFile malformed{8, "malformed.kyna", "let first = ; let second = ;"};
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
                                         "set response = fetch(\"http://example.test/products\");");
  assert(unprotected.ok());
  assert(hasCode(unprotected.diagnostics, "K2601"));
  assert(hasCode(unprotected.diagnostics, "K2603"));
  const auto protectedFetch = session.checkSource(
      "protected.kyna", "try { set response = fetch(\"https://example.test/products\"); } "
                      "catch (message) { print(message); }");
  assert(protectedFetch.ok());
  assert(!hasCode(protectedFetch.diagnostics, "K2601"));
  const auto emptyCatch =
      session.checkSource("empty-catch.kyna", "try { error(\"failure\"); } catch (message) { }");
  assert(emptyCatch.ok());
  assert(hasCode(emptyCatch.diagnostics, "K2602"));
  const auto unprotectedDatabase = session.checkSource(
      "database-warning.kyna",
      "set sql = \"SELECT * FROM users\"; set result = db.query(\"postgres://local\", sql);");
  assert(unprotectedDatabase.ok());
  assert(hasCode(unprotectedDatabase.diagnostics, "K2601"));
  assert(hasCode(unprotectedDatabase.diagnostics, "K2610"));
  const auto nullMember = session.checkSource(
      "null-member.kyna", "set missing = null; print(missing.name);");
  assert(!nullMember.ok());
  assert(hasCode(nullMember.diagnostics, "KSEM2401"));

  auto files = std::make_shared<RecordingFiles>();
  auto processes = std::make_shared<RecordingProcesses>();
  auto network = std::make_shared<RecordingNetwork>();
  auto clock = std::make_shared<RecordingClock>();
  auto database = std::make_shared<RecordingDatabase>();
  kyna::LanguageSessionOptions deterministicOptions;
  deterministicOptions.capabilities = {files, processes, network, clock, database};
  kyna::LanguageSession deterministicSession(std::move(deterministicOptions));
  assert(deterministicSession
             .runSource("capabilities.kyna",
                        "writeFile(\"memory\", \"value\"); readFile(\"memory\"); "
                        "processRun(\"command\"); processEnv(\"NAME\"); sleep(1); "
                        "httpGet(\"http://example.test\"); "
                        "set response = fetch(\"http://example.test\"); "
                        "set values = response.json(); set ordered = sort(values); "
                        "if (ordered[0] != 1) { error(\"sort failed\"); } "
                        "set metadata = process.json(\"{\\\"ready\\\":true}\"); "
                        "if (metadata.ready != true) { error(\"json failed\"); } "
                        "set createdResponse = fetch(\"https://example.test/products\", "
                        "{ method: \"POST\", body: \"{\\\"title\\\":\\\"created\\\"}\", "
                        "headers: { Authorization: \"Bearer test\" } }); "
                        "set created = createdResponse.json(); "
                        "if (createdResponse.status != 201 || created.id != 31 || "
                        "createdResponse.headers[\"content-type\"] != \"application/json\") { error(\"POST failed\"); } "
                        "try { set query = db.query(\"postgres://test\", "
                        "\"SELECT id, name, active, nickname FROM users WHERE id = $1\", [7]); "
                        "if (query.affectedRows != 1 || query.command != \"SELECT 1\" || "
                        "query.rows[0].id != 7 || query.rows[0].active != true || "
                        "query.rows[0].nickname != null) { error(\"database mapping failed\"); } "
                        "} catch (message) { error(message); } "
                        "try { createDirectory(\"cache\"); "
                        "writeJsonFile(\"cache/products.json\", created); "
                        "set saved = readJsonFile(\"cache/products.json\"); "
                        "if (saved.id != 31 || !fileExists(\"cache/products.json\")) { "
                        "error(\"JSON file failed\"); } "
                        "set entries = listDirectory(\"cache\"); "
                        "if (entries[0] != \"products.json\") { error(\"list failed\"); } "
                        "removePath(\"cache/products.json\"); "
                        "} catch (message) { error(message); }")
             .ok());
  assert(files->writes == 2 && files->reads == 2);
  assert(files->contents == "{\"id\":31,\"title\":\"created\"}");
  assert(files->directories == 1 && files->removals == 1);
  assert(processes->runs == 1);
  assert(network->requests == 3);
  assert(network->lastMethod == "POST");
  assert(network->lastBody == "{\"title\":\"created\"}");
  assert(network->lastHeaders.at("Authorization") == "Bearer test");
  assert(clock->sleeps == 1);
  assert(database->queries == 1);
  assert(database->lastRequest.parameters.size() == 1);
  assert(std::get<std::int64_t>(database->lastRequest.parameters[0]) == 7);
  const auto bytecodeReads = files->reads;
  const auto bytecodeWrites = files->writes;
  const auto bytecodeDirectories = files->directories;
  const auto bytecodeRequests = network->requests;
  const auto bytecodeSleeps = clock->sleeps;
  const auto bytecodeCapabilities = deterministicSession.runSource(
      "bytecode-capabilities.kyna",
      "writeFile(\"vm.txt\", \"bytecode-host\"); "
      "set contents = readFile(\"vm.txt\"); "
      "if (contents != \"bytecode-host\" || !fileExists(\"vm.txt\")) { error(\"file\"); } "
      "createDirectory(\"vm-cache\"); "
      "if (processEnv(\"NAME\") != \"test\") { error(\"environment\"); } "
      "sleep(1); "
      "if (httpGet(\"http://example.test\") != \"[3,1,2]\") { error(\"network\"); }");
  assert(bytecodeCapabilities.ok());
  assert(files->reads == bytecodeReads + 1);
  assert(files->writes == bytecodeWrites + 1);
  assert(files->directories == bytecodeDirectories + 1);
  assert(network->requests == bytecodeRequests + 1);
  assert(clock->sleeps == bytecodeSleeps + 1);
  const auto bytecodeFetchRequests = network->requests;
  const auto bytecodeFetch = deterministicSession.runSource(
      "bytecode-fetch.kyna",
      "try { "
      "set response = fetch(\"https://example.test/products\", "
      "{ method: \"POST\", body: \"{\\\"title\\\":\\\"created\\\"}\", "
      "timeout: 1200, headers: { Authorization: \"Bearer bytecode\" } }); "
      "set product = response.json(); "
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
  const auto bytecodeJson = deterministicSession.runSource(
      "bytecode-json.kyna",
      "set decoded = jsonParse(\"{\\\"items\\\":[20,22],\\\"ready\\\":true}\"); "
      "if (decoded.items[1] != 22 || decoded.ready != true) { error(\"decode\"); } "
      "set encoded = jsonStringify(decoded); "
      "if (encoded != \"{\\\"items\\\":[20,22],\\\"ready\\\":true}\") { error(\"encode\"); }");
  assert(bytecodeJson.ok());
  const auto caughtJsonFailure = deterministicSession.runSource(
      "bytecode-json-failure.kyna",
      "try { jsonParse(\"{broken}\"); } catch (failure) { "
      "if (failure.code != \"K5100\") { throw failure; } }");
  assert(caughtJsonFailure.ok());
  const auto bytecodeCollections = deterministicSession.runSource(
      "bytecode-collections.kyna",
      "let values = [3,1,2,2]; push(values, 4); "
      "if (pop(values) != 4) { error(\"pop\"); } "
      "set ordered = sort(values); set distinct = unique(ordered); "
      "set names = keys({ beta: 2, alpha: 1 }); "
      "if (ordered[0] != 1 || ordered[3] != 3 || len(distinct) != 3 || "
      "names[0] != \"alpha\" || names[1] != \"beta\") { error(\"collections\"); }");
  assert(bytecodeCollections.ok());
  const auto bytecodeText = deterministicSession.runSource(
      "bytecode-text.kyna",
      "set greeting = \"  Héllo 世界  \"; "
      "if (len(\"世界\") != 2 || textFind(greeting, \"世界\") != 8 || "
      "textSlice(greeting, 2, 7) != \"Héllo\" || !textContains(greeting, \"éll\") || "
      "textReplace(\"a世界a\", \"世界\", \"Kyna\") != \"aKynaa\" || "
      "textTrim(greeting) != \"Héllo 世界\" || textLower(\"ÄBC\") != \"äbc\" || "
      "textUpper(\"kyna\") != \"KYNA\") { error(\"unicode text\"); } "
      "set pieces = textSplit(\"one::two::three\", \"::\"); "
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
      "writeJsonFile(\"products.json\", { id: 42, ready: true }); "
      "set saved = readJsonFile(\"products.json\"); "
      "set entries = listDirectory(\".\"); "
      "if (saved.id != 42 || entries[0] != \"products.json\" || "
      "processRun(\"deterministic\") != 0 || !removePath(\"products.json\")) { "
      "error(\"host utilities\"); }");
  assert(bytecodeHostUtilities.ok());
  assert(files->writes == bytecodeJsonFileWrites + 1);
  assert(files->reads == bytecodeJsonFileReads + 1);
  assert(files->removals == bytecodeRemovals + 1);
  assert(processes->runs == bytecodeProcessRuns + 1);
  auto collectionsResult = deterministicSession.runSource(
      "collections.kyna",
      "func kynaDouble(value: int): int { return value * 2; } "
      "func kynaSum(total: int, value: int): int { return total + value; } "
      "func kynaEven(value: int): bool { return value % 2 == 0; } "
      "set collectionInput = [1, 2, 2, 3]; "
      "set mappedValues = map(collectionInput, kynaDouble); "
      "set totalValue = reduce(collectionInput, kynaSum, 0); "
      "set firstEven = find(collectionInput, kynaEven); "
      "set distinctValues = unique(collectionInput); "
      "set keyedValue = process.json(\"{\\\"content-type\\\":\\\"application/json\\\"}\"); "
      "if (mappedValues[3] != 6 || totalValue != 8 || firstEven != 2 || "
      "distinctValues[2] != 3 || !any(collectionInput, kynaEven) || "
      "all(collectionInput, kynaEven) || keyedValue[\"content-type\"] != \"application/json\") { "
      "error(\"collection algorithms failed\"); }");
  assert(collectionsResult.ok());
  auto failedNetwork = deterministicSession.runSource(
      "network-failure.kyna",
      "set response = fetch(\"https://failure.test\", { timeout: 1000 });");
  assert(!failedNetwork.ok());
  assert(hasCode(failedNetwork.diagnostics, "KNET2001"));
  assert(failedNetwork.diagnostics.back().location.line == 1);
  auto failedDatabase = deterministicSession.runSource(
      "database-failure.kyna",
      "set result = db.query(\"postgres://test\", \"FAIL\");");
  assert(!failedDatabase.ok());
  assert(hasCode(failedDatabase.diagnostics, "KDB2001"));
  assert(failedDatabase.diagnostics.back().location.line == 1);
  auto invalidIndex = deterministicSession.runSource(
      "invalid-index.kyna", "set boundsProbe = [1]; print(boundsProbe[2]);");
  assert(!invalidIndex.ok());
  assert(hasCode(invalidIndex.diagnostics, "KRT2104"));
  auto zeroRemainder = deterministicSession.runSource(
      "zero-remainder.kyna", "print(5 % 0);");
  assert(!zeroRemainder.ok());
  assert(hasCode(zeroRemainder.diagnostics, "KRT2201"));
  assert(!session
              .checkSource("private.kyna", "class Secret { value: int; } let secret = new Secret(); "
                                         "let leaked = secret.value;")
              .ok());
  assert(
      !session.checkSource("final.kyna", "final class Base { } class Derived extends Base { }").ok());
  assert(!session
              .checkSource("override.kyna",
                           "class Base { public func value(): int { return 1; } } "
                           "class Derived extends Base { public func value(): int { return 2; } }")
              .ok());
  assert(!session
              .checkSource("abstract.kyna",
                           "abstract class Base { public abstract func value(): int; } "
                           "class Derived extends Base { }")
              .ok());

  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory =
      std::filesystem::temp_directory_path() / ("kyna-v03-language-test-" + std::to_string(nonce));
  std::filesystem::create_directories(directory);
  writeSource(directory / "math.kyna", "export func add(a: int, b: int): int { return a + b; } "
                                     "func hidden(): int { return 9; }");
  writeSource(directory / "main.kyna",
              "import \"./math.kyna\" as math; let answer: int = math.add(20, 22);");
  kyna::LanguageSession moduleSession;
  assert(moduleSession.check(directory / "main.kyna").ok());
  assert(moduleSession.run(directory / "main.kyna").ok());

  auto moduleFiles = std::make_shared<RecordingFiles>();
  kyna::LanguageSessionOptions cachedModuleOptions;
  cachedModuleOptions.capabilities = {moduleFiles, processes, network, clock, database};
  kyna::LanguageSession cachedModuleSession(std::move(cachedModuleOptions));
  writeSource(directory / "side-effect.kyna",
              "export set value = 1; writeFile(\"initialization\", \"once\");");
  writeSource(directory / "cached-main.kyna", "import \"./side-effect.kyna\" as side;");
  assert(cachedModuleSession.run(directory / "cached-main.kyna").ok());
  assert(cachedModuleSession.run(directory / "cached-main.kyna").ok());
  assert(moduleFiles->writes == 1);

  auto overlay = moduleSession.checkSourceAtPath(
      directory / "main.kyna", "import \"./math.kyna\" as math; let answer: int = math.add(1, 2);");
  assert(overlay.ok());

  writeSource(directory / "private-import.kyna",
              "import \"./math.kyna\" as math; let answer = math.hidden();");
  auto privateImport = moduleSession.check(directory / "private-import.kyna");
  assert(!privateImport.ok());
  assert(privateImport.diagnostics.front().message.find("no exported member") != std::string::npos);

  writeSource(directory / "cycle-a.kyna", "import \"./cycle-b.kyna\" as b;");
  writeSource(directory / "cycle-b.kyna", "import \"./cycle-a.kyna\" as a;");
  auto cycle = moduleSession.check(directory / "cycle-a.kyna");
  assert(!cycle.ok());
  assert(hasCode(cycle.diagnostics, "K4002"));

  const auto persistedDirectory = directory / "persisted";
  const auto persistedFile = persistedDirectory / "products.json";
  const auto persistenceSource =
      "try { createDirectory(\"" + persistedDirectory.string() + "\"); writeJsonFile(\"" +
      persistedFile.string() + "\", { id: 1, title: \"saved\" }); set saved = readJsonFile(\"" +
      persistedFile.string() + "\"); if (saved.id != 1 || !fileExists(\"" + persistedFile.string() +
      "\")) { error(\"persistence failed\"); } set entries = listDirectory(\"" +
      persistedDirectory.string() +
      "\"); if (entries[0] != \"products.json\") { error(\"listing failed\"); } } "
      "catch (message) { error(message); }";
  kyna::LanguageSession productionFileSession;
  assert(productionFileSession.runSource("persistence.kyna", persistenceSource).ok());
  assert(std::filesystem::is_regular_file(persistedFile));

  std::filesystem::remove_all(directory);
}
