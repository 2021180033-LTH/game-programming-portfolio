import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: shell

  property int currentPage: 0

  onVisibleChanged: {
    if (visible)
      currentPage = 0
  }

  RowLayout {
    anchors.fill: parent
    spacing: 0

    Rectangle {
      Layout.preferredWidth: 236
      Layout.fillHeight: true
      color: "#0d141c"
      border.color: "#202b37"

      ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
          Layout.fillWidth: true
          Layout.preferredHeight: 74
          spacing: 12

          Rectangle {
            Layout.preferredWidth: 46
            Layout.preferredHeight: 46
            radius: 23
            color: "#9d2430"
            border.color: "#d3b56d"

            Text {
              anchors.centerIn: parent
              text: "MU"
              color: "white"
              font.pixelSize: 14
              font.weight: Font.Bold
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
              text: game.clubShortName
              color: "#f1f4f6"
              font.pixelSize: 16
              font.weight: Font.DemiBold
              elide: Text.ElideRight
              Layout.fillWidth: true
            }

            Text {
              text: "구단주 · " + game.ownerName
              color: "#718093"
              font.pixelSize: 11
              elide: Text.ElideRight
              Layout.fillWidth: true
            }
          }
        }

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 1
          color: "#202b37"
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "⌂"
          label: "구단주 홈"
          active: shell.currentPage === 0
          onClicked: shell.currentPage = 0
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "✉"
          label: "받은 편지함"
          badgeText: game.unreadCount > 0 ? String(game.unreadCount) : ""
          active: shell.currentPage === 1
          onClicked: shell.currentPage = 1
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "XI"
          label: "선수단"
          active: shell.currentPage === 2
          onClicked: shell.currentPage = 2
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "↗"
          label: "이적 시장"
          badgeText: game.decisionCount > 0 ? String(game.decisionCount) : ""
          active: shell.currentPage === 3
          onClicked: shell.currentPage = 3
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "◎"
          label: "스카우팅"
          badgeText: game.activeScoutingAssignmentCount > 0
                     ? String(game.activeScoutingAssignmentCount) : ""
          active: shell.currentPage === 4
          onClicked: shell.currentPage = 4
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "⇄"
          label: "계약·임대"
          badgeText: game.pendingLoanDecisionCount > 0
                     ? String(game.pendingLoanDecisionCount) : ""
          active: shell.currentPage === 5
          onClicked: shell.currentPage = 5
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "M"
          label: "감독"
          active: shell.currentPage === 6
          onClicked: shell.currentPage = 6
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "£"
          label: "재정"
          active: shell.currentPage === 7
          onClicked: shell.currentPage = 7
        }

        NavigationButton {
          Layout.fillWidth: true
          symbol: "⚽"
          label: "경기"
          active: shell.currentPage === 8
          onClicked: shell.currentPage = 8
        }

        Item { Layout.fillHeight: true }

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 88
          radius: 8
          color: "#131d27"
          border.color: "#202d3a"

          Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: game.saveName
              width: 180
              color: "#a6b1bd"
              font.pixelSize: 11
              font.weight: Font.Medium
              elide: Text.ElideRight
              horizontalAlignment: Text.AlignHCenter
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: game.playTime + " · MVP 0.7.0"
              color: "#6f7d8e"
              font.pixelSize: 10
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "역사 기반 모드"
              color: "#4db58b"
              font.pixelSize: 10
            }
          }
        }
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 72
        color: "#141d27"
        border.color: "#25313e"

        RowLayout {
          anchors.fill: parent
          anchors.leftMargin: 24
          anchors.rightMargin: 24
          spacing: 12

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
              text: game.currentDate
              color: "#f0f4f6"
              font.pixelSize: 18
              font.weight: Font.DemiBold
            }

            Text {
              text: game.leagueName + " · 프리시즌 · " + game.saveName
              color: "#7f8d9e"
              font.pixelSize: 11
              elide: Text.ElideRight
              Layout.maximumWidth: 520
            }
          }

          Text {
            visible: game.statusMessage.length > 0
            text: game.statusMessage
            color: "#7f8d9e"
            font.pixelSize: 11
            Layout.maximumWidth: 280
            elide: Text.ElideRight
          }

          Button {
            id: saveButton
            text: "저장"
            onClicked: game.saveGame()
            background: Rectangle {
              color: saveButton.hovered ? "#263544" : "#1d2935"
              radius: 7
              border.color: "#354657"
            }
            contentItem: Text {
              text: saveButton.text
              color: "#c8d0da"
              font.pixelSize: 13
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          Button {
            id: menuButton
            text: "메뉴"
            onClicked: gameMenu.open()
            background: Rectangle {
              color: menuButton.hovered ? "#263544" : "#1d2935"
              radius: 7
              border.color: "#354657"
            }
            contentItem: Text {
              text: menuButton.text
              color: "#c8d0da"
              font.pixelSize: 13
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          Button {
            id: oneDayButton
            text: "+1일"
            onClicked: game.advanceDays(1)
            background: Rectangle {
              color: oneDayButton.hovered ? "#256b59" : "#1f594b"
              radius: 7
              border.color: "#347866"
            }
            contentItem: Text {
              text: oneDayButton.text
              color: "#edf7f3"
              font.pixelSize: 13
              font.weight: Font.DemiBold
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          Button {
            id: nextEventButton
            text: "다음 일정 ▶"
            onClicked: game.advanceToNextEvent()
            background: Rectangle {
              color: nextEventButton.hovered ? "#2c7a65" : "#256854"
              radius: 7
              border.color: "#3a8b73"
            }
            contentItem: Text {
              text: nextEventButton.text
              color: "white"
              font.pixelSize: 13
              font.weight: Font.DemiBold
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }
        }
      }

      Rectangle {
        visible: game.errorMessage.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 44 : 0
        color: "#5b2b2c"

        RowLayout {
          anchors.fill: parent
          anchors.leftMargin: 20
          anchors.rightMargin: 12

          Text {
            Layout.fillWidth: true
            text: "처리 오류 · " + game.errorMessage
            color: "#ffd9d6"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
          }

          Button {
            text: "닫기"
            flat: true
            onClicked: game.clearError()
          }
        }
      }

      StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: shell.currentPage

        DashboardPage { }
        InboxPage { }
        SquadPage { }
        TransfersPage { }
        ScoutingPage { }
        ContractsPage { }
        ManagerPage { }
        FinancePage { }
        MatchesPage { }
      }
    }
  }

  Dialog {
    id: gameMenu
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 420
    title: "게임 메뉴"

    contentItem: ColumnLayout {
      spacing: 10

      Text {
        Layout.fillWidth: true
        text: game.saveName
        color: "#9eabb7"
        font.pixelSize: 12
        elide: Text.ElideRight
      }

      Button {
        Layout.fillWidth: true
        text: "저장"
        onClicked: {
          if (game.saveGame())
            gameMenu.close()
        }
      }

      Button {
        Layout.fillWidth: true
        text: "다른 이름으로 저장"
        onClicked: {
          saveAsField.text = game.saveName
          gameMenu.close()
          saveAsDialog.open()
        }
      }

      Button {
        Layout.fillWidth: true
        text: "메인 메뉴로"
        onClicked: {
          gameMenu.close()
          mainMenuConfirm.open()
        }
      }

      Button {
        Layout.fillWidth: true
        text: "게임 종료"
        onClicked: {
          gameMenu.close()
          exitConfirm.open()
        }
      }
    }
  }

  Dialog {
    id: saveAsDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 470
    title: "다른 이름으로 저장"

    contentItem: ColumnLayout {
      spacing: 12

      Text {
        Layout.fillWidth: true
        text: "현재 진행을 별도 커리어로 복제합니다. 승인·거절 분기를 각각 보존할 수 있습니다."
        color: "#7f8d9e"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      TextField {
        id: saveAsField
        Layout.fillWidth: true
        maximumLength: 80
        selectByMouse: true
      }

      Text {
        visible: game.saveNameExists(saveAsField.text)
        text: "같은 이름이 있습니다. 저장을 누르면 덮어쓰기 확인이 표시됩니다."
        color: "#d2ab61"
        font.pixelSize: 11
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: saveAsDialog.close()
        }
        Button {
          text: "저장"
          enabled: saveAsField.text.trim().length > 0
          onClicked: {
            if (game.saveNameExists(saveAsField.text)) {
              overwriteConfirm.pendingName = saveAsField.text.trim()
              saveAsDialog.close()
              overwriteConfirm.open()
            } else if (game.saveGameAs(saveAsField.text, false)) {
              saveAsDialog.close()
            }
          }
        }
      }
    }
  }

  Dialog {
    id: overwriteConfirm
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 450
    title: "기존 세이브 덮어쓰기"
    property string pendingName: ""

    contentItem: ColumnLayout {
      spacing: 14

      Text {
        Layout.fillWidth: true
        text: "‘" + overwriteConfirm.pendingName
              + "’에 저장된 진행을 현재 커리어 상태로 교체할까요?"
        color: "#c8d0d8"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      Text {
        text: "해당 세이브의 이전 자동 저장도 정리됩니다."
        color: "#d46b65"
        font.pixelSize: 11
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: overwriteConfirm.close()
        }
        Button {
          text: "덮어쓰기"
          onClicked: {
            if (game.saveGameAs(overwriteConfirm.pendingName, true))
              overwriteConfirm.close()
          }
        }
      }
    }
  }

  Dialog {
    id: mainMenuConfirm
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 430
    title: "메인 메뉴로"

    contentItem: ColumnLayout {
      spacing: 14

      Text {
        Layout.fillWidth: true
        text: "현재 커리어를 저장한 뒤 시작 화면으로 돌아갑니다."
        color: "#c8d0d8"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: mainMenuConfirm.close()
        }
        Button {
          text: "저장하고 돌아가기"
          onClicked: {
            if (game.returnToMainMenu())
              mainMenuConfirm.close()
          }
        }
      }
    }
  }

  Dialog {
    id: exitConfirm
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 430
    title: "게임 종료"

    contentItem: ColumnLayout {
      spacing: 14

      Text {
        Layout.fillWidth: true
        text: "현재 커리어를 저장하고 Football Owner를 종료할까요?"
        color: "#c8d0d8"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: exitConfirm.close()
        }
        Button {
          text: "저장하고 종료"
          onClicked: game.exitGame()
        }
      }
    }
  }
}
