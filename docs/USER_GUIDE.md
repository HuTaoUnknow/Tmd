# Tmd user guide

[中文](使用教程.md) · [Installation and backup](INSTALLATION.md) · [Home](../README.md)

## 1. Your library

The installed edition keeps your library under **Documents/Tmd**:

```text
Tmd/
├─ md_data/                   Markdown documents and relationships
│  ├─ markdown学习/           Four starter documents
│  ├─ .tree-md-files.json      Local file inventory and move history
│  └─ .tree-md-relations.json  Relationship metadata
└─ md_photo/                  Local document images
```

The portable edition uses the same folders beside `tree_md.exe`. Image paths are relative to each Markdown document. Back up both folders, including the hidden relationship file.

Markdown filenames must be unique throughout `md_data`, including different subfolders, with case-insensitive comparison. Create, import, Save As and rename reject duplicates. Conflicts introduced outside Tmd appear in a rename window; existing files are preserved.

Tmd records the document and image folder structures and automatically updates local references after identifiable moves or renames. It updates links in both moved and stationary documents, without confirmation. A missing local link or image target replaces the unsaved white dot with a red dot, even for saved documents. Hover for details. Restoring the target or fixing the reference clears red; pending text still shows white. Web links and network images are not marked missing based on temporary network availability. Code examples are unchanged, and automatic URL repairs do not save pending text. Ambiguous destinations remain unresolved rather than being guessed.

Deliberate deletion leaves references missing. A newly created file with the same contents does not impersonate the deleted target; restoring the original file triggers another check.

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

The rendered view is editable. Use Source for precise changes to table syntax, image paths or code fences. Both views share undo history. Switching documents keeps unsaved edits in memory; a gray-white dot at the right of each file row marks pending changes. `Ctrl+S` saves only the current document. Closing Tmd prompts once to save all pending documents, exit without saving, or cancel; a failed save keeps the window open and preserves the affected edits.

Press `Ctrl+F` to open a small, nonmodal Find window for the current document. Matches have rounded highlights, with a stronger color for the current result and a position/total counter. While an input has focus, use Up / Down or the arrow buttons to move between results, wrapping at either end. Find mode also supports Enter / Shift+Enter. Esc or the rounded close button clears highlights. Search is literal and case insensitive, including ordinary spaces. Document view searches displayed text; Source view also searches Markdown syntax. Queries remain when switching views or documents, and results update after edits. Highlights do not change Markdown or add undo entries.

The Find / Replace buttons use the same segmented style as Document / Source. Tab switches between them. In Replace mode, Enter replaces only the current match and keeps the replacement selected. Repeated Enter stays on that same range. Up / Down, the arrow buttons, or changing the search query starts another search and releases the selection; closing also releases it. An empty replacement deletes the match. Each replacement can be undone in one step. Document view retains the surrounding Markdown structure, including headings, bold text, and code fences.

Ctrl+click a Markdown link in either view to follow it. MD files within the current `md_data` library open in Tmd. Other files, including MD files outside the library, use the system default application; web links use the default browser. Relative paths resolve from the current document, with support for library-root paths and a `md_data/` prefix. A regular click edits the link.

After selecting body text, type symmetric Markdown delimiters such as asterisks, underscores, tildes or backticks. The source and selection remain intact while a small preview shows the pending wrapper. Enter confirms, for example `**body**`, without adding a newline. Backspace corrects pending delimiters; Esc cancels. Combined delimiters close in reverse nesting order. Confirmation is a single undoable edit.

Within fenced code, successive Ctrl+A presses select the current logical line, the code body, then the entire document. The first two stages exclude the fences, including for single-line and empty blocks. Moving the caret or another operation restarts the sequence. Outside code, Ctrl+A selects the whole document normally.

In Document view, a space after a valid heading, list, numbered-list or quote prefix immediately activates its preview. Additional spaces and text belong to the editable body, including at the end of a line. Backspace at the start of an indented list item first removes one indentation level, even if the item is empty. For an unindented empty item, Backspace removes the activation space and reveals the original prefix, including the complete `- [ ]` or `- [x]` task marker; another space activates the preview again. Arrow keys can still enter a hidden prefix to edit it character by character.

Enter continues a quote in a separate paragraph; Enter on an empty nested quote removes one quote layer. Down moves through code lines and leaves the panel only from its last line. Deleting code text preserves the fences; Backspace at the start of an otherwise empty code panel removes the whole panel.

The document editor preserves ordinary spaces and physical newlines. Enter starts the next visible line; Shift+Enter behaves the same outside tables. Headings end in plain body text. Empty lists continue; Backspace outdents first, then restores or removes the prefix. Moving a parent item moves its descendants too.

In tables, Tab moves to the next cell and leaves the table at the end. Shift+Tab inserts a whole column to the right; Shift+Enter inserts a whole row below; Enter breaks the current cell. Ragged tables are completed in both source and preview and remain unsaved until you save. Links use Ctrl+click to open; clicking edits. Double-click images to edit their Markdown; Backspace first selects the image, then removes its reference. HTML stays literal. See the [complete editing policy](syntax-review/RULES.md) for detailed examples and boundary behavior.

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

Select a card and click **删除选中节点 (Delete selected node)** or press Delete to remove all its incoming and outgoing connections in one operation. Its Markdown file and unsaved edits are preserved. Documents outside the remaining tree return to the left list. Removing the center switches to a remaining node; after clearing the tree, double-click a document in the left list to establish a new center.

Changing the center preserves the positions, sizes and connections of the same tree. It updates the highlight and five-step visible scope; editing relationships recalculates the layout.

Use the wheel to zoom the tree and drag empty space to pan. Double-click empty space to return to the current group. Tree zoom is independent of text zoom and remembers its setting. Documents beyond the five-step scope remain in the left list.

## 5. Add images

Paste screenshots or copied image files, or drop image files into the editor. Images are copied into one flat `md_photo` directory beside the library and referenced by absolute paths. Identical content shares one file; originals are kept. Export with images converts references to relative paths.

**设置 → 图片保存方式 (Settings → Image storage)** opens a separate window with three options across the top. Select a mode and click Apply:

- **相对目录 (Relative directory)**: set a storage directory relative to `md_data`, defaulting to `../md_photo`. Images are stored flat with absolute references.
- **绝对目录 (Absolute directory)**: the default mode. Specify a complete storage directory, or leave empty to use `md_photo` beside the library. Local files and screenshots are copied into this directory with absolute references.
- **图床 (Image host)**: manage named image URLs, add/edit/delete records, choose a default, or insert a saved link. Removing a record leaves remote images and document references intact.

The upload interface accepts POST multipart/form-data. Configure the endpoint, file field, JSON URL path such as `data.url`, header/form/query authentication, credential prefix and extra form fields. Pasted and dropped images then upload and insert the returned URL on success. Without an endpoint, direct image links remain usable and local images use the flat directory with absolute references. This interface has not been validated against a real image hosting service.

Token/Key field credentials use Windows DPAPI local protection and are omitted from configuration exports. Re-enter them when moving computers or switching services. New directory settings apply to future images; existing image paths are preserved.

Renaming, moving or copying documents through Tmd maintains image references and copies referenced local images from other directories into the shared directory. When moving files externally, check those paths yourself. F5 refreshes the document and images.

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

In Document mode, paired code fences have a language label at the top left and a copy icon at the top right. Copy keeps the code's blank lines, tabs and line breaks, without fences or following prose, and leaves the selection and caret unchanged. Empty language labels show plain text; unmatched fences remain literal.

Both views use language-specific syntax colors, including HTML/XML tags and attributes, C/C++ types and preprocessor directives, multiline strings and comments, and JavaScript/CSS inside HTML or Vue. Adjacent code blocks keep separate lexical states. Plain text and unknown languages stay uncolored; turning off syntax colors keeps the labels and copy buttons available.

**Apply** saves and refreshes the current document. The dialog shows the automatic `Tmd-settings.txt` location. Boolean options are bit-packed, numbers and colors use compact fixed fields, and font names use UTF-8. The payload is encoded as hexadecimal TXT, cached after parsing and saved atomically.

**导入配置 (Import configuration)** and **导出配置 (Export configuration)** appear at the bottom and transfer hexadecimal TXT. Exports include configured image locations and omit documents, image files and Token/Key credentials. Review imported values and click Apply to save. Version 1 files remain compatible, with defaults for new image parameters. Invalid files preserve settings. Cancel and unapplied defaults do not change saved values. **About Tmd** is also in the Settings menu.

## Shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+N | New document |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as |
| Ctrl+P | Quick search |
| Ctrl+F | Find; Up / Down changes result, Tab switches Find/Replace, Replace-mode Enter replaces the current match, Esc closes |
| Ctrl+Shift+D / Ctrl+Shift+M | Document / Source |
| Ctrl+Shift+O / Ctrl+Shift+K | Outline / Knowledge tree |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+mouse wheel / Ctrl+0 | Text zoom / Reset |
| F5 | Refresh |
