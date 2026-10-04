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

Use the wheel to zoom the tree and drag empty space to pan. Double-click empty space to return to the current group. Tree zoom is independent of text zoom and remembers its setting. Documents beyond the five-step scope remain in the left list.

## 5. Add images

Paste screenshots or copied image files, or drop image files into the editor. Relative storage is the default: images are copied into `md_photo`, mirroring the document's folder structure. The original file is kept. Images appear directly in the document.

**文件 → 图片存放方式 (File → Image storage)** offers:

- **相对路径 (Relative path)**: copy local images, or download remote image URLs, into the library.
- **绝对路径 (Absolute path)**: reference local files at their original locations.
- **图床 (Remote URL)**: keep HTTP(S) image URLs. Local images still use local storage. This release does not upload images to hosting services.

Rename documents through Tmd to maintain managed relative image paths. When moving files externally, check those paths yourself. F5 refreshes the document and images.

## 6. Import and export

**导入 (Import)** adds existing Markdown and copies readable local/embedded images. **另存为新文档 (Save as a new document)** creates a copy inside the library. **导出 Markdown (Export Markdown)** writes to an external location.

Select **携带图片 (Include images)** during export to place images beside the exported document and rewrite its image paths. The library's original content stays intact. An unreadable image causes export to report failure; correct its location or network access and retry.

A single-document export shares the document and images. To share a complete knowledge tree, also include its relationship metadata.

## 7. External changes and backups

Tmd watches external file changes. Unmodified documents refresh automatically; unsaved edits are preserved and a conflict is shown. Decide which content to keep before saving or reloading.

Back up `md_data` and `md_photo` together. Close Tmd before upgrading. Uninstalling the installed edition removes application files and registration while preserving **Documents/Tmd**.

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
