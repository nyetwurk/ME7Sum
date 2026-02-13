# Release Process

This document describes the GitHub Actions workflows and how releases are published.

## GitHub Workflows

Two workflows run on every push/PR to `master` and on every **published** release:

### Triggers

- **push** to `master` — builds run, artifacts uploaded to the Actions run (not to a release)
- **pull_request** to `master` — same as push
- **release: published** — builds run and artifacts are uploaded to the release

### Important: Release vs Tag

Artifacts are **not** created when you push a tag. They are created only when you **publish a release** on GitHub. Creating a tag alone does not trigger artifact uploads.

### Workflows

| Workflow | Runners | Output |
|----------|---------|--------|
| `windows-build.yml` | `windows-latest` | `me7sum.exe`, `ME7Check.exe`, `README.md` → `me7sum-<tag>-win.zip` |
| `unix-build.yml` | `ubuntu-latest`, `macos-latest` | `me7sum`, `ME7Check_linux`, `README.md` → `me7sum-<tag>-ubuntu.zip`, `me7sum-<tag>-macos.zip` |

On push/PR, artifacts are uploaded to the Actions run with short SHA in the name. On release publish, they are zipped and attached to the release via `gh release upload`.

Artifacts typically appear 5–10 minutes after publishing. Refresh the release page to see them.

---

## Release Notes with git-cliff

We use [git-cliff](https://git-cliff.org/) to generate release notes from the commit history. Configuration is in `cliff.toml`.

Generate changelog for the next release (from last tag to HEAD):

```bash
git cliff
```

Or for a specific range (e.g. v1.1.3..HEAD):

```bash
git cliff v1.1.3..HEAD
```

---

## Creating a Release

Releases are created manually via the GitHub web UI:

1. Run `git cliff` (or `git cliff <previous-tag>..HEAD`) and copy the output
2. Go to **Releases** → **New release**
3. Create or select the tag (e.g. `v1.1.4`)
4. Paste the git-cliff output into the release description
5. Click **Publish release**
6. Wait 5–10 minutes for the workflows to build and attach artifacts
7. Refresh the release page to verify the zip files are present
