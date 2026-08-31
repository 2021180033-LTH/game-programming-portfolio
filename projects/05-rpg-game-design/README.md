# 계약과 공명 — 선택형 파티 RPG 기획

계약 생물과 인간의 관계, party 조합과 선택의 결과를 중심으로 설계한 PC single-player 2D pixel RPG 기획 프로젝트입니다.

- 작성일: 2026.07.14
- 인원: 1명, AI 보조
- 현재 상태: GDD v0.4 기반 기획·기술 설계 단계
- 목표 기술: C++20, SFML, Lua, CMake
- 대표 문서: [`design/RPG_게임_기획서_v0.4.docx`](design/RPG_게임_기획서_v0.4.docx)

> C++20·SFML·Lua·CMake는 **예정 기술 stack**입니다. 현재 이 폴더에는 실행 code나 prototype이 없으며, 구현 완료 프로젝트로 주장하지 않습니다.

## 기획 핵심

### 전투

- 4인 party
- character마다 독립적인 timeline이 흐르는 turn battle
- 행동 속도, skill 비용, status effect와 순서 개입을 전투 선택으로 연결

### 계약과 관계

- 인간과 계약 생물을 단순 장비 관계가 아니라 양방향 관계로 설계
- character 성장, creature 성장, pair resonance를 세 축으로 분리
- 관계와 선택이 동료, 지역, boss와 ending 변화로 이어지는 구조

### 목표 범위

- vertical slice: 30–60분
- 1회 완주 목표: 10–15시간
- 기획 단계에서 전투·성장·선택 branch와 목표 기술 구조까지 정의

## 문서에서 확인할 내용

1. 핵심 fantasy와 player experience
2. party·timeline 전투 규칙
3. 인간/계약 생물/pair resonance 성장 구조
4. 선택에 따른 동료·지역·boss·ending branch
5. vertical slice scope와 C++/SFML/Lua module 계획

## AI 활용

AI를 아이디어 정리, 문서 구조화와 누락 검토에 활용했습니다. 게임 방향, 규칙 선택과 최종 편집은 이태현이 수행했습니다. 실제 적용·build·playtest가 완료된 것처럼 표현하지 않습니다.

## 공개 범위

v0.4에는 embedded image, comment와 개인 작성자 metadata가 확인되지 않아 대표 문서로 포함했습니다. 이전 v0.1–v0.3은 중복 검토를 줄이기 위해 공개 묶음에서 제외했습니다.

기존 작업 저장소는 공개 여부를 별도로 확인하기 전 링크하지 않습니다. 채용 검토에는 이 통합 저장소에 포함한 v0.4 원문과 위 요약을 사용합니다.
