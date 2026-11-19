# 비디오숍 대여 관리 시스템 · 요구사항 추적표 발췌

대여 요청 한 건을 처리하려면 회원 자격과 연체 상태를 확인하고, 처리 결과를 대여 이력에 기록해야 한다. 아래는 요구사항 추적표(RTM) 3차본에서 이 흐름과 반납에 해당하는 행을 선별한 것이다. 요구사항, 설계도, 클래스·메서드, 데이터 항목의 식별자는 원문 표기를 따랐다.

| 요구사항 | 설계도 식별자 | 클래스·서비스 | 데이터 |
| --- | --- | --- | --- |
| FR-002 회원 확인 | UC01-BCE | MemberManager | TBL_MEMBER |
| FR-010 비디오 대여 처리 | UC01-BCE, UC01-SQD | RentalManager / IRentalService | TBL_RENTAL (DD-03) |
| FR-012 연체 확인 연동 | UC01-BCE, VD-SD01 | RentalManager.checkOverdueStatus | TBL_MEMBER.IS_OVERDUE |
| FR-013 연체자 대여 차단 | UC01-BCE, UC01-AD | RentalManager | TBL_MEMBER |
| FR-015 대여 목록 기록 | UC01-SQD | RentalManager.createRentalRecord | TBL_RENTAL |
| FR-020 반납 처리 | UC02-BCE, UC02-SQD | RentalManager.processReturn | TBL_RENTAL |

FR-012는 회원의 연체 상태를 읽는 단계이고, FR-013은 그 결과에 따라 대여를 차단하는 단계다. 두 요구사항을 각각 연결해 두었기 때문에 차단 기준이 달라질 경우 살펴볼 설계도와 회원 데이터 항목을 찾을 수 있다.
