# Host-script audit and branch tracking, 2026-10-30

This is a fork-only record. It is excluded from the upstream BSP,
peripheral and application series. No board reset, flash, application
restart, sound or motor action was performed for this audit.

## Current stack

The base remains upstream main at
`2f3eb1268edd1bb2ca002eb4be339a635003b1e4`.

| Branch | Tip | Commits in its layer |
| --- | --- | --- |
| bsp | `f5015fcfcad52749763bc53109aed84d5ea5f18d` | 11 |
| peripheral | `8942fff282f942cc39fab7b544124794a938b4dd` | 14 |
| application | `4de2d0aa00f8eccac69e62b3795c63498390795a` | 7 |

The dependency chain is `upstream/main -> bsp -> peripheral -> application`.
The BSP layer now includes the host-tool portability commit. Peripheral
retains its fourteen driver commits. The renderer output-directory option
is folded into the final application examples commit. Application adds no
system script, kernel driver, device tree or boot service.

The local peripheral ref had incorrectly advanced to the application tip.
Its tracking configuration used the local BSP branch, and application used
the local peripheral branch. This made an ordinary push capable of moving
a parent ref. Peripheral was restored from its verified fork ref. All three
branches now track their matching `origin` branches. Stacked dependency is
represented by commit ancestry, independently of tracking configuration.
Local main and fork main were left unchanged.

Backups before this audit are under
`archive/stack-before-script-audit-20261030/`. Full old/new commit mappings
remain in ignored `build/script-audit/stack-rewrite/manifest.json`.

## Portability changes

- One Python selector honors an explicit interpreter, an activated SDK
  environment, then an installed environment matching the checkout's IDF
  version. It does not silently select another installed SDK version.
- Make, setup and packaging share that selection. Make prefers a RISC-V
  compiler already on PATH and retains the explicit prefix override.
- SDK patch paths, Buildroot arguments and Python executable paths retain
  their argument boundaries. Buildroot itself still forbids whitespace in
  source and output paths; both READMEs document that restriction and the
  POSIX requirement for continuous UART recording and interactive console.
- The example renderer can put its images in a chosen directory when the
  checkout is read-only. Its default output and rendered images are unchanged.

Local connection/upload helpers and raw captures remain ignored. No host
service definition, personal SSH setup, machine-specific device selector or
local build environment is added to any upstream stack.
The existing local ESP-IDF patch commit is preserved but not staged; all
three stack trees keep the pinned upstream submodule revision.

## Sensitive-information review

Reviewed tracked project files and the named text blobs reachable from the
three stacks, original fork main and fork-only test history beyond the
upstream baseline. Checks covered private keys, common access-token formats,
SSH key material, concrete Wi-Fi credential assignments, personal home
paths and the local serial identity. No matching sensitive location was
found. Public authorship/maintainer attribution and generic upstream network
examples are retained. This is a scoped source review, not a guarantee that
every possible secret format is detectable. External submodule histories
and ignored local artifacts are outside this project-history scan.

## Verification

Each of the three final committed trees was exported separately:

- All four Buildroot defconfigs match their fragments at every branch head.
- All nineteen host tests pass at every branch head: UART retention and
  proxy behavior, native USB flashing identity and argument checks, SDK
  selection, relocated patch idempotence and build argument forwarding.
- Shell syntax and Python AST checks pass for all committed scripts:
  BSP has 48 shell / 10 Python files; peripheral 52 / 12; application 52 / 13.
- Diff whitespace checks pass for the audit changes at all branch heads.
- The release bundle was produced with the existing local build outputs;
  archive integrity passes. A second bundle using a Python executable path
  containing spaces also passes. This was packaging validation, not a fresh
  firmware build or a claim that package contents are credential-audited.
- Home, IMU and compass examples rendered to a separate output directory
  match the existing PNGs byte-for-byte.

Hardware and accepted LVGL behavior are unchanged. Previous full-build and
board validation records retain their original source revisions.
