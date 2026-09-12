# Packaging

NanoX ships as a single `nanox` binary plus docs and one example. Every
release produces archives with CPack:

| Platform | Artifact | Built by |
|----------|----------|----------|
| Linux    | `nanox-<version>-Linux.tar.gz`, `nanox_<version>_amd64.deb` | `ubuntu-latest` CI |
| macOS    | `nanox-<version>-Darwin.tar.gz`, `-Darwin.zip` | `macos-latest` CI |
| Windows  | `nanox-<version>-win64.zip`, `-win64.tar.gz` | `windows-latest` CI |

Every archive keeps the install tree at its root:

```
bin/nanox[.exe]
share/doc/nanox/README.md
share/doc/nanox/LICENSE
share/nanox/examples/hello.nx
```

## Building packages locally

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack --config build/CPackConfig.cmake -B dist
```

The DEB generator only runs on Linux (it needs the dpkg tooling). To install
the deb directly:

```sh
sudo apt install ./dist/nanox_0.1.0_amd64.deb
```

## Package-manager manifests

Templates live next to this file. The `url`/`sha256` placeholders (`<sha256>`)
are filled from the artifacts of the matching GitHub release — the
`Release` workflow (`.github/workflows/release.yml`) builds and attaches them
on every `v*` tag.

- **Homebrew** — `homebrew/nanox.rb`
  `shasum -a 256 nanox-0.1.0-Darwin.tar.gz`, then publish the formula in a tap
  (e.g. `sunZshanY/homebrew-nanox`).
- **Scoop** — `scoop/nanox.json`
  `Get-FileHash nanox-0.1.0-win64.zip`, then add the manifest to a bucket.
- **AUR** — `aur/PKGBUILD`
  Builds from the tagged source tree; run `updpkgsums` after bumping
  `pkgver` and submit with `git push` to `ssh://aur@aur.archlinux.org/nanox.git`.

## Notes

- The version comes from `project(NanoX VERSION ...)` in the top-level
  `CMakeLists.txt`; bump it once per release.
- The license is GPL-2.0 (see `LICENSE`), which the manifests declare as
  `GPL-2.0-only` (SPDX) or `GPL2` (AUR).
