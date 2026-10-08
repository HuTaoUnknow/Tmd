#pragma once
#include "core/settings/MarkdownSettings.h"
class MarkdownSettingsCodec {
public:
    static constexpr int MaximumTextSize = 524288;
    static bool validate(const MarkdownSettings &settings, QString *error);
    static QString encode(const MarkdownSettings &settings, bool local, QString *error = nullptr);
    static bool decode(const QString &text, MarkdownSettings &settings, QString *error);
};
