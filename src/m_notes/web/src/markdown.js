// SPDX-License-Identifier: GPL-3.0-or-later
//
// The m_notes Markdown subset on top of the pinned @tiptap/markdown 3.32.0.
//
// The stock extension is not safe for notes: it does not escape block
// syntax in text, splits code that contains a fence, interprets or drops raw
// HTML, keeps javascript: links, and turns remote images into image nodes. Each override below fixes
// one of those, and each has a regression fixture in test/.

import { Marked } from "marked";
import StarterKit from "@tiptap/starter-kit";
import { Markdown } from "@tiptap/markdown";
import { TableKit, TableCell, TableHeader } from "@tiptap/extension-table";
import { TaskList, TaskItem } from "@tiptap/extension-list";
import Image from "@tiptap/extension-image";
import Paragraph from "@tiptap/extension-paragraph";
import CodeBlock from "@tiptap/extension-code-block";

export const kLinkSchemes = ["http:", "https:", "mailto:"];
export const kAssetPrefix = "assets/";

// 1. Raw HTML is never interpreted: every html token becomes literal text
//    before Tiptap sees it, except <br>, the one way GFM writes a line break
//    in a table cell. MarkdownManager lexes with `new marked.Lexer()`, so the
//    rewrite lives in a Lexer subclass (walkTokens never runs there, and a
//    tokenizer returning false falls back to marked's own).
function htmlToText(tokens) {
  for (const tok of tokens ?? []) {
    if (tok.type === "html" && !tok.block && /^<br\s*\/?>$/i.test(tok.raw ?? "")) {
      Object.assign(tok, { type: "br" });
      continue;
    }
    if (tok.type === "html") {
      const raw = tok.raw ?? tok.text ?? "";
      if (tok.block) {
        const text = raw.replace(/\n+$/, "");
        Object.assign(tok, {
          type: "paragraph", raw, text,
          tokens: [{ type: "text", raw: text, text, escaped: true }],
        });
      } else {
        Object.assign(tok, { type: "text", raw, text: raw, escaped: true });
      }
      delete tok.block;
      delete tok.pre;
    }
    htmlToText(tok.tokens);
    for (const item of tok.items ?? []) htmlToText(item.tokens);
    for (const cell of tok.header ?? []) htmlToText(cell.tokens);
    for (const row of tok.rows ?? []) for (const cell of row) htmlToText(cell.tokens);
  }
  return tokens;
}

export function makeMarked() {
  const marked = new Marked({ gfm: true });
  const Base = marked.Lexer;
  marked.Lexer = class extends Base {
    lex(src) { return htmlToText(super.lex(src)); }
    blockTokens(src, tokens, clipped) { return htmlToText(super.blockTokens(src, tokens, clipped)); }
    inlineTokens(src, tokens) { return htmlToText(super.inlineTokens(src, tokens)); }
  };
  return marked;
}

// 2. Line starts that would be read back as block syntax are escaped; leading
//    spaces (an indented code block otherwise) become &#32;.
const kBlockStart =
  /^( {0,3})(#{1,6}(?=\s|$)|[-+*](?=\s|$)|\d{1,9}(?=[.)](\s|$))|>|=+\s*$|([-*_])(\s*\4){2,}\s*$)/;
export function escapeLineStarts(markdown) {
  return markdown
    .split("\n")
    .map((line) => {
      line = line.replace(/^( {4,})/, (s) => "&#32;".repeat(s.length));
      return line.replace(kBlockStart, (m, indent, rest) =>
        /^\d/.test(rest) ? `${indent}${rest}\\` : `${indent}\\${rest}`);
    })
    .join("\n");
}

const SafeParagraph = Paragraph.extend({
  renderMarkdown(node, helpers, ctx) {
    return escapeLineStarts(this.parent?.(node, helpers, ctx) ?? "");
  },
});

// 3. A fence longer than any backtick run in the code.
const SafeCodeBlock = CodeBlock.extend({
  renderMarkdown(node) {
    const text = (node.content ?? []).map((c) => c.text ?? "").join("");
    const longest = Math.max(2, ...[...text.matchAll(/`+/g)].map((m) => m[0].length));
    const fence = "`".repeat(longest + 1);
    return `${fence}${node.attrs?.language ?? ""}\n${text}\n${fence}`;
  },
});

// 4. Images: alt escaped; shown from the editor's own resource scheme. The
//    document keeps "assets/<name>", the page shows gfres://<session>/...
export const NoteImage = Image.extend({
  renderMarkdown(node) {
    const alt = (node.attrs?.alt ?? "").replace(/([\\[\]])/g, "\\$1");
    const src = node.attrs?.src ?? "";
    return `![${alt}](${/[\s()<>]/.test(src) ? `<${src}>` : src})`;
  },
  renderHTML({ HTMLAttributes }) {
    const src = HTMLAttributes.src ?? "";
    const host = typeof location !== "undefined" ? location.host : "";
    const shown = src.startsWith(kAssetPrefix) && host ? `gfres://${host}/${src}` : "";
    return ["img", { ...HTMLAttributes, src: shown, "data-src": src, draggable: "false" }];
  },
});

// 5. Table cells hold inline content only: nothing a GFM cell cannot write.
const InlineCell = TableCell.extend({ content: "paragraph" });
const InlineHeader = TableHeader.extend({ content: "paragraph" });

export function extensions() {
  return [
    StarterKit.configure({
      paragraph: false,
      codeBlock: false,
      link: { openOnClick: false, autolink: true, protocols: ["mailto"] },
    }),
    SafeParagraph,
    SafeCodeBlock,
    Markdown.configure({ marked: makeMarked() }),
    TableKit.configure({ tableCell: false, tableHeader: false, table: { resizable: false } }),
    InlineCell,
    InlineHeader,
    TaskList,
    TaskItem.configure({ nested: true }),
    NoteImage,
  ];
}

// 6. After every load: drop unsafe link marks (the text stays) and turn any
//    image that is not a packaged asset into visible text.
export function allowedHref(href) {
  try {
    return kLinkSchemes.includes(new URL(href).protocol);
  } catch {
    return false;
  }
}

export function isAssetPath(src) {
  return typeof src === "string" && src.startsWith(kAssetPrefix) &&
    !src.split("/").some((s) => s === "" || s === "." || s === "..") &&
    !/^[a-z][a-z0-9+.-]*:/i.test(src);
}

export function sanitize(node) {
  if (node.type === "image" && !isAssetPath(node.attrs?.src)) {
    return { type: "text", text: `[image: ${node.attrs?.alt || node.attrs?.src || ""}]` };
  }
  const out = { ...node };
  if (out.marks) {
    out.marks = out.marks.filter((m) => m.type !== "link" || allowedHref(m.attrs?.href ?? ""));
    if (out.marks.length === 0) delete out.marks;
  }
  if (out.content) {
    out.content = out.content.map(sanitize);
    if (out.type === "doc") {
      out.content = out.content.map((c) => (c.type === "text" ? { type: "paragraph", content: [c] } : c));
    }
  }
  return out;
}

// Front matter is not editable: split off before loading, put back verbatim.
const kFrontMatter = /^---\r?\n[\s\S]*?\r?\n(---|\.\.\.)\r?\n?/;
export function splitFrontMatter(markdown) {
  const m = markdown.match(kFrontMatter);
  return m ? { front: m[0], body: markdown.slice(m[0].length) } : { front: "", body: markdown };
}

// What the editor cannot keep, so the user is told before editing.
export function unsupportedConstructs(markdown) {
  const found = [];
  if (/^\[\^[^\]]+\]:/m.test(markdown) || /\[\^[^\]]+\]/.test(markdown)) found.push("footnotes");
  if (/<\/?[a-z][a-z0-9-]*(\s[^>]*)?>|<!--/i.test(markdown)) found.push("html");
  if (/(^|[^\\])\$\$[\s\S]+?\$\$/.test(markdown)) found.push("math");
  return found;
}

export function load(editor, markdown) {
  const json = editor.markdown.parse(markdown);
  editor.commands.setContent(sanitize(json), { emitUpdate: false });
}

export function save(editor) {
  return editor.getMarkdown();
}

// The asset paths the document refers to, in order, once each.
export function referencedAssets(json, out = new Set()) {
  if (json.type === "image" && isAssetPath(json.attrs?.src)) out.add(json.attrs.src);
  for (const c of json.content ?? []) referencedAssets(c, out);
  return [...out];
}
