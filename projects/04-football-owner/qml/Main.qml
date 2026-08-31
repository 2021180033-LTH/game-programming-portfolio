import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "pages"

ApplicationWindow {
  id: window

  property bool allowClose: false

  width: 1440
  height: 900
  minimumWidth: 1120
  minimumHeight: 700
  visible: true
  title: game.careerLoaded
         ? "Football Owner · " + game.saveName + " · MVP 0.7.0"
         : "Football Owner · MVP 0.7.0"
  color: "#101720"

  StackLayout {
    anchors.fill: parent
    currentIndex: game.careerLoaded ? 1 : 0

    StartPage { }
    GameShell { }
  }

  Connections {
    target: game

    function onQuitRequested() {
      window.allowClose = true
    }
  }

  onClosing: function(close) {
    if (!allowClose && game.careerLoaded) {
      close.accepted = false
      game.exitGame()
    }
  }
}
