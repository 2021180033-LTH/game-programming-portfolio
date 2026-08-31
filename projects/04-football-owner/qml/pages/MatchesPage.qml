import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: page

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 24
    spacing: 16

    RowLayout {
      Layout.fillWidth: true

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 3

        Text {
          text: "경기"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: "결과형 엔진 1단계 · 관람 방식이 달라도 동일한 결과를 사용합니다"
          color: "#7f8d9e"
          font.pixelSize: 13
        }
      }

      Button {
        id: matchesAdvanceButton
        text: "다음 일정까지 진행"
        onClicked: game.advanceToNextEvent()
        background: Rectangle {
          color: matchesAdvanceButton.hovered ? "#2c7a65" : "#256854"
          radius: 7
          border.color: "#3a8b73"
        }
        contentItem: Text {
          text: matchesAdvanceButton.text
          color: "white"
          font.pixelSize: 12
          font.weight: Font.DemiBold
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 14

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 190

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          Text { text: "다음 경기"; color: "#8a97a7"; font.pixelSize: 11 }
          Text { text: game.nextFixtureTitle; color: "#f2f5f7"; font.pixelSize: 22; font.weight: Font.DemiBold }
          Text { text: game.nextFixtureDate; color: "#b0bac5"; font.pixelSize: 13 }
          Text { text: game.nextFixtureStatus; color: "#718093"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
      }

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 190

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          Text { text: "최근 결과"; color: "#8a97a7"; font.pixelSize: 11 }
          Text { text: game.recentResult; color: "#f2f5f7"; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
          Text { text: "득점은 팀 전력과 홈 이점을 사용한 시드 기반 확률 모델로 생성됩니다."; color: "#718093"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.fillHeight: true
      padding: 0

      ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 42
          color: "#101821"

          RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Text { text: "날짜"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 100 }
            Text { text: "대회"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 150 }
            Text { text: "홈"; color: "#7e8b9a"; font.pixelSize: 10; Layout.fillWidth: true }
            Text { text: "결과"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignHCenter }
            Text { text: "원정"; color: "#7e8b9a"; font.pixelSize: 10; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
          }
        }

        ListView {
          id: fixtureList
          Layout.fillWidth: true
          Layout.fillHeight: true
          model: game.fixtures
          clip: true
          boundsBehavior: Flickable.StopAtBounds

          delegate: Rectangle {
            required property var modelData
            required property int index
            property var rowData: modelData

            width: fixtureList.width
            height: 52
            color: index % 2 === 0 ? "#18222d" : "#151f29"

            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: 16
              anchors.rightMargin: 16
              Text { text: rowData.date; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 100 }
              Text { text: rowData.competition; color: "#7f8d9e"; font.pixelSize: 10; Layout.preferredWidth: 150 }
              Text { text: rowData.home; color: "#e9edef"; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
              Text { text: rowData.score; color: rowData.played ? "#f0f3f5" : "#6f7d8e"; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.away; color: "#e9edef"; font.pixelSize: 12; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
            }
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: 250
      padding: 0

      ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 42
          color: "#101821"

          RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Text { text: "순위"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 42 }
            Text { text: "팀"; color: "#7e8b9a"; font.pixelSize: 10; Layout.fillWidth: true }
            Text { text: "경기"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignHCenter }
            Text { text: "승"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
            Text { text: "무"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
            Text { text: "패"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
            Text { text: "득실"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignHCenter }
            Text { text: "승점"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignHCenter }
          }
        }

        ListView {
          id: standingsList
          Layout.fillWidth: true
          Layout.fillHeight: true
          model: game.leagueStandings
          clip: true
          boundsBehavior: Flickable.StopAtBounds

          delegate: Rectangle {
            required property var modelData
            required property int index
            property var rowData: modelData

            width: standingsList.width
            height: 34
            color: rowData.isPlayerClub ? "#243a36" : (index % 2 === 0 ? "#18222d" : "#151f29")

            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: 16
              anchors.rightMargin: 16
              Text { text: rowData.position; color: "#b9c3ce"; font.pixelSize: 11; Layout.preferredWidth: 42 }
              Text { text: rowData.clubName; color: rowData.isPlayerClub ? "#f7f0c9" : "#e9edef"; font.pixelSize: 12; font.weight: rowData.isPlayerClub ? Font.DemiBold : Font.Normal; Layout.fillWidth: true; elide: Text.ElideRight }
              Text { text: rowData.played; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.won; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.drawn; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.lost; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.goalDifference > 0 ? "+" + rowData.goalDifference : "" + rowData.goalDifference; color: "#9aa5b3"; font.pixelSize: 11; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignHCenter }
              Text { text: rowData.points; color: "#f0f3f5"; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignHCenter }
            }
          }
        }
      }
    }
  }
}
