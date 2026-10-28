// SPDX-License-Identifier: GPL-3.0-or-later
//
// The m_notes editor page. Runs under the Host's policy (script only from
// this bundle, no network, no eval) and talks to the module only through
// window.gfBridge, in JSON objects. It never persists anything: the document
// lives in this page's memory until the module asks for it.
//
// Protocol (module -> page):
//   {type:"ui", theme, strings}                     the application's colours and words
//   {type:"load", id, epoch, markdown}             -> {type:"loaded", re, epoch, gen,
//                                                       unsupported}
//   {type:"get", id, epoch}                         -> {type:"markdown", re, epoch, gen,
//                                                       markdown, assets, pending}
//   {type:"image.ready", token, path} | {type:"image.rejected", token, reason}
//   {type:"find.open"} | {type:"wipe"}
// Protocol (page -> module):
//   {type:"ready"} {type:"dirty", gen} {type:"image.add", token, mime, data}
//   {type:"link.open", url} {type:"notice", text}

import { Editor, Extension } from "@tiptap/core";
import { Plugin, PluginKey } from "@tiptap/pm/state";
import { Decoration, DecorationSet } from "@tiptap/pm/view";
import * as md from "./markdown.js";
import { icon } from "./icons.js";

// The module's own limits (NoteModel.h Budgets, NotePageView.cpp), checked
// here first so the user hears at once.
const kMaxImageBytes = 20 * 1024 * 1024;
const kMaxPendingImages = 4;
const kImageTypes = ["image/png", "image/jpeg", "image/gif", "image/webp"];

const bridge = window.gfBridge;
const post = (obj) => bridge && bridge.post(obj);
const $ = (id) => document.getElementById(id);

let editor = null;
let gen = 0;
let epoch = 0;
const pending = new Map(); // token -> {pos}

// --- words -------------------------------------------------------------------
//
// English here; the module sends the application's translation in a "ui"
// message. Only plain strings are taken, and only ever set as text.

const S = {
  formatting: "Formatting",
  paragraph: "Normal text",
  heading1: "Heading 1",
  heading2: "Heading 2",
  heading3: "Heading 3",
  textStyle: "Text style",
  bold: "Bold",
  italic: "Italic",
  strike: "Strikethrough",
  code: "Inline code",
  link: "Link",
  bulletList: "Bulleted list",
  orderedList: "Numbered list",
  taskList: "Checklist",
  quote: "Quote",
  codeBlock: "Code block",
  rule: "Divider",
  table: "Table",
  rowAdd: "Add row below",
  colAdd: "Add column to the right",
  rowDelete: "Delete row",
  colDelete: "Delete column",
  tableDelete: "Delete table",
  undo: "Undo",
  redo: "Redo",
  find: "Find",
  more: "More",
  findPlaceholder: "Find in note",
  noResults: "No results",
  previous: "Previous",
  next: "Next",
  close: "Close",
  linkPlaceholder: "Paste or type a web address",
  apply: "Apply",
  removeLink: "Remove link",
  placeholder: "Start writing...",
};

const kMac = /Mac|iPhone|iPad/.test(navigator.platform);
function keys(spec) {
  if (!spec) return "";
  return spec.split("+").map((k) => {
    if (k === "Mod") return kMac ? "⌘" : "Ctrl";
    if (k === "Alt") return kMac ? "⌥" : "Alt";
    if (k === "Shift") return kMac ? "⇧" : "Shift";
    return k;
  }).join(kMac ? "" : "+");
}

// --- theme -------------------------------------------------------------------

const kHex = /^#[0-9a-f]{6}$/i;

function rgb(hex) {
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
}

/** @p a moved @p t of the way towards @p b, both "#rrggbb". */
function mix(a, b, t) {
  const [x, y] = [rgb(a), rgb(b)];
  return `#${x.map((v, i) => Math.round(v + (y[i] - v) * t).toString(16).padStart(2, "0")).join("")}`;
}

function applyTheme(theme) {
  if (!theme || typeof theme !== "object") return;
  const c = {};
  for (const k of ["base", "text", "window", "muted", "border", "accent", "accentText", "warning"]) {
    if (!kHex.test(theme[k] ?? "")) return;
    c[k] = theme[k].toLowerCase();
  }
  const dark = !!theme.dark;
  const vars = {
    "--bg": c.base,
    "--fg": c.text,
    "--muted": c.muted,
    "--line": c.border,
    "--chrome": c.window,
    "--accent": c.accent,
    "--accent-fg": c.accentText,
    "--warning": c.warning,
    "--hover": mix(c.window, c.text, dark ? 0.12 : 0.07),
    "--pressed": mix(c.window, c.text, dark ? 0.18 : 0.12),
    "--active-bg": mix(c.window, c.accent, dark ? 0.28 : 0.14),
    "--surface": dark ? mix(c.base, c.text, 0.06) : c.base,
    "--code-bg": mix(c.base, c.text, dark ? 0.08 : 0.05),
    "--mark": mix(c.base, "#f5c400", dark ? 0.35 : 0.3),
    "--mark-current": mix(c.base, "#f59e00", dark ? 0.6 : 0.55),
    "--selection": mix(c.base, c.accent, dark ? 0.4 : 0.22),
    "--shadow": dark ? "rgba(0,0,0,.5)" : "rgba(15,23,42,.16)",
  };
  const root = document.documentElement;
  for (const [k, v] of Object.entries(vars)) root.style.setProperty(k, v);
  root.style.colorScheme = dark ? "dark" : "light";
}

function applyStrings(strings) {
  if (!strings || typeof strings !== "object") return;
  for (const k of Object.keys(S)) {
    const v = strings[k];
    if (typeof v === "string" && v.length > 0 && v.length <= 200) S[k] = v;
  }
}

// --- find --------------------------------------------------------------------

const findKey = new PluginKey("gf-find");
const Find = Extension.create({
  name: "gfFind",
  addProseMirrorPlugins() {
    return [new Plugin({
      key: findKey,
      state: {
        init: () => ({ query: "", matches: [], current: -1 }),
        apply(tr, value) {
          const meta = tr.getMeta(findKey);
          if (meta) return meta;
          if (!tr.docChanged || !value.query) return value;
          return search(tr.doc, value.query, value.current);
        },
      },
      props: {
        decorations(state) {
          const { matches, current } = findKey.getState(state);
          return DecorationSet.create(state.doc, matches.map((m, i) =>
            Decoration.inline(m.from, m.to, { class: i === current ? "find-current" : "find-match" })));
        },
      },
    })];
  },
});

// "Start writing..." in an empty note, as a decoration: nothing is added to
// the document, so nothing of it is ever saved.
const Placeholder = Extension.create({
  name: "gfPlaceholder",
  addProseMirrorPlugins() {
    return [new Plugin({
      props: {
        decorations(state) {
          const doc = state.doc;
          const first = doc.firstChild;
          if (doc.childCount !== 1 || !first || first.type.name !== "paragraph" || first.content.size) {
            return null;
          }
          return DecorationSet.create(doc, [Decoration.node(0, first.nodeSize,
            { class: "is-empty", "data-placeholder": S.placeholder })]);
        },
      },
    })];
  },
});

function search(doc, query, current = 0) {
  const matches = [];
  if (query) {
    const needle = query.toLocaleLowerCase();
    doc.descendants((node, pos) => {
      if (!node.isText) return;
      const text = node.text.toLocaleLowerCase();
      for (let i = text.indexOf(needle); i >= 0; i = text.indexOf(needle, i + needle.length)) {
        matches.push({ from: pos + i, to: pos + i + needle.length });
      }
    });
  }
  return { query, matches, current: matches.length ? Math.min(Math.max(current, 0), matches.length - 1) : -1 };
}

function setFind(query, current) {
  const value = search(editor.state.doc, query, current);
  editor.view.dispatch(editor.state.tr.setMeta(findKey, value));
  const count = $("find-count");
  count.textContent = value.matches.length
    ? `${value.current + 1} / ${value.matches.length}`
    : (query ? S.noResults : "");
  $("find").classList.toggle("empty", !!query && !value.matches.length);
  $("find-prev").disabled = value.matches.length < 2;
  $("find-next").disabled = value.matches.length < 2;
  const m = value.matches[value.current];
  if (m) {
    const dom = editor.view.domAtPos(m.from).node;
    (dom.nodeType === 3 ? dom.parentElement : dom)?.scrollIntoView({ block: "nearest" });
  }
}

function openFind() {
  closePopovers("find");
  $("find").hidden = false;
  setPressed("find", true);
  const input = $("find-input");
  input.focus();
  input.select();
  setFind(input.value, 0);
}

function closeFind(refocus = true) {
  if ($("find").hidden) return;
  $("find").hidden = true;
  setPressed("find", false);
  if (editor) setFind("", -1);
  if (refocus) editor?.commands.focus();
}

function stepFind(delta) {
  const { query, matches, current } = findKey.getState(editor.state);
  if (!matches.length) return;
  setFind(query, (current + delta + matches.length) % matches.length);
}

// --- images ------------------------------------------------------------------

function toBase64(bytes) {
  let s = "";
  for (let i = 0; i < bytes.length; i += 0x8000) {
    s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  }
  return btoa(s);
}

function newToken() {
  const b = new Uint8Array(12);
  crypto.getRandomValues(b);
  return Array.from(b, (x) => x.toString(16).padStart(2, "0")).join("");
}

async function addImage(file, pos) {
  if (!kImageTypes.includes(file.type)) {
    post({ type: "notice", text: "unsupported-image" });
    return;
  }
  if (file.size > kMaxImageBytes) {
    post({ type: "notice", text: "image-too-large" });
    return;
  }
  if (pending.size >= kMaxPendingImages) {
    post({ type: "notice", text: "images-busy" });
    return;
  }
  const token = newToken();
  pending.set(token, { pos });
  // Read once, into memory, and handed over; the Blob is not kept.
  const bytes = new Uint8Array(await file.arrayBuffer());
  post({ type: "image.add", token, mime: file.type, data: toBase64(bytes) });
}

function imageFiles(list) {
  return Array.from(list ?? []).filter((f) => f.type.startsWith("image/"));
}

// Remote or file: images in pasted HTML never load: they become their text.
function stripRemoteImages(html) {
  const doc = new DOMParser().parseFromString(html, "text/html");
  for (const img of doc.querySelectorAll("img")) {
    const label = img.getAttribute("alt") || img.getAttribute("src") || "";
    img.replaceWith(doc.createTextNode(`[image: ${label}]`));
  }
  for (const el of doc.querySelectorAll("script,style,iframe,object,embed,link,meta")) el.remove();
  return doc.body.innerHTML;
}

// --- toolbar -----------------------------------------------------------------
//
// Groups, left to right. When the tab is too narrow, whole groups move into
// the "More" menu, lowest priority first; the text style, the history and
// Find always stay. The table group only appears inside a table.

const inHeaderRow = (e) => e.isActive("tableHeader");

const kBlocks = [
  { id: "paragraph", keys: "Mod+Alt+0", run: (c) => c.setParagraph(),
    active: (e) => e.isActive("paragraph") },
  { id: "heading1", keys: "Mod+Alt+1", run: (c) => c.toggleHeading({ level: 1 }),
    active: (e) => e.isActive("heading", { level: 1 }) },
  { id: "heading2", keys: "Mod+Alt+2", run: (c) => c.toggleHeading({ level: 2 }),
    active: (e) => e.isActive("heading", { level: 2 }) },
  { id: "heading3", keys: "Mod+Alt+3", run: (c) => c.toggleHeading({ level: 3 }),
    active: (e) => e.isActive("heading", { level: 3 }) },
];

const kGroups = [
  { id: "marks", priority: 4, tools: [
    { id: "bold", keys: "Mod+B", run: (c) => c.toggleBold(), active: (e) => e.isActive("bold") },
    { id: "italic", keys: "Mod+I", run: (c) => c.toggleItalic(), active: (e) => e.isActive("italic") },
    { id: "strike", keys: "Mod+Shift+S", run: (c) => c.toggleStrike(), active: (e) => e.isActive("strike") },
    { id: "code", keys: "Mod+E", run: (c) => c.toggleCode(), active: (e) => e.isActive("code") },
    { id: "link", keys: "Mod+K", action: () => openLink(), active: (e) => e.isActive("link"),
      can: (e) => !e.state.selection.empty || e.isActive("link") },
  ] },
  { id: "lists", priority: 3, tools: [
    { id: "bulletList", keys: "Mod+Shift+8", run: (c) => c.toggleBulletList(), active: (e) => e.isActive("bulletList") },
    { id: "orderedList", keys: "Mod+Shift+7", run: (c) => c.toggleOrderedList(), active: (e) => e.isActive("orderedList") },
    { id: "taskList", keys: "Mod+Shift+9", run: (c) => c.toggleTaskList(), active: (e) => e.isActive("taskList") },
  ] },
  { id: "insert", priority: 1, tools: [
    { id: "quote", keys: "Mod+Shift+B", run: (c) => c.toggleBlockquote(), active: (e) => e.isActive("blockquote") },
    { id: "codeBlock", keys: "Mod+Alt+C", run: (c) => c.toggleCodeBlock(), active: (e) => e.isActive("codeBlock") },
    // In a cell it would split the table in two and drop the cell's text.
    { id: "rule", run: (c) => c.setHorizontalRule(), can: (e) => !e.isActive("table") },
    // Not in a list, which Markdown cannot carry a table in, nor in a table,
    // which it would split.
    { id: "table", run: (c) => c.insertTable({ rows: 3, cols: 3, withHeaderRow: true }),
      active: (e) => e.isActive("table"),
      can: (e) => !e.isActive("listItem") && !e.isActive("taskItem") && !e.isActive("table") },
  ] },
  { id: "table", priority: 5, contextual: (e) => e.isActive("table"), tools: [
    { id: "rowAdd", run: (c) => c.addRowAfter() },
    { id: "colAdd", run: (c) => c.addColumnAfter() },
    { id: "rowDelete", run: (c) => c.deleteRow(), can: (e) => !inHeaderRow(e) },
    { id: "colDelete", run: (c) => c.deleteColumn() },
    { id: "tableDelete", run: (c) => c.deleteTable(), danger: true },
  ] },
];

const kHistory = [
  { id: "undo", keys: "Mod+Z", run: (c) => c.undo() },
  { id: "redo", keys: kMac ? "Mod+Shift+Z" : "Mod+Y", run: (c) => c.redo() },
];

const allTools = () => [...kGroups.flatMap((g) => g.tools), ...kHistory];
const toolById = (id) => allTools().find((t) => t.id === id);

function tip(tool) {
  const k = keys(tool.keys);
  return k ? `${S[tool.id]} (${k})` : S[tool.id];
}

function enabled(tool) {
  if (!editor || !editor.isEditable) return false;
  if (tool.can && !tool.can(editor)) return false;
  if (tool.run) {
    try {
      return tool.run(editor.can().chain()).run();
    } catch {
      return false;
    }
  }
  return true;
}

function runTool(tool) {
  if (!enabled(tool)) return;
  if (tool.action) return tool.action();
  tool.run(editor.chain().focus()).run();
}

function button(id, iconName, title, onClick) {
  const b = document.createElement("button");
  b.type = "button";
  b.className = "tool";
  b.dataset.tool = id;
  b.title = title;
  b.setAttribute("aria-label", title);
  b.append(icon(iconName));
  // The editor keeps the selection: a toolbar click never takes focus.
  b.addEventListener("mousedown", (ev) => ev.preventDefault());
  b.addEventListener("click", onClick);
  return b;
}

function separator() {
  const s = document.createElement("span");
  s.className = "sep";
  s.setAttribute("role", "separator");
  return s;
}

function buildToolbar() {
  const bar = $("toolbar");
  bar.textContent = "";
  bar.setAttribute("aria-label", S.formatting);

  const style = document.createElement("button");
  style.type = "button";
  style.id = "block-style";
  style.className = "tool select";
  style.title = S.textStyle;
  style.setAttribute("aria-haspopup", "menu");
  const label = document.createElement("span");
  label.className = "label";
  style.append(label, icon("chevronDown", 14));
  style.addEventListener("mousedown", (ev) => ev.preventDefault());
  style.addEventListener("click", () => toggleMenu("block-menu", style, blockMenuItems()));
  bar.append(style);

  for (const g of kGroups) {
    const group = document.createElement("div");
    group.className = "group";
    group.dataset.group = g.id;
    group.append(separator());
    for (const t of g.tools) {
      const b = button(t.id, t.id, tip(t), () => runTool(t));
      if (t.danger) b.classList.add("danger");
      group.append(b);
    }
    bar.append(group);
  }

  const more = button("more", "more", S.more, () =>
    toggleMenu("more-menu", $("toolbar").querySelector('[data-tool="more"]'), moreMenuItems()));
  more.setAttribute("aria-haspopup", "menu");
  more.hidden = true;
  bar.append(more);

  const spacer = document.createElement("span");
  spacer.className = "spacer";
  bar.append(spacer);

  const history = document.createElement("div");
  history.className = "group fixed";
  for (const t of kHistory) history.append(button(t.id, t.id, tip(t), () => runTool(t)));
  history.append(separator());
  history.append(button("find", "find", tip({ id: "find", keys: "Mod+F" }), () => {
    if ($("find").hidden) openFind(); else closeFind();
  }));
  bar.append(history);

  layoutToolbar();
}

function setPressed(id, on) {
  const b = $("toolbar").querySelector(`[data-tool="${id}"]`);
  if (!b) return;
  b.classList.toggle("active", on);
  b.setAttribute("aria-pressed", on ? "true" : "false");
}

function refreshToolbar() {
  const bar = $("toolbar");
  for (const t of allTools()) {
    const b = bar.querySelector(`[data-tool="${t.id}"]`);
    if (!b) continue;
    const on = !!editor && !!t.active && t.active(editor);
    b.classList.toggle("active", on);
    if (t.active) b.setAttribute("aria-pressed", on ? "true" : "false");
    b.disabled = !enabled(t);
  }
  let relayout = false;
  for (const g of kGroups.filter((x) => x.contextual)) {
    const el = bar.querySelector(`[data-group="${g.id}"]`);
    const show = !!editor && editor.isEditable && g.contextual(editor);
    if (el.classList.contains("absent") === show) {
      el.classList.toggle("absent", !show);
      relayout = true;
    }
  }
  const style = $("block-style");
  const current = editor && kBlocks.find((b) => b.active(editor));
  const inCode = !current && !!editor && editor.isActive("codeBlock");
  style.querySelector(".label").textContent = current ? S[current.id] : (inCode ? S.codeBlock : S.paragraph);
  style.disabled = !editor || !editor.isEditable;
  bar.querySelector('[data-tool="find"]').disabled = !editor;
  if (!$("find").hidden) setPressed("find", true);
  if (!$("link").hidden) setPressed("link", true);
  if (relayout) layoutToolbar();
}

// Fold groups into "More" until the bar fits on one line.
function layoutToolbar() {
  const bar = $("toolbar");
  const more = bar.querySelector('[data-tool="more"]');
  const groups = kGroups
    .map((g) => ({ g, el: bar.querySelector(`[data-group="${g.id}"]`) }))
    .sort((a, b) => a.g.priority - b.g.priority);
  for (const { el } of groups) el.classList.remove("folded");
  more.hidden = true;
  for (const { el } of groups) {
    if (bar.scrollWidth <= bar.clientWidth) break;
    if (el.classList.contains("absent")) continue;
    el.classList.add("folded");
    more.hidden = false;
  }
  closeMenu("more-menu");
}

// --- menus -------------------------------------------------------------------

function blockMenuItems() {
  return kBlocks.map((b) => ({
    id: b.id, label: S[b.id], keys: keys(b.keys), className: `preview-${b.id}`,
    checked: !!editor && b.active(editor),
    // The current style reads as chosen, not as unavailable.
    enabled: !!editor && editor.isEditable && (b.active(editor) || enabled(b)),
    run: () => b.run(editor.chain().focus()).run(),
  }));
}

function moreMenuItems() {
  const bar = $("toolbar");
  const items = [];
  for (const g of kGroups) {
    const el = bar.querySelector(`[data-group="${g.id}"]`);
    if (!el.classList.contains("folded") || el.classList.contains("absent")) continue;
    if (items.length) items.push(null);
    for (const t of g.tools) {
      items.push({ id: t.id, icon: t.id, label: S[t.id], keys: keys(t.keys),
                   checked: !!editor && !!t.active && t.active(editor),
                   enabled: enabled(t), danger: t.danger, run: () => runTool(t) });
    }
  }
  return items;
}

function closeMenu(id) {
  const menu = $(id);
  if (!menu || menu.hidden) return;
  menu.hidden = true;
  menu.anchor?.classList.remove("open");
  menu.anchor?.setAttribute("aria-expanded", "false");
}

function placeBelow(el, anchor, alignRight = false) {
  const r = anchor.getBoundingClientRect();
  const width = el.offsetWidth;
  let left = alignRight ? r.right - width : r.left;
  left = Math.max(8, Math.min(left, window.innerWidth - width - 8));
  el.style.left = `${left}px`;
  el.style.top = `${r.bottom + 6}px`;
}

function toggleMenu(id, anchor, items) {
  const menu = $(id);
  if (!menu.hidden) return closeMenu(id);
  closePopovers(id);
  menu.textContent = "";
  for (const item of items) {
    if (item === null) {
      const s = document.createElement("div");
      s.className = "menu-sep";
      menu.append(s);
      continue;
    }
    const b = document.createElement("button");
    b.type = "button";
    b.setAttribute("role", item.checked !== undefined ? "menuitemradio" : "menuitem");
    if (item.checked !== undefined) b.setAttribute("aria-checked", item.checked ? "true" : "false");
    b.className = `menu-item ${item.className ?? ""}`;
    if (item.danger) b.classList.add("danger");
    b.disabled = !item.enabled;
    const mark = document.createElement("span");
    mark.className = "mark";
    if (item.icon) mark.append(icon(item.icon));
    else if (item.checked) mark.append(icon("check", 16));
    const text = document.createElement("span");
    text.className = "text";
    text.textContent = item.label;
    const k = document.createElement("span");
    k.className = "keys";
    k.textContent = item.keys ?? "";
    b.append(mark, text, k);
    if (item.icon && item.checked) b.classList.add("checked");
    b.addEventListener("mousedown", (ev) => ev.preventDefault());
    b.addEventListener("click", () => {
      closeMenu(id);
      item.run();
    });
    menu.append(b);
  }
  menu.hidden = false;
  menu.anchor = anchor;
  anchor.classList.add("open");
  anchor.setAttribute("aria-expanded", "true");
  placeBelow(menu, anchor, id === "more-menu");
  menu.querySelector(".menu-item:not(:disabled)")?.focus({ preventScroll: true });
}

function menuKeys(ev) {
  const menu = ev.currentTarget;
  const items = Array.from(menu.querySelectorAll(".menu-item:not(:disabled)"));
  const at = items.indexOf(document.activeElement);
  if (ev.key === "ArrowDown" || ev.key === "ArrowUp") {
    ev.preventDefault();
    const next = (at + (ev.key === "ArrowDown" ? 1 : -1) + items.length) % items.length;
    items[next]?.focus();
  } else if (ev.key === "Escape") {
    ev.preventDefault();
    closeMenu(menu.id);
    editor?.commands.focus();
  }
}

// Find stays open while the user works in the note; menus and the link
// editor close on any click elsewhere.
function closePopovers(except) {
  for (const id of ["block-menu", "more-menu"]) if (id !== except) closeMenu(id);
  if (except !== "link") closeLink(false);
}

// --- links -------------------------------------------------------------------

function normaliseUrl(text) {
  const trimmed = text.trim();
  if (!trimmed) return "";
  const withScheme = /^[a-z][a-z0-9+.-]*:/i.test(trimmed) ? trimmed : `https://${trimmed}`;
  // Spaces would end a Markdown link destination early.
  return withScheme.replace(/ /g, "%20");
}

function openLink() {
  if (!editor || !editor.isEditable) return;
  if (!enabled(toolById("link"))) return;
  closePopovers("link");
  const pop = $("link");
  const input = $("link-input");
  const existing = editor.getAttributes("link").href ?? "";
  input.value = existing;
  $("link-remove").hidden = !existing;
  pop.hidden = false;
  setPressed("link", true);
  // Under the Link button, or under "More" when the button is folded away.
  const own = $("toolbar").querySelector('[data-tool="link"]');
  const anchor = own && own.offsetParent !== null
    ? own : $("toolbar").querySelector('[data-tool="more"]');
  placeBelow(pop, anchor && anchor.offsetParent !== null ? anchor : $("block-style"));
  input.focus();
  input.select();
}

function closeLink(refocus = true) {
  if ($("link").hidden) return;
  $("link").hidden = true;
  refreshToolbar();
  if (refocus) editor?.commands.focus();
}

function applyLink() {
  const url = normaliseUrl($("link-input").value);
  if (url && !md.allowedHref(url)) {
    post({ type: "notice", text: "link-scheme" });
    $("link-input").focus();
    return;
  }
  closeLink(false);
  const chain = editor.chain().focus().extendMarkRange("link");
  if (!url) return chain.unsetLink().run();
  chain.setLink({ href: url }).run();
}

function removeLink() {
  closeLink(false);
  editor.chain().focus().extendMarkRange("link").unsetLink().run();
}

// --- the editor --------------------------------------------------------------

function create(markdown) {
  editor?.destroy();
  pending.clear();
  gen = 0;
  editor = new Editor({
    element: $("editor"),
    extensions: [...md.extensions(), Find, Placeholder],
    editorProps: {
      attributes: { spellcheck: "false", "aria-label": "Note" },
      transformPastedHTML: stripRemoteImages,
      handlePaste(view, event) {
        const files = imageFiles(event.clipboardData?.files);
        if (!files.length) return false;
        for (const f of files) addImage(f, view.state.selection.from);
        return true;
      },
      handleDrop(view, event) {
        const files = imageFiles(event.dataTransfer?.files);
        if (!files.length) return false;
        const at = view.posAtCoords({ left: event.clientX, top: event.clientY });
        for (const f of files) addImage(f, at?.pos ?? view.state.selection.from);
        return true;
      },
      handleClickOn(view, pos, node, nodePos, event) {
        // Ctrl/Cmd+click opens a link -- through the module, which asks first.
        if (!(event.ctrlKey || event.metaKey)) return false;
        const link = view.state.doc.resolve(pos).marks().find((m) => m.type.name === "link");
        if (link && md.allowedHref(link.attrs.href)) post({ type: "link.open", url: link.attrs.href });
        return !!link;
      },
    },
    onTransaction({ transaction }) {
      for (const p of pending.values()) p.pos = transaction.mapping.map(p.pos);
      refreshToolbar();
    },
    onUpdate() {
      gen += 1;
      post({ type: "dirty", gen });
    },
  });
  md.load(editor, markdown);
  // A fresh document has no history to undo into.
  editor.view.updateState(editor.view.state.reconfigure({ plugins: editor.view.state.plugins }));
  refreshToolbar();
}

function wipe() {
  closePopovers();
  closeFind(false);
  editor?.destroy();
  editor = null;
  pending.clear();
  gen = 0;
  $("editor").textContent = "";
  $("find-input").value = "";
  $("link-input").value = "";
  refreshToolbar();
}

function relabel() {
  $("find-input").placeholder = S.findPlaceholder;
  $("find-input").setAttribute("aria-label", S.findPlaceholder);
  $("find-prev").title = S.previous;
  $("find-next").title = S.next;
  $("find-close").title = S.close;
  $("link-input").placeholder = S.linkPlaceholder;
  $("link-input").setAttribute("aria-label", S.link);
  $("link-apply").textContent = S.apply;
  $("link-remove").title = S.removeLink;
  $("link-remove").setAttribute("aria-label", S.removeLink);
  buildToolbar();
  refreshToolbar();
  // The placeholder is a decoration: redraw it in the new words.
  if (editor) editor.view.dispatch(editor.state.tr);
}

function handle(msg) {
  switch (msg.type) {
    case "ui":
      applyTheme(msg.theme);
      applyStrings(msg.strings);
      relabel();
      break;
    case "load":
      epoch = msg.epoch;
      create(String(msg.markdown ?? ""));
      post({ type: "loaded", re: msg.id, epoch, gen,
             unsupported: md.unsupportedConstructs(String(msg.markdown ?? "")) });
      editor.commands.focus("start");
      break;
    case "get":
      if (!editor || msg.epoch !== epoch) return;
      post({
        type: "markdown", re: msg.id, epoch, gen,
        markdown: md.save(editor),
        assets: md.referencedAssets(editor.getJSON()),
        pending: pending.size,
      });
      break;
    case "image.ready": {
      const p = pending.get(msg.token);
      pending.delete(msg.token);
      if (!p || !editor || !md.isAssetPath(msg.path)) return;
      const at = Math.min(p.pos, editor.state.doc.content.size);
      editor.chain().insertContentAt(at, { type: "image", attrs: { src: msg.path, alt: "" } }).run();
      break;
    }
    case "image.rejected":
      pending.delete(msg.token);
      break;
    case "find.open":
      if (editor) openFind();
      break;
    case "wipe":
      wipe();
      break;
    default:
      break;
  }
}

function wire() {
  for (const [id, name] of [["find-prev", "chevronUp"], ["find-next", "chevronDown"],
                            ["find-close", "close"], ["link-remove", "unlink"]]) {
    $(id).append(icon(name, 16));
  }
  $("find-icon").append(icon("find", 16));
  $("link-icon").append(icon("link", 16));
  relabel();

  const input = $("find-input");
  input.addEventListener("input", () => setFind(input.value, 0));
  input.addEventListener("keydown", (ev) => {
    if (ev.key === "Enter") { ev.preventDefault(); stepFind(ev.shiftKey ? -1 : 1); }
    if (ev.key === "Escape") { ev.preventDefault(); closeFind(); }
  });
  $("find-prev").addEventListener("click", () => stepFind(-1));
  $("find-next").addEventListener("click", () => stepFind(1));
  $("find-close").addEventListener("click", () => closeFind());

  const link = $("link-input");
  link.addEventListener("keydown", (ev) => {
    if (ev.key === "Enter") { ev.preventDefault(); applyLink(); }
    if (ev.key === "Escape") { ev.preventDefault(); closeLink(); }
  });
  $("link-apply").addEventListener("click", applyLink);
  $("link-remove").addEventListener("click", removeLink);

  for (const id of ["block-menu", "more-menu"]) $(id).addEventListener("keydown", menuKeys);
  document.addEventListener("mousedown", (ev) => {
    if (!ev.target.closest(".menu, .popover, #block-style, [data-tool='more']")) closePopovers();
  });
  document.addEventListener("keydown", (ev) => {
    const mod = ev.ctrlKey || ev.metaKey;
    const key = ev.key.toLowerCase();
    if (mod && !ev.altKey && key === "f" && editor) {
      ev.preventDefault();
      openFind();
    } else if (mod && !ev.altKey && key === "k" && editor) {
      ev.preventDefault();
      openLink();
    } else if (!kMac && mod && !ev.shiftKey && key === "y" && editor) {
      ev.preventDefault();
      editor.commands.redo();
    }
  });
  window.addEventListener("resize", () => {
    layoutToolbar();
    closePopovers();
  });
  bridge?.onMessage(handle);
  post({ type: "ready" });
}

wire();
