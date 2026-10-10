# Test fixtures

Fixtures are recorded or hand-written Nextcloud Notes API v1 responses used by the contract and
integration tests. They MUST be anonymized (constitution Principle IV) before they are committed;
`scripts/check-fixtures.sh` (run by CTest) enforces the mechanical rules.

## Anonymization procedure

| Real data | Replace with |
|-----------|--------------|
| server host name | `cloud.example.com` |
| user names | `alice`, `bob` |
| e-mail addresses | `alice@example.com` |
| note text and titles | generated placeholder text (for example "Shopping list", "Meeting notes") |
| ETags | freshly generated hex strings (do not keep real ones) |
| note ids | small integers (1, 2, 3, ...) |
| `Authorization` headers or values | remove completely |

## Naming

Fixtures recorded from a live server are named `<name>.json`. Fixtures written by hand from the
public API documentation, before they could be verified against a server, are named
`<name>.unverified.json` (task T022 renames them once verified).

## Checks

- `scripts/check-fixtures.sh` fails on hosts other than `example.com`/`example.org`/`localhost`/
  `*.invalid`, on e-mail addresses outside `@example.com`, and on any `Authorization` value.
- `scripts/check-fixtures.sh --self-test` proves that the checker rejects bad fixtures.
