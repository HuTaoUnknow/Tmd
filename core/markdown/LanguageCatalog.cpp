#include "core/markdown/LanguageCatalog.h"
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
