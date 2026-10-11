# Device walkthrough: feature 004 (title + category picker)

**Date**: 2026-10-11 · **Device**: InkPad One · **Build**: pbnotes 1.1.0 · **Server**: real Nextcloud Notes (disposable account)

Source: the user ran quickstart §3 on the reader and reported "all ok". Per-row detail beyond pass/fail was not recorded.

| # | Result |
|---|--------|
| 1-4 | Pass |
| 5-8 | Pass |
| 9-12 | Pass |
| 13 | **Skipped** (30+ categories not created). Paging stays covered by host tests only; SC-006 not verified on the device. |
| 14-15 | Pass |
| 16 | Pass as reported (no note lost). The concrete titles the server reported for duplicates and for `/` were not noted, so the R-3 server details (duplicate adjustment, forbidden characters) are not documented here. |
| 17 | Pass |
| Upgrade | Schema-3 upgrade on the device database passed; no deviations reported. |

## Open items

- Row 13 (picker paging with 30+ categories) not run on the device.
- R-3 [VERIFY]: the exact server behavior for duplicate titles and `/` was not captured.
