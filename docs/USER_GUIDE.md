# Tmd user guide

[中文](使用教程.md) · [Installation and backup](INSTALLATION.md) · [Home](../README.md)

## 1. Your library

The installed edition keeps your library under **Documents/Tmd**:

```text
Tmd/
├─ md_data/                   Markdown documents and relationships
│  ├─ markdown学习/           Five starter documents
│  └─ .tree-md-relations.json  Relationship metadata
└─ md_photo/                  Local document images
```

The portable edition uses the same folders beside `tree_md.exe`. Image paths are relative to each Markdown document. Back up both folders, including the hidden relationship file.

The application UI is currently in Simplified Chinese. This guide includes the labels needed to navigate it.

### File sidebar context actions

Right-click a Markdown document to rename it or move that specific file to the Windows Recycle Bin. Right-click a folder to create a subfolder or recycle the entire folder. Right-click blank list space to create a folder under `md_data`. Empty folders remain visible.

Folder deletion includes all nested files. Unsaved current documents offer Save, Discard or Cancel before continuing. The File menu also uses the Recycle Bin, with no permanent-delete fallback on failure. Related knowledge-tree connections are removed; `md_photo` images are kept for restoration and shared references. After restoring files, press F5 to refresh and recreate any removed knowledge-tree connections.

## 2. Write your first note

1. Choose **文件 → 新建 (File → New)** and enter a relative path such as `My learning/First note.md`.
2. Add a main heading, two section headings and your own explanation.
3. Switch between **文档 (Document)** and **原文 (Source)** to check the rendered result and markup.
4. Press `Ctrl+S` to save.

```markdown
# My first learning note

## Question

How can I connect my notes into a learning route?

## Practice and review

- Describe one clear topic.
- Identify the knowledge it depends on.
- Record what I can do next.
```

The rendered view is editable. Use Source for precise changes to table syntax, image paths or code fences. Both views share undo history. Switching documents or closing the window prompts when changes remain unsaved.

## 3. Search and navigate

- The left sidebar follows actual file paths.
- `Ctrl+P` opens quick search. Search titles or paths, select with arrow keys, press Enter to open, or Esc to close.
- **目录 (Outline)** toggles heading navigation on the right. Click a heading to move to its section.
- `Ctrl+mouse wheel` changes text zoom between 50% and 200%; `Ctrl+0` resets to 100%.

Narrowing the left sidebar turns it into a small icon/search rail, then hides it. Portrait windows collapse the sidebar automatically; narrower portrait windows hide both sidebars. Quick search remains available, and Outline can be opened manually.

## 4. Build a knowledge tree

Open a document, select **知识树 (Knowledge tree)**, then **完善知识树 (Edit knowledge tree)**. The left list contains documents outside the currently displayed tree. The right canvas explores up to five relationship steps from the current document.

| Relationship | Meaning |
| --- | --- |
| Previous | Knowledge to study before this topic |
| Next | What to study next; previous/next update reciprocally |
| Alternative, shown above | Interchangeable knowledge sharing the same learning route |
| Detail, shown below | Explanations, examples or practice; may be nested |

Drag a document from the list toward a card. Check the preview and relationship label, then release to save. Existing cards can also be dragged toward other cards, or connected from their attachment points.

Learning routes run horizontally. Alternatives touch the top of their main card; details are smaller cards below it with orthogonal connectors. Select a connection and press Delete, or **移除选中连线 (Remove selected connection)**, to remove the relationship while keeping the document. Double-click a card to explore from that document.

Changing the center preserves the positions, sizes and connections of the same tree. It updates the highlight and five-step visible scope; editing relationships recalculates the layout.

Use the wheel to zoom the tree and drag empty space to pan. Double-click empty space to return to the current group. Tree zoom is independent of text zoom and remembers its setting. Documents beyond the five-step scope remain in the left list.

## 5. Add images

Paste screenshots or copied image files, or drop image files into the editor. Relative storage is the default: images are copied into `md_photo`, mirroring the document's folder structure. The original file is kept. Images appear directly in the document.

**设置 → 图片保存方式 (Settings → Image storage)** opens a separate window with three options across the top. Select a mode and click Apply:

- **相对路径 (Relative path)**: set a directory relative to `md_data`, defaulting to `../md_photo`. The resolved location is shown. New images mirror the document folder structure.
- **绝对路径 (Absolute path)**: specify a directory to copy new images into and use absolute references. Leave empty to reference original local files and keep screenshots in the default image directory.
- **图床 (Image host)**: manage named image URLs, add/edit/delete records, choose a default, or insert a saved link. Removing a record leaves remote images and document references intact.

The upload interface accepts POST multipart/form-data. Configure the endpoint, file field, JSON URL path such as `data.url`, header/form/query authentication, credential prefix and extra form fields. Pasted and dropped images then upload and insert the returned URL on success. Without an endpoint, direct image links remain usable and local images use relative storage. This interface has not been validated against a real image hosting service.

Token/Key field credentials use Windows DPAPI local protection and are omitted from configuration exports. Re-enter them when moving computers or switching services. New directory settings apply to future images; existing image paths are preserved.

Rename documents through Tmd to maintain managed relative image paths. When moving files externally, check those paths yourself. F5 refreshes the document and images.

## 6. Import and export

**导入 (Import)** adds existing Markdown and copies readable local/embedded images. **另存为新文档 (Save as a new document)** creates a copy inside the library. **导出 Markdown (Export Markdown)** writes to an external location.

Select **携带图片 (Include images)** during export to place images beside the exported document and rewrite its image paths. The library's original content stays intact. An unreadable image causes export to report failure; correct its location or network access and retry.

A single-document export shares the document and images. To share a complete knowledge tree, also include its relationship metadata.

## 7. External changes and backups

Tmd watches external file changes. Unmodified documents refresh automatically; unsaved edits are preserved and a conflict is shown. Decide which content to keep before saving or reloading.

Back up `md_data` and `md_photo` together. Close Tmd before upgrading. Uninstalling the installed edition removes application files and registration while preserving **Documents/Tmd**.

## 8. Fonts and Markdown appearance

Open **设置 → MD加载样式 (Settings → Markdown loading styles)**. All five main sections start collapsed; click an arrow row to reveal its controls. Choose primary and fallback body fonts and a separate code font. Adjust all six heading sizes, spacing, list indentation, quote and code backgrounds, table colors and related options. Text zoom uses these font sizes as its 100% baseline.

Typing three backticks opens language suggestions in both Document and Source. Type to filter, press Tab or Enter to complete, Esc to dismiss, or Ctrl+Space to reopen. Language completion, syntax highlighting, code wrapping and automatic rendering while editing have separate switches.

**Apply** saves and refreshes the current document. The dialog shows the automatic `Tmd-settings.txt` location. Boolean options are bit-packed, numbers and colors use compact fixed fields, and font names use UTF-8. The payload is encoded as hexadecimal TXT, cached after parsing and saved atomically.

**导入配置 (Import configuration)** and **导出配置 (Export configuration)** appear at the bottom and transfer hexadecimal TXT. Exports include configured image locations and omit documents, image files and Token/Key credentials. Review imported values and click Apply to save. Version 1 files remain compatible, with defaults for new image parameters. Invalid files preserve settings. Cancel and unapplied defaults do not change saved values. **About Tmd** is also in the Settings menu.

## Shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+N | New document |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as |
| Ctrl+P | Quick search |
| Ctrl+Shift+D / Ctrl+Shift+M | Document / Source |
| Ctrl+Shift+O / Ctrl+Shift+K | Outline / Knowledge tree |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+mouse wheel / Ctrl+0 | Text zoom / Reset |
| F5 | Refresh |
