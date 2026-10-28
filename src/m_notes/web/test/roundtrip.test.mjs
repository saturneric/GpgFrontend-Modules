// SPDX-License-Identifier: GPL-3.0-or-later
//
// The m_notes Markdown subset, against the pinned bundle dependencies: every
// subset construct round-trips and is stable, typed text never turns into
// structure, unsafe input is neutralised. One fixture per override in
// src/markdown.js. Run: npm test

import { test } from "node:test";
import assert from "node:assert/strict";
import { JSDOM } from "jsdom";

const dom = new JSDOM("<!doctype html><html><body></body></html>");
for (const k of ["window", "document", "navigator", "Node", "HTMLElement",
                 "DOMParser", "getComputedStyle", "MutationObserver"]) {
  Object.defineProperty(globalThis, k, { value: dom.window[k], configurable: true, writable: true });
}

const { Editor } = await import("@tiptap/core");
const md = await import("../src/markdown.js");

const editor = new Editor({ element: document.createElement("div"), extensions: md.extensions() });
const json = () => JSON.parse(JSON.stringify(editor.getJSON()));
const roundtrip = (text) => {
  md.load(editor, text);
  const j1 = json();
  const s1 = md.save(editor);
  md.load(editor, s1);
  return { j1, s1, j2: json(), s2: md.save(editor) };
};
const typed = (content) => {
  editor.commands.setContent({ type: "doc", content });
  const before = json();
  md.load(editor, md.save(editor));
  return { before, after: json() };
};
const p = (...content) => ({ type: "paragraph", content });
const t = (text, marks) => (marks ? { type: "text", text, marks } : { type: "text", text });
const allText = (j) => (j.text ?? "") + (j.content ?? []).map(allText).join("");
const types = (j, acc = new Set()) => {
  if (j.type) acc.add(j.type);
  for (const m of j.marks ?? []) acc.add(`mark:${m.type}`);
  for (const c of j.content ?? []) types(c, acc);
  return acc;
};

const kSubset = {
  paragraph: "Hello world.\n\nSecond paragraph.",
  headings: "# One\n\n## Two\n\n### Three\n\n#### Four",
  emphasis: "Some **bold**, *italic*, ~~struck~~ and `code` text.",
  hard_break: "line one  \nline two",
  bullet_list: "- a\n- b\n  - b1\n- c",
  ordered_list: "1. one\n2. two\n   1. two.one",
  task_list: "- [ ] todo\n- [x] done",
  blockquote: "> quoted\n>\n> > nested",
  code_block: "```ts\nconst x = 1;\n```",
  hr: "above\n\n---\n\nbelow",
  link: "See [GpgFrontend](https://gpgfrontend.bktus.com).",
  image_asset: "![a cat](assets/cat.png)",
  table: "| a | b |\n| --- | --- |\n| 1 | **2** |",
  unicode: "Grüße, 你好, 🔐, مرحبا",
};

for (const [name, text] of Object.entries(kSubset)) {
  test(`subset: ${name} keeps its meaning and is stable`, () => {
    const r = roundtrip(text);
    assert.deepEqual(r.j2, r.j1);
    assert.equal(r.s2, r.s1);
  });
}

const kTypedText = ["1. not a list", "- not a list", "+ not a list", "# not a heading",
  "> not a quote", "---", "    four leading spaces", "5 < 6 & 7 > 3 <b>x</b>",
  "a | b | c", "C:\\path and \\* star", "*a* _b_ **c**", "[not](a link)"];
for (const text of kTypedText) {
  test(`typed text stays text: ${JSON.stringify(text)}`, () => {
    const { before, after } = typed([p(t(text))]);
    assert.deepEqual(after, before);
  });
}

test("a code block containing a fence survives", () => {
  const { before, after } = typed([{ type: "codeBlock", attrs: { language: null },
    content: [t("```\ninside\n```")] }]);
  assert.deepEqual(after, before);
});

test("an image alt with brackets survives", () => {
  const { before, after } = typed([{ type: "image", attrs: { src: "assets/a.png", alt: "x] y" } }]);
  assert.equal(after.content[0].attrs.alt, before.content[0].attrs.alt);
});

test("raw HTML is text, never markup, and never dropped", () => {
  md.load(editor, "<script>alert(1)</script>\n\ntext <img src=x onerror=alert(1)> more");
  const j = json();
  assert.ok(allText(j).includes("<script>alert(1)</script>"));
  assert.ok(allText(j).includes("<img src=x onerror=alert(1)>"));
  assert.ok(!types(j).has("image"));
});

test("a <br> in a table cell stays a line break", () => {
  const { before, after } = typed([{ type: "table", content: [
    { type: "tableRow", content: [{ type: "tableHeader", content: [p(t("a"), { type: "hardBreak" }, t("b"))] }] },
    { type: "tableRow", content: [{ type: "tableCell", content: [p(t("c"))] }] }] }]);
  assert.deepEqual(after, before);
});

test("unsafe links lose their mark, keep their text", () => {
  for (const bad of ["javascript:alert(1)", "data:text/html,x", "file:///etc/passwd", "vbscript:x"]) {
    md.load(editor, `[click](${bad})`);
    const j = json();
    assert.equal(allText(j), "click");
    assert.ok(!types(j).has("mark:link"), bad);
  }
  md.load(editor, "[ok](mailto:a@example.org)");
  assert.ok(types(json()).has("mark:link"));
});

test("only packaged assets are images", () => {
  for (const src of ["https://tracker/p.png", "file:///etc/passwd", "assets/../x.png",
                     "/abs.png", "data:image/png;base64,AA"]) {
    md.load(editor, `![t](${src})`);
    const j = json();
    assert.ok(!types(j).has("image"), src);
    assert.ok(allText(j).startsWith("[image: "), src);
  }
});

test("table cells take inline content only", () => {
  md.load(editor, "| h |\n| - |\n| c |");
  let pos = -1;
  editor.state.doc.descendants((n, at) => { if (pos < 0 && n.type.name === "tableCell") pos = at + 2; });
  editor.commands.setTextSelection(pos);
  assert.equal(editor.commands.toggleBulletList(), false);
  assert.equal(editor.commands.toggleTaskList(), false);
  assert.equal(editor.commands.setCodeBlock(), false);
});

test("front matter is split off and put back verbatim", () => {
  const front = "---\ntitle: x\ntags: [a, b]\n---\n";
  const { front: f, body } = md.splitFrontMatter(`${front}\n# Body\n`);
  assert.equal(f, front);
  assert.equal(body, "\n# Body\n");
  assert.equal(md.splitFrontMatter("# no front matter").front, "");
});

test("what the editor cannot keep is detected", () => {
  assert.deepEqual(md.unsupportedConstructs("a[^1]\n\n[^1]: n"), ["footnotes"]);
  assert.deepEqual(md.unsupportedConstructs("<details>x</details>"), ["html"]);
  assert.deepEqual(md.unsupportedConstructs("$$x^2$$"), ["math"]);
  assert.deepEqual(md.unsupportedConstructs("plain **text**"), []);
});

test("referenced assets are listed once each", () => {
  md.load(editor, "![a](assets/1.png)\n\n![b](assets/2.png)\n\n![c](assets/1.png)");
  assert.deepEqual(md.referencedAssets(editor.getJSON()), ["assets/1.png", "assets/2.png"]);
});
