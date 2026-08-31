import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
  id: dialog

  property string pursuitId: ""
  property string playerName: ""

  signal termsSubmitted(
      string pursuitId,
      var weeklyWagePounds,
      int contractYears,
      var signingBonusPounds,
      var loyaltyBonusPounds,
      var appearanceFeePounds,
      var goalBonusPounds,
      var agentFeePounds,
      var releaseClausePounds,
      string squadRole)

  modal: true
  dim: true
  parent: Overlay.overlay
  anchors.centerIn: parent
  width: 640
  title: playerName.length > 0 ? playerName + " · 선수 계약 조건" : "선수 계약 조건"
  standardButtons: Dialog.Ok | Dialog.Cancel
  closePolicy: Popup.CloseOnEscape

  function roleIndex(role) {
    for (let index = 0; index < roleBox.model.length; ++index) {
      if (roleBox.model[index] === role)
        return index
    }
    return 1
  }

  function openFor(idValue, nameValue, wage, years, signing, loyalty,
                   appearance, goal, agent, releaseClause, role) {
    pursuitId = idValue
    playerName = nameValue
    wageField.text = String(wage)
    yearsBox.currentIndex = Math.max(0, Math.min(4, Number(years) - 1))
    signingField.text = String(signing)
    loyaltyField.text = String(loyalty)
    appearanceField.text = String(appearance)
    goalField.text = String(goal)
    agentField.text = String(agent)
    releaseField.text = String(releaseClause)
    roleBox.currentIndex = roleIndex(role)
    open()
  }

  onAccepted: termsSubmitted(
      pursuitId,
      Number(wageField.text),
      Number(yearsBox.currentText),
      Number(signingField.text),
      Number(loyaltyField.text),
      Number(appearanceField.text),
      Number(goalField.text),
      Number(agentField.text),
      Number(releaseField.text),
      roleBox.currentText)

  background: Rectangle {
    color: "#171d24"
    border.color: "#435568"
    radius: 10
  }

  contentItem: ColumnLayout {
    spacing: 12

    Text {
      Layout.fillWidth: true
      text: "모든 금액은 파운드 단위입니다. 계약금·로열티 보너스·에이전트 수수료도 이적 최대 지출에 포함되며 전체 약정은 현재 가용 현금을 넘을 수 없습니다. 바이아웃은 지급 비용이 아니라 타 구단이 계약을 해지할 수 있는 기준액입니다."
      color: "#95a3b2"
      font.pixelSize: 11
      wrapMode: Text.WordWrap
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 4
      columnSpacing: 12
      rowSpacing: 9

      Label { text: "주급 (£)"; color: "#c5ced7" }
      TextField {
        id: wageField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 1; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "계약 기간"; color: "#c5ced7" }
      ComboBox {
        id: yearsBox
        Layout.fillWidth: true
        model: ["1", "2", "3", "4", "5"]
      }

      Label { text: "계약금 (£)"; color: "#c5ced7" }
      TextField {
        id: signingField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "로열티 보너스 (£)"; color: "#c5ced7" }
      TextField {
        id: loyaltyField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "경기 출전 수당 (£)"; color: "#c5ced7" }
      TextField {
        id: appearanceField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "득점 보너스 (£)"; color: "#c5ced7" }
      TextField {
        id: goalField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "에이전트 수수료 (£)"; color: "#c5ced7" }
      TextField {
        id: agentField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "바이아웃 (£, 0=없음)"; color: "#c5ced7" }
      TextField {
        id: releaseField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "선수단 지위"; color: "#c5ced7" }
      ComboBox {
        id: roleBox
        Layout.columnSpan: 3
        Layout.fillWidth: true
        model: ["Key Player", "First Team", "Rotation", "Backup", "Prospect"]
      }
    }
  }
}
