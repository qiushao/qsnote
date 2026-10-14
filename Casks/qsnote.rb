cask "qsnote" do
  version :latest
  sha256 :no_check

  url "https://github.com/qiushao/qsnote/releases/latest/download/qsnote-macos-x86_64.dmg"
  name "QSNote"
  desc "Markdown notebook with local diagram and math rendering"
  homepage "https://github.com/qiushao/qsnote"

  depends_on formula: "graphviz"
  depends_on formula: "plantuml"
  depends_on arch: :x86_64
  depends_on macos: :monterey

  app "qsnote.app"
end
