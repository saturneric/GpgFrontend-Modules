// SPDX-License-Identifier: GPL-3.0-or-later
//
// The editor page itself, in a DOM without layout: the "ui" message takes
// only colours and plain words, the placeholder is never part of the note,
// the table tools exist only inside a table, Find counts. Run: npm test

import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { JSDOM } from "jsdom";

const html = readFileSync(new URL("../src/index.html", import.meta.url), "utf8")
  .replace(/<script[^>]*><\/script>/, "");
const dom = new JSDOM(html, { pretendToBeVisual: true });
for (const k of ["window", "document", "navigator", "Node", "HTMLElement", "DOMParser",
                 "getComputedStyle", "MutationObserver", "requestAnimationFrame"]) {
  Object.defineProperty(globalThis, k, { value: dom.window[k], configurable: true, writable: true });
}
// jsdom has no layout; ProseMirror asks for rectangles when it scrolls.
dom.window.HTMLElement.prototype.scrollIntoView = () => {};
dom.window.document.createRange = (() => {
  const make = dom.window.document.createRange.bind(dom.window.document);
  return () => {
    const r = make();
    r.getClientRects = () => [];
    r.getBoundingClientRect = () => ({ left: 0, top: 0, right: 0, bottom: 0, width: 0, height: 0 });
    return r;
  };
})();

const sent = [];
let deliver = null;
window.gfBridge = { post: (m) => sent.push(m), onMessage: (f) => { deliver = f; } };
await import("../src/editor.js");
const $ = (id) => document.getElementById(id);
const last = (type) => sent.filter((m) => m.type === type).at(-1);

test("the page says it is ready", () => {
  assert.equal(sent[0].type, "ready");
  assert.ok(deliver);
});

test("ui: words are text, colours must be colours", () => {
  deliver({ type: "ui",
            theme: { base: "#101010", text: "#eeeeee", window: "#202020", muted: "#999999",
                     border: "#333333", accent: "#3366ff", accentText: "#ffffff",
                     warning: "#ffaa00", dark: true },
            strings: { bold: "<img src=x onerror=alert(1)>", findPlaceholder: "Suchen", nope: "x" } });
  const bold = $("toolbar").querySelector('[data-tool="bold"]');
  assert.equal(bold.title.startsWith("<img src=x onerror=alert(1)>"), true);
  assert.equal($("toolbar").querySelector("img"), null);
  assert.equal($("find-input").placeholder, "Suchen");
  assert.equal(document.documentElement.style.getPropertyValue("--bg"), "#101010");

  deliver({ type: "ui", theme: { base: "red; background: url(x)", text: "#000000" }, strings: {} });
  assert.equal(document.documentElement.style.getPropertyValue("--bg"), "#101010");
});

function load(markdown) {
  deliver({ type: "load", id: 1, epoch: 7, markdown });
}
function get() {
  deliver({ type: "get", id: 2, epoch: 7 });
  return last("markdown").markdown;
}

test("the placeholder shows on an empty note and is never saved", () => {
  load("");
  assert.ok($("editor").querySelector("p.is-empty[data-placeholder]"));
  assert.equal(get().trim(), "");
  load("text");
  assert.equal($("editor").querySelector(".is-empty"), null);
});

test("table tools appear only inside a table", () => {
  const group = () => $("toolbar").querySelector('[data-group="table"]');
  load("before\n\n| a | b |\n| --- | --- |\n| 1 | 2 |");
  assert.ok(group().classList.contains("absent"));
  const cell = $("editor").querySelector("td p");
  assert.ok(cell);
  // Put the caret in the first body cell through the editor itself.
  const ed = document.querySelector(".ProseMirror").editor;
  let pos = -1;
  ed.state.doc.descendants((n, at) => { if (pos < 0 && n.type.name === "tableCell") pos = at + 2; });
  ed.commands.setTextSelection(pos);
  assert.ok(!group().classList.contains("absent"));
  assert.equal(group().querySelector('[data-tool="rowDelete"]').disabled, false);
  // Inside a table only inline structure: a divider would split it.
  for (const id of ["rule", "table", "bulletList", "codeBlock"]) {
    assert.equal($("toolbar").querySelector(`[data-tool="${id}"]`).disabled, true, id);
  }
  ed.chain().addRowAfter().run();
  assert.match(get(), /\|\s*\|\s*\|/);
});

test("no table can be put in a list, which Markdown cannot keep", () => {
  load("1. item\n\n> quoted");
  const ed = document.querySelector(".ProseMirror").editor;
  const table = () => $("toolbar").querySelector('[data-tool="table"]');
  ed.commands.setTextSelection(4);
  assert.equal(table().disabled, true);
  ed.commands.setTextSelection(ed.state.doc.content.size - 3);
  assert.equal(table().disabled, false);
});

test("the text style menu shows the current block", () => {
  load("# Title\n\nbody");
  const ed = document.querySelector(".ProseMirror").editor;
  ed.commands.setTextSelection(2);
  assert.equal($("block-style").textContent, "Heading 1");
  ed.commands.setTextSelection(ed.state.doc.content.size - 1);
  assert.equal($("block-style").textContent, "Normal text");
});

test("find counts matches and says when there are none", () => {
  load("apple pie, apple tart");
  deliver({ type: "find.open" });
  assert.equal($("find").hidden, false);
  const input = $("find-input");
  input.value = "apple";
  input.dispatchEvent(new window.Event("input"));
  assert.equal($("find-count").textContent, "1 / 2");
  input.value = "pear";
  input.dispatchEvent(new window.Event("input"));
  assert.ok($("find").classList.contains("empty"));
});

test("wipe empties the page and disables the tools", () => {
  deliver({ type: "wipe" });
  assert.equal($("editor").textContent, "");
  assert.equal($("toolbar").querySelector('[data-tool="bold"]').disabled, true);
  assert.equal($("find").hidden, true);
});
