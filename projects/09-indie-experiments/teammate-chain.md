# Teammate Chain

시작 선수와 목표 선수를 두고, 같은 시즌에 같은 1부 클럽 소속이었던 선수들을 차례로 이어 목표에 도달하는 축구 퍼즐입니다. 국가와 시즌 범위를 좁히면 쓸 수 있는 연결도 달라집니다. 목표를 달성한 뒤에는 도착 선수를 출발점으로 삼아 다음 목표에 도전합니다.

## 한 번의 연결을 인정하는 근거

게임은 선수마다 소속 이력을 ‘클럽과 시즌’의 조합으로 저장합니다. 두 선수가 같은 클럽에 있었어도 시즌이 다르면 연결되지 않습니다. 같은 시즌에 활동했어도 클럽이 다르면 마찬가지입니다. 실제 입·퇴단 날짜의 겹침은 V1의 판정 범위에 포함하지 않습니다.

`backend/game.py`의 `Graph.connection` 발췌입니다. 같은 선수 ID를 다시 입력하면 빈 결과를 돌려줍니다. 서로 다른 선수는 공통된 클럽·시즌 ID를 찾고, 일치한 기록을 시작 연도 순으로 돌려줍니다.

```python
    def connection(self, a, b):
        if a == b: return []
        shared = set(self.by_player.get(a, ())).intersection(self.by_player.get(b, ()))
        return [self.evidence[cs] for cs in sorted(shared,key=lambda cs:(self.evidence[cs]['start_year'],cs))]
```

## 원본 회귀 시험 발췌

시험 데이터 속 a와 b는 잉글랜드에서 같은 시즌 한 클럽에 속합니다. a와 c는 같은 클럽에서 서로 다른 시즌에 뛰었습니다. c와 d의 연결은 전체 국가를 허용하면 성립하지만 잉글랜드로 범위를 좁히면 성립하지 않습니다.

```python
    def test_same_club_different_season_is_not_teammate(self):
        self.assertFalse(self.game.connection('a','c',Filter('England'))['valid'])
        self.assertTrue(self.game.connection('a','b',Filter('England'))['valid'])

    def test_country_season_and_same_id_rules(self):
        self.assertFalse(self.game.connection('c','d',Filter('England'))['valid'])
        self.assertTrue(self.game.connection('c','d',Filter())['valid'])
        self.assertFalse(self.game.connection('b','c',Filter('England',2000,2000))['valid'])
        self.assertFalse(self.game.connection('a','a',Filter())['valid'])
```

두 시험과 최단 경로·시즌 범위 시험은 가상 선수와 가상 클럽으로 만든 SQLite 데이터에서 실행했습니다. 경기 출전 여부나 선수의 실제 소속 이력에 대한 주장은 포함하지 않습니다.
