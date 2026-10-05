# Constrained-decode mask performance

Bounded Unicode strings previously expanded into nullable repetition helper chains.
Vocabulary-mask lookahead repeatedly traversed those chains; a 1–180-scalar string
could take seconds per speculative mask operation and leave the GPU waiting.

The native compiler now delegates provably independent string-length assertions
to request-owned decoded-scalar counters. Immutable vocabulary bitsets enforce
plain-token bounds in word operations; quote/escape transitions retain semantic
validation. The original schema remains authoritative. Correlated alternatives
that cannot safely delegate keep their grammar assertions. Reasoning-delimiter
bitsets include overlapping prefix borders, and forks/draft previews remain independent.

## Measured configuration

Measured on 2026-10-05 with one RTX 5090, driver 610.62, Ubuntu under WSL,
GCC 13 / Release, a converted Qwen3.8-27B NVFP4 artifact, FP8 KV,
eight active requests, 227008 shared KV tokens, 102400 context ceiling,
MTP with three draft tokens and the optimized proposal head, and prefill chunk 4096.
These are experimental settings, not hardcoded runtime defaults.

The text fixture copies synthetic citation strings into an object whose dynamic
registry values are two-element tuples: a source identifier and a string with
`minLength: 1`, `maxLength: 180`. Input identities differ between requests.
Every response is checked against the unchanged schema and exact source strings.

| Boundary | Measured time |
|---|---:|
| Original mask, 4 columns, one lane, 64-scalar prefix, 248077-token vocabulary | 6194 ms |
| Final mask, 4 columns, one lane, tested prefixes | 0.33–0.51 ms |
| Final whole eight-lane mask batch, ASCII, 4 columns | 5.56–5.63 ms |
| Final whole eight-lane mask batch, Chinese | 3.12–3.29 ms |
| Final whole eight-lane mask batch, emoji | 1.85–1.96 ms |
| Runtime whole-batch decode host work, full eight-lane intervals | 6.51–7.13 ms/round |
| Runtime decode device wait, the same intervals | 17.30–18.51 ms/round |

The runtime comparison uses five complete one-second intervals with no computed
prefill and exactly eight rows in every decode round. CPU time includes mask work
and the other decode host work; GPU time is the recorded decode device-wait duration,
not an isolated kernel-event measurement. The slowest measured CPU interval was
shorter than the fastest measured GPU-wait interval. This establishes the budget
for the exercised shapes, not every arbitrary schema or zero CPU overhead.

| Synthetic strict-copy workload | Single request | Eight-request wave |
|---|---:|---:|
| English, 16 citation strings/request | 256.6 tok/s | 1008.1 tok/s |
| Chinese, 12 citation strings/request | 245.0 tok/s | 1047.2 tok/s |

Wave rates include prefill, admission and drain, and use actual completion totals.
Exact source copying and schema checks passed for all requests. Token totals can
differ because equivalent JSON whitespace spellings differ; rates do not imply
universal model-content accuracy.

Seven native CPU regression targets passed. Independent original-schema checks
covered 4954 base pattern/calendar/Unicode/cache cases, 732 semantic cases and
1072 length cases. The semantic set retains expected compile failures and declared
serialization subsets. Existing canonical const-object whitespace restrictions
remain explicit. The test entry points are documented in [tests/README.md](../../tests/README.md).
