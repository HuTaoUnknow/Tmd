#pragma once
#include <QColor>
#include <QStringList>
#include <array>
#include <QMap>

struct HostedImageLink { QString name, url; };

struct MarkdownSettings {
    enum Number { BodySize, CodeSize, H1, H2, H3, H4, H5, H6, LineHeight, HeadingLineHeight,
        ParagraphMargin, HeadingTop, HeadingBottom, ListMargin, ListIndent, QuoteIndent,
        CodePadding, CodeMargin, TabWidth, TablePadding, RenderDelay, NumberCount };
    enum Color { PageBackground, TextColor, HeadingColor, BlockBackground, InlineBackground,
        QuoteBackground, LinkColor, TableBorder, TableHeader, TableEven, TableOdd, ColorCount };
    enum Flag { AutoRender = 1, LanguageCompletion = 2, SyntaxColors = 4, BoldHeadings = 8,
        HeadingRules = 16, CodeBorder = 32, UnderlineLinks = 64, StripedTables = 128, WrapCode = 256 };
    QStringList fonts{"Open Sans", "Microsoft YaHei", "DejaVu Sans Mono"};
    std::array<quint16, NumberCount> numbers{120, 105, 225, 180, 165, 150, 135, 120, 160, 140,
        8, 24, 14, 4, 40, 20, 16, 14, 36, 10, 450};
    std::array<QRgb, ColorCount> colors{0xff212121, 0xffe7e7e7, 0xfff8f8f8, 0xff363636, 0xff303030,
        0xff27312b, 0xff9ed3b5, 0xff46554d, 0xff344239, 0xff2b332e, 0xff242b27};
    quint16 flags = 255;
    quint8 imageMode = 0;
    QString relativePhotoPath = "../md_photo", absolutePhotoPath;
    QList<HostedImageLink> hostedLinks;
    int defaultHostedLink = -1;
    QString uploadEndpoint, uploadFileField = "file", uploadUrlPath = "data.url";
    quint8 uploadAuthKind = 1; // 0 none, 1 header, 2 form, 3 query
    QString uploadAuthName = "Authorization", uploadAuthPrefix = "Bearer ";
    QString uploadToken; // Local protected credential; omitted from portable exports.
    QMap<QString, QString> uploadFields;
    bool enabled(Flag flag) const { return flags & flag; }
    qreal number(Number key) const;
    QColor color(Color key) const { return QColor::fromRgb(colors[key]); }
};

struct MarkdownNumberSpec { QString label, suffix; int minimum, maximum, scale, section; };
struct MarkdownColorSpec { QString label; int section; };
const std::array<MarkdownNumberSpec, MarkdownSettings::NumberCount> &markdownNumberSpecs();
const std::array<MarkdownColorSpec, MarkdownSettings::ColorCount> &markdownColorSpecs();

// Portable configuration omits document data and upload credentials. Core owns
// the versioned binary schema, hexadecimal text, validation and atomic file I/O.
class MarkdownSettingsStore {
public:
    static const MarkdownSettings &current();
    static QString storagePath();
    static QString loadError();
    static bool validate(const MarkdownSettings &settings, QString *error = nullptr);
    static QString encode(const MarkdownSettings &settings);
    static bool decode(const QString &text, MarkdownSettings &settings, QString *error = nullptr);
    static bool read(const QString &path, MarkdownSettings &settings, QString *error = nullptr);
    static bool write(const QString &path, const MarkdownSettings &settings, QString *error = nullptr);
    static bool save(const MarkdownSettings &settings, QString *error = nullptr);
    static bool rememberHostedLink(const QString &name, const QString &url, QString *error = nullptr);
    static void setStoragePath(const QString &path);
};
