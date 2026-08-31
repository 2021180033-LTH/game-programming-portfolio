# 구현 위치 지도

| 영역 | 주요 파일 | 시작점 |
|---|---|---|
| IOCP 초기화 | `source/Server/IocpServer.cpp` | `Start`, `InitializeWinsock`, `CreateListenSocket`, `CreateIocpHandle` |
| 비동기 accept | `IocpServer.cpp`, `IocpEvent.h` | `PostAccept`, `HandleAccept` |
| recv와 TCP framing | `IocpServer.cpp`, `ClientSession.cpp` | `PostRecv`, `HandleRecv`, session packet buffer |
| send 적체 제한 | `IocpServer.cpp`, `ClientSession.cpp` | `SendPacket`, `HandleSend`, `TryBeginSend`, `EndSend` |
| packet 검증·분기 | `PacketDispatcher.cpp` | `Dispatch` |
| session 상태 | `ClientSession.*` | login/player state, inventory, equipment, buffs |
| DB queue | `GameWorld.cpp` | `EnqueueDbJob`, `DbWorkerLoop`, `ProcessDbCompletions` |
| world update | `GameWorld.cpp` | `WorldUpdateLoop` |
| 관심 영역 | `GameWorld.cpp` | sector와 `Update*VisibilityFor` |
| monster AI | `GameWorld.cpp` | `UpdateMonsterAi`, `FindPathAStar` |
| 전투·drop | `GameWorld.cpp` | `HandlePlayerAttack`, `CreatePersonalDropsForMonster` |
| inventory·skill | `GameWorld.cpp` | `UseItem`, `SellItem`, `UseSkill` |
| ODBC persistence | `DatabaseManager.cpp` | connect, load/save functions |
| Lua data | `GameWorld.cpp`, `Scripts/*.lua` | monster/NPC/price loaders |
| SFML client | `source/Client/main.cpp` | socket thread, input, UI state |

## Thread model

```text
IOCP workers (logical CPU count, fallback 4)
  └─ socket completion → framing → PacketDispatcher
     └─ 요청 기반 movement/combat/item/skill/visibility GameWorld 호출

DB worker (DB 연결 성공 시 1)
  └─ login/load/save job → ODBC → completion queue

World update thread (1)
  └─ monster AI / buff / respawn / regeneration / drop expiry
```

AcceptEx 요청은 64개를 선등록합니다. 이 값은 worker 수가 아니라 동시에 대기시키는 accept context 수입니다.

DB I/O와 주기적 world update는 별도 thread이지만, 요청으로 즉시 실행되는 `GameWorld` 로직은 `PacketDispatcher`를 호출한 IOCP worker에서 동기 실행됩니다.
