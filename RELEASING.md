# Development, Testing, and Releases

This guide covers local development, testing published builds, and creating GitHub releases with Actions.

## Development and published builds

- The working source repository is `C:\asciiwall`. The development executable is `C:\asciiwall\dist\asciiwall.exe`.
- To rebuild and run from the project directory in w64devkit, use `make run`. It requests a graceful exit from the current AsciiWall instance before rebuilding; if the app remains open, close it from the tray menu.
- Unchecking **Enable animated wallpaper** only hides the scene; it leaves the app running and can lock the executable.
- Development and installed builds use the same single-instance lock and `%APPDATA%\AsciiWall\settings.ini`. They cannot run independently under the same Windows account, and their settings are shared.
- For isolated release testing, use Windows Sandbox, a VM, or a separate Windows account. Avoid installing a test build over your regular per-user installation if you want to keep it unchanged.

The **Defaults** button restores scene controls but leaves both Screen checkboxes unchanged: wallpaper enabled and Windows startup retain their current states.

## Build and test locally

Building requires Windows x64 and w64devkit. Building the installer also requires Inno Setup 6. Run commands from `C:\asciiwall` in a w64devkit terminal:

```text
make
make package installer
```

The root `VERSION` file is the single source for the Settings version and release filenames. Update it once when preparing a new version.

The outputs are:

- `build\AsciiWall-<version>-windows-x64.zip`
- `build\AsciiWall-<version>-windows-x64.zip.sha256`
- `build\installer\AsciiWall-Setup-<version>.exe`
- `build\installer\AsciiWall-Setup-<version>.exe.sha256`

Test the ZIP or installer before publishing. Use a clean Windows account, Sandbox, or VM to verify first-run defaults. The installer can be removed from Windows **Settings → Apps → Installed apps**; it preserves `%APPDATA%\AsciiWall`. ZIP users remove the extracted folder themselves.

## Commits and version tags

You can make and push several commits to `main` without publishing a release. A release is built from the exact commit named by its version tag. Therefore, commit and push every change intended for that release before pushing its tag.
The workflow file must be committed and pushed to `main` before the first release tag is pushed.

For a release, the tag must match the value in `VERSION`:

```powershell
$version = (Get-Content -LiteralPath .\VERSION -Raw).Trim()
$tag = "v$version"
git status
git add <the-files-you-reviewed>
git diff --cached --check
git diff --cached
git commit -m "Prepare $tag release"
git push origin main
git tag $tag
git push origin $tag
```

For `git add`, replace the placeholder with the specific paths you reviewed; do not type angle brackets literally.
The release workflow verifies the tag matches `VERSION` and uses that value for the build. Pushing to `main` alone does not run the release workflow. All commits reachable from the tagged commit are included; later commits are not.

Do not move or reuse a tag after its release has been published. Use a new version instead.

Confirm the value in `VERSION` is the next unused version before tagging.

## What the release workflow does

The workflow is `.github/workflows/release.yml`. It runs when a tag matching `vX.Y.Z` is pushed:

1. Checks out the tagged source on a Windows runner.
2. Reads the canonical version from `VERSION` and verifies the tag matches it.
3. Installs GCC and Make through MSYS2 and Inno Setup.
4. Runs `make package installer` with the tag's version.
5. Creates the GitHub Release, generates release notes, and attaches the ZIP, installer, and both checksums.

The workflow uses the repository's built-in `GITHUB_TOKEN` with `contents: write`; no personal access token is required. After pushing a tag, follow the run under the repository's **Actions** tab. When it succeeds, find the release under **Releases**. Do not create the release manually; the workflow creates it.

If an Actions run fails, open the run and inspect the failed step's log. Rerun it for a temporary failure. If source code needs fixing, commit and push the fix first. A tag still points to its original commit.

Only if the failed tag has **no GitHub Release**, you may retarget it to the fixed commit:

```powershell
$tag = Read-Host "Enter the unpublished failed version tag"
git fetch --tags origin
git tag -d $tag
git push origin --delete $tag
git tag $tag
git push origin $tag
```

Replace the example with the unpublished failed version. Never retarget a published release tag.

## What users download

Users choose one release asset:

- The **Setup** `.exe` for the easiest installation.
- The **ZIP** for extraction and use without installation.

The `.sha256` files are optional integrity checks. GitHub's automatic **Source code** archives are not needed to run AsciiWall.

Windows SmartScreen may warn that the publisher is unknown because the installer is not code-signed. Publishing a new version does not remove that warning; code signing and publisher reputation are separate from the release workflow.

