# Refined Checkpoint Strategy (Priority-Based)

## Overview

This document describes the refined checkpoint strategy that combines:
1. **CryptoNoteConfig.h** checkpoints (PRIORITY 1 - highest)
2. **DNS_CHECKPOINT_DOMAIN** checkpoints (PRIORITY 2 - secondary)
3. **blockchain.dat** (PRIORITY 3 - fallback)
4. **P2P consensus** (for chunks beyond trusted checkpoints)

## Priority Order for Block Hashes in Chunks

When generating chunk hashes, the system uses the following priority order:

1. **CryptoNoteConfig.h** (PRIORITY 1) - Hardcoded in source code, highest trust
2. **DNS_CHECKPOINT_DOMAIN** (PRIORITY 2) - Fetched from DNS, secondary trusted source
3. **blockchain.dat** (PRIORITY 3) - Actual block hash from local blockchain (fallback)

**Example:**
- Chunk contains blocks 1690001-1700000
- Block 1690753 has checkpoint in DNS
- Block 1700000 has checkpoint in CryptoNoteConfig.h
- Result:
  - Blocks 1690001-1690752: use blockchain.dat
  - Block 1690753: use DNS checkpoint hash
  - Blocks 1690754-1699999: use blockchain.dat
  - Block 1700000: use CryptoNoteConfig.h checkpoint hash

## Phase 1: Initial Setup (No checkpoint.dat)

**When:** Node starts with `blockchain.dat` but no `checkpoint.dat`

**Process:**
1. Load checkpoints from **CryptoNoteConfig.h** (PRIORITY 1)
2. Fetch checkpoints from **DNS_CHECKPOINT_DOMAIN** (PRIORITY 2)
3. Generate chunks up to the last **CryptoNoteConfig.h** checkpoint:
   - Use priority order: CryptoNoteConfig.h > DNS > blockchain.dat
   - Validate that blockchain.dat matches checkpoints
   - Save chunks to `checkpoint.dat` (auto-confirmed)
4. If DNS checkpoints extend beyond CryptoNoteConfig.h:
   - Create chunks up to highest DNS checkpoint
   - Use priority order during generation
   - Auto-confirm (validated against DNS checkpoints)
5. Beyond DNS checkpoints:
   - Create chunks in memory only
   - Seek P2P validation before saving to `checkpoint.dat`

**Result:** `checkpoint.dat` contains chunks validated against CryptoNoteConfig.h and DNS checkpoints.

## Phase 2: Milestone Chunk Creation (During Sync)

**When:** Every `chunk_size` blocks (10,000 mainnet, 25,000 testnet)

**Process:**
1. Compute chunk hash using **priority order**:
   - For each block in chunk:
     - If checkpoint in CryptoNoteConfig.h → use it
     - Else if checkpoint in DNS → use it
     - Else → use blockchain.dat hash
2. Store chunk hash **in memory only** (not saved to `checkpoint.dat` yet)
3. Seek **P2P validation**:
   - Sample K peers (M must agree)
   - Require network diversity (n distinct /16 networks)
   - Require peer uptime > minimum (12k blocks mainnet, 28k testnet)
4. If consensus reached:
   - Save chunk to `checkpoint.dat` (confirmation)
5. If consensus fails:
   - Trigger blockchain rollback
   - Truncate `checkpoint.dat` to last valid chunk

**Key Point:** Priority order ensures trusted checkpoints (CryptoNoteConfig.h, DNS) take precedence over blockchain.dat during chunk generation.

## Phase 3: Startup Validation

**When:** Node restarts with existing `checkpoint.dat`

**Process:**
1. Load all chunks from `checkpoint.dat` (trusted - already validated)
2. Get current blockchain height
3. Find highest checkpoints from:
   - CryptoNoteConfig.h (PRIORITY 1)
   - DNS (PRIORITY 2)
4. For each checkpoint we can verify (checkpoint height <= current blockchain height):
   - Compute chunk hash using **priority order**:
     - CryptoNoteConfig.h checkpoints in chunk
     - DNS checkpoints in chunk (if not in CryptoNoteConfig.h)
     - blockchain.dat for remaining blocks
   - Compare with stored chunk hash in `checkpoint.dat`
   - If mismatch:
     - Rollback blockchain to chunk boundary
     - Truncate `checkpoint.dat` to last valid chunk
5. If checkpoint is beyond current height:
   - Continue syncing
   - Validate when reaching that height via P2P

**Example:**
- Current blockchain height: 1,800,000
- CryptoNoteConfig.h highest: 1,700,000 (chunk 170)
- DNS highest: 1,900,000 (chunk 190, can't verify yet - not synced)
- We can only verify chunk 170 (CryptoNoteConfig.h checkpoint)
- Chunk 190 will be validated when we sync to height 1,900,000

## Priority Order Implementation

### Chunk Generation (`generate_chunks_from_block_ids`, `add_chunk_from_block_ids`)

1. Get block IDs from blockchain.dat for the chunk
2. Build checkpoint map with priority:
   - Add CryptoNoteConfig.h checkpoints (PRIORITY 1)
   - Add DNS checkpoints (PRIORITY 2, only if not in CryptoNoteConfig.h)
3. For each checkpoint in chunk:
   - Validate blockchain.dat matches checkpoint hash
   - Replace blockchain.dat hash with checkpoint hash
4. Compute chunk hash from modified block IDs
5. Store chunk hash

### Startup Validation (`validate_chunks_against_checkpoints`)

1. Find highest checkpoints from CryptoNoteConfig.h and DNS
2. For each checkpoint we can verify (based on current height):
   - Get chunk containing checkpoint
   - Compute chunk hash using priority order
   - Compare with stored chunk hash
   - If mismatch → rollback

## Benefits

1. **Trust Hierarchy:** CryptoNoteConfig.h > DNS > blockchain.dat
2. **Emergency Updates:** DNS can provide checkpoints without code release
3. **Autonomous:** P2P consensus for chunks beyond trusted checkpoints
4. **Backward Compatible:** Works with existing CryptoNoteConfig.h checkpoints
5. **Flexible:** Can rely on P2P when DNS unavailable

## Chunk Sizes

- **Mainnet:** 10,000 blocks per chunk
- **Testnet:** 25,000 blocks per chunk

## Consensus Requirements

### Mainnet
- M = 3 (minimum agreements)
- K = 5 (peers to sample)
- n = 2 (distinct /16 networks)
- Uptime: > 12,000 blocks (~16.7 days)

### Testnet
- M = 2 (minimum agreements)
- K = 3 (peers to sample)
- n = 1 (distinct /16 networks)
- Uptime: > 28,000 blocks (~38.9 days)

## DNS Checkpoint Format

DNS TXT records should be in format:
```
height:hash
```

Example:
```
100000:55cf271a5c97785fb35fea7ed177cb75f47c18688bd86fc01ae66508878029d6
200000:52533de7f1596154c6954530ae8331fe4f92e92d476f097c6d7d20ebab1c2748
```

## Summary

This refined strategy provides:
- **Clear priority order** for checkpoint sources
- **Autonomous operation** via P2P consensus
- **Emergency capability** via DNS checkpoints
- **Efficient validation** using chunked checkpoints
- **Backward compatibility** with existing hardcoded checkpoints

