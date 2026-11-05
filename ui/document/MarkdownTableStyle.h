#pragma once
class QTextDocument;
struct MarkdownSettings;
namespace MarkdownTableStyle {
void apply(QTextDocument *document, const MarkdownSettings &settings);
}
