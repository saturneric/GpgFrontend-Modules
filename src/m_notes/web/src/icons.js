// SPDX-License-Identifier: GPL-3.0-or-later
//
// The toolbar's icons: drawn for this editor on a 24-unit grid, stroked in
// the current text colour so they follow the theme. Static markup only.

const kIcons = {
  bold: '<path d="M7 5h6a3.5 3.5 0 0 1 0 7H7zM7 12h7a3.5 3.5 0 0 1 0 7H7z"/>',
  italic: '<path d="M19 4h-9M14 20H5M15 4 9 20"/>',
  strike: '<path d="M16 5.5A4 4 0 0 0 12.5 4h-1A3.5 3.5 0 0 0 8 7.5c0 1.6 1 2.9 3 3.5M8.5 18.5A4.5 4.5 0 0 0 12 20h.5a3.5 3.5 0 0 0 3.5-3.5c0-.9-.3-1.8-.9-2.5M4 12h16"/>',
  code: '<path d="m16 18 6-6-6-6M8 6l-6 6 6 6"/>',
  link: '<path d="M10 13a5 5 0 0 0 7.5.5l3-3a5 5 0 0 0-7-7l-1.7 1.7"/><path d="M14 11a5 5 0 0 0-7.5-.5l-3 3a5 5 0 0 0 7 7l1.7-1.7"/>',
  bulletList: '<path d="M9 6h12M9 12h12M9 18h12"/><circle cx="4.5" cy="6" r="1" fill="currentColor"/><circle cx="4.5" cy="12" r="1" fill="currentColor"/><circle cx="4.5" cy="18" r="1" fill="currentColor"/>',
  orderedList: '<path d="M10 6h11M10 12h11M10 18h11M4 4.5 5.5 4v5M4 9h3M7 19.5H4.2c0-1.2 2.8-2 2.8-3.3a1.3 1.3 0 0 0-2.6-.2"/>',
  taskList: '<rect x="3" y="4" width="6" height="6" rx="1.5"/><path d="m3.5 16.5 2 2 3.5-4M13 7h8M13 17h8"/>',
  quote: '<path d="M4 5v14M9 7h11M9 12h11M9 17h7"/>',
  codeBlock: '<rect x="3" y="4" width="18" height="16" rx="2.5"/><path d="m10 9.5-2.5 2.5 2.5 2.5M14 9.5l2.5 2.5-2.5 2.5"/>',
  rule: '<path d="M3 12h18"/><path d="M7 6.5h10M7 17.5h10" opacity=".4"/>',
  table: '<rect x="3" y="4" width="18" height="16" rx="2.5"/><path d="M3 9.5h18M3 14.75h18M9.5 9.5V20M14.5 9.5V20"/>',
  rowAdd: '<rect x="3" y="3" width="18" height="10" rx="2.5"/><path d="M12 16v6M9 19h6"/>',
  colAdd: '<rect x="3" y="3" width="10" height="18" rx="2.5"/><path d="M16 12h6M19 9v6"/>',
  rowDelete: '<rect x="3" y="3" width="18" height="10" rx="2.5"/><path d="M9 19h6"/>',
  colDelete: '<rect x="3" y="3" width="10" height="18" rx="2.5"/><path d="M16 12h6"/>',
  tableDelete: '<path d="M4 7h16M10 11v6M14 11v6M6 7l1 12.5a1.5 1.5 0 0 0 1.5 1.5h7a1.5 1.5 0 0 0 1.5-1.5L18 7M9 7V4.5A.5.5 0 0 1 9.5 4h5a.5.5 0 0 1 .5.5V7"/>',
  undo: '<path d="M9 14 4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 0 11H11"/>',
  redo: '<path d="m15 14 5-5-5-5"/><path d="M20 9H9.5a5.5 5.5 0 0 0 0 11H13"/>',
  find: '<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>',
  more: '<circle cx="5" cy="12" r="1.2" fill="currentColor"/><circle cx="12" cy="12" r="1.2" fill="currentColor"/><circle cx="19" cy="12" r="1.2" fill="currentColor"/>',
  chevronDown: '<path d="m6 9 6 6 6-6"/>',
  chevronUp: '<path d="m6 15 6-6 6 6"/>',
  close: '<path d="M6 6l12 12M18 6 6 18"/>',
  check: '<path d="m5 12.5 4.5 4.5L19 7.5"/>',
  unlink: '<path d="M10 13a5 5 0 0 0 7.5.5l3-3a5 5 0 0 0-7-7l-1.7 1.7"/><path d="M14 11a5 5 0 0 0-7.5-.5l-3 3a5 5 0 0 0 7 7l1.7-1.7"/><path d="M3 3l18 18"/>',
};

const kSvgNs = "http://www.w3.org/2000/svg";

/** A fresh <svg> for @p name; empty for a name that has no icon. */
export function icon(name, size = 18) {
  const svg = document.createElementNS(kSvgNs, "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("width", String(size));
  svg.setAttribute("height", String(size));
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "1.75");
  svg.setAttribute("stroke-linecap", "round");
  svg.setAttribute("stroke-linejoin", "round");
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("focusable", "false");
  svg.innerHTML = kIcons[name] ?? "";
  return svg;
}

