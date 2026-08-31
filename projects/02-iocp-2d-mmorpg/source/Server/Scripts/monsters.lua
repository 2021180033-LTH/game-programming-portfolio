-- monsters.lua
-- 2000 x 2000 월드에 200,000마리 몬스터를 Lua Script로 생성한다.
-- 서버는 luaL_dofile()로 이 파일을 실행하고 반환된 테이블을 읽어서 몬스터를 초기화한다.

local TARGET_MONSTER_COUNT = 200000
local START_MONSTER_ID = 10001

local monsterTypes = {
    { type = 1, name = "Slime",    level = 1,  hp = 30,  attack = 3,  ai = "Peace" },
    { type = 2, name = "Goblin",   level = 3,  hp = 60,  attack = 5,  ai = "Peace" },
    { type = 3, name = "Wolf",     level = 5,  hp = 90,  attack = 8,  ai = "Agro"  },
    { type = 4, name = "Orc",      level = 8,  hp = 150, attack = 12, ai = "Agro"  },
    { type = 5, name = "Skeleton", level = 10, hp = 180, attack = 14, ai = "Peace" },
    { type = 6, name = "DarkMage", level = 12, hp = 120, attack = 16, ai = "Agro"  },
}

local function in_rect(x, y, left, top, right, bottom)
    return x >= left and x <= right and y >= top and y <= bottom
end

local function is_blocked_or_reserved(x, y)
    -- 플레이어 시작 마을/상점 주변은 비워둔다.
    if in_rect(x, y, 80, 80, 130, 130) then return true end

    -- GameWorld::InitializeMap()의 장애물과 맞춘다.
    if in_rect(x, y, 105, 98, 105, 102) then return true end
    if in_rect(x, y, 400, 100, 430, 650) then return true end
    if in_rect(x, y, 700, 300, 1200, 330) then return true end
    if in_rect(x, y, 1000, 700, 1030, 1300) then return true end
    if in_rect(x, y, 1600, 1500, 1900, 1530) then return true end
    if in_rect(x, y, 1600, 1530, 1630, 1900) then return true end
    if in_rect(x, y, 1870, 1530, 1900, 1900) then return true end

    -- 성 입구는 통로이므로 비워둔다.
    if in_rect(x, y, 1730, 1500, 1770, 1530) then return true end

    return false
end

local monsters = {}
local count = 0

for y = 20, 1980, 3 do
    for x = 20, 1980, 4 do
        if not is_blocked_or_reserved(x, y) then
            count = count + 1
            local template = monsterTypes[((count - 1) % #monsterTypes) + 1]

            monsters[count] = {
                id = START_MONSTER_ID + count - 1,
                type = template.type,
                name = template.name,
                level = template.level,
                hp = template.hp,
                attack = template.attack,
                x = x,
                y = y,
                ai = template.ai,
                -- 전체 몬스터를 모두 움직이면 부하가 너무 크므로 일부만 로밍으로 둔다.
                move = (count % 4 == 0) and "Roaming" or "Fixed",
            }

            if count >= TARGET_MONSTER_COUNT then
                return monsters
            end
        end
    end
end

return monsters
