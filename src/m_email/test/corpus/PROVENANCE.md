# Corpus provenance

Two buckets, two sets of rules.

`golden/` holds fixtures GpgFrontend wrote. They are well-formed, CRLF, and
carry byte-level contracts: raw header slices, signature-region offsets and
line-ending damage are all asserted directly. The OpenPGP armor in them is a
placeholder, never real cryptography; they pin structure, never verification.

`public/` holds messages taken verbatim from third-party test suites. They are
adversarial or simply old, and we did not write their bytes. Assertions over
them are semantic: parsed header values, decoded bytes, part shape, attachment
payloads. No byte-for-byte contract, and nothing may assume CRLF -- most of
them ship with bare LF and that is part of what they test.

Both buckets are marked `-text` in `.gitattributes`. Line endings are fixture
data; a checkout that converts them silently invalidates the corpus, so
`EMailCorpusTest.GoldenFixturesKeepTheirCrlfLineEndings` fails loudly if that
protection is ever missing.

Imported 2026-09-12. `sha256` is over the file as checked in here; `blob` is
the git object id, which is also the upstream blob id, because no file was
modified on the way in.

## Apache James mime4j

Upstream: <https://github.com/apache/james-mime4j>, path
`core/src/test/resources/testmsgs/`. License: Apache-2.0 (compatible with
GPL-3.0-or-later distribution; attribution retained by this file). No personal
data: every address in these messages is synthetic or absent.

| Local | Upstream file | sha256 | blob |
|---|---|---|---|
| `public/p01-boundary-name-clash.eml` | `boundary-name-clash.msg` | `f672cc0a4628a69c21eef243e94c2a3ec4ea58eb1d3a87ca20b1fce56447fdda` | `47580f7b2c0e5f185a9ea631cd6d69e19f9907e2` |
| `public/p02-ending-boundaries.eml` | `ending-boundaries.msg` | `8081bf7ec6e81ab7f5ac001e8430e539738aab9cd35392552044593e47c6b895` | `1a2444b198933a37da607d712bd9e462cbc96176` |
| `public/p03-boundary-text-in-body.eml` | `misplaced-boundary.msg` | `7ec45319b2baf77e586564a82c60dc7dceb66ec706406491015759e01f668da2` | `31d324aa604adea3f79ff2efcd0924c28d1c1c8d` |
| `public/p04-no-header-separator.eml` | `malformedHeaderStartsBody-nocrlfcrlf.msg` | `5398da5a1c16270e0293850e01d4da68a3caf47e4f494ba30cd74954ddfeed68` | `53db46e4295a34121e09f967cc50f9b0fe34b199` |
| `public/p05-base64-rfc822-bare-lf.eml` | `bad-newlines-multiple-parts-base64.msg` | `b4f2d622324f8f8883e66642ed5f369ae770bd80293bfea99fa9e8a8321c4eae` | `3b4f0b557782e6a686930e29c9e1cefebdb29c3c` |
| `public/p06-complex-multipart.eml` | `example.msg` | `f880bbcd8153b1cc2e9437137e19bf483a25d0eb85fef9aabc9c39c0016b55ed` | `cbab741f591181faf846ab21988cdb9429c3fb3f` |

Local modifications: none, beyond the rename.

## CPython, `Lib/test/test_email/data`

Upstream: <https://github.com/python/cpython>. License: PSF License Agreement
(permissive, GPL-compatible; notice retained by this file).

| Local | Upstream file | sha256 | blob |
|---|---|---|---|
| `public/p07-cte-matrix.eml` | `msg_10.txt` | `31b6aa0a2168c412559b6c9667846d84de86554af573a1a9dfa5dc753de3754a` | `d49e477a818dc9a71febbd75d939aae410f6e30a` |
| `public/p08-nested-charsets.eml` | `msg_12a.txt` | `defa4275a55f7778d400fcbf0628822dcae95d8239da065ba8e40049daaa32e4` | `2092aa0c351b43eb295b2ce5c8b9d2426afdc83a` |
| `public/p09-duplicate-attachment-names.eml` | `msg_44.txt` | `67f41bd0b0ac605c5431ad8c658c0c8e3c5d766eac8fbb81d51132f9fb818bfc` | `15a225287bd8f7a107082b69e2e5817ce6ff1f64` |

Local modifications: none, beyond the rename.

Privacy: these carry `barry@python.org` / `barry@digicool.com`, the address of
CPython's e-mail maintainer, who published them himself as test data, and
`cravindogs@cravindogs.com`, which is not a real correspondent. Nothing here is
third-party correspondence. `msg_16.txt` was considered and rejected for this
reason: it is a genuine bounce carrying a named individual's university
address, and `public/p06` covers the same nested `message/rfc822` shape without
it.

## MimeKit

Upstream: <https://github.com/jstedfast/MimeKit>, path
`UnitTests/TestData/messages/`. License: MIT (attribution retained by this
file).

| Local | Upstream file | sha256 | blob |
|---|---|---|---|
| `public/p10-iso2022jp-encoded-subject.eml` | `japanese.txt` | `4cf69e9eb15322bc1df5197d70d5a376a62546753ffff9b57ad782f3c05d65a8` | `0f95472ad079dccfececbf50c41d99e51ab0a1ac` |
| `public/p11-empty-multipart.eml` | `empty-multipart.txt` | `075b14b04c98f9b9d81370ca6a4cc73491ecf18ae374e5a637c7dbb28c115c6f` | `3afda9815ce6bb649c026c324f3a7e3e75428437` |
| `public/p12-preamble-epilogue.eml` | `epilogue.txt` | `34f7c18db581bbc1d0e4a66a187c8a6085527369f8bf3a4de7605f715f87de41` | `27e32a8fc4c813c0a609bee9a2aa5414de811af5` |

Local modifications: none, beyond the rename. Addresses are already
placeholders upstream (`x@x.com`, `mimekit@example.com`).

## Rejected sources

* **jwz MIME torture test** (via MimeKit's `jwz.mbox`) -- around a megabyte
  with 150+ bodies. The shapes it holds that we care about are already covered
  by the mime4j boundary cases at a fraction of the size.
* **Enron corpus, SpamAssassin public corpus** -- real third-party
  correspondence, unremarkable MIME, privacy exposure with no test value.
* **The vendored `vmime/tests/parser`** -- inline C++ string literals rather
  than message files, and it tests vmime's own contract, not ours. Worth
  reading before adding a case here, to avoid re-testing the library.
