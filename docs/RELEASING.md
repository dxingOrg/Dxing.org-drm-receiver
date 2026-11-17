# Making a release (for the maintainer)

1. Update `CHANGELOG.md` (move "unreleased" to the new version).
2. Commit, then tag: `git tag v3.3` and `git push --tags` (GitHub Desktop: Branch,
   Create tag).
3. Build the package as described in `BUILDING.md` (without `INCLUDE_FDK`).
4. On GitHub: **Releases, Draft a new release**, choose the tag, attach
   `Dxing-DRM-receiver-win64-<version>.zip`.
5. Release notes (copy the entry of the version from `CHANGELOG.md`) and always this
   paragraph:

   > The package does not contain `libfdk-aac-2.dll`. Stations with AAC/xHE-AAC sound
   > need it: see the section "AAC DECODER" in `README.txt` in the package.

6. Binaries (`.exe`, `.dll`, `.zip`) belong **only** in Releases, never in the
   repository.
