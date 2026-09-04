Memory Management (`ply-system.h`)
================================

[Documentation coming soon.]

<svg viewBox="0 0 624 214" style="display:block;width:624px;max-width:100%;height:auto;margin-inline:auto">
 <g fill="none" stroke="var(--text-muted)" stroke-width="1">
  <path d="m318.3 134.8-4.81 6.43-4.81-6.43"/>
  <path d="M313.5 141.9V115"/>
  <path d="m318.3 182.8-4.81 6.43-4.81-6.43"/>
  <path d="M313.5 188.8V165"/>
 </g>
 <rect x="4" y="3" width="616" height="111" ry="17" fill="none" stroke="var(--border-table)" stroke-width="2"/>
 <g fill="var(--diagram-solid-fill)" stroke="var(--border-content)" stroke-width="1">
  <rect x="490.5" y="44.5" width="119" height="60" ry="11.45"/>
  <rect x="152.5" y="44.5" width="119" height="60" ry="11.45"/>
  <rect x="254.5" y="190.5" width="122" height="22" ry="7.6"/>
  <rect x="389.5" y="44.5" width="79" height="23" ry="7.94"/>
  <rect x="290.5" y="44.5" width="79" height="41" ry="10.1"/>
  <rect x="14.5" y="44.5" width="119" height="60" ry="11.45"/>
  <rect x="279.5" y="142.5" width="68" height="22" ry="8.6"/>
 </g>
 <g fill="var(--text-code)" font-family="jetbrains_mono,monospace" font-size="14px" text-anchor="middle">
  <text x="314" y="206">VirtualMemory</text>
  <text x="430" y="61">Variant</text>
  <text x="312" y="158">Heap</text>
  <text x="74" y="61">String</text>
  <text x="74" y="79">StringView</text>
  <text x="74" y="97">MutStringView</text>
  <text x="212" y="60">Array</text>
  <text x="212" y="78">ArrayView</text>
  <text x="212" y="96">FixedArray</text>
  <text x="330" y="60">Set</text>
  <text x="330" y="78">Map</text>
  <text x="550" y="60">Owned</text>
  <text x="550" y="78">Reference</text>
  <text x="550" y="96">RefCounted</text>
 </g>
 <g fill="var(--text-secondary)" font-family="source_sans_3,sans-serif" font-size="16px" text-anchor="middle">
  <text x="550" y="22">Object<tspan x="550" y="38">Ownership</tspan></text>
  <text x="330" y="21">Associative<tspan x="330" y="37">Maps</tspan></text>
  <text x="74" y="38">Strings</text>
  <text x="212" y="37">Arrays</text>
  <text x="429" y="38">Variants</text>
 </g>
</svg>

- [Virtual Memory](/docs/system/memory/virtual-memory.md): Map virtual address space to physical memory pages.
- [Heap](/docs/system/memory/heap.md): Plywood's built-in heap. All dynamic allocations in Plywood go through here.
- [Strings](/docs/system/memory/strings.md): String classes suitable for UTF-8 or arbitrary binary data.
- [Arrays](/docs/system/memory/arrays.md): Class templates providing resizable arrays.
- [Associative Maps](/docs/system/memory/associative-maps.md): Resizable collections supporting fast hash lookup.
- [Variants](/docs/system/memory/variants.md): Variant types similar to tagged unions with safety checks.
- [Object Ownership](/docs/system/memory/object-ownership.md): Reference-counting and owning pointers.
