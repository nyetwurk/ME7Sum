# Release Process

This document describes the GitHub Actions workflows and how releases are published.

## GitHub Workflows

Three workflows run on every push/PR to **any branch** and on every **tag push** a release is created and builds run:

### Triggers

- **push** to any branch — builds run, artifacts uploaded to the Actions run (not to a release)
- **pull_request** to any branch — same as push
- **push** tags `v*` — create-release creates a release (prerelease if tag contains `-rc`), then `release: published` fires and builds upload artifacts

### Tag Rules

- **v1.1.1** (no suffix) → full release. Changelog: commits since last full release (RCs ignored).
- **v1.1.1-rc1** (contains `-rc`) → prerelease. Changelog: commits since last full or RC release.

### Workflows

| Workflow | Runners | Output |
|----------|---------|--------|
| `create-release.yml` | `ubuntu-latest` | Runs on tag push: creates release with git-cliff notes; prerelease if tag matches `*-rc*` |
| `windows-build.yml` | `windows-latest` | `me7sum.exe`, `ME7Check.exe`, `README.md` → `me7sum-<tag>-win.zip` |
| `unix-build.yml` | `ubuntu-latest`, `macos-latest` | `me7sum`, `ME7Check_linux`, `README.md` → `me7sum-<tag>-ubuntu.zip`, `me7sum-<tag>-macos.zip` |

On push/PR, artifacts are uploaded to the Actions run with short SHA in the name. On tag push, create-release creates the release; `release: published` then triggers the build workflows, which zip and attach artifacts via `gh release upload`.

Artifacts typically appear 5–10 minutes after pushing the tag. Refresh the release page to see them.

---

## Release Notes with git-cliff

We use [git-cliff](https://git-cliff.org/) to generate release notes from the commit history. Configuration is in `cliff.toml`.

- **Conventional commits**: Commits are grouped by type (Features, Bug Fixes, Documentation, etc.); see `commit_parsers` in `cliff.toml`.
- **Tag pattern**: Stable tags `vX.Y.Z` and RC tags `vX.Y.Z-rcN` are matched.
- **CI**: On tag push, `create-release.yml` runs git-cliff and creates the release. Full releases use `--ignore-tags` for RC tags so the changelog covers commits since the last full release; RC releases include RCs so the changelog covers commits since the last full or RC release.

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
2. Wait 5–10 minutes for create-release to create the release, then build workflows to attach artifacts
3. Refresh the release page to verify the zip files and changelog are present

Example:

```bash
git tag v1.1.4
git push origin v1.1.4
```
