# CI overhaul plan

Goal: PR feedback in **~10–12 min warm** (from 30+), without losing any coverage
that exists today — everything removed from the PR path moves to nightly.

**Success metric:** median PR wall-clock (slowest required job) ≤ 12 min warm;
docs-only PR ≤ 1 min. Measured on real PRs via the Actions usage page before/after.

## 0. Baseline: what one PR costs today

Every PR to `main` currently starts **15 jobs**:

| Workflow | Jobs | What it does | Est. time |
|---|---|---|---|
| `humble` / `build` | 1 | apt deps + `setup-ros` + full 28-pkg build **with coverage** + serial tests + lcov + codecov | 30+ min (long pole) |
| `humble` / `build-platforms` | 6 | full aerostack2 build *again* + 1 platform repo each | ~20–30 min each |
| `jazzy` / `build` | 1 | same full build, also with `coverage-gcc` | ~30 min |
| `pixi` / `build` | 6 | `pixi install` on humble/jazzy × linux-64/aarch64/osx-arm64 | 5–15 min each |

Compounding problems:

- No ccache, so ~200 C++ translation units compile cold every run.
- No change detection — a README typo rebuilds and retests all 28 packages.
- No `concurrency` groups — a fixup push leaves the stale run burning minutes.
- `parallel-workers: 1` serializes tests across ~35 test packages.
- `setup-ros` re-provisions an already-provisioned `osrf/ros` container.
- `action-ros-ci` pinned at three different versions (v0.2 / v0.3 / v0.4).
- Package lists are duplicated in 3 workflows and have already drifted: the
  nightly codecov list is missing `as2_behaviors_swarm_flocking`; jazzy's list
  is missing the `aerostack2` metapackage.

---

## Phase 1 — Remove waste from the PR path

*Scope: `build-humble.yaml`, `build-jazzy.yaml`, `pixi-build.yaml`,
`codecov_test.yaml`. No test-semantics change, safe to land as one PR.*

### 1.1 Concurrency cancellation (all 3 PR workflows)

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}
```

Keyed on PR number so pushes to `main` never cancel each other.

### 1.2 Trim PR trigger types

`review_requested` and `ready_for_review` re-run identical SHAs. Keep:

```yaml
pull_request:
  types: [opened, synchronize, reopened]
```

(Skipping draft PRs via `if: !github.event.pull_request.draft` is a separate,
behavior-changing decision — not part of this phase.)

### 1.3 Strip coverage from PR builds

In `build-humble.yaml` and `build-jazzy.yaml`:

- Delete `"mixin": ["coverage-gcc"]` from `colcon-defaults` (keep the block for
  `parallel-workers` for now).
- Delete the `Codecov` step and the lcov-related apt packages (`lcov`,
  `python3-colcon-lcov-result`, `python3-colcon-coveragepy-result`).

In `codecov_test.yaml` (nightly, keeps sole ownership of coverage):

- Sync the package list (add `as2_behaviors_swarm_flocking`).
- Bump `codecov-action@v1.2.1` → v4 (v1 uses a deprecated upload endpoint).
- Add `workflow_dispatch` so coverage can be re-run on demand.

Effect: PR builds compile without `-O0 --coverage`, skip the lcov aggregation
pass, and PRs stop double-reporting coverage that the nightly owns.

### 1.4 ccache

In both build workflows:

```yaml
env:
  CCACHE_DIR: ${{ github.workspace }}/.ccache

steps:
  - run: sudo apt-get install -y ccache   # add to existing deps line
  - uses: actions/cache@v4
    with:
      path: ${{ github.workspace }}/.ccache
      key: ccache-humble-${{ github.sha }}
      restore-keys: ccache-humble-
  - uses: ros-tooling/action-ros-ci@v0.4
    with:
      extra-cmake-args: >-
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
      ...
```

Details that matter:

- `CCACHE_DIR` must live under `github.workspace` — the mount shared between
  the container and the cache action.
- `restore-keys` gives every PR the newest cache from any branch; caches
  created on `main` are readable by all PRs.
- Cap size (`ccache -M 1.5G` after restore). Two distros × 1.5 GB fits in the
  10 GB repo cache quota.
- First run after merge is cold — announce that so nobody reverts after one
  slow build.

### 1.5 Uniform action pinning

`action-ros-ci@v0.4` and `setup-ros@v0.7` everywhere.

**Phase 1 verification:** re-run a representative PR on the new workflows twice
(cold, then warm). Expect warm humble build ~12–15 min with tests as the new
long pole. Confirm: a second push cancels the in-flight run; nightly codecov
still uploads.

---

## Phase 2 — Build only what changed

*Scope: new `changes` job + script, restructured triggers. The
monorepo-scaling piece.*

### 2.1 Path → package mapping job

New first job in `build-humble.yaml` (seconds, bare runner, no container):

```yaml
jobs:
  changes:
    runs-on: ubuntu-latest
    outputs:
      packages: ${{ steps.detect.outputs.packages }}   # space-separated
      full_build: ${{ steps.detect.outputs.full_build }}
      skip: ${{ steps.detect.outputs.skip }}
    steps:
      - uses: actions/checkout@v4
        with: { fetch-depth: 0 }
      - id: detect
        run: pip install colcon-common-extensions && python3 .github/scripts/changed_packages.py
```

`.github/scripts/changed_packages.py` (~60 lines, versioned with the workflows):

1. `git diff --name-only origin/main...HEAD` (merge-base diff, so rebases don't
   over-trigger).
2. For each changed file, walk up to the nearest `package.xml`; collect package
   names.
3. **Escalation rules** (ordered):
   - Any changed path in `.github/**`, `as2_core/**`, `as2_msgs/**`,
     `aerostack2/**` (metapackage), or root build files (`pixi.toml`,
     `codecov.yaml`) → `full_build=true`.
   - No changed file maps to any package and none escalates (README, docs,
     LICENSE) → `skip=true`.
4. Expand to dependents: `colcon list --packages-above <changed> --names-only`
   (colcon reads the `package.xml` graph without building). Output the
   expanded set.

Escalating on `as2_core`/`as2_msgs` costs nothing in practice — they are the
dependency roots, so `--packages-above` would return nearly everything anyway;
the explicit rule just makes the policy legible.

### 2.2 Wire into the build job

```yaml
  build:
    needs: changes
    if: needs.changes.outputs.skip != 'true'
    ...
      - uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: ${{ needs.changes.outputs.full_build == 'true' && '<full list>' || needs.changes.outputs.packages }}
```

`action-ros-ci` builds `--packages-up-to <list>` (dependencies included) and
tests `--packages-select <list>` — since the list already includes dependents,
a change to `as2_motion_controller` builds+tests it plus the behaviors that
consume it, and nothing else.

**`main` pushes always run the full list.** That keeps `main` verified
end-to-end regardless of PR-level selection, and keeps the ccache warm.

### 2.3 Demote jazzy + platform builds from PR-blocking

- **Split `build-platforms` out of `build-humble.yaml`** into
  `build-platforms.yaml`:

  ```yaml
  on:
    schedule: [{ cron: '0 4 * * *' }]
    workflow_dispatch:
    pull_request:
      paths: ['as2_core/**', 'as2_msgs/**', 'as2_aerial_platforms/**', '.github/workflows/build-platforms.yaml']
  ```

  A platform repo breaking signals interface drift — nightly latency is fine;
  the `paths` filter keeps the check on exactly the PRs that can cause it.
- **`build-jazzy.yaml`**: nightly + `workflow_dispatch` + PRs only for `paths`
  matching `as2_core/**`, `as2_msgs/**`, `.github/**`. Humble is the primary
  supported distro per the README; it stays the universal PR gate.
- **Nightly failure routing:** final `if: failure()` step using
  `gh issue create` with a `ci-nightly` label, deduped by title via
  `gh issue list --label ci-nightly --state open`.

### 2.4 Pixi matrix diet

PRs: `humble` × `linux-64` only. Full 6-combo matrix on `main` push + nightly.
macOS + QEMU-emulated aarch64 are the most expensive, least PR-informative
minutes in the account.

### Branch-protection gotcha

If `humble / build` is a required status check, a job-level `if:` skip counts
as pending forever, blocking docs PRs (workflow-level `paths` skips pass;
job-level skips don't). Handle it by keeping the gate always-running: make
`changes` itself the required check, or add a no-op `ci-passed` fan-in job
that `needs` everything and succeeds when skips are intentional. **Must land
together with 2.1/2.2, not after.**

**Phase 2 verification (probe PRs):**

1. README-only change → `skip=true`, zero build jobs, PR still mergeable.
2. Touch `as2_geozones` only → builds it + dependents; test log shows only
   those packages tested.
3. Touch `as2_core` → full build via escalation rule.
4. Negative control: deliberate API break in `as2_motion_controller` → a
   dependent behavior package fails on the PR.

---

## Phase 3 — Structural cleanups

### 3.1 CI base image

New `docker/ci/Dockerfile` (per distro, `ARG ROS_DISTRO`):

- `FROM osrf/ros:${ROS_DISTRO}-desktop`.
- Everything the workflows' "Install deps" steps do today: apt packages,
  `ros-humble-behaviortree-cpp` + the multiarch symlink workaround (already in
  `docker/humble/Dockerfile` — single source of truth going forward), ccache,
  vcstool, colcon extensions.
- Real `rosdep install --from-paths src --ignore-src -y` against a snapshot of
  the tree. rosdep still runs in-job (no-op when the image is current), so a PR
  adding a new dependency pays only the incremental apt cost.

Publishing: extend `docker-nightly.yaml` with a `ci-base` build
(`aerostack2/ci-base-humble:latest`) + `workflow_dispatch` for forced refresh.
PR workflows switch `container:` to it and delete "Install deps" + `setup-ros`
(~2–4 min/job, one less network flake source — current jobs hit apt mirrors,
PyPI, and GitHub raw URLs before compiling anything).

Rollout: run new-image and old-path workflows side by side on 2–3 PRs
(temporary duplicate job), then delete the old steps.

### 3.2 Test parallelism

> **Deprioritized by Phase 1 data:** with coverage stripped and ccache warm,
> the full humble test phase measures **2m48s** at `parallel-workers: 1` —
> tests were never the bottleneck. The build phase (15m37s at 72% ccache
> hits: configure + link + misses) is, and Phase 2's package selection is
> what removes it. Keep this section only as a reference if test volume
> grows.

Decision tree — investigate before changing:

1. Instrument: full test suite at `parallel-workers: 4` on a scratch branch,
   three times. Collect failures.
2. **If clean:** raise it. Done — likely saves 8–12 min.
3. **If DDS cross-talk** (discovery of another test's nodes, port binds):
   `ROS_LOCALHOST_ONLY=1` + per-test `ROS_DOMAIN_ID` isolation via
   `domain_coordinator` (the pattern ROS 2 core CI uses) in fixtures that
   launch nodes; re-run step 1.
4. **If a few tests are inherently exclusive** (e.g. gazebo-spawning): keep
   those packages in a serial second `colcon test` pass
   (`--packages-select` the exclusive set) rather than serializing all ~35
   packages for the sins of two.

Record the outcome as a comment next to `parallel-workers` in the workflow so
nobody has to re-derive it.

### 3.3 Package-list deduplication

After Phase 2 the "full list" exists in humble, jazzy, and codecov workflows.
Replace with one source: the `aerostack2` metapackage's `package.xml` already
declares every package — have `changed_packages.py --all` emit the list and
the workflows consume it. Kills the drift class of bug permanently (the
swarm_flocking omission was exactly this).

---

## Verifying in a fork before any upstream PR

All of this can be validated end-to-end in `alvgaona/aerostack2` (public fork)
with zero upstream footprint. Actions minutes are free for public repos,
including `ubuntu-24.04-arm` and `macos-14` runners, and the fork has its own
isolated 10 GB cache quota — so timing measurements are clean.

### Setup (once)

1. **Enable Actions** on the fork (Settings → Actions — forks default to
   disabled). Note: `schedule` crons don't fire on forks reliably; every
   workflow in this plan also has `workflow_dispatch`, use that to simulate
   nightlies.
2. **Sync the fork's `main`** with upstream `main` so baselines are comparable.
3. **Secrets:** don't copy upstream's. `DOCKERHUB_USERNAME`/`DOCKERHUB_TOKEN`
   are only needed for Phase 3 image publishing — for fork testing, push the
   `ci-base` image to `ghcr.io/alvgaona/…` instead, which needs only the
   built-in `GITHUB_TOKEN` (`permissions: packages: write`). Codecov uploads
   from the fork are unnecessary; leave `CODECOV_TOKEN` unset and the step
   no-ops/fails soft (`fail_ci_if_error: false`).
4. **Replicate branch protection** on the fork's `main` (required checks:
   whatever upstream requires today) — otherwise the Phase 2
   required-check/skip interaction goes untested until upstream, where it
   would bite hardest.

### Measure the baseline first

Before changing anything, open one dummy PR against the fork's *unmodified*
`main` and let the current workflows run 2–3 times (re-run jobs to sample
variance). Record per-job wall-clock:

```bash
gh run list -R alvgaona/aerostack2 --limit 20 \
  --json displayTitle,workflowName,createdAt,updatedAt,conclusion
```

This matters because upstream's historical timings come from different cache
states and runner contention — the fork baseline is the only apples-to-apples
reference for the fork results.

### Per-phase verification in the fork

**Phase 1** — branch `ci/phase1` → PR **into the fork's own `main`** (base =
`alvgaona:main`, not upstream). `pull_request` triggers fire with identical
semantics to an upstream PR.

- Run twice: first run populates ccache (cold), re-run measures warm. Expect
  warm ~12–15 min.
- Push a trivial fixup commit → confirm the in-flight run cancels.
- Merge to fork `main`, trigger `codecov_test.yaml` via `workflow_dispatch` →
  confirm the build passes with the synced package list (upload step may
  no-op without a token; the build/test/lcov stages are what's being
  verified).
- Check cache hygiene: Actions → Caches shows `ccache-humble-*` ≤ 1.5 GB.

**Phase 2** — merge Phase 1 to fork `main` first (selection builds on it),
then run the four probe PRs from the Phase 2 verification section as fork-
internal PRs:

- Probe 1 (README-only) additionally verifies the branch-protection fan-in:
  with required checks replicated, the docs PR must show green, not eternally
  pending.
- Probe 4 (deliberate API break) confirms under-selection is caught.
- `build-platforms.yaml` clones platform repos from the public `aerostack2`
  org via raw URLs — works unmodified from a fork; trigger it with
  `workflow_dispatch` to simulate the nightly.
- Verify the escalation rule by checking the `changes` job's outputs in the
  run log for each probe.

**Phase 3** — build and push `ci-base` to `ghcr.io/alvgaona/aerostack2-ci`
from the fork; point the fork's workflows at it and run the side-by-side
comparison PRs there. The parallel-workers decision tree (3.2) runs entirely
on fork scratch branches — three repetitions at `parallel-workers: 4`, then
the DDS-isolation fixes if needed. Flaky-test findings here are themselves
upstreamable fixes, independent of the CI work.

### Promoting to upstream

- Upstream PRs should be **cherry-picks of the fork commits**, phase by phase
  — not a re-implementation — so what was tested is what ships. Attach the
  fork's before/after run links (they're public) as evidence in each PR
  description.
- Two things change at the boundary and need re-checking upstream:
  1. Image references flip from `ghcr.io/alvgaona/…` back to
     `aerostack2/ci-base-*` on Docker Hub (upstream has the Docker Hub
     secrets; the fork never did).
  2. Upstream branch protection must add/rename required checks in the same
     merge window as the Phase 2 PR (repo settings aren't in git — this is
     the one step that can't be cherry-picked).
- First post-merge upstream run is ccache-cold; expect one slow build per
  distro before steady state.

---

## Sequencing & risk

Order: Phase 1 → observe on real PRs (~3 days) → Phase 2 + probes → demotions
→ Phase 3 (base image and parallelism investigation can run concurrently) →
list dedup. Each phase merges independently and is independently revertible.

| Risk | Phase | Mitigation |
|---|---|---|
| ccache masks a real incremental-build bug (stale hits) | 1 | `main` full builds keep a correct reference; ccache hashes compiler+flags+content; 2-line revert |
| Change detection under-selects → broken `main` | 2 | dependents-expansion via colcon graph; escalation rules for roots; full build on every `main` push catches escapes at merge time |
| Required-check deadlock on skipped jobs | 2 | fan-in `ci-passed` job designed in from the start; verified with probe PR #1 in the fork |
| Platform/jazzy breakage discovered a day late | 2 | accepted tradeoff (decision #2); `paths` triggers cover causally-linked PRs; nightly issue routing makes failures loud |
| Stale CI image drifts from rosdep reality | 3 | rosdep still runs in-job (no-op when current); nightly rebuild; `workflow_dispatch` refresh |

## Open decisions

| # | Question | Blocks | Default if undecided |
|---|---|---|---|
| 1 | ~~History of `parallel-workers: 1`?~~ | — | Moot: tests measure 2m48s serial; not worth touching |
| 2 | ~~Jazzy + platforms non-blocking on PRs?~~ | — | **Decided:** both distros gate every PR (package-scoped, linux-64); only the 6-repo platforms matrix and the pixi arm/osx platforms move to nightly/merge |
| 3 | Where should nightly failures land? | 2.3 | Auto-issue, `ci-nightly` label, deduped |
| 4 | Releases: bloom mechanics, lockstep versioning, or changelogs? | — | Separate track, planned once CI lands |
