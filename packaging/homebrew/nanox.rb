# Homebrew formula for NanoX (macOS).
#
# This file is a template: after a release, update `url` and `sha256` from the
# attached artifacts (see ../README.md).
#
#   shasum -a 256 nanox-0.1.0-Darwin.tar.gz
#
# Then publish it in a tap, e.g. sunZshanY/homebrew-nanox, or submit it to
# homebrew-core once NanoX has tagged stable releases.
class Nanox < Formula
  desc "NanoX programming language: lexer, project tools and IDE-style TUI"
  homepage "https://github.com/sunZshanY/nanox"
  url "https://github.com/sunZshanY/nanox/releases/download/v0.1.0/nanox-0.1.0-Darwin.tar.gz"
  sha256 "<sha256>"
  license "GPL-2.0-only"

  def install
    bin.install "bin/nanox"
    doc.install "share/doc/nanox/README.md"
    (share/"nanox/examples").install "share/nanox/examples/hello.nx"
  end

  test do
    assert_match "usage: nanox", shell_output("#{bin}/nanox --help")
  end
end
