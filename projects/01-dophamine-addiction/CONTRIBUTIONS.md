# 기여 범위

이 표의 현재 줄 비율은 고정 commit `4cb26cc0cbcf8e530ecd662277ab697c236a45ad`에 대한 `git blame` 보조 지표입니다. 줄 수는 설계 소유권이나 저작권을 자동으로 판정하지 않으므로, 실제 역할·개발 기록·commit과 함께 사용합니다.

| 파일 | 이태현 현재 줄 | 주요 기여 범위 |
|---|---:|---|
| `Server/ServerMain.cpp` | 823 / 1,138 (72.3%) | admission/rate limit, packet framing, send FIFO·partial send, I/O 종료 처리, listen 설정 |
| `Server/LobbyService.cpp` | 1,950 / 3,362 (58.0%) | port pool·격리·회수, Dedi process/Job Object lifecycle, ticket·control token, ready/abort/end ACK |
| `MainGameMode.cpp` | 4,675 / 5,143 (90.9%) | ticket·입장·재접속, streaming/card ACK gate, 서버 phase, 위치 복구, 종료·Lobby 통지 |
| `MainGameMode.h` | 458 / 591 (77.5%) | 위 서버 상태와 설정 선언 |
| `MainPlayerController.cpp` | 1,751 / 2,313 (75.7%) | RPC rate/phase 검증, loading/card ACK, 위치 보정, 섯다 요청 |
| `MainPlayerController.h` | 246 / 388 (63.4%) | RPC·ACK·보정 interface |
| `MainPlayerState.cpp` | 318 / 540 (58.9%) | 카드 소유 상태, reconnect snapshot, upgrade 일부 |
| `CardGameService.cpp` | 1,353 / 2,979 (45.4%) | reconnect, selection/bet-turn timeout, all-in 상태·진행 보강, 동점 분배 patch와 카드 catch-up; 기반 betting core는 공동 코드 |
| `CardPlacementService.cpp` | 361 / 1,404 (25.7%) | 지면 trace와 collision fallback 일부 |
| `GoldDropActor.cpp` | 114 / 122 (93.4%) | replication 초기화와 authority-only overlap 획득; actor spawn은 `MainGameMode::DropGoldFromPlayer` 본인 범위 |
| `GoldDropActor.h` | 53 / 53 (100%) | 전체 현재 줄 직접 작성 확인 |

## 표현 원칙

- “서버 담당”은 프로젝트 역할을 설명합니다.
- 공동 파일은 파일 전체를 단독 구현했다고 표현하지 않습니다.
- `SOURCE_LINKS.md`에 연결한 로컬 함수 발췌본과 개발 기록으로 구체적인 기여를 설명합니다.
- 물리 역할 중 직접 코드 근거는 서버 권위 overlap·획득 판정과 카드 배치의 지면 trace/collision fallback 일부로 제한합니다.
- AI를 활용한 후기 분석·수정 기록은 저장소 공통 [`AUTHORSHIP_AND_AI_USE.md`](../../docs/AUTHORSHIP_AND_AI_USE.md)에 공개합니다.
