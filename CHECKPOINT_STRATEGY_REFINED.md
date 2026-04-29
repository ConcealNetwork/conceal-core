# Refined Checkpoint Strategy (Priority-Based)

## Overview

This document describes the checkpoint strategy combining four sources in priority order:

1. **CryptoNoteConfig.h** (PRIORITY 1 — hardcoded, highest trust)
2. **DNS_CHECKPOINT_DOMAIN** (PRIORITY 2 — updatable without release)
3. **blockchain.dat** (PRIORITY 3 — local fallback)
4. **P2P consensus** (for chunks beyond all trusted checkpoints)

`get_greatest_target_height()` returns `max(hardcoded_max, dns_max)` and is the canonical
ceiling used everywhere.

---

## checkpoint.dat Formats

| Format | Description | Detection |
|--------|-------------|-----------|
| **Old-style** | One hash per individual block (~60 MB) | First entry == genesis block hash |
| **New-style (chunks)** | One hash per `chunk_size` blocks (~6 KB) | First entry != genesis block hash |

`is_old_style_checkpoint_file()` reads only the first 32 bytes and compares against
`m_old_checkpoint_hashes[0]` (genesis hash from `CryptoNoteConfig.h`).

---

## Startup Init Flow

```
init_targets()                    ← populate hardcoded + DNS hashes from CryptoNoteConfig.h

is_old_style_checkpoint_file()?
  YES → skip load (file will be overwritten later)
  NO  → load_checkpoints_from_file() (fast, ~6 KB read)

[blockchain.dat loaded]

computeUpTo = min(get_greatest_target_height(), currentHeight)

if old-style file detected:
  ── ONE-TIME TRANSITION ──────────────────────────────────────────────────────
  convert_old_checkpoints_to_list_hashes(computeUpTo)
    └─ updates m_targets with cumulative list hashes (legacy P2P compatibility)
    └─ guard: skips if chunks already cover max_height (safety net — should not happen in normal flow)
  generate chunks 0 .. (computeUpTo-1)/chunk_size → add_verified_chunk_to_file()
    └─ writing chunk 0 truncates the old file → transition never repeats on next restart

else (new-style or absent):
  ── NORMAL PATH ──────────────────────────────────────────────────────────────
  append chunks [currentCoveredHeight+1 .. computeUpTo] only
    └─ each chunk validated against hardcoded/DNS checkpoint at its boundary
    └─ stops immediately if validation fails (blockchain mismatch)
```

**One-time transition guarantee:** after the old-style path runs, chunk 0 is written with
a chunk hash (not the genesis block hash), so `is_old_style_checkpoint_file()` returns
`false` on every subsequent restart.

---

## Priority Order for Block Hashes in Chunks

When building a chunk hash, each block's hash is resolved in priority order:

1. **CryptoNoteConfig.h** — if a hardcoded checkpoint exists at that height, use it
2. **DNS** — else if a DNS checkpoint exists at that height, use it
3. **blockchain.dat** — otherwise use the locally stored block hash

**Example** (chunk covers blocks 1 690 001–1 700 000):
- Block 1 690 753 → DNS checkpoint → DNS hash used
- Block 1 700 000 → CryptoNoteConfig.h checkpoint → hardcoded hash used
- All others → blockchain.dat hash

---

## Phase 2: Chunk Creation During Sync

**Trigger:** every `chunk_size` blocks added to the chain.

1. Compute chunk hash using priority order above.
2. Store in memory only (not yet in `checkpoint.dat`).
3. Seek **P2P consensus**:
   - Sample K peers (require M agreements, n distinct /16 networks, uptime > minimum).
4. Consensus reached → `add_verified_chunk_to_file()` (appends to `checkpoint.dat`).
5. Consensus failed → rollback blockchain, truncate `checkpoint.dat` to last valid chunk.

---

## Phase 3: Restart Validation

**Trigger:** node restarts with an existing new-style `checkpoint.dat`.

1. Load all chunks (auto-confirmed — file only ever contains verified chunks).
2. Validate chunks against hardcoded/DNS checkpoints via `validate_chunks_against_checkpoints()`.
3. Mismatch → rollback + truncate.
4. Append any missing chunks up to `computeUpTo` (normal path above).

---

## `convert_old_checkpoints_to_list_hashes` Guards

The function is skipped if either condition holds:

| Guard | Reason |
|-------|--------|
| `get_covered_height() >= max_height` | Safety net against accidental calls from other code paths — if chunks already cover the range, skip silently |

---

## Chunk Sizes

| Network | Blocks per chunk |
|---------|-----------------|
| Mainnet | 10 000 |
| Testnet | 25 000 |

---

## P2P Consensus Requirements

| Parameter | Mainnet | Testnet |
|-----------|---------|---------|
| M (min agreements) | 3 | 2 |
| K (peers sampled) | 5 | 3 |
| n (distinct /16 networks) | 2 | 1 |
| Min peer uptime | 12 000 blocks (~16.7 days) | 28 000 blocks (~38.9 days) |

---

## DNS Checkpoint Format

DNS TXT records:
```
height:hash
```
Example:
```
100000:55cf271a5c97785fb35fea7ed177cb75f47c18688bd86fc01ae66508878029d6
200000:52533de7f1596154c6954530ae8331fe4f92e92d476f097c6d7d20ebab1c2748
```

---

## Summary of Key Properties

- **Old-style detection** is exact: first 32 bytes compared to genesis hash — no heuristics.
- **computeUpTo** = `min(max(hardcoded, DNS), localChainHeight)` — never computes beyond what the node has.
- **Transition is one-shot**: old-style file is overwritten in a single startup; no repeat work.
- **Normal path is minimal**: only appends the chunks that are actually missing.
- **Trust hierarchy**: CryptoNoteConfig.h > DNS > blockchain.dat > P2P.
