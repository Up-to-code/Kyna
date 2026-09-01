#include "../cli_commands.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace kyna::cli {

std::string renderRichDiagnostics(const std::vector<Diagnostic> &diagnostics,
                                  const SourceManager &sources) {
  using namespace ftxui;
  Elements rendered;
  for (const auto &diagnostic : diagnostics) {
    const auto *source = sources.find(diagnostic.location.source);
    const auto file = source ? source->displayName() : std::string("<source>");
    const std::string severity = diagnostic.warning ? "warning" : "error";
    const auto severityColor = diagnostic.warning ? Color::Yellow : Color::Red;
    Elements details{
        text(severity + "[" + diagnostic.code + "] " + diagnostic.message) | bold |
            color(severityColor),
        text(file + ":" + std::to_string(diagnostic.location.line) + ":" +
             std::to_string(diagnostic.location.column)) |
            dim,
    };
    const auto sourceLine = sources.line(diagnostic.location.source, diagnostic.location.line);
    if (!sourceLine.empty()) {
      details.push_back(separator());
      details.push_back(text(sourceLine));
      details.push_back(text(std::string(
                                 static_cast<std::size_t>(
                                     std::max(0, diagnostic.location.column - 1)),
                                 ' ') +
                             "^") |
                        color(severityColor));
    }
    for (const auto &note : diagnostic.notes)
      details.push_back(text("note: " + note) | color(Color::GrayDark));
    for (const auto &cause : diagnostic.causes)
      details.push_back(text("caused by " + cause.domain + "[" + cause.code + "]: " +
                             cause.message) |
                        color(Color::GrayLight));
    if (!diagnostic.help.empty())
      details.push_back(text("help: " + diagnostic.help) | color(Color::Cyan));
    rendered.push_back(vbox(std::move(details)) | borderRounded);
  }
  auto document = vbox(std::move(rendered));
  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  return screen.ToString();
}

} // namespace kyna::cli
