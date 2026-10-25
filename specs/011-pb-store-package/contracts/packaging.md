# Contract: Packaging Scripts

Interfaces of the two scripts and the template. The store's own contract is `docs/contracts/package-manifest.md` and
`docs/contracts/cli.md` of `appstore-pocketbook` (tag v0.1.1); this file only states what PB Notes adds on top.

## `scripts/package-device.sh <built-arm-elf> [version] [out-dir]`

Arguments, defaults and the output name are **unchanged** (`version` defaults to the `VERSION` file, `out-dir` to `dist`;
output `<out-dir>/pbnotes-inkpad-one-<version>.zip`, path printed on stdout as before).

| Environment | Meaning |
|-------------|---------|
| `PBSTORE_CATALOG` | Optional. Path of the `pbstore-catalog` executable. If set it must be an executable file; if unset, `pbstore-catalog` is looked up on `PATH`. |

Order and failures (exit 1, message on stderr, no new ZIP in `<out-dir>`):

| Step | Failure message (start of line) |
|------|---------------------------------|
| inputs present (binary, launcher, certs, font, icons, license manifest and texts, **template**) | `missing input: <path>` |
| version looks like `MAJOR.MINOR.PATCH` | `version is not MAJOR.MINOR.PATCH: <value>` |
| binary contains the version string | `version mismatch: …` (unchanged text) |
| tool found | `pbstore-catalog not found. Put it on PATH or set PBSTORE_CATALOG …` / `PBSTORE_CATALOG is set but is not an executable file: <value>` |
| manifest written, no `@…@` left | `manifest template has an unfilled placeholder: <text>` |
| two packs identical | `package is not reproducible: two builds from the same files differ` |
| `check-package` passes | the tool's own `error[E-PKG-…]` lines, then `package check failed` |

`INSTALL.md` is no longer a required input and is never staged. `zip` is no longer required.

On success stdout carries only the ZIP path (so `ZIP=$(scripts/package-device.sh …)` keeps working); the tool's own
report (`size:`, `sha256:`, `app:`) is passed on to stderr.

## `scripts/check-release.sh [<zip>] [<tag>]`

Defaults: `<zip>` = `dist/pbnotes-inkpad-one-$(cat VERSION).zip`, `<tag>` = `v$(cat VERSION)`.

| Exit | Meaning |
|------|---------|
| 0 | tag = `v` + manifest version; manifest version = `VERSION`; ZIP file name = `pbnotes-inkpad-one-<manifest version>.zip`. Prints `release check passed (<version>)` |
| 1 | at least one mismatch; one line per mismatch: `release check FAILED: tag <t> does not match manifest version <v>` (similarly for `VERSION` and the file name); also when the ZIP has no readable `manifest.json` |
| 2 | usage error (ZIP not found) |

`scripts/check-release.sh --self-test` proves the rules on throw-away ZIPs (pass, tag mismatch, `VERSION` mismatch, name
mismatch, no manifest).

## `packaging/manifest.template.json`

Exactly the fixed values of [data-model.md](../data-model.md) and the single placeholder `@VERSION@`. Changing a fixed
value (for example `min_firmware`) is an edit of this file only; no script change.

## Release steps (documented in the README)

```bash
scripts/package-device.sh build-arm/app/pbnotes.app "" dist     # needs pbstore-catalog (PATH or PBSTORE_CATALOG)
git tag "v$(cat VERSION)" &&                                   # after the merge
scripts/check-release.sh &&                                     # tag, VERSION, ZIP name, manifest version agree
git push origin "v$(cat VERSION)" &&
gh release create "v$(cat VERSION)" "dist/pbnotes-inkpad-one-$(cat VERSION).zip" --notes-file <notes>
```

The steps are chained with `&&` on purpose: when `check-release.sh` reports a mismatch, nothing is pushed or published
(FR-013). `<notes>` = the `CHANGELOG.md` entry plus a link to `INSTALL.md` at that tag.
