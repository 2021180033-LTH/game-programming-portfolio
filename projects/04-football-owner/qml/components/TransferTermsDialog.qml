import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
  id: dialog

  property string actionKind: ""
  property string targetId: ""
  property string targetName: ""

  signal termsSubmitted(
      string actionKind,
      string targetId,
      var upfrontPounds,
      var installmentPounds,
      int installmentCount,
      var appearanceBonusPounds,
      int appearanceThreshold,
      int sellOnPercentage)

  modal: true
  dim: true
  parent: Overlay.overlay
  anchors.centerIn: parent
  width: 600
  title: targetName.length > 0 ? targetName + " · 이적 조건" : "이적 조건"
  standardButtons: Dialog.Ok | Dialog.Cancel
  closePolicy: Popup.CloseOnEscape

  function openFor(kind, idValue, nameValue, upfront, installments,
                   installmentCountValue, appearanceBonus,
                   appearanceThresholdValue, sellOnValue) {
    actionKind = kind
    targetId = idValue
    targetName = nameValue
    upfrontField.text = String(upfront)
    installmentField.text = String(installments)
    installmentCountBox.currentIndex = Math.max(
          0, Math.min(4, Number(installmentCountValue)))
    appearanceBonusField.text = String(appearanceBonus)
    appearanceThresholdField.text = String(appearanceThresholdValue)
    sellOnField.text = String(sellOnValue)
    open()
  }

  onAccepted: termsSubmitted(
      actionKind,
      targetId,
      Number(upfrontField.text),
      Number(installmentField.text),
      Number(installmentCountBox.currentText),
      Number(appearanceBonusField.text),
      Number(appearanceThresholdField.text),
      Number(sellOnField.text))

  background: Rectangle {
    color: "#171d24"
    border.color: "#435568"
    radius: 10
  }

  contentItem: ColumnLayout {
    spacing: 12

    Text {
      Layout.fillWidth: true
      text: "모든 금액은 파운드 단위입니다. 보장 이적료는 즉시 지급액과 할부 총액의 합이며, 출전 보너스는 조건이 달성될 때만 지급됩니다."
      color: "#95a3b2"
      font.pixelSize: 11
      wrapMode: Text.WordWrap
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 2
      columnSpacing: 14
      rowSpacing: 9

      Label { text: "즉시 지급액 (£)"; color: "#c5ced7" }
      TextField {
        id: upfrontField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "할부 총액 (£)"; color: "#c5ced7" }
      TextField {
        id: installmentField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "할부 횟수"; color: "#c5ced7" }
      ComboBox {
        id: installmentCountBox
        Layout.fillWidth: true
        model: ["0", "1", "2", "3", "4"]
      }

      Label { text: "출전 보너스 (£)"; color: "#c5ced7" }
      TextField {
        id: appearanceBonusField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 2000000000 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "보너스 출전 기준"; color: "#c5ced7" }
      TextField {
        id: appearanceThresholdField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 100 }
        inputMethodHints: Qt.ImhDigitsOnly
      }

      Label { text: "다음 이적 수익 (%)"; color: "#c5ced7" }
      TextField {
        id: sellOnField
        Layout.fillWidth: true
        validator: IntValidator { bottom: 0; top: 50 }
        inputMethodHints: Qt.ImhDigitsOnly
      }
    }

    Rectangle {
      Layout.fillWidth: true
      implicitHeight: 48
      radius: 6
      color: "#202934"

      Text {
        anchors.fill: parent
        anchors.margins: 9
        text: dialog.actionKind === "outgoing"
              ? "판매 수입 조건입니다. 할부 총액이 0이면 횟수도 0, 출전 보너스가 0이면 출전 기준도 0으로 입력해야 합니다."
              : "현재 이적 가용 현금 " + game.availableTransferCash
                + " · 즉시 지급·할부·조건부 보너스의 최대 지출 합계가 이 금액을 넘으면 제안할 수 없습니다. 할부 총액이 0이면 횟수도 0으로 입력해야 합니다."
        color: "#9aabb9"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
      }
    }
  }
}
