// SPDX-License-Identifier: GPL-3.0-or-later
// Builds dist/ -- what the module embeds -- from src/. Deterministic for a
// given package-lock.json: no source maps, no timestamps, legal comments kept.
import { build } from "esbuild";
import { copyFileSync, mkdirSync } from "node:fs";

mkdirSync("dist", { recursive: true });
await build({
  entryPoints: ["src/editor.js"],
  bundle: true,
  format: "iife",          // a classic script: the policy allows script-src gfasset: only
  target: ["es2020"],
  minify: true,
  legalComments: "inline",
  charset: "utf8",
  outfile: "dist/editor.js",
  logLevel: "warning",
});
copyFileSync("src/editor.css", "dist/editor.css");
copyFileSync("src/index.html", "dist/index.html");
