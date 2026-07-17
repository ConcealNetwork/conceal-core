# WalletGreen shutdown and dispatcher threading

Status: Implemented (2026-07-15, short-term roadmap item PR 1a)
Scope: `src/Wallet/WalletGreen`, `src/Platform/{Linux,OSX,Windows}/System/Dispatcher`

## Context

`WalletGreen::shutdown()` and `~WalletGreen()` used to end with a raw
`m_dispatcher.yield()` ("let remote spawns finish"). `Dispatcher` is
single-threaded by design: every call except `remoteSpawn()` is only legal on
the thread that constructed and pumps the dispatcher.

conceal-desktop closes the wallet from the GUI thread
(`WalletAdapter::close()` → `m_wallet.reset()`), while the node thread pumps
the same dispatcher inside the node's `init()` loop. Calling `yield()` from
the GUI thread raced the node thread on the same epoll/kqueue/IOCP event loop
and could hang the UI or corrupt dispatcher state on wallet close.

## Current design

- Each platform `Dispatcher` records its owning thread at construction and
  exposes `bool isOwnerThread() const`. On Windows this reuses the existing
  `threadId` member (which yield/dispatch already assert on); Linux and macOS
  gained an `m_ownerThreadId` member.
- `WalletGreen::waitForRemoteSpawns()` (private) replaces the two raw
  `yield()` call sites in `shutdown()` and the destructor:
  - On the owner thread it still calls `yield()` — unchanged CLI/walletd
    behavior.
  - On a foreign thread it queues a barrier through `remoteSpawn()` (the only
    thread-safe dispatcher entry point) and waits for the owner thread to
    execute it. Once the barrier runs, every callback queued before shutdown
    has executed and can no longer touch the destroyed wallet.
  - The foreign-thread wait is bounded (10 s) so a dispatcher that is no
    longer being pumped produces a logged warning instead of a permanent
    freeze.

## Rationale

Draining is still required: pending `remoteSpawn` callbacks (for example from
`synchronizationProgressUpdated`) capture `this` and would be a use-after-free
if the wallet were destroyed before they run. The barrier gives the same
guarantee as `yield()` without breaking the dispatcher's single-thread
contract.

This is distinct from the eventfd `EAGAIN`/`EINTR` fixes that landed with
PR #360: those stopped a false-fatal abort in `dispatch()`/`yield()`; this
change stops a cross-thread hang on wallet close.

## Compatibility impact

None on-protocol. `IWallet` API unchanged. conceal-desktop benefits without
source changes; a follow-up in that repo can additionally make
`WalletAdapter::close()` call `stop()`/`shutdown()` explicitly (matching the
`WalletService` teardown order: `stop()` → wait → `shutdown()`).

## Tests

- `DispatcherTests.isOwnerThreadDetectsForeignThread`
- `DispatcherTests.remoteSpawnFromForeignThreadRunsOnOwnerThread`
- Full `system_tests` and CI-filtered `unit_tests` pass (the pre-existing
  `WalletApi.shutdownInit` failure is unrelated and excluded from CI).
