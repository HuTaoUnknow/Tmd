#include "core/markdown/LanguageCatalog.h"
#include <QHash>
#include <QRegularExpression>
const QList<QPair<QString, QString>> &LanguageCatalog::entries() {
    static const QList<QPair<QString, QString>> languages = {
        {"plaintext", "纯文本"}, {"python", "Python"}, {"cpp", "C++"}, {"c", "C"}, {"csharp", "C#"},
        {"java", "Java"}, {"javascript", "JavaScript"}, {"typescript", "TypeScript"}, {"js", "JavaScript"}, {"ts", "TypeScript"},
        {"sql", "SQL"}, {"bash", "Bash"}, {"shell", "Shell"}, {"powershell", "PowerShell"}, {"html", "HTML"},
        {"css", "CSS"}, {"scss", "SCSS"}, {"json", "JSON"}, {"yaml", "YAML"}, {"xml", "XML"},
        {"markdown", "Markdown"}, {"go", "Go"}, {"rust", "Rust"}, {"kotlin", "Kotlin"}, {"php", "PHP"},
        {"swift", "Swift"}, {"ruby", "Ruby"}, {"vue", "Vue"}, {"dockerfile", "Dockerfile"}, {"ini", "INI"},
        {"toml", "TOML"}, {"diff", "Diff"}, {"mermaid", "Mermaid"}, {"latex", "LaTeX"}
    };
    return languages;
}
QString LanguageCatalog::canonicalName(const QString &info) {
    QString name = info.trimmed().section(QRegularExpression("\\s+"), 0, 0).toLower();
    if (name.startsWith("language-")) name.remove(0, 9);
    if (name.startsWith("{.")) name = name.mid(2).section('}', 0, 0);
    static const QHash<QString, QString> aliases = {
        {"", "plaintext"}, {"text", "plaintext"}, {"txt", "plaintext"}, {"plain", "plaintext"}, {"none", "plaintext"},
        {"c++", "cpp"}, {"cc", "cpp"}, {"cxx", "cpp"}, {"hpp", "cpp"}, {"hxx", "cpp"}, {"h", "c"},
        {"c#", "csharp"}, {"cs", "csharp"}, {"py", "python"}, {"python3", "python"},
        {"js", "javascript"}, {"jsx", "javascript"}, {"ts", "typescript"}, {"tsx", "typescript"},
        {"sh", "bash"}, {"shell", "bash"}, {"zsh", "bash"}, {"ps1", "powershell"}, {"pwsh", "powershell"},
        {"htm", "html"}, {"xhtml", "html"}, {"markup", "html"}, {"svg", "xml"}, {"yml", "yaml"},
        {"md", "markdown"}, {"rs", "rust"}, {"rb", "ruby"}, {"golang", "go"}, {"kt", "kotlin"},
        {"tex", "latex"}, {"docker", "dockerfile"}
    };
    return aliases.value(name, name);
}
QString LanguageCatalog::displayName(const QString &info) {
    const QString canonical = canonicalName(info);
    for (const auto &entry : entries()) if (entry.first == canonical) return entry.second;
    return info.trimmed().section(QRegularExpression("\\s+"), 0, 0);
}
