# v0.11 데모 규칙과 재현 자료

[개정 결정표](docs/change_register.md)에서 두 차례 뒤의 보스 공격, 약점 반응, 행동 지연과 보호막 판정의 변경 이유를 확인할 수 있습니다. [게임 입력](data/demo.lua)과 [동일한 JSON 데이터](data/demo.json), [전투 화면 시안](ui/layout_probe.html)을 함께 제공합니다.

Python 3.10 이상에서 이 폴더를 작업 디렉터리로 열고 실행합니다.

~~~
python tools/verify.py
python tools/replay_golden.py
~~~

첫 명령은 피해·자원·대상·시간·거래 규칙을 검사합니다. 두 번째 명령은 정해진 순서로 수류와 바람 기술을 사용해 반응 결과를 대조합니다. 결과는 reports 폴더에 기록되며, 실제 플레이에서의 난이도나 밸런스 평가는 별도입니다.
