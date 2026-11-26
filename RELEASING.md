# Development, Testing, and Releases

This guide covers local development, testing published builds, and creating GitHub releases with Actions.

## Development and published builds

- The working source repository is `C:\asciiwall`. The development executable is `C:\asciiwall\dist\asciiwall.exe`.
- To rebuild and run from the project directory in w64devkit, use `make run`.
- Exit AsciiWall from its tray menu before rebuilding. Unchecking **Enable animated wallpaper** only hides the scene; it leaves the app running and can lock the executable.
- Development and installed builds use the same single-instance lock and `%APPDATA%\AsciiWall\settings.ini`. They cannot run independently under the same Windows account, and their settings are shared.
- For isolated release testing, use Windows Sandbox, a VM, or a separate Windows account. Avoid installing a test build over your regular per-user installation if you want to keep it unchanged.

The **Defaults** button restores scene controls but leaves both Screen checkboxes unchanged: wallpaper enabled and Windows startup retain their current states.

## Build and test locally

Building requires Windows x64 and w64devkit. Building the installer also requires Inno Setup 6. Run commands from `C:\asciiwall` in a w64devkit terminal:

```text
make
make package installer VERSION=0.1.4
```

Replace `0.1.4` with the version you are preparing. The same value is shown in the Settings heading and used for the installer and package filenames.

The outputs are:

- `build\AsciiWall-<version>-windows-x64.zip`
- `build\AsciiWall-<version>-windows-x64.zip.sha256`
- `build\installer\AsciiWall-Setup-<version>.exe`
- `build\installer\AsciiWall-Setup-<version>.exe.sha256`

Test the ZIP or installer before publishing. Use a clean Windows account, Sandbox, or VM to verify first-run defaults. The installer can be removed from Windows **Settings → Apps → Installed apps**; it preserves `%APPDATA%\AsciiWall`. ZIP users remove the extracted folder themselves.

## Commits and version tags

You can make and push several commits to `main` without publishing a release. A release is built from the exact commit named by its version tag. Therefore, commit and push every change intended for that release before pushing its tag.
The workflow file must be committed and pushed to `main` before the first release tag is pushed.

For a release, replace `0.1.4` below with the next unused version:

```powershell
git status
git add <the-files-you-reviewed>
git diff --cached --check
git diff --cached
git commit -m "Prepare v0.1.4 release"
git push origin main
git tag v0.1.4
git push origin v0.1.4
```

For `git add`, replace the placeholder with the specific paths you reviewed; do not type angle brackets literally.
The tag is the release version source for GitHub Actions: `v0.1.4` becomes `0.1.4` for the build. Pushing to `main` alone does not run the release workflow. All commits reachable from the tagged commit are included; later commits are not.

Do not move or reuse a tag after its release has been published. Use a new version instead.

As of 2026-11-26, `v0.1.4` is the current release target. Confirm it is the next unused version before tagging.

## What the release workflow does

The workflow is `.github/workflows/release.yml`. It runs when a tag matching `vX.Y.Z` is pushed:

1. Checks out the tagged source on a Windows runner.
2. Reads and validates the version from the tag.
3. Installs GCC and Make through MSYS2 and Inno Setup.
4. Runs `make package installer` with the tag's version.
5. Creates the GitHub Release, generates release notes, and attaches the ZIP, installer, and both checksums.

The workflow uses the repository's built-in `GITHUB_TOKEN` with `contents: write`; no personal access token is required. After pushing a tag, follow the run under the repository's **Actions** tab. When it succeeds, find the release under **Releases**. Do not create the release manually; the workflow creates it.

If an Actions run fails, open the run and inspect the failed step's log. Rerun it for a temporary failure. If source code needs fixing, commit and push the fix first. A tag still points to its original commit.

Only if the failed tag has **no GitHub Release**, you may retarget it to the fixed commit:

```powershell
git fetch --tags origin
git tag -d v0.1.4
git push origin --delete v0.1.4
git tag v0.1.4
git push origin v0.1.4
```

Replace the example with the unpublished failed version. Never retarget a published release tag.

## What users download

Users choose one release asset:

- The **Setup** `.exe` for the easiest installation.
- The **ZIP** for extraction and use without installation.

The `.sha256` files are optional integrity checks. GitHub's automatic **Source code** archives are not needed to run AsciiWall.

Windows SmartScreen may warn that the publisher is unknown because the installer is not code-signed. Publishing a new version does not remove that warning; code signing and publisher reputation are separate from the release workflow.

