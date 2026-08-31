import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Flickable {
  id: page

  clip: true
  contentWidth: width
  contentHeight: content.implicitHeight + 48
  boundsBehavior: Flickable.StopAtBounds
  ScrollBar.vertical: ScrollBar { }

  ColumnLayout {
    id: content
    x: 24
    y: 24
    width: page.width - 48
    spacing: 18

    ColumnLayout {
      Layout.fillWidth: true
      spacing: 3

      Text {
        text: "재정"
        color: "#f1f4f6"
        font.pixelSize: 26
        font.weight: Font.DemiBold
      }

      Text {
        text: "내부 계산은 실제 금액으로 유지하고, 화면에서는 구단주가 판단할 핵심 영향만 먼저 보여줍니다"
        color: "#7f8d9e"
        font.pixelSize: 13
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.preferredHeight: 52
      radius: 8
      color: "#242b25"
      border.color: "#5f6442"

      Text {
        anchors.fill: parent
        anchors.margins: 13
        text: "주의 · 현재 금액은 저장·UI·예산 흐름 검증을 위한 임시 추정치입니다. 역사적 재정 데이터로 확정되지 않았습니다."
        color: "#d4d3ac"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
      }
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 4
      columnSpacing: 12

      StatCard { Layout.fillWidth: true; label: "현금"; value: game.cash; hint: "이적 가용 " + game.availableTransferCash; accent: "#4db58b" }
      StatCard { Layout.fillWidth: true; label: "이적 예산"; value: game.transferBudget; hint: "이사회 배정"; accent: "#63a7d8" }
      StatCard { Layout.fillWidth: true; label: "주간 주급"; value: game.weeklyWageCommitment; hint: game.wageUsagePercent + "% 사용"; accent: "#c7a35a" }
      StatCard { Layout.fillWidth: true; label: "부채"; value: game.debt; hint: "시드 데이터 기준"; accent: "#aa83d4" }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 14

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 235

        ColumnLayout {
          anchors.fill: parent
          spacing: 12

          RowLayout {
            Layout.fillWidth: true
            Text { text: "주급 예산"; color: "#f0f3f5"; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Text { text: game.wageUsagePercent + "%"; color: game.wageUsagePercent >= 95 ? "#e37872" : "#6fc6a4"; font.pixelSize: 16; font.weight: Font.DemiBold }
          }

          ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: Math.min(100, game.wageUsagePercent)
            background: Rectangle { color: "#0f171f"; radius: 5 }
            contentItem: Item {
              Rectangle {
                width: parent.width * (Math.min(100, game.wageUsagePercent) / 100)
                height: parent.height
                radius: 5
                color: game.wageUsagePercent >= 95 ? "#d46b65" : "#c7a35a"
              }
            }
          }

          RowLayout {
            Layout.fillWidth: true
            Text { text: "현재 약정"; color: "#7f8d9e"; font.pixelSize: 11; Layout.fillWidth: true }
            Text { text: game.weeklyWageCommitment; color: "#c5cdd6"; font.pixelSize: 12 }
          }

          RowLayout {
            Layout.fillWidth: true
            Text { text: "주간 한도"; color: "#7f8d9e"; font.pixelSize: 11; Layout.fillWidth: true }
            Text { text: game.weeklyWageBudget; color: "#c5cdd6"; font.pixelSize: 12 }
          }

          Item { Layout.fillHeight: true }

          Text {
            text: "주급 약정과 이적 지급 일정은 승인·합류·지급일에 맞춰 각각 반영됩니다."
            color: "#6f7d8e"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
          }
        }
      }

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 235

        ColumnLayout {
          anchors.fill: parent
          spacing: 11

          Text { text: "이번 MVP의 재정 범위"; color: "#f0f3f5"; font.pixelSize: 16; font.weight: Font.DemiBold }
          Text { text: "✓ 현금·이적·주급 예산 스냅샷"; color: "#9bcdb9"; font.pixelSize: 12 }
          Text { text: "✓ 역사 원본과 세이브 변화량 분리"; color: "#9bcdb9"; font.pixelSize: 12 }
          Text { text: "✓ 날짜 진행 후 자동 저장"; color: "#9bcdb9"; font.pixelSize: 12 }
          Text { text: "다음: 전체 거래 원장·월간 손익·스폰서·구단주 투자"; color: "#7f8d9e"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
      }
    }
  }
}
