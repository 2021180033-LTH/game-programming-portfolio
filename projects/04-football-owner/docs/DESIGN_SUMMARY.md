# Football Owner 공개용 기획 요약

## 한 문장

Football Manager의 구단 운영 경험을 기반으로, 감독보다 상위의 구단주가 장기 방향·자금·인사 결정을 내리고 그 선택이 조직과 역사에 남는 축구 경영 시뮬레이션입니다.

## 차별화하려는 관점

| 일반적인 감독 관점 | Football Owner의 구단주 관점 |
|---|---|
| 전술·선발과 경기 준비가 중심 | 자금 조달·예산·시설·조직·인사 승인 중심 |
| 감독이 영입을 직접 주도 | 감독·스카우트가 제안하고 구단주가 조건과 위험을 승인 |
| 단일 시즌 성적이 핵심 | 구단 가치, 부채, 팬 신뢰와 장기 조직 구조가 누적 |
| 실무 결정을 한 화면에서 처리 | 운영 회의와 책임자 의견을 기록하고 의사결정 근거를 보존 |

## 핵심 loop

1. 날짜와 inbox를 확인한다.
2. 감독·스카우트·재무 담당자의 제안과 근거를 검토한다.
3. 영입·매각·임대·재계약 조건을 승인, 거절 또는 재협상한다.
4. 현금, 예산, 주급과 미래 약정을 확인한다.
5. 날짜를 진행해 협상·경기·지급·계약 event를 반영한다.
6. 행동 log가 감독 신뢰, 구단주 평판과 팬 신뢰에 누적된다.

## 구현된 범위

- 이름을 붙인 독립 save와 자동 저장
- 이적·판매의 단계별 협상, 메디컬·등록과 최종 승인
- 즉시 지급, 할부, bonus, sell-on과 future payment schedule
- scouting assignment/report/staleness와 directive
- 양방향 loan, wage contribution, recall, 종료와 future fee
- contract renewal과 만료 lifecycle
- player 구단과 같은 재정 제약을 사용하는 AI market
- fixture 진행과 standings 기초
- squad planning opinion, operating meeting, owner action log와 fan trust

## 의도적으로 남긴 후속 범위

- 전체 역사 roster를 AI decision pool에 사용하는 balancing
- 컵/하위 league 전체 schedule과 결과 importer
- 경기 engine의 전술·선수 능력 세부 simulation
- 시설·부채·자금 조달과 이사회 정치의 완전한 연결
- 기자회견과 fan/media reaction의 확장

## 데이터 정책

실제 역사와 gameplay estimate를 같은 사실처럼 취급하지 않습니다. 신원·소속·실제 거래처럼 확인 가능한 정보와, 기능 검증을 위해 만든 능력·가치·주급·AI 성향을 구분해 관리합니다. 공개 코드 묶음에서는 실존 인물·구단의 대규모 dataset을 제거했습니다.

