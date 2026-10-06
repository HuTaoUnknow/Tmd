# Tmd settings TXT format / 设置 TXT 格式

`core/MarkdownSettings.h` defines the settings and validation ranges. `core/MarkdownSettings.cpp` owns storage, parsing, import and export. UI code only edits values and applies the parsed settings.

`core/MarkdownSettings.h` 定义选项和有效范围，`core/MarkdownSettings.cpp` 负责存储、解析、导入与导出。界面只调整设置值并应用已解析的设置。

## Version 2, compatible with version 1 / 第二版，兼容第一版

The file is uppercase hexadecimal text, grouped into four characters with whitespace for readability. The decoder accepts either case and whitespace. All multibyte integers use big-endian order.

文件使用大写十六进制文本，每四个字符分组，空白便于阅读。解析支持大小写与空白；多字节整数均采用大端序。

| Field / 字段 | Bytes / 字节 | Description / 含义 |
| --- | --- | --- |
| Magic / 标识 | 4 | ASCII `TMDP` |
| Version / 版本 | 2 | Unsigned integer / 无符号整数 `2`; reader also accepts `1` / 解析兼容 `1` |
| Packed options / 打包选项 | 2 | Bits 0–8: boolean flags; bits 10–11: image mode / 0–8 位保存开关，10–11 位保存图片方式 |
| Numbers / 数值 | 42 | 21 unsigned 16-bit fields, in `MarkdownSettings::Number` order / 按 Number 枚举顺序保存 21 个无符号 16 位值 |
| Colors / 颜色 | 33 | 11 RGB triplets, in `MarkdownSettings::Color` order / 按 Color 枚举顺序保存 11 个 RGB 颜色 |
| Fonts / 字体 | Variable / 可变 | Three UTF-8 names; each begins with a 16-bit byte length / 三个 UTF-8 字体名，每个以 16 位字节长度开头 |
| Checksum / 校验 | 4 | First four bytes of SHA-256 of all preceding bytes / 前述所有字节的 SHA-256 前四个字节 |

Flags in order: automatic rendering, language completion, syntax highlighting, bold headings, heading rules, code borders, underlined links, striped tables, wrapped code. Image modes are `00` relative, `01` absolute, `10` remote URL; `11` is invalid. Reserved bits must be zero. Font sizes use tenths of a point; other numbers use the units and limits in `markdownNumberSpecs()`.

开关依次为自动预加载、语言补全、语法高亮、标题加粗、标题下划线、代码边框、链接下划线、表格交替背景、代码自动换行。图片方式为 `00` 相对路径、`01` 绝对路径、`10` 图床链接，`11` 无效；保留位必须为零。字号采用十分之一磅，其他值的单位与限制由 `markdownNumberSpecs()` 定义。

The default settings occupy fewer than 768 text characters. Parsing is cached in memory. Saving uses the atomic UTF-8 writer in `MarkdownFileIO`; decoding validates into a temporary value before replacing settings. Unsupported versions, unknown bits, invalid ranges or UTF-8, truncated data and checksum failures are rejected. This checksum detects accidental corruption; it is not authentication.

默认设置文本少于 768 个字符，解析结果缓存在内存中。保存使用 `MarkdownFileIO` 的原子 UTF-8 写入；解析先验证临时值，再替换设置。不支持的版本、未知位、无效范围或 UTF-8、截断数据及校验失败均拒绝导入。校验用于检测意外损坏，不提供身份认证。

The automatic file is `Tmd-settings.txt` under Qt's application configuration directory; the settings dialog shows the exact path. Export uses the same format and includes configured image paths, links and API parameters, while omitting documents, image files, zoom, window geometry and Token/Key credentials. Import previews values until Apply is selected.

自动保存文件位于 Qt 应用配置目录下的 `Tmd-settings.txt`，设置窗口显示完整路径。导出使用同一格式，包含配置的图片位置、链接及接口参数，不包含文档、图片文件、缩放、窗口位置和 Token/Key 凭据。导入先展示设置，点击“应用”后才保存。

Version 2 adds relative/absolute image locations (UTF-8 strings with unsigned 16-bit byte lengths), a 16-bit saved-link count and selected index (`FFFF` means none), name/URL pairs, endpoint/file-field/JSON-path strings, an 8-bit authentication mode (0 none, 1 header, 2 form, 3 query), authentication name/prefix strings and a 16-bit count of extra form name/value pairs. The last field is a 16-bit local-credential length and Windows DPAPI ciphertext. Portable exports always write credential length zero. The checksum follows these fields. Version 1 omits this extension and uses default image settings.

第二版在字体字段之后增加相对/绝对图片位置（16 位字节长度及 UTF-8 文本）、16 位链接数量及默认索引（`FFFF` 表示无默认项）、名称/链接对、接口/文件字段/JSON 路径字符串、8 位鉴权方式（0 无鉴权、1 请求头、2 表单、3 查询参数）、鉴权名称/前缀字符串、16 位附加表单字段数量及名称/值对。最后为 16 位本机凭据长度及 Windows DPAPI 密文，导出配置时该长度始终为零，然后附加校验。第一版省略这些扩展，采用图片设置默认值。

`core/ImageUploadJob` adapts configurable POST multipart interfaces. It only inserts a validated HTTP(S) URL after a successful response, supports cancellation and timeouts, and does not resend uploads across redirects. No real image-host service was used for validation; service-specific integration remains for later feedback.

`core/ImageUploadJob` 对接可配置的 POST multipart 接口，只在响应成功且返回有效 HTTP(S) 地址后插入图片链接，支持取消和超时，不跨重定向重新发送上传。尚未使用真实图床验证，具体服务适配留待后续反馈。