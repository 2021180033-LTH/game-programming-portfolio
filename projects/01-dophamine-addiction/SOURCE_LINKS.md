# 공개 코드 증거 인덱스

원본 4인 팀 저장소는 비공개이며 Config token, 외부 asset과 팀원 코드가 함께 있어 Public으로 전환하지 않았습니다. 대신 고정 commit `4cb26cc0cbcf8e530ecd662277ab697c236a45ad`에서 기여 근거를 감사한 뒤, 본인 구현을 확인할 수 있는 line range를 이 저장소 안에 발췌했습니다.

각 `.cpp`의 상단에는 다음을 적었습니다.

- 원본 `Manager` 프로젝트 기준 path와 고정 commit
- 원본 line range
- 해당 범위의 현재 `git blame` 근거와 공동 코드 예외
- 심사자가 확인할 구현 목적

발췌본은 주변 선언·팀 코드를 의도적으로 제외한 **코드 리뷰용 비독립 실행 파일**입니다. 완전한 빌드본인 것처럼 제출하지 않습니다.

## 1. IOCP 로그인·로비 서버

- [`ServerMain` connection admission과 packet limit](evidence/excerpts/server/ServerMain_connection_admission_L294-L484.cpp)
- [`ServerMain` single-close와 closing lifecycle](evidence/excerpts/server/ServerMain_close_lifecycle_L491-L534.cpp)
- [`LobbyService` Dedicated Server port/process/ticket lifecycle](evidence/excerpts/server/LobbyService_dedi_lifecycle_ticket_L516-L1269.cpp)
- [`LobbyService` match end·abort·ready control handler](evidence/excerpts/server/LobbyService_match_end_ready_L2990-L3362.cpp)

## 2. Dedicated Server 상태·재접속·준비 동기화

- [`MainGameMode` participant ledger와 reconnect 복원](evidence/excerpts/unreal/MainGameMode_ledger_reconnect_L1256-L1556.cpp)
- [`MainGameMode` recovery watchdog·abort·readiness](evidence/excerpts/unreal/MainGameMode_watchdog_abort_ready_L4606-L4986.cpp)
- [`MainPlayerController` server RPC rate limit](evidence/excerpts/unreal/MainPlayerController_rpc_rate_limit_L291-L344.cpp)
- [`MainPlayerController` input lock·서버 위치 보정·transform 검증·상점 강제 종료](evidence/excerpts/unreal/MainPlayerController_input_position_lock_L696-L960.cpp)
- [`MainPlayerController` phase/streaming/card readiness ACK](evidence/excerpts/unreal/MainPlayerController_readiness_ack_L1013-L1394.cpp)
- [`MainPlayerState` replicated card와 reconnect snapshot](evidence/excerpts/unreal/MainPlayerState_card_reconnect_snapshot_L101-L274.cpp)

## 3. 공동 카드게임에 추가한 서버 안정화

- [`CardGameService` reconnect detach/reattach state](evidence/excerpts/unreal/CardGameService_reconnect_state_L38-L211.cpp)
- [`CardGameService` selection timeout과 자동 fallback](evidence/excerpts/unreal/CardGameService_selection_timeout_L655-L835.cpp)
- [`CardGameService` bet-turn timeout과 check/die fallback](evidence/excerpts/unreal/CardGameService_bet_turn_timeout_L1020-L1117.cpp)
- [`CardGameService` all-in 상태·턴·settlement 안정화 hunk](evidence/excerpts/unreal/CardGameService_all_in_stability_hunks.cpp)
- [`CardGameService` 결정적 동점 pot 분배 hunk](evidence/excerpts/unreal/CardGameService_tie_payout_L1420-L1469.cpp)
- [`CardGameService` reconnect card catch-up](evidence/excerpts/unreal/CardGameService_card_catchup_L2420-L2542.cpp)

`AllIn` action과 기존 betting core 전체를 단독 구현했다고 주장하지 않습니다. 위 파일은 공동 시스템에 추가한 timeout, all-in 교착 방지, tie payout과 reconnect 보강 범위입니다.

## 4. 서버 권위 골드·카드 배치 판정

- [`MainGameMode` 사망 골드 드롭 판정·직접 spawn·ledger 반영](evidence/excerpts/unreal/MainGameMode_drop_gold_decision_L981-L1061.cpp)
- [`GoldDropActor` replication과 authority-only overlap 획득](evidence/excerpts/unreal/GoldDropActor_authority_pickup_L14-L44_L52-L122.cpp) — 이 actor 파일에는 spawn 호출이 없고, 위 `MainGameMode` 발췌본이 spawn을 담당
- [`GoldDropActor.h` 전체 원본](source/GoldDropActor.h) — 현재 줄 53/53 본인 attribution
- [`CardPlacementService` 사망 카드 지면 trace fallback과 거리·겹침·상단 여유 검증](evidence/excerpts/unreal/CardPlacement_drop_rule_L1297-L1376.cpp)

## 함께 볼 근거

- 파일 전체의 공동 기여 경계: [`CONTRIBUTIONS.md`](CONTRIBUTIONS.md)
- 작업 당시 작성한 개발 기록: [`docs/devlog`](docs/devlog/)
- 원본과 공개 복사본의 hash·변환 상태: [`../../docs/SOURCE_MANIFEST.csv`](../../docs/SOURCE_MANIFEST.csv)
