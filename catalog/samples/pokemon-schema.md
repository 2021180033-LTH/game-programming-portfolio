# 포켓몬스터 게임 DB · 정규화 모델의 테이블

최종 제출한 MySQL Workbench 모델에서 일곱 테이블의 열과 키를 읽어 옮겼다. 트레이너별로 같은 종을 몇 번 봤는지 기록하는 도감과, 실제로 소유한 개체의 상태를 별도로 관리한다.

| 테이블 | 원본 열 | 기본키 | 모델에 등록된 외래키 |
| --- | --- | --- | --- |
| `Trainer` | Trainer_ID, Name, Region, Gender, CurrentLocation, Money, PlayTime, StartDate | Trainer_ID | — |
| `Trainer_Badge` | BadgeName, Trainer_Trainer_ID | BadgeName, Trainer_Trainer_ID | Trainer_Trainer_ID → Trainer(Trainer_ID) |
| `Species` | Dex_num, Name, Description, B_HP, B_ATK, B_DEF, B_S_ATK, B_S_DEF, B_SPD | Dex_num | — |
| `Pokemon` | Pokemon_ID, Trainer_ID, Species_ID, Nickname, Level, Exp, Nature, Obtained_Date, HP, ATK, DEF, S_ATK, S_DEF, SPD | Pokemon_ID | — |
| `Pokemon_Moves` | Moves_ID, Pokemon_Pokemon_ID | Moves_ID, Pokemon_Pokemon_ID | Pokemon_Pokemon_ID → Pokemon(Pokemon_ID) |
| `Pokedex` | Status, Seen_Count, Caught_Count, First_Encounter_Date, Trainer_Trainer_ID, Species_Dex_num | Trainer_Trainer_ID, Species_Dex_num | Trainer_Trainer_ID → Trainer(Trainer_ID)<br>Species_Dex_num → Species(Dex_num) |
| `Trainer_Party` | Slot_Number, Pokemon_Pokemon_ID, Trainer_Trainer_ID | Slot_Number, Pokemon_Pokemon_ID, Trainer_Trainer_ID | Pokemon_Pokemon_ID → Pokemon(Pokemon_ID)<br>Trainer_Trainer_ID → Trainer(Trainer_ID) |

발표 자료의 설계 의도와 모델 파일 사이에도 차이가 있다. 발표에서는 `Pokemon.Trainer_ID`와 `Pokemon.Species_ID`를 소유자·종 참조로 설명했다. 제출한 Workbench 모델의 `Pokemon` 테이블에는 두 열이 있지만 물리 외래키가 선언되어 있지 않다. `Trainer_Party`의 현재 복합 기본키도 트레이너마다 같은 슬롯 번호의 재사용을 단독으로 막지는 않는다.
