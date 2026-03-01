# Release Process

This document describes the GitHub Actions workflows and how releases are published.

## GitHub Workflows

Three workflows run on every push/PR to **any branch**; on tag push, a single workflow builds and creates the release:

### Triggers

- **push** to any branch — builds run, artifacts uploaded to the Actions run (not to a release)
- **pull_request** to any branch — same as push
- **push** tags `v*` — `release.yml` builds on ubuntu/macos/windows, generates changelog, creates release with all artifacts

### Tag Rules

- **v1.1.1** (no suffix) → full release. Changelog: commits since last full release (RCs ignored).
- **v1.1.1-rc1** (contains `-rc`) → prerelease. Changelog: commits since last full or RC release.

### Workflows

| Workflow | Runners | Output |
|----------|---------|--------|
| `release.yml` | ubuntu, macos, windows | On tag push: builds all platforms, git-cliff notes, creates release with `me7sum-<tag>-ubuntu.zip`, `me7sum-<tag>-macos.zip`, `me7sum-<tag>-win.zip` |
| `windows-build.yml` | `windows-latest` | Push/PR: `me7sum.exe`, `ME7Check.exe`, `README.md` → artifact |
| `unix-build.yml` | `ubuntu-latest`, `macos-latest` | Push/PR: `me7sum`, `ME7Check_linux`, `README.md` → artifact |

On push/PR, artifacts are uploaded to the Actions run with short SHA in the name. On tag push, `release.yml` does builds and release creation in one run (no separate release trigger needed).

Artifacts typically appear 5–10 minutes after pushing the tag. Refresh the release page to see them.

---

## Release Notes with git-cliff

We use [git-cliff](https://git-cliff.org/) to generate release notes from the commit history. Configuration is in `cliff.toml`.

- **Conventional commits**: Commits are grouped by type (Features, Bug Fixes, Documentation, etc.); see `commit_parsers` in `cliff.toml`.
- **Tag pattern**: Stable tags `vX.Y.Z` and RC tags `vX.Y.Z-rcN` are matched.
- **CI**: On tag push, `release.yml` runs git-cliff and creates the release. Full releases use `--ignore-tags` for RC tags so the changelog covers commits since the last full release; RC releases include RCs so the changelog covers commits since the last full or RC release.

Generate changelog locally for the next release (from last tag to HEAD):

```bash
git cliff
```

For the latest tag only (same as CI):

```bash
git cliff --latest --strip header
```

For a specific range (e.g. v1.1.3..HEAD):

```bash
git cliff v1.1.3..HEAD
```

---

## Creating a Release

Releases are created automatically when you push a tag:

1. Create and push a tag (e.g. `v1.1.4` for full release, `v1.1.4-rc1` for prerelease)
2. Wait 5–10 minutes for `release.yml` to build and create the release
3. Refresh the release page to verify the zip files and changelog are present

Example:

```bash
git tag v1.1.4
git push origin v1.1.4
```
