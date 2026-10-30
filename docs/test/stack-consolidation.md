# Stack consolidation, 2026-10-30

This record belongs to the fork-only test branch. No PR was opened.
The running board and its accepted dark LVGL interface were left untouched.

| Branch | Previous tip | Consolidated tip |
| --- | --- | --- |
| `bsp` | `e772163cdae1b89d77ea2a45e296fd49090e51d1` | `0fa1e658963510651cc476766c645bc6353f94e4` |
| `peripheral` | `515cfcbfd95c3acf8cc43972f8a4a024a3b8aac3` | `7276869db5f5daa5c785bd0900beb7ce4c85789b` |
| `application` | `c29292dceb2d85a3112a7f5c2adaf1936954fc2d` | `b264e8aefceaa9294bbebcf489e0687185891a7d` |

The dependency chain remains `upstream/main -> bsp -> peripheral -> application`,
with the original upstream baseline `2f3eb1268edd1bb2ca002eb4be339a635003b1e4`. Local backup branches are
`archive/stack-before-lvgl-squash-20261030/bsp`, `.../peripheral` and
`.../application`. The complete commit mapping stays in the ignored local
`build/stack-validation/stack-rewrite/manifest.json`.

## Boundaries

- BSP has ten commits. The four-page zsmalloc chain limit is folded into
  minimal board support. Kernel-input invalidation for the Linux build
  path moves into BSP as a separate build-system commit.
- Peripheral has fourteen commits for display, touch, GPIO interrupts,
  sensors, camera, audio and USB. It adds no application and carries no
  separate memory-policy or generic build-cache follow-up.
- Application has seven commits: tiled camera preview, its selectable
  Buildroot package, the LVGL dependency, the inherited-framebuffer
  embedding interface, the final dark visual launcher, its package, and
  reproducible host-rendered examples.

The preview's full-frame allocation and later tile conversion no longer
appear as two implementation stages. The launcher is introduced with the
accepted sensor visualizations and charcoal palette. Later visual fixups
are folded into that implementation commit. Packaging and host examples
retain separate review boundaries.

Application changes only application sources, LVGL's submodule, its package
definitions/selections and the README package entry. Kernel drivers,
kernel configuration, device trees, loader, boot services, storage policy
and system scripts have no application-layer diff. Both applications still
require manual launch; no automatic audio or motor action was added.

## Verification

The peripheral tree is byte-for-byte identical to the previously tested
`515cfcbfd95c3acf8cc43972f8a4a024a3b8aac3` tree. The application tree is
byte-for-byte identical to the accepted and flashed
`c29292dceb2d85a3112a7f5c2adaf1936954fc2d` tree, including all submodule pins.
Historical hardware records retain those original revision identifiers.

Compared with the previous BSP tip, the only file changes are the memory
configuration moved forward from peripheral and the existing build-cache
invalidation logic. Kconfig resolution confirms the four-page zsmalloc
limit, zram, SPI NAND, UBIFS and NAND swap, with display, input and runtime
USB disabled in minimal BSP.

Generated Buildroot defconfigs match their fragments at all three branch
heads. The original boards retain their existing package selections.
Every commit has a sign-off and a wrapped description; whitespace checks
pass, excluding nested patch context lines that belong to patch syntax.
Native and RV32 launcher builds pass, as do all twelve UART recorder and
native USB flashing helper tests.
No credentials, raw serial logs, board captures or diagnostic binaries
are added to the upstream series.

Since the final peripheral and application trees are unchanged, this
history-only consolidation does not require another firmware flash.
The firmware currently running on the board continues to identify its
original build revision. Detailed prior checks remain in
[lvgl-visuals.md](lvgl-visuals.md).
