-- npcs.lua
-- Static NPC placement data.
-- type 1: Merchant, 2: Blacksmith, 3: Healer, 4: Skill Trainer, 5: Guard, 6: Guide

return {
    { id = 1, type = 1, name = "General Merchant", x = 102, y = 100 },
    { id = 2, type = 2, name = "Blacksmith",       x = 96,  y = 100 },
    { id = 3, type = 3, name = "Healer",           x = 100, y = 96  },
    { id = 4, type = 4, name = "Skill Trainer",    x = 100, y = 104 },
    { id = 5, type = 5, name = "Town Guard",       x = 90,  y = 90  },
    { id = 6, type = 6, name = "Village Guide",    x = 110, y = 90  },

    { id = 101, type = 5, name = "Forest Guard",   x = 320, y = 300 },
    { id = 102, type = 1, name = "Forest Trader",  x = 340, y = 305 },

    { id = 201, type = 5, name = "Orc Camp Scout", x = 860, y = 520 },
    { id = 202, type = 3, name = "Field Medic",    x = 880, y = 520 },

    { id = 301, type = 5, name = "Ruins Guard",    x = 1500, y = 1200 },
    { id = 302, type = 4, name = "Arcane Mentor",  x = 1520, y = 1210 },

    { id = 401, type = 1, name = "Castle Trader",  x = 1745, y = 1490 },
    { id = 402, type = 5, name = "Castle Gatekeeper", x = 1750, y = 1540 },
}
