-- RPG v0.11 authoritative game input. JSON is the equivalent test mirror.
-- Empty optional equipment slots use false, not nil, to preserve array positions.
return {
  ["version"] = "0.11",
  ["schema_version"] = 1,
  ["scope"] = "sediment_mine_lv10_demo",
  ["numeric_origin"] = "v0.10 values retained unless listed in docs/change_register.md; missing values authored in v0.11; not playtested",
  ["rules"] = {
    ["rounding"] = "nonnegative_half_up",
    ["tl_per_second"] = nil,
    ["balance_storage_per_display_unit"] = 2,
    ["normal_delay_min"] = 300,
    ["spd_min"] = 0,
    ["spd_max"] = 100,
    ["delay_base_numerator"] = 100000,
    ["delay_base_offset"] = 100,
    ["resonance_start_base"] = 25,
    ["resonance_start_per_rank"] = 5,
    ["resonance_max"] = 100,
    ["ultimate_resonance_cost"] = 80,
    ["resonance_gain_base"] = 4,
    ["resonance_gain_role"] = 3,
    ["resonance_gain_weakness"] = 3,
    ["resonance_gain_cap"] = 10,
    ["direct_move_cap_bp"] = 3000,
    ["break_move_bp"] = 3500,
    ["total_delay_cap_bp"] = 6000,
    ["rear_tick"] = 500,
    ["rear_hp_bp"] = 300,
    ["rear_sp_bp"] = 600,
    ["rear_hp_cap_bp"] = 2400,
    ["rear_sp_cap_bp"] = 4800,
    ["trace_ttl"] = 1000,
    ["status_ttl"] = 2000,
    ["dot_tick"] = 1000,
    ["guard_damage_bp"] = 7000,
    ["guard_sp_immediate_bp"] = 400,
    ["guard_sp_on_hit_bp"] = 800,
    ["basic_sp_bp"] = 800,
    ["damage_weak_bp"] = 11500,
    ["damage_neutral_bp"] = 10000,
    ["damage_resist_bp"] = 7500,
    ["crit_base_bp"] = 500,
    ["crit_damage_bp"] = 15000,
    ["variance_bp_min"] = 9500,
    ["variance_bp_max"] = 10500,
    ["default_attack_delay_bp"] = 10000,
    ["status_scale_min_bp"] = 7000,
    ["status_scale_max_bp"] = 12000,
    ["save_version"] = 1,
    ["human_has_balance"] = false,
    ["same_tl_order"] = {
      [1] = "periodic_damage_batch",
      [2] = "death_mark",
      [3] = "expiry_and_expiry_recoil",
      [4] = "death_mark",
      [5] = "rear_regeneration",
      [6] = "auto_promote",
      [7] = "victory_defeat",
      [8] = "ready_action"
    },
    ["weapon_types"] = {
      [1] = "sword",
      [2] = "spear",
      [3] = "dagger",
      [4] = "bow",
      [5] = "blunt",
      [6] = "staff"
    },
    ["elements"] = {
      [1] = "fire",
      [2] = "water",
      [3] = "wind",
      [4] = "earth",
      [5] = "light",
      [6] = "shadow"
    },
    ["world_flags"] = {
      [1] = "STABILITY",
      [2] = "RIGHTS",
      [3] = "TRANSITION",
      [4] = "COOPERATION"
    },
    ["guard_type"] = "direct_physical_or_resonance; not_dot_not_shared",
    ["target_ai_tiebreak"] = "front_slot_ascending_then_spawn_id",
    ["weapon_context"] = "required_weapon_else_staff_if_available_else_first_weapon",
    ["enemy_equipment_power"] = 0,
    ["break_restore_bp"] = {
      ["normal"] = 6000,
      ["elite"] = 6000,
      ["boss"] = 5000
    },
    ["passive_reaction_depth_max"] = 1,
    ["double_ko_result"] = "defeat",
    ["normal_action_balance_recovery"] = 1,
    ["ready_tie_order"] = "player_front_slot_ascending_then_enemy_spawn_ascending_then_reservation_id",
    ["regen_full_target_consumes_quota"] = false
  },
  ["characters"] = {
    [1] = {
      ["id"] = "char.raen",
      ["name"] = "라엔",
      ["level"] = 10,
      ["stats_basis"] = "naked_before_equipment_and_growth",
      ["stats"] = {
        ["HP"] = 900,
        ["SP"] = 100,
        ["ATK"] = 82,
        ["DEF"] = 78,
        ["RES"] = 70,
        ["WIL"] = 72,
        ["SPD"] = 32
      },
      ["role"] = "수호",
      ["weapons"] = {
        [1] = "sword",
        [2] = "spear"
      },
      ["contract"] = "contract.rumo",
      ["rank"] = 5,
      ["affinity"] = {
        ["fire"] = "B",
        ["water"] = "B",
        ["wind"] = "B",
        ["earth"] = "B",
        ["light"] = "B",
        ["shadow"] = "B"
      },
      ["passive"] = "passive.raen",
      ["initial_skills"] = {
        [1] = "skill.raen.parry",
        [2] = "skill.raen.intercept_thrust",
        [3] = "skill.raen.resonance_cut"
      },
      ["all_skills"] = {
        [1] = "skill.raen.parry",
        [2] = "skill.raen.intercept_thrust",
        [3] = "skill.raen.resonance_cut",
        [4] = "skill.raen.intervention_step",
        [5] = "skill.raen.steady_stance"
      },
      ["ultimate"] = "skill.raen.boundary_cut",
      ["growth_per_level"] = {
        ["HP"] = 38,
        ["SP"] = 2,
        ["ATK"] = 4,
        ["DEF"] = 3,
        ["RES"] = 3,
        ["WIL"] = 3,
        ["SPD"] = 1
      },
      ["growth_scope"] = "prototype_relative_to_level10; clamp SPD to100; no biological age mapping"
    },
    [2] = {
      ["id"] = "char.seria",
      ["name"] = "세리아",
      ["level"] = 10,
      ["stats_basis"] = "naked_before_equipment_and_growth",
      ["stats"] = {
        ["HP"] = 720,
        ["SP"] = 110,
        ["ATK"] = 72,
        ["DEF"] = 50,
        ["RES"] = 68,
        ["WIL"] = 62,
        ["SPD"] = 52
      },
      ["role"] = "지원",
      ["weapons"] = {
        [1] = "dagger",
        [2] = "bow"
      },
      ["contract"] = "contract.asel",
      ["rank"] = 3,
      ["affinity"] = {
        ["fire"] = "B",
        ["water"] = "B",
        ["wind"] = "B",
        ["earth"] = "B",
        ["light"] = "B",
        ["shadow"] = "B"
      },
      ["passive"] = "passive.seria",
      ["initial_skills"] = {
        [1] = "skill.seria.precise_shot",
        [2] = "skill.seria.cut_pulse",
        [3] = "skill.seria.orbit_order"
      },
      ["all_skills"] = {
        [1] = "skill.seria.precise_shot",
        [2] = "skill.seria.cut_pulse",
        [3] = "skill.seria.orbit_order",
        [4] = "skill.seria.analyze",
        [5] = "skill.seria.cover_retreat"
      },
      ["ultimate"] = "skill.seria.blindspot_lock",
      ["growth_per_level"] = {
        ["HP"] = 30,
        ["SP"] = 2,
        ["ATK"] = 3,
        ["DEF"] = 2,
        ["RES"] = 3,
        ["WIL"] = 2,
        ["SPD"] = 1
      },
      ["growth_scope"] = "prototype_relative_to_level10; clamp SPD to100; no biological age mapping"
    },
    [3] = {
      ["id"] = "char.kael",
      ["name"] = "카엘",
      ["level"] = 10,
      ["stats_basis"] = "naked_before_equipment_and_growth",
      ["stats"] = {
        ["HP"] = 980,
        ["SP"] = 90,
        ["ATK"] = 95,
        ["DEF"] = 68,
        ["RES"] = 50,
        ["WIL"] = 55,
        ["SPD"] = 25
      },
      ["role"] = "공격",
      ["weapons"] = {
        [1] = "blunt",
        [2] = "spear"
      },
      ["contract"] = "contract.kara",
      ["rank"] = 3,
      ["affinity"] = {
        ["fire"] = "B",
        ["water"] = "B",
        ["wind"] = "B",
        ["earth"] = "B",
        ["light"] = "B",
        ["shadow"] = "B"
      },
      ["passive"] = "passive.kael",
      ["initial_skills"] = {
        [1] = "skill.kael.smash",
        [2] = "skill.kael.pierce",
        [3] = "skill.kael.overdrive"
      },
      ["all_skills"] = {
        [1] = "skill.kael.smash",
        [2] = "skill.kael.pierce",
        [3] = "skill.kael.overdrive",
        [4] = "skill.kael.repair",
        [5] = "skill.kael.release"
      },
      ["ultimate"] = "skill.kael.limit_line",
      ["growth_per_level"] = {
        ["HP"] = 42,
        ["SP"] = 2,
        ["ATK"] = 5,
        ["DEF"] = 3,
        ["RES"] = 2,
        ["WIL"] = 2,
        ["SPD"] = 1
      },
      ["growth_scope"] = "prototype_relative_to_level10; clamp SPD to100; no biological age mapping"
    },
    [4] = {
      ["id"] = "char.mirea",
      ["name"] = "미레아",
      ["level"] = 10,
      ["stats_basis"] = "naked_before_equipment_and_growth",
      ["stats"] = {
        ["HP"] = 760,
        ["SP"] = 120,
        ["ATK"] = 48,
        ["DEF"] = 55,
        ["RES"] = 92,
        ["WIL"] = 88,
        ["SPD"] = 38
      },
      ["role"] = "술법",
      ["weapons"] = {
        [1] = "bow",
        [2] = "staff"
      },
      ["contract"] = "contract.nema",
      ["rank"] = 3,
      ["affinity"] = {
        ["fire"] = "B",
        ["water"] = "B",
        ["wind"] = "B",
        ["earth"] = "B",
        ["light"] = "B",
        ["shadow"] = "B"
      },
      ["passive"] = "passive.mirea",
      ["initial_skills"] = {
        [1] = "skill.mirea.clean_arrow",
        [2] = "skill.mirea.heal",
        [3] = "skill.mirea.flow_away"
      },
      ["all_skills"] = {
        [1] = "skill.mirea.clean_arrow",
        [2] = "skill.mirea.heal",
        [3] = "skill.mirea.flow_away",
        [4] = "skill.mirea.cleanse",
        [5] = "skill.mirea.slow_current"
      },
      ["ultimate"] = "skill.mirea.clear_cycle",
      ["growth_per_level"] = {
        ["HP"] = 32,
        ["SP"] = 3,
        ["ATK"] = 2,
        ["DEF"] = 2,
        ["RES"] = 4,
        ["WIL"] = 4,
        ["SPD"] = 1
      },
      ["growth_scope"] = "prototype_relative_to_level10; clamp SPD to100; no biological age mapping"
    }
  },
  ["contracts"] = {
    [1] = {
      ["id"] = "contract.rumo",
      ["name"] = "루모",
      ["species"] = "결정등 거북",
      ["element"] = "earth",
      ["intelligence"] = "감응형",
      ["level"] = 10,
      ["power"] = 36,
      ["protection"] = 50,
      ["growth_per_level"] = {
        ["power"] = 3,
        ["protection"] = 4
      },
      ["growth_formula"] = "max(1, value_at_10 + per_level * (L-10))",
      ["passive"] = "passive.rumo",
      ["imprint"] = "passive.imprint_rumo",
      ["role_variants"] = {
        ["공격"] = {
          ["kind"] = "on_shield_exhaust_next_human_damage",
          ["bonus_bp"] = 1000,
          ["ttl"] = 2000
        },
        ["수호"] = {
          ["kind"] = "share_shield",
          ["fraction_bp"] = 3000,
          ["target"] = "lowest_front_hp_ratio_excluding_primary",
          ["allow_self_fallback"] = false
        },
        ["지원"] = {
          ["kind"] = "healing_received",
          ["bp"] = 1500,
          ["ttl"] = 2000
        },
        ["술법"] = {
          ["kind"] = "next_contract_cost",
          ["delta"] = -5,
          ["ttl"] = 2000
        }
      },
      ["actives"] = {
        [1] = "skill.rumo.shield",
        [2] = "skill.rumo.anchor",
        [3] = "skill.rumo.echo"
      },
      ["ultimate"] = "skill.rumo.double_wall",
      ["evolution"] = {
        ["id"] = "evolution.rumo",
        ["level"] = 12,
        ["event"] = "event.growth.rumo",
        ["materials"] = {
          ["material.clear_resin"] = 2
        },
        ["permanent_bonus"] = {
          ["power"] = 4,
          ["protection"] = 4
        },
        ["consent_required"] = true,
        ["once"] = true,
        ["scope"] = "growth_test_only_not_public_demo"
      },
      ["role_variant_default_skill"] = "skill.rumo.shield",
      ["role_variant_min_rank"] = 3
    },
    [2] = {
      ["id"] = "contract.asel",
      ["name"] = "아셀",
      ["species"] = "청익 솔개",
      ["element"] = "wind",
      ["intelligence"] = "감응형",
      ["level"] = 10,
      ["power"] = 42,
      ["protection"] = 20,
      ["growth_per_level"] = {
        ["power"] = 4,
        ["protection"] = 2
      },
      ["growth_formula"] = "max(1, value_at_10 + per_level * (L-10))",
      ["passive"] = "passive.asel",
      ["imprint"] = "passive.imprint_asel",
      ["role_variants"] = {
        ["공격"] = {
          ["kind"] = "skill_damage_bonus",
          ["skill"] = "skill.asel.feather",
          ["bp"] = 1000,
          ["condition"] = "weak_hit"
        },
        ["수호"] = {
          ["kind"] = "skill_grants_cover",
          ["skill"] = "skill.asel.accelerate",
          ["damage_bp"] = 9000,
          ["ttl"] = 2000
        },
        ["지원"] = {
          ["kind"] = "skill_cost_delta",
          ["skill"] = "skill.asel.accelerate",
          ["res"] = -5
        },
        ["술법"] = {
          ["kind"] = "trace_ttl_bonus",
          ["element"] = "wind",
          ["tl"] = 250
        }
      },
      ["actives"] = {
        [1] = "skill.asel.accelerate",
        [2] = "skill.asel.feather",
        [3] = "skill.asel.scout"
      },
      ["ultimate"] = "skill.asel.rapid_turn",
      ["evolution"] = {
        ["id"] = "evolution.asel",
        ["level"] = 12,
        ["event"] = "event.growth.asel",
        ["materials"] = {
          ["material.clear_resin"] = 2
        },
        ["permanent_bonus"] = {
          ["power"] = 4,
          ["protection"] = 4
        },
        ["consent_required"] = true,
        ["once"] = true,
        ["scope"] = "growth_test_only_not_public_demo"
      },
      ["role_variant_default_skill"] = "skill.asel.accelerate",
      ["role_variant_min_rank"] = 3
    },
    [3] = {
      ["id"] = "contract.kara",
      ["name"] = "카라",
      ["species"] = "잿불 여우",
      ["element"] = "fire",
      ["intelligence"] = "지성형",
      ["level"] = 10,
      ["power"] = 55,
      ["protection"] = 15,
      ["growth_per_level"] = {
        ["power"] = 5,
        ["protection"] = 1
      },
      ["growth_formula"] = "max(1, value_at_10 + per_level * (L-10))",
      ["passive"] = "passive.kara",
      ["imprint"] = "passive.imprint_kara",
      ["role_variants"] = {
        ["공격"] = {
          ["kind"] = "skill_damage_bonus",
          ["skill"] = "contract_fire_attacks",
          ["bp"] = 1000,
          ["condition"] = "target_burning"
        },
        ["수호"] = {
          ["kind"] = "ignite_override",
          ["atk_bp"] = 1500,
          ["recoil_bp"] = 0
        },
        ["지원"] = {
          ["kind"] = "ignite_expiry_resonance",
          ["amount"] = 4,
          ["limit"] = "once_per_cast_natural_expiry"
        },
        ["술법"] = {
          ["kind"] = "reaction_damage_bonus",
          ["element"] = "fire",
          ["bp"] = 1500
        }
      },
      ["actives"] = {
        [1] = "skill.kara.burst",
        [2] = "skill.kara.claws",
        [3] = "skill.kara.ignite"
      },
      ["ultimate"] = "skill.kara.red_limit",
      ["evolution"] = {
        ["id"] = "evolution.kara",
        ["level"] = 12,
        ["event"] = "event.growth.kara",
        ["materials"] = {
          ["material.clear_resin"] = 2
        },
        ["permanent_bonus"] = {
          ["power"] = 4,
          ["protection"] = 4
        },
        ["consent_required"] = true,
        ["once"] = true,
        ["scope"] = "growth_test_only_not_public_demo"
      },
      ["role_variant_default_skill"] = "skill.kara.burst",
      ["role_variant_min_rank"] = 3
    },
    [4] = {
      ["id"] = "contract.nema",
      ["name"] = "네마",
      ["species"] = "맑은샘 수달",
      ["element"] = "water",
      ["intelligence"] = "감응형",
      ["level"] = 10,
      ["power"] = 38,
      ["protection"] = 35,
      ["growth_per_level"] = {
        ["power"] = 3,
        ["protection"] = 3
      },
      ["growth_formula"] = "max(1, value_at_10 + per_level * (L-10))",
      ["passive"] = "passive.nema",
      ["imprint"] = "passive.imprint_nema",
      ["role_variants"] = {
        ["공격"] = {
          ["kind"] = "heal_grants_next_weak_damage",
          ["bp"] = 800,
          ["ttl"] = 2000
        },
        ["수호"] = {
          ["kind"] = "skill_cover_override",
          ["skill"] = "skill.nema.wave",
          ["damage_bp"] = 6500
        },
        ["지원"] = {
          ["kind"] = "skill_cost_delta",
          ["skill"] = "skill.nema.heal",
          ["res"] = -5
        },
        ["술법"] = {
          ["kind"] = "trace_ttl_bonus",
          ["element"] = "water",
          ["tl"] = 250
        }
      },
      ["actives"] = {
        [1] = "skill.nema.heal",
        [2] = "skill.nema.clean_stream",
        [3] = "skill.nema.wave",
        [4] = "skill.nema.water_lance"
      },
      ["ultimate"] = "skill.nema.returning_water",
      ["evolution"] = {
        ["id"] = "evolution.nema",
        ["level"] = 12,
        ["event"] = "event.growth.nema",
        ["materials"] = {
          ["material.clear_resin"] = 2
        },
        ["permanent_bonus"] = {
          ["power"] = 4,
          ["protection"] = 4
        },
        ["consent_required"] = true,
        ["once"] = true,
        ["scope"] = "growth_test_only_not_public_demo"
      },
      ["role_variant_default_skill"] = "skill.nema.heal",
      ["role_variant_min_rank"] = 3
    }
  },
  ["skills"] = {
    [1] = {
      ["id"] = "skill.basic.sword",
      ["name"] = "검 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "sword",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [2] = {
      ["id"] = "skill.basic.spear",
      ["name"] = "창 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "spear",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [3] = {
      ["id"] = "skill.basic.dagger",
      ["name"] = "단검 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "dagger",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [4] = {
      ["id"] = "skill.basic.bow",
      ["name"] = "활 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "bow",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [5] = {
      ["id"] = "skill.basic.blunt",
      ["name"] = "타격 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "blunt",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [6] = {
      ["id"] = "skill.basic.staff",
      ["name"] = "지팡이 기본 공격",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "staff",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [7] = {
      ["id"] = "skill.common.guard",
      ["name"] = "방어",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 9000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "guard",
          ["damage_bp"] = 7000,
          ["ttl"] = 2000,
          ["until"] = "owner_next_action_start",
          ["sp_now_bp"] = 400,
          ["sp_on_hit_bp"] = 800
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [8] = {
      ["id"] = "skill.common.flee",
      ["name"] = "도주",
      ["owner"] = "common",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "flee",
          ["success"] = true,
          ["rewards"] = false,
          ["field_grace_frames"] = 120
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [9] = {
      ["id"] = "skill.raen.parry",
      ["name"] = "받아넘기기",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 18,
        ["resonance"] = 0
      },
      ["delay_bp"] = 8500,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "sword",
      ["effects"] = {
        [1] = {
          ["kind"] = "parry",
          ["damage_bp"] = 6500,
          ["ttl"] = 2000,
          ["until"] = "owner_next_action_start",
          ["next_weapon_balance"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [10] = {
      ["id"] = "skill.raen.intercept_thrust",
      ["name"] = "견제 찌르기",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 20,
        ["resonance"] = 0
      },
      ["delay_bp"] = 9000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "spear",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 12500,
          ["balance"] = 2,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "balance_add",
          ["amount"] = 1,
          ["condition"] = "target_preparing"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [11] = {
      ["id"] = "skill.raen.resonance_cut",
      ["name"] = "공명 절단",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 24,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "RES",
          ["coefficient_bp"] = 13500,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1,
          ["fixed"] = 0
        },
        [2] = {
          ["kind"] = "dispel",
          ["count"] = 1,
          ["filter"] = "resonance_buff",
          ["fallback"] = {
            ["kind"] = "status",
            ["status"] = "status.wil_down",
            ["ttl"] = 1000,
            ["strength_bp"] = 1000
          }
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [12] = {
      ["id"] = "skill.raen.intervention_step",
      ["name"] = "개입 보법",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 18,
        ["resonance"] = 0
      },
      ["delay_bp"] = 7500,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 8000
        },
        [2] = {
          ["kind"] = "next_weak_sp",
          ["amount"] = 6,
          ["ttl"] = 2000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [13] = {
      ["id"] = "skill.raen.steady_stance",
      ["name"] = "안정 자세",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 22,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.def_up",
          ["ttl"] = 2000,
          ["strength_bp"] = 2000
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.wil_up",
          ["ttl"] = 2000,
          ["strength_bp"] = 2000
        },
        [3] = {
          ["kind"] = "cleanse",
          ["filter"] = "overload",
          ["count"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [14] = {
      ["id"] = "skill.raen.boundary_cut",
      ["name"] = "경계선 절단",
      ["owner"] = "char.raen",
      ["cost"] = {
        ["sp"] = 70,
        ["resonance"] = 0
      },
      ["delay_bp"] = 13500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "sword",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 24000,
          ["balance"] = 3,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "move",
          ["bp"] = 1500,
          ["condition"] = "target_preparing"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["ultimate"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [15] = {
      ["id"] = "skill.seria.precise_shot",
      ["name"] = "정밀 사격",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 18,
        ["resonance"] = 0
      },
      ["delay_bp"] = 8500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "bow",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 13000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "balance_add",
          ["amount"] = 1,
          ["condition"] = "known_weak_before_action"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [16] = {
      ["id"] = "skill.seria.cut_pulse",
      ["name"] = "절맥",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 20,
        ["resonance"] = 0
      },
      ["delay_bp"] = 8000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "dagger",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 13500,
          ["balance"] = 2,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "move",
          ["bp"] = 1000,
          ["condition"] = "weak_hit"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [17] = {
      ["id"] = "skill.seria.orbit_order",
      ["name"] = "선회 지시",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 22,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_front_other",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "move",
          ["bp"] = -2000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [18] = {
      ["id"] = "skill.seria.analyze",
      ["name"] = "표적 분석",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 16,
        ["resonance"] = 0
      },
      ["delay_bp"] = 7000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "reveal",
          ["weapon"] = 1,
          ["element"] = 1,
          ["fallback"] = "none"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [19] = {
      ["id"] = "skill.seria.cover_retreat",
      ["name"] = "엄호 후퇴",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 20,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "single_cover",
          ["damage_bp"] = 7500,
          ["ttl"] = 2000
        },
        [2] = {
          ["kind"] = "next_swap_interval",
          ["multiplier_bp"] = 8500,
          ["ttl"] = 2000,
          ["condition"] = "covered_target_hit"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [20] = {
      ["id"] = "skill.seria.blindspot_lock",
      ["name"] = "사각 봉쇄",
      ["owner"] = "char.seria",
      ["cost"] = {
        ["sp"] = 65,
        ["resonance"] = 0
      },
      ["delay_bp"] = 13500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "reveal",
          ["weapon"] = 99,
          ["element"] = 99,
          ["fallback"] = "none"
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.balance_exposed",
          ["ttl"] = 2000,
          ["strength_bp"] = 2500,
          ["boss_reorganize_strength_bp"] = 1000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["ultimate"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [21] = {
      ["id"] = "skill.kael.smash",
      ["name"] = "파쇄타",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 22,
        ["resonance"] = 0
      },
      ["delay_bp"] = 11500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "blunt",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 14500,
          ["balance"] = 3,
          ["element"] = "none",
          ["hits"] = 1,
          ["shield_bonus_bp"] = 2000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [22] = {
      ["id"] = "skill.kael.pierce",
      ["name"] = "관통창",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 20,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "spear",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 15500,
          ["balance"] = 2,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.def_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000,
          ["condition"] = "weak_hit"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [23] = {
      ["id"] = "skill.kael.overdrive",
      ["name"] = "과출력",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 25,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.atk_up",
          ["ttl"] = 2000,
          ["strength_bp"] = 2500
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.wil_down",
          ["ttl"] = 2000,
          ["strength_bp"] = 1500,
          ["self_inflicted"] = true
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [24] = {
      ["id"] = "skill.kael.repair",
      ["name"] = "긴급 정비",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 18,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "cleanse",
          ["filter"] = "ordinary_debuff",
          ["count"] = 1
        },
        [2] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 9000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [25] = {
      ["id"] = "skill.kael.release",
      ["name"] = "압력 해제",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 28,
        ["resonance"] = 0
      },
      ["delay_bp"] = 12500,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "blunt",
      ["effects"] = {
        [1] = {
          ["kind"] = "consume_atk_buff",
          ["damage_bonus_bp"] = 2000,
          ["balance_bonus"] = 1
        },
        [2] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 18000,
          ["balance"] = 2,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [26] = {
      ["id"] = "skill.kael.limit_line",
      ["name"] = "제어 한계선",
      ["owner"] = "char.kael",
      ["cost"] = {
        ["sp"] = 75,
        ["resonance"] = 0
      },
      ["delay_bp"] = 14000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "blunt",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 27000,
          ["balance"] = 3,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.def_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000,
          ["target"] = "self",
          ["self_inflicted"] = true
        },
        [3] = {
          ["kind"] = "status",
          ["status"] = "status.wil_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000,
          ["target"] = "self",
          ["self_inflicted"] = true
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["ultimate"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [27] = {
      ["id"] = "skill.mirea.clean_arrow",
      ["name"] = "정화 화살",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 18,
        ["resonance"] = 0
      },
      ["delay_bp"] = 9000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "bow",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 12000,
          ["balance"] = 1,
          ["element"] = "none",
          ["hits"] = 1,
          ["bonus_vs"] = {
            [1] = "residue",
            [2] = "polluted"
          },
          ["bonus_bp"] = 1500
        },
        [2] = {
          ["kind"] = "shorten_buff",
          ["filter"] = "resonance_buff",
          ["amount_tl"] = 500
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = true,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [28] = {
      ["id"] = "skill.mirea.heal",
      ["name"] = "순환 치유 · 인간",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 22,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "staff",
      ["effects"] = {
        [1] = {
          ["kind"] = "heal",
          ["scale"] = "human_res",
          ["coefficient_bp"] = 11000,
          ["fixed"] = 0
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [29] = {
      ["id"] = "skill.mirea.flow_away",
      ["name"] = "흘려보내기",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 20,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "single_cover",
          ["damage_bp"] = 8000,
          ["ttl"] = 2000,
          ["ally_resonance_on_hit"] = 4
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [30] = {
      ["id"] = "skill.mirea.cleanse",
      ["name"] = "공명 정화",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 24,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "cleanse_choice",
          ["choices"] = {
            [1] = {
              ["filter"] = "ordinary_debuff",
              ["count"] = 2
            },
            [2] = {
              ["filter"] = "overload",
              ["count"] = 1
            }
          },
          ["choice_required"] = true
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [31] = {
      ["id"] = "skill.mirea.slow_current",
      ["name"] = "완류",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 22,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "hot",
          ["coefficient_bp"] = 3500,
          ["ttl"] = 2000,
          ["tick"] = 1000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [32] = {
      ["id"] = "skill.mirea.clear_cycle",
      ["name"] = "맑은 순환",
      ["owner"] = "char.mirea",
      ["cost"] = {
        ["sp"] = 70,
        ["resonance"] = 0
      },
      ["delay_bp"] = 13500,
      ["target"] = "ally_front_all",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "heal",
          ["scale"] = "human_res",
          ["coefficient_bp"] = 9000,
          ["fixed"] = 0
        },
        [2] = {
          ["kind"] = "cleanse",
          ["filter"] = "ordinary_debuff",
          ["count"] = 1
        },
        [3] = {
          ["kind"] = "status",
          ["status"] = "status.wil_up",
          ["ttl"] = 1000,
          ["strength_bp"] = 1500
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["ultimate"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [33] = {
      ["id"] = "skill.rumo.shield",
      ["name"] = "공명막",
      ["owner"] = "contract.rumo",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "shield",
          ["formula"] = "shared",
          ["coefficient_bp"] = 10000,
          ["ttl"] = 2000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["trace_support"] = "earth",
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [34] = {
      ["id"] = "skill.rumo.anchor",
      ["name"] = "지각 고정",
      ["owner"] = "contract.rumo",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 30
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "direct_damage_multiplier",
          ["bp"] = 8000,
          ["ttl"] = 1000
        },
        [2] = {
          ["kind"] = "incoming_push_multiplier",
          ["bp"] = 7500,
          ["ttl"] = 1000
        }
      },
      ["unlock_rank"] = 2,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [35] = {
      ["id"] = "skill.rumo.echo",
      ["name"] = "결정 반향",
      ["owner"] = "contract.rumo",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 35
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 13500,
          ["balance"] = 2,
          ["element"] = "earth",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "balance_add",
          ["amount"] = 1,
          ["condition"] = "caster_or_own_shield_target_hit_last_1000"
        }
      },
      ["unlock_rank"] = 3,
      ["allow_critical"] = false,
      ["trace"] = "earth",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [36] = {
      ["id"] = "skill.rumo.double_wall",
      ["name"] = "쌍중 방벽",
      ["owner"] = "contract.rumo",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 80
      },
      ["delay_bp"] = 15000,
      ["target"] = "ally_front_all",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "shield",
          ["formula"] = "shared",
          ["coefficient_bp"] = 16000,
          ["ttl"] = 2000
        },
        [2] = {
          ["kind"] = "share_single",
          ["share_bp"] = 2500,
          ["recipient"] = "caster",
          ["ttl"] = 1000,
          ["limit"] = "first_single_hit_for_party",
          ["cannot_kill_recipient"] = true
        }
      },
      ["unlock_rank"] = 5,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = true,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [37] = {
      ["id"] = "skill.asel.accelerate",
      ["name"] = "선회가속",
      ["owner"] = "contract.asel",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 30
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_front_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 7500,
          ["ttl"] = 2000
        }
      },
      ["unlock_rank"] = 2,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [38] = {
      ["id"] = "skill.asel.feather",
      ["name"] = "절풍 깃",
      ["owner"] = "contract.asel",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 13000,
          ["balance"] = 2,
          ["element"] = "wind",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "wind",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [39] = {
      ["id"] = "skill.asel.scout",
      ["name"] = "상공 정찰",
      ["owner"] = "contract.asel",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "reveal",
          ["weapon"] = 1,
          ["element"] = 0,
          ["fallback"] = "element"
        },
        [2] = {
          ["kind"] = "reveal_intent_type"
        }
      },
      ["unlock_rank"] = 3,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [40] = {
      ["id"] = "skill.asel.rapid_turn",
      ["name"] = "푸른 급선회",
      ["owner"] = "contract.asel",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 80
      },
      ["delay_bp"] = 15000,
      ["target"] = "ally_front_all_plus_enemy",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 8500,
          ["ttl"] = 2000,
          ["target"] = "ally_front_all"
        },
        [2] = {
          ["kind"] = "move",
          ["bp"] = 1500,
          ["target"] = "selected_enemy"
        }
      },
      ["unlock_rank"] = 5,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = true,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [41] = {
      ["id"] = "skill.kara.burst",
      ["name"] = "불씨 폭발",
      ["owner"] = "contract.kara",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 30
      },
      ["delay_bp"] = 11500,
      ["target"] = "enemy_all",
      ["target_mode"] = "formation",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 15000,
          ["balance"] = 1,
          ["element"] = "fire",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 2,
      ["allow_critical"] = false,
      ["trace"] = "fire",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [42] = {
      ["id"] = "skill.kara.claws",
      ["name"] = "달궈진 발톱",
      ["owner"] = "contract.kara",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 6000,
          ["balance"] = 1,
          ["element"] = "fire",
          ["hits"] = 2
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.burn",
          ["ttl"] = 2000,
          ["tier"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [43] = {
      ["id"] = "skill.kara.ignite",
      ["name"] = "위험한 점화",
      ["owner"] = "contract.kara",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 35
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.atk_up",
          ["ttl"] = 1000,
          ["strength_bp"] = 2000
        },
        [2] = {
          ["kind"] = "expiry_recoil",
          ["current_hp_bp"] = 500,
          ["cannot_kill"] = true,
          ["ttl"] = 1000
        }
      },
      ["unlock_rank"] = 3,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [44] = {
      ["id"] = "skill.kara.red_limit",
      ["name"] = "붉은 한계선",
      ["owner"] = "contract.kara",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 80
      },
      ["delay_bp"] = 15000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 22000,
          ["balance"] = 3,
          ["element"] = "fire",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.burn",
          ["ttl"] = 2000,
          ["tier"] = 2
        },
        [3] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 11500,
          ["target"] = "self"
        }
      },
      ["unlock_rank"] = 5,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = true,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [45] = {
      ["id"] = "skill.nema.heal",
      ["name"] = "순환 치유 · 계약",
      ["owner"] = "contract.nema",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 30
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "heal",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 12500,
          ["fixed"] = 0
        }
      },
      ["unlock_rank"] = 2,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [46] = {
      ["id"] = "skill.nema.clean_stream",
      ["name"] = "맑은 흐름",
      ["owner"] = "contract.nema",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "cleanse",
          ["filter"] = "ordinary_debuff",
          ["count"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [47] = {
      ["id"] = "skill.nema.wave",
      ["name"] = "물결 흘리기",
      ["owner"] = "contract.nema",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 30
      },
      ["delay_bp"] = 10000,
      ["target"] = "ally_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "single_cover",
          ["damage_bp"] = 7500,
          ["ttl"] = 2000,
          ["attacker_push_bp"] = 800
        }
      },
      ["unlock_rank"] = 3,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [48] = {
      ["id"] = "skill.nema.water_lance",
      ["name"] = "수맥 찌르기",
      ["owner"] = "contract.nema",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 25
      },
      ["delay_bp"] = 10000,
      ["target"] = "enemy_one",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 13000,
          ["balance"] = 2,
          ["element"] = "water",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "water",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [49] = {
      ["id"] = "skill.nema.returning_water",
      ["name"] = "되돌아오는 물길",
      ["owner"] = "contract.nema",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 80
      },
      ["delay_bp"] = 15000,
      ["target"] = "ally_front_all",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "heal",
          ["scale"] = "CONTRACT",
          ["coefficient_bp"] = 12500,
          ["fixed"] = 0
        },
        [2] = {
          ["kind"] = "cleanse",
          ["filter"] = "ordinary_debuff",
          ["count"] = 1
        },
        [3] = {
          ["kind"] = "dot_multiplier",
          ["bp"] = 8000,
          ["ttl"] = 1000
        }
      },
      ["unlock_rank"] = 5,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = true,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false
    },
    [50] = {
      ["id"] = "skill.enemy.scratch",
      ["name"] = "침전 긁기",
      ["owner"] = "enemy.residue",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "front_slot",
      ["target_mode"] = "slot",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 10000,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [51] = {
      ["id"] = "skill.enemy.dust",
      ["name"] = "잔향 분진",
      ["owner"] = "enemy.residue",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "front_slot",
      ["target_mode"] = "slot",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.wil_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [52] = {
      ["id"] = "skill.enemy.harden",
      ["name"] = "갑각 굳히기",
      ["owner"] = "enemy.carapace",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.def_up",
          ["ttl"] = 1000,
          ["strength_bp"] = 2500
        },
        [2] = {
          ["kind"] = "balance_restore",
          ["amount"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [53] = {
      ["id"] = "skill.enemy.drill",
      ["name"] = "굴착 강타",
      ["owner"] = "enemy.carapace",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 12500,
      ["target"] = "front_slot",
      ["target_mode"] = "slot",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 14500,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 1,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [54] = {
      ["id"] = "skill.enemy.memory",
      ["name"] = "기억 파동",
      ["owner"] = "enemy.sensor",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "human_entity_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "RES",
          ["coefficient_bp"] = 12000,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.wil_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["can_hit_rear"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [55] = {
      ["id"] = "skill.enemy.mark",
      ["name"] = "잔향 표식",
      ["owner"] = "enemy.sensor",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "human_entity_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.echo_mark",
          ["ttl"] = 2000,
          ["strength_bp"] = 1500,
          ["source_restricted"] = true,
          ["consume_on"] = "next_memory_hit"
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["can_hit_rear"] = true,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [56] = {
      ["id"] = "skill.core.needle",
      ["name"] = "추적침",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 11000,
      ["target"] = "human_entity_any",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "RES",
          ["coefficient_bp"] = 15500,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.wil_down",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["can_hit_rear"] = true,
      ["telegraph"] = 1,
      ["telegraph_name"] = "표적 고정",
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [57] = {
      ["id"] = "skill.core.pressure",
      ["name"] = "압력 분출",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 12000,
      ["target"] = "front_slot",
      ["target_mode"] = "slot",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 14000,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 1,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [58] = {
      ["id"] = "skill.core.pressure_ii",
      ["name"] = "압력 분출 · 강화",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 12500,
      ["target"] = "front_slot",
      ["target_mode"] = "slot",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "ATK",
          ["coefficient_bp"] = 16000,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 1,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [59] = {
      ["id"] = "skill.core.rectifier",
      ["name"] = "정류막",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "shield",
          ["formula"] = "fixed",
          ["amount"] = 220,
          ["coefficient_bp"] = 0,
          ["ttl"] = 2000,
          ["dispel_group"] = "resonance_buff"
        },
        [2] = {
          ["kind"] = "balance_restore",
          ["amount"] = 2
        },
        [3] = {
          ["kind"] = "recovery_bonus",
          ["amount"] = 1,
          ["ttl"] = 2000
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    },
    [60] = {
      ["id"] = "skill.core.backflow",
      ["name"] = "침전 역류",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 15000,
      ["target"] = "human_front_all",
      ["target_mode"] = "formation",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "damage",
          ["scale"] = "RES",
          ["coefficient_bp"] = 21000,
          ["balance"] = 0,
          ["element"] = "none",
          ["hits"] = 1
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.overload",
          ["ttl"] = 2000,
          ["tier"] = 1,
          ["major"] = true
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 2,
      ["indomitable"] = true,
      ["can_choose_rear_initially"] = false
    },
    [61] = {
      ["id"] = "skill.core.amplify",
      ["name"] = "잔향 증폭",
      ["owner"] = "enemy.core",
      ["cost"] = {
        ["sp"] = 0,
        ["resonance"] = 0
      },
      ["delay_bp"] = 10000,
      ["target"] = "self",
      ["target_mode"] = "entity",
      ["weapon"] = "none",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.atk_up",
          ["ttl"] = 2000,
          ["strength_bp"] = 1500
        },
        [2] = {
          ["kind"] = "status",
          ["status"] = "status.res_up",
          ["ttl"] = 2000,
          ["strength_bp"] = 1500
        }
      },
      ["unlock_rank"] = 0,
      ["allow_critical"] = false,
      ["trace"] = "none",
      ["trace_ttl"] = 1000,
      ["once_per_bond"] = false,
      ["post_recoil"] = false,
      ["telegraph"] = 0,
      ["indomitable"] = false,
      ["can_choose_rear_initially"] = false
    }
  },
  ["passives"] = {
    [1] = {
      ["id"] = "passive.raen",
      ["name"] = "경계 감응",
      ["scope"] = "front",
      ["trigger"] = "first_direct_hit_per_enemy_intent",
      ["effects"] = {
        [1] = {
          ["kind"] = "balance_add",
          ["amount"] = 1,
          ["condition"] = "target_preparing"
        }
      },
      ["limit"] = "per_intent_id"
    },
    [2] = {
      ["id"] = "passive.seria",
      ["name"] = "현장 판독",
      ["scope"] = "front",
      ["trigger"] = "first_direct_hit_per_enemy_instance",
      ["effects"] = {
        [1] = {
          ["kind"] = "reveal",
          ["weapon"] = 1,
          ["element"] = 0,
          ["fallback"] = "element"
        }
      },
      ["limit"] = "per_battle_target"
    },
    [3] = {
      ["id"] = "passive.kael",
      ["name"] = "제어된 출력",
      ["scope"] = "front",
      ["trigger"] = "human_skill_base_delay_at_least_12000",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_basic",
          ["sp_bonus_bp"] = 400,
          ["balance_bonus_on_weak"] = 1
        }
      },
      ["limit"] = "replace_not_stack"
    },
    [4] = {
      ["id"] = "passive.mirea",
      ["name"] = "순환 감응",
      ["scope"] = "front",
      ["trigger"] = "effective_heal_or_cleanse",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.wil_up",
          ["ttl"] = 1000,
          ["strength_bp"] = 1000,
          ["target"] = "effect_recipient"
        }
      }
    },
    [5] = {
      ["id"] = "passive.rumo",
      ["name"] = "안정된 통로",
      ["scope"] = "front",
      ["trigger"] = "continuous",
      ["effects"] = {
        [1] = {
          ["kind"] = "overload_strength_multiplier",
          ["bp"] = 8500
        },
        [2] = {
          ["kind"] = "stat_pct",
          ["stat"] = "WIL",
          ["bp"] = 500,
          ["condition"] = "has_shield"
        }
      }
    },
    [6] = {
      ["id"] = "passive.asel",
      ["name"] = "상승기류",
      ["scope"] = "front",
      ["trigger"] = "manual_or_automatic_front_entry",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_interval",
          ["multiplier_bp"] = 9000
        }
      },
      ["stack_group"] = "entry_interval"
    },
    [7] = {
      ["id"] = "passive.kara",
      ["name"] = "잔열",
      ["scope"] = "front",
      ["trigger"] = "weak_hit",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_fire_damage",
          ["bonus_bp"] = 1000,
          ["ttl"] = 2000
        }
      },
      ["limit"] = "replace_not_stack"
    },
    [8] = {
      ["id"] = "passive.nema",
      ["name"] = "순환수",
      ["scope"] = "front",
      ["trigger"] = "effective_heal_or_cleanse",
      ["effects"] = {
        [1] = {
          ["kind"] = "next_dot_multiplier",
          ["bp"] = 8000,
          ["target"] = "effect_recipient",
          ["ttl"] = 2000
        }
      }
    },
    [9] = {
      ["id"] = "passive.imprint_rumo",
      ["name"] = "나눈 무게",
      ["scope"] = "any",
      ["trigger"] = "lethal_single_direct_hit_if_hp_at_least_30pct",
      ["effects"] = {
        [1] = {
          ["kind"] = "flat_damage_reduce",
          ["owner_max_hp_bp"] = 2000
        }
      },
      ["limit"] = "once_per_battle"
    },
    [10] = {
      ["id"] = "passive.imprint_asel",
      ["name"] = "선회 예측",
      ["scope"] = "front",
      ["trigger"] = "first_swap_with_nearest_intent_slot",
      ["effects"] = {
        [1] = {
          ["kind"] = "single_cover",
          ["damage_bp"] = 8500,
          ["ttl"] = 2000
        }
      },
      ["limit"] = "once_per_battle"
    },
    [11] = {
      ["id"] = "passive.imprint_kara",
      ["name"] = "제어된 과열",
      ["scope"] = "front",
      ["trigger"] = "natural_expiry_atk_buff",
      ["effects"] = {
        [1] = {
          ["kind"] = "sp_add",
          ["amount"] = 8
        }
      },
      ["limit"] = "twice_per_battle"
    },
    [12] = {
      ["id"] = "passive.imprint_nema",
      ["name"] = "순환 정화",
      ["scope"] = "front",
      ["trigger"] = "first_effective_cleanse",
      ["effects"] = {
        [1] = {
          ["kind"] = "sp_add",
          ["amount"] = 10
        }
      },
      ["limit"] = "once_per_battle"
    }
  },
  ["statuses"] = {
    [1] = {
      ["id"] = "status.atk_up",
      ["name"] = "공격 강화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "ATK",
      ["strength_bp"] = 1500,
      ["beneficial"] = true,
      ["cleanse"] = "buff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [2] = {
      ["id"] = "status.atk_down",
      ["name"] = "공격 약화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "ATK",
      ["strength_bp"] = -1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [3] = {
      ["id"] = "status.res_up",
      ["name"] = "공명 강화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "RES",
      ["strength_bp"] = 1500,
      ["beneficial"] = true,
      ["cleanse"] = "buff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [4] = {
      ["id"] = "status.res_down",
      ["name"] = "공명 약화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "RES",
      ["strength_bp"] = -1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [5] = {
      ["id"] = "status.def_up",
      ["name"] = "방어 강화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "DEF",
      ["strength_bp"] = 1500,
      ["beneficial"] = true,
      ["cleanse"] = "buff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [6] = {
      ["id"] = "status.def_down",
      ["name"] = "방어 약화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "DEF",
      ["strength_bp"] = -1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [7] = {
      ["id"] = "status.wil_up",
      ["name"] = "의지 강화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "WIL",
      ["strength_bp"] = 1500,
      ["beneficial"] = true,
      ["cleanse"] = "buff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [8] = {
      ["id"] = "status.wil_down",
      ["name"] = "의지 약화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "WIL",
      ["strength_bp"] = -1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [9] = {
      ["id"] = "status.haste",
      ["name"] = "가속",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "interval",
      ["strength_bp"] = -1500,
      ["beneficial"] = true,
      ["cleanse"] = "buff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [10] = {
      ["id"] = "status.slow",
      ["name"] = "둔화",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "interval",
      ["strength_bp"] = 1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["operator"] = "stat_delta_bp_or_interval_delta_bp; status sign determines debuff direction"
      }
    },
    [11] = {
      ["id"] = "status.burn",
      ["name"] = "화상",
      ["clock"] = "dot",
      ["ttl"] = 2000,
      ["tick"] = 1000,
      ["tier_max"] = 2,
      ["stat"] = "none",
      ["strength_bp"] = 0,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["coefficient_bp"] = {
          [1] = 3500,
          [2] = 5500
        },
        ["source_snapshot"] = true,
        ["target_wil_now"] = true,
        ["ignore_guard_shield"] = true,
        ["no_reactions"] = true
      }
    },
    [12] = {
      ["id"] = "status.corrosion",
      ["name"] = "부식",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 2,
      ["stat"] = "none",
      ["strength_bp"] = 0,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["def_wil_delta_bp"] = {
          [1] = -1000,
          [2] = -2000
        },
        ["healing_received_delta_bp"] = {
          [1] = -1500,
          [2] = -3000
        }
      }
    },
    [13] = {
      ["id"] = "status.frost",
      ["name"] = "빙결",
      ["clock"] = "next_action",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "interval",
      ["strength_bp"] = 3500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["next_interval_bp"] = 13500,
        ["boss_interval_bp"] = 12000,
        ["balance_received_bp"] = 12500,
        ["consume_on"] = "owner_action_end"
      }
    },
    [14] = {
      ["id"] = "status.bind",
      ["name"] = "속박",
      ["clock"] = "next_action",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "interval",
      ["strength_bp"] = 2000,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["next_interval_bp"] = 12000,
        ["blocks_direct_pull"] = true,
        ["consume_on"] = "owner_action_end"
      }
    },
    [15] = {
      ["id"] = "status.steam",
      ["name"] = "증기",
      ["clock"] = "next_action",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "interval",
      ["strength_bp"] = 1000,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["next_interval_bp"] = 11000,
        ["balance_outgoing_bp"] = 5000,
        ["consume_on"] = "owner_action_end"
      }
    },
    [16] = {
      ["id"] = "status.lava",
      ["name"] = "용암",
      ["clock"] = "dot",
      ["ttl"] = 2000,
      ["tick"] = 1000,
      ["tier_max"] = 1,
      ["stat"] = "none",
      ["strength_bp"] = 0,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["coefficient_bp"] = {
          [1] = 4500
        },
        ["source_snapshot"] = true,
        ["target_wil_now"] = true,
        ["balance_recovery_bp"] = 5000,
        ["ignore_guard_shield"] = true,
        ["no_reactions"] = true
      }
    },
    [17] = {
      ["id"] = "status.overload",
      ["name"] = "공명 과부하",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 2,
      ["stat"] = "none",
      ["strength_bp"] = 0,
      ["beneficial"] = false,
      ["cleanse"] = "overload",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["resonance_gain_bp"] = {
          [1] = 7500,
          [2] = 5000
        },
        ["contract_cost_bp"] = {
          [1] = 11000,
          [2] = 12500
        },
        ["res_wil_delta_bp"] = {
          [1] = 0,
          [2] = -1500
        }
      }
    },
    [18] = {
      ["id"] = "status.stun",
      ["name"] = "기절",
      ["clock"] = "next_action",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "skip",
      ["strength_bp"] = 1,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["skip_opportunities"] = 1,
        ["before_swap"] = true,
        ["boss_immune_unless_flag"] = true
      }
    },
    [19] = {
      ["id"] = "status.balance_exposed",
      ["name"] = "균형 노출",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "balance",
      ["strength_bp"] = 2500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "enemy",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = false,
      ["mechanics"] = {
        ["boss_reorganize_strength_bp"] = 1000
      }
    },
    [20] = {
      ["id"] = "status.echo_mark",
      ["name"] = "잔향 표식",
      ["clock"] = "timed",
      ["ttl"] = 2000,
      ["tick"] = 0,
      ["tier_max"] = 1,
      ["stat"] = "incoming_res_damage",
      ["strength_bp"] = 1500,
      ["beneficial"] = false,
      ["cleanse"] = "ordinary_debuff",
      ["target_restriction"] = "any",
      ["stack"] = "highest_then_refresh_no_duration_sum",
      ["wil_scaled"] = true,
      ["mechanics"] = {
        ["skill_filter"] = "skill.enemy.memory",
        ["same_source_only"] = true,
        ["charges"] = 1
      }
    }
  },
  ["equipment"] = {
    [1] = {
      ["id"] = "equip.field_sword",
      ["name"] = "현장검 II",
      ["slot"] = "weapon",
      ["weapon"] = "sword",
      ["attack"] = 35,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "balance_add",
          ["amount"] = 0.5,
          ["condition"] = "target_preparing"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [2] = {
      ["id"] = "equip.guard_spear",
      ["name"] = "경비창 II",
      ["slot"] = "weapon",
      ["weapon"] = "spear",
      ["attack"] = 34,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "action_atk_bonus",
          ["bp"] = 500,
          ["condition"] = "target_intent_slot"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [3] = {
      ["id"] = "equip.survey_dagger",
      ["name"] = "조사단검 II",
      ["slot"] = "weapon",
      ["weapon"] = "dagger",
      ["attack"] = 30,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "after_weak_next_interval",
          ["multiplier_bp"] = 9500
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [4] = {
      ["id"] = "equip.patrol_bow",
      ["name"] = "순찰복합궁 II",
      ["slot"] = "weapon",
      ["weapon"] = "bow",
      ["attack"] = 32,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "damage_bonus",
          ["bp"] = 800,
          ["condition"] = "known_weak_before_action"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [5] = {
      ["id"] = "equip.repair_hammer",
      ["name"] = "정비망치 II",
      ["slot"] = "weapon",
      ["weapon"] = "blunt",
      ["attack"] = 38,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_sp_delta",
          ["delta"] = -2,
          ["condition"] = "base_balance_at_least_3"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [6] = {
      ["id"] = "equip.industrial_spear",
      ["name"] = "공업창 II",
      ["slot"] = "weapon",
      ["weapon"] = "spear",
      ["attack"] = 34,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "damage_bonus",
          ["bp"] = 600,
          ["condition"] = "caster_atk_buff"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [7] = {
      ["id"] = "equip.forest_bow",
      ["name"] = "수림궁 II",
      ["slot"] = "weapon",
      ["weapon"] = "bow",
      ["attack"] = 32,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "damage_bonus",
          ["bp"] = 800,
          ["condition"] = "target_residue_or_polluted"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [8] = {
      ["id"] = "equip.resonance_staff",
      ["name"] = "공명봉 II",
      ["slot"] = "weapon",
      ["weapon"] = "staff",
      ["attack"] = 28,
      ["stats"] = {
        ["RES"] = 8
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_sp_delta",
          ["delta"] = -2,
          ["condition"] = "skill_heal"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [9] = {
      ["id"] = "equip.border_armor",
      ["name"] = "변경 경갑",
      ["slot"] = "armor",
      ["attack"] = 0,
      ["stats"] = {
        ["DEF"] = 14,
        ["WIL"] = 6
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "physical_damage_multiplier",
          ["bp"] = 9700
        }
      },
      ["price"] = 800,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [10] = {
      ["id"] = "equip.survey_coat",
      ["name"] = "조사외투",
      ["slot"] = "armor",
      ["attack"] = 0,
      ["stats"] = {
        ["DEF"] = 8,
        ["WIL"] = 10,
        ["SPD"] = 2
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_delay_multiplier",
          ["bp"] = 9500,
          ["condition"] = "skill_analysis"
        }
      },
      ["price"] = 800,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [11] = {
      ["id"] = "equip.heat_armor",
      ["name"] = "내열 작업갑",
      ["slot"] = "armor",
      ["attack"] = 0,
      ["stats"] = {
        ["DEF"] = 12,
        ["WIL"] = 8
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "burn_damage_multiplier",
          ["bp"] = 9000
        }
      },
      ["price"] = 800,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [12] = {
      ["id"] = "equip.coexist_robe",
      ["name"] = "공존 수의",
      ["slot"] = "armor",
      ["attack"] = 0,
      ["stats"] = {
        ["DEF"] = 9,
        ["WIL"] = 14,
        ["RES"] = 4
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "cleanse_wil_buff_ttl_bonus",
          ["tl"] = 250
        }
      },
      ["price"] = 800,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = false
    },
    [13] = {
      ["id"] = "equip.stable_knot",
      ["name"] = "안정 매듭",
      ["slot"] = "accessory",
      ["attack"] = 0,
      ["stats"] = {
        ["WIL"] = 6
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "overload_strength_multiplier",
          ["bp"] = 9500
        }
      },
      ["price"] = 300,
      ["tier"] = 2,
      ["max_upgrade"] = 0,
      ["unique"] = false
    },
    [14] = {
      ["id"] = "equip.balance_weight",
      ["name"] = "균형 추",
      ["slot"] = "accessory",
      ["attack"] = 0,
      ["stats"] = {
        ["ATK"] = 4
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "balance_add",
          ["amount"] = 0.5,
          ["condition"] = "weak_hit"
        }
      },
      ["price"] = 300,
      ["tier"] = 2,
      ["max_upgrade"] = 0,
      ["unique"] = false
    },
    [15] = {
      ["id"] = "equip.light_cord",
      ["name"] = "경량 끈",
      ["slot"] = "accessory",
      ["attack"] = 0,
      ["stats"] = {
        ["SPD"] = 3
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "next_entry_interval",
          ["multiplier_bp"] = 9500
        }
      },
      ["price"] = 300,
      ["tier"] = 2,
      ["max_upgrade"] = 0,
      ["unique"] = false
    },
    [16] = {
      ["id"] = "equip.clean_stone",
      ["name"] = "정화석",
      ["slot"] = "accessory",
      ["attack"] = 0,
      ["stats"] = {
        ["RES"] = 5
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "dot_multiplier",
          ["bp"] = 9000
        }
      },
      ["price"] = 300,
      ["tier"] = 2,
      ["max_upgrade"] = 0,
      ["unique"] = false
    },
    [17] = {
      ["id"] = "equip.sediment_sword",
      ["name"] = "침전 차단검",
      ["slot"] = "weapon",
      ["weapon"] = "sword",
      ["attack"] = 39,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "balance_add",
          ["amount"] = 1,
          ["condition"] = "target_preparing"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = true
    },
    [18] = {
      ["id"] = "equip.echo_dagger",
      ["name"] = "잔향 추적단검",
      ["slot"] = "weapon",
      ["weapon"] = "dagger",
      ["attack"] = 34,
      ["stats"] = {

      },
      ["effects"] = {
        [1] = {
          ["kind"] = "after_reveal_next_damage",
          ["bp"] = 1000,
          ["ttl"] = 2000
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = true
    },
    [19] = {
      ["id"] = "equip.stable_staff",
      ["name"] = "안정 공명봉",
      ["slot"] = "weapon",
      ["weapon"] = "staff",
      ["attack"] = 30,
      ["stats"] = {
        ["RES"] = 12
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_sp_delta",
          ["delta"] = -4,
          ["condition"] = "skill.mirea.cleanse"
        }
      },
      ["price"] = 900,
      ["tier"] = 2,
      ["max_upgrade"] = 3,
      ["unique"] = true
    }
  },
  ["items"] = {
    [1] = {
      ["id"] = "item.hp_potion",
      ["name"] = "회복약",
      ["price"] = 60,
      ["target"] = "ally_any",
      ["delay_bp"] = 7500,
      ["effects"] = {
        [1] = {
          ["kind"] = "heal_fixed",
          ["amount"] = 300
        }
      },
      ["max_stack"] = 20,
      ["battle_limit"] = 3
    },
    [2] = {
      ["id"] = "item.sp_tonic",
      ["name"] = "기력제",
      ["price"] = 75,
      ["target"] = "ally_any",
      ["delay_bp"] = 7500,
      ["effects"] = {
        [1] = {
          ["kind"] = "sp_add",
          ["amount"] = 25
        }
      },
      ["max_stack"] = 20,
      ["battle_limit"] = 3
    },
    [3] = {
      ["id"] = "item.cleanser",
      ["name"] = "정화제",
      ["price"] = 90,
      ["target"] = "ally_any",
      ["delay_bp"] = 7500,
      ["effects"] = {
        [1] = {
          ["kind"] = "cleanse",
          ["filter"] = "ordinary_debuff",
          ["count"] = 1
        }
      },
      ["max_stack"] = 20,
      ["battle_limit"] = 3
    },
    [4] = {
      ["id"] = "item.reviver",
      ["name"] = "응급 소생제",
      ["price"] = 180,
      ["target"] = "ally_dead",
      ["delay_bp"] = 12500,
      ["effects"] = {
        [1] = {
          ["kind"] = "revive",
          ["hp_bp"] = 2500,
          ["resonance"] = 0,
          ["swap_lock"] = 500
        }
      },
      ["max_stack"] = 20,
      ["battle_limit"] = 3
    },
    [5] = {
      ["id"] = "material.echo_powder",
      ["name"] = "정제 잔향가루",
      ["price"] = 10,
      ["max_stack"] = 999,
      ["quest_material"] = false,
      ["repeatable"] = true
    },
    [6] = {
      ["id"] = "material.sediment_crystal",
      ["name"] = "침전 결정편",
      ["price"] = 40,
      ["max_stack"] = 999,
      ["quest_material"] = false,
      ["repeatable"] = false,
      ["sellable_in_demo"] = false
    },
    [7] = {
      ["id"] = "material.rectifier_plate",
      ["name"] = "정류판 조각",
      ["price"] = 30,
      ["max_stack"] = 999,
      ["quest_material"] = false,
      ["repeatable"] = true
    },
    [8] = {
      ["id"] = "material.drill_iron",
      ["name"] = "굴착철편",
      ["price"] = 15,
      ["max_stack"] = 999,
      ["quest_material"] = false,
      ["repeatable"] = true
    },
    [9] = {
      ["id"] = "material.clear_resin",
      ["name"] = "맑은 수지",
      ["price"] = 10,
      ["max_stack"] = 999,
      ["quest_material"] = false,
      ["repeatable"] = true
    }
  },
  ["recipes"] = {
    [1] = {
      ["id"] = "recipe.sediment_sword",
      ["base"] = "equip.field_sword",
      ["result"] = "equip.sediment_sword",
      ["gold"] = 250,
      ["materials"] = {
        ["material.rectifier_plate"] = 1,
        ["material.drill_iron"] = 2,
        ["material.sediment_crystal"] = 1
      },
      ["preserve_upgrade"] = true,
      ["atomic"] = true
    },
    [2] = {
      ["id"] = "recipe.echo_dagger",
      ["base"] = "equip.survey_dagger",
      ["result"] = "equip.echo_dagger",
      ["gold"] = 250,
      ["materials"] = {
        ["material.echo_powder"] = 3,
        ["material.sediment_crystal"] = 1
      },
      ["preserve_upgrade"] = true,
      ["atomic"] = true
    },
    [3] = {
      ["id"] = "recipe.stable_staff",
      ["base"] = "equip.resonance_staff",
      ["result"] = "equip.stable_staff",
      ["gold"] = 250,
      ["materials"] = {
        ["material.clear_resin"] = 2,
        ["material.sediment_crystal"] = 1
      },
      ["preserve_upgrade"] = true,
      ["atomic"] = true
    }
  },
  ["enemies"] = {
    [1] = {
      ["id"] = "enemy.residue",
      ["name"] = "침전 잔향충",
      ["type"] = "residue",
      ["stats"] = {
        ["HP"] = 650,
        ["ATK"] = 95,
        ["RES"] = 70,
        ["DEF"] = 45,
        ["WIL"] = 55,
        ["SPD"] = 42
      },
      ["balance"] = 4,
      ["weapon_weak"] = {
        [1] = "sword"
      },
      ["element_weak"] = {
        [1] = "wind"
      },
      ["resist"] = {
        [1] = "earth"
      },
      ["boss"] = false,
      ["immunities"] = {},
      ["ai"] = "ai.residue"
    },
    [2] = {
      ["id"] = "enemy.carapace",
      ["name"] = "광맥 갑각수",
      ["type"] = "beast",
      ["stats"] = {
        ["HP"] = 1100,
        ["ATK"] = 125,
        ["RES"] = 60,
        ["DEF"] = 80,
        ["WIL"] = 55,
        ["SPD"] = 24
      },
      ["balance"] = 6,
      ["weapon_weak"] = {
        [1] = "blunt"
      },
      ["element_weak"] = {
        [1] = "fire"
      },
      ["resist"] = {
        [1] = "sword",
        [2] = "earth"
      },
      ["boss"] = false,
      ["immunities"] = {},
      ["ai"] = "ai.carapace"
    },
    [3] = {
      ["id"] = "enemy.sensor",
      ["name"] = "폐갱 감응체",
      ["type"] = "residue",
      ["stats"] = {
        ["HP"] = 900,
        ["ATK"] = 80,
        ["RES"] = 110,
        ["DEF"] = 55,
        ["WIL"] = 80,
        ["SPD"] = 35
      },
      ["balance"] = 5,
      ["weapon_weak"] = {
        [1] = "bow"
      },
      ["element_weak"] = {
        [1] = "water",
        [2] = "light"
      },
      ["resist"] = {
        [1] = "shadow"
      },
      ["boss"] = false,
      ["immunities"] = {},
      ["ai"] = "ai.sensor"
    },
    [4] = {
      ["id"] = "enemy.core",
      ["name"] = "침전 조율핵",
      ["type"] = "construct",
      ["stats"] = {
        ["HP"] = 5200,
        ["ATK"] = 165,
        ["RES"] = 160,
        ["DEF"] = 95,
        ["WIL"] = 90,
        ["SPD"] = 32
      },
      ["balance"] = 18,
      ["weapon_weak"] = {
        [1] = "spear"
      },
      ["element_weak"] = {
        [1] = "water"
      },
      ["resist"] = {
        [1] = "shadow"
      },
      ["boss"] = true,
      ["immunities"] = {
        [1] = "stun"
      },
      ["ai"] = "ai.core"
    }
  },
  ["encounters"] = {
    [1] = {
      ["id"] = "encounter.B",
      ["room"] = "B",
      ["enemies"] = {
        [1] = "enemy.residue",
        [2] = "enemy.residue"
      },
      ["gold"] = 65,
      ["human_exp"] = 90,
      ["contract_exp"] = 90,
      ["materials"] = {
        ["material.echo_powder"] = 2
      },
      ["can_flee"] = false,
      ["elite"] = false,
      ["reward_id"] = "reward.B",
      ["repeatable"] = false,
      ["stat_bp"] = 10000,
      ["hp_bp"] = 10000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    },
    [2] = {
      ["id"] = "encounter.C1",
      ["room"] = "C1",
      ["enemies"] = {
        [1] = "enemy.sensor",
        [2] = "enemy.sensor"
      },
      ["gold"] = 75,
      ["human_exp"] = 110,
      ["contract_exp"] = 110,
      ["materials"] = {
        ["material.echo_powder"] = 2
      },
      ["can_flee"] = true,
      ["elite"] = false,
      ["reward_id"] = "reward.C1",
      ["repeatable"] = false,
      ["stat_bp"] = 10000,
      ["hp_bp"] = 10000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    },
    [3] = {
      ["id"] = "encounter.C2",
      ["room"] = "C2",
      ["enemies"] = {
        [1] = "enemy.carapace",
        [2] = "enemy.residue"
      },
      ["gold"] = 65,
      ["human_exp"] = 110,
      ["contract_exp"] = 110,
      ["materials"] = {
        ["material.rectifier_plate"] = 2,
        ["material.drill_iron"] = 2,
        ["material.echo_powder"] = 1
      },
      ["can_flee"] = false,
      ["elite"] = false,
      ["reward_id"] = "reward.C2",
      ["repeatable"] = false,
      ["stat_bp"] = 10000,
      ["hp_bp"] = 10000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    },
    [4] = {
      ["id"] = "encounter.elite",
      ["room"] = "D",
      ["enemies"] = {
        [1] = "enemy.sensor",
        [2] = "enemy.carapace"
      },
      ["gold"] = 85,
      ["human_exp"] = 170,
      ["contract_exp"] = 170,
      ["materials"] = {
        ["material.rectifier_plate"] = 1,
        ["material.drill_iron"] = 1,
        ["material.echo_powder"] = 1
      },
      ["can_flee"] = true,
      ["elite"] = true,
      ["reward_id"] = "reward.elite",
      ["repeatable"] = false,
      ["stat_bp"] = 11000,
      ["hp_bp"] = 11000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    },
    [5] = {
      ["id"] = "encounter.E",
      ["room"] = "E",
      ["enemies"] = {
        [1] = "enemy.residue",
        [2] = "enemy.carapace",
        [3] = "enemy.sensor"
      },
      ["gold"] = 65,
      ["human_exp"] = 150,
      ["contract_exp"] = 150,
      ["materials"] = {
        ["material.drill_iron"] = 1,
        ["material.echo_powder"] = 2
      },
      ["can_flee"] = false,
      ["elite"] = false,
      ["reward_id"] = "reward.E",
      ["repeatable"] = false,
      ["stat_bp"] = 10000,
      ["hp_bp"] = 10000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    },
    [6] = {
      ["id"] = "encounter.F",
      ["room"] = "F",
      ["enemies"] = {
        [1] = "enemy.core"
      },
      ["gold"] = 300,
      ["human_exp"] = 400,
      ["contract_exp"] = 400,
      ["materials"] = {
        ["material.sediment_crystal"] = 1
      },
      ["can_flee"] = false,
      ["elite"] = false,
      ["reward_id"] = "reward.F",
      ["repeatable"] = false,
      ["stat_bp"] = 10000,
      ["hp_bp"] = 10000,
      ["stat_fields"] = {
        [1] = "ATK",
        [2] = "RES"
      },
      ["hp_field"] = "HP"
    }
  },
  ["presets"] = {
    [1] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {

      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.accelerate",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.heal",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.public",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = nil
        },
        [2] = {
          ["front"] = "char.seria",
          ["rear"] = nil
        },
        [3] = {
          ["front"] = "char.kael",
          ["rear"] = nil
        },
        [4] = {
          ["front"] = "char.mirea",
          ["rear"] = nil
        }
      },
      ["encounter"] = nil,
      ["start_room"] = "A",
      ["run_type"] = "full"
    },
    [2] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {

      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.accelerate",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.heal",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.combat",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = nil
        },
        [2] = {
          ["front"] = "char.seria",
          ["rear"] = nil
        },
        [3] = {
          ["front"] = "char.kael",
          ["rear"] = nil
        },
        [4] = {
          ["front"] = "char.mirea",
          ["rear"] = nil
        }
      },
      ["encounter"] = "encounter.B"
    },
    [3] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {

      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.accelerate",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.heal",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.swap",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = "char.kael"
        },
        [2] = {
          ["front"] = "char.mirea",
          ["rear"] = "char.seria"
        },
        [3] = {
          ["front"] = nil,
          ["rear"] = nil
        },
        [4] = {
          ["front"] = nil,
          ["rear"] = nil
        }
      },
      ["encounter"] = "encounter.C2",
      ["tutorial"] = "slot_then_swap"
    },
    [4] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {

      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.feather",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.water_lance",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.reaction",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = nil
        },
        [2] = {
          ["front"] = "char.seria",
          ["rear"] = nil
        },
        [3] = {
          ["front"] = "char.kael",
          ["rear"] = nil
        },
        [4] = {
          ["front"] = "char.mirea",
          ["rear"] = nil
        }
      },
      ["encounter"] = "encounter.C1",
      ["tutorial"] = "water_then_wind",
      ["ready_override"] = {
        ["char.mirea"] = 400,
        ["char.seria"] = 500,
        ["enemy.sensor.1"] = 1500,
        ["enemy.sensor.2"] = 1600
      }
    },
    [5] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {

      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.feather",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.water_lance",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {
        ["route"] = "purify",
        ["c1_read"] = true
      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.boss",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = "char.kael"
        },
        [2] = {
          ["front"] = "char.mirea",
          ["rear"] = "char.seria"
        },
        [3] = {
          ["front"] = nil,
          ["rear"] = nil
        },
        [4] = {
          ["front"] = nil,
          ["rear"] = nil
        }
      },
      ["encounter"] = "encounter.F"
    },
    [6] = {
      ["level_lock"] = true,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 900,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {
        ["material.echo_powder"] = 3,
        ["material.drill_iron"] = 3,
        ["material.rectifier_plate"] = 2,
        ["material.clear_resin"] = 2,
        ["material.sediment_crystal"] = 1
      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.accelerate",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.heal",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.economy",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = nil
        },
        [2] = {
          ["front"] = "char.seria",
          ["rear"] = nil
        },
        [3] = {
          ["front"] = "char.kael",
          ["rear"] = nil
        },
        [4] = {
          ["front"] = "char.mirea",
          ["rear"] = nil
        }
      },
      ["encounter"] = nil,
      ["start_room"] = "A"
    },
    [7] = {
      ["level_lock"] = false,
      ["rewards_to_ledger"] = true,
      ["mode"] = "demo",
      ["gold"] = 400,
      ["items"] = {
        ["item.hp_potion"] = 2,
        ["item.sp_tonic"] = 1,
        ["item.cleanser"] = 1,
        ["item.reviver"] = 1
      },
      ["materials"] = {
        ["material.clear_resin"] = 2
      },
      ["units"] = {
        [1] = {
          ["id"] = "char.raen",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.raen.C1",
            [2] = "node.raen.C2",
            [3] = "node.raen.C3",
            [4] = "node.raen.C4",
            [5] = "node.raen.A1",
            [6] = "node.raen.B1",
            [7] = "node.raen.A2"
          },
          ["equipment"] = {
            [1] = "equip.field_sword",
            [2] = "equip.guard_spear",
            [3] = "equip.border_armor",
            [4] = "equip.stable_knot",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.raen.parry",
            [2] = "skill.raen.intercept_thrust",
            [3] = "skill.raen.resonance_cut"
          },
          ["contract"] = "contract.rumo",
          ["contract_level"] = 10,
          ["rank"] = 5,
          ["rp"] = 700,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3",
            [3] = "rank_gate_4",
            [4] = "rank_gate_5"
          },
          ["contract_active"] = "skill.rumo.shield",
          ["passive_imprints"] = {
            [1] = "passive.imprint_rumo",
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [2] = {
          ["id"] = "char.seria",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.seria.C1",
            [2] = "node.seria.C2",
            [3] = "node.seria.C3",
            [4] = "node.seria.C4",
            [5] = "node.seria.A1",
            [6] = "node.seria.B1",
            [7] = "node.seria.A2"
          },
          ["equipment"] = {
            [1] = "equip.survey_dagger",
            [2] = "equip.patrol_bow",
            [3] = "equip.survey_coat",
            [4] = "equip.light_cord",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.seria.precise_shot",
            [2] = "skill.seria.analyze",
            [3] = "skill.seria.orbit_order"
          },
          ["contract"] = "contract.asel",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.asel.accelerate",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [3] = {
          ["id"] = "char.kael",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.kael.C1",
            [2] = "node.kael.C2",
            [3] = "node.kael.C3",
            [4] = "node.kael.C4",
            [5] = "node.kael.A1",
            [6] = "node.kael.B1",
            [7] = "node.kael.A2"
          },
          ["equipment"] = {
            [1] = "equip.repair_hammer",
            [2] = "equip.industrial_spear",
            [3] = "equip.heat_armor",
            [4] = "equip.balance_weight",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.kael.smash",
            [2] = "skill.kael.pierce",
            [3] = "skill.kael.overdrive"
          },
          ["contract"] = "contract.kara",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.kara.burst",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        },
        [4] = {
          ["id"] = "char.mirea",
          ["level"] = 10,
          ["level_exp"] = 0,
          ["growth_points_total"] = 9,
          ["growth_points_spent"] = 8,
          ["growth_points_unspent"] = 1,
          ["nodes"] = {
            [1] = "node.mirea.C1",
            [2] = "node.mirea.C2",
            [3] = "node.mirea.C3",
            [4] = "node.mirea.C4",
            [5] = "node.mirea.A1",
            [6] = "node.mirea.B1",
            [7] = "node.mirea.A2"
          },
          ["equipment"] = {
            [1] = "equip.forest_bow",
            [2] = "equip.resonance_staff",
            [3] = "equip.coexist_robe",
            [4] = "equip.clean_stone",
            [5] = false
          },
          ["upgrade_levels"] = {
            [1] = 0,
            [2] = 0,
            [3] = 0,
            [4] = 0,
            [5] = 0
          },
          ["actives"] = {
            [1] = "skill.mirea.heal",
            [2] = "skill.mirea.cleanse",
            [3] = "skill.mirea.clean_arrow"
          },
          ["contract"] = "contract.nema",
          ["contract_level"] = 10,
          ["rank"] = 3,
          ["rp"] = 260,
          ["relation_flags"] = {
            [1] = "cooperate",
            [2] = "rank_gate_3"
          },
          ["contract_active"] = "skill.nema.heal",
          ["passive_imprints"] = {
            [1] = false,
            [2] = false
          },
          ["active_imprint"] = nil,
          ["hp"] = "max",
          ["sp"] = "max",
          ["resonance"] = "battle_default",
          ["status"] = {}
        }
      },
      ["seed"] = 11011,
      ["knowledge"] = {
        ["weaknesses"] = {

        },
        ["tutorials"] = {}
      },
      ["region_flags"] = {

      },
      ["world_flags"] = {
        ["STABILITY"] = 0,
        ["RIGHTS"] = 0,
        ["TRANSITION"] = 0,
        ["COOPERATION"] = 0
      },
      ["id"] = "preset.growth",
      ["pairs"] = {
        [1] = {
          ["front"] = "char.raen",
          ["rear"] = nil
        },
        [2] = {
          ["front"] = "char.seria",
          ["rear"] = nil
        },
        [3] = {
          ["front"] = "char.kael",
          ["rear"] = nil
        },
        [4] = {
          ["front"] = "char.mirea",
          ["rear"] = nil
        }
      },
      ["encounter"] = nil,
      ["start_room"] = "A",
      ["growth_debug"] = true
    }
  },
  ["growth_nodes"] = {
    [1] = {
      ["id"] = "node.raen.C1",
      ["character"] = "char.raen",
      ["name"] = "기초 체력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {},
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_pct",
          ["stat"] = "HP",
          ["bp"] = 800
        }
      }
    },
    [2] = {
      ["id"] = "node.raen.C2",
      ["character"] = "char.raen",
      ["name"] = "기초 기력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {
        [1] = "node.raen.C1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SP",
          ["amount"] = 10
        }
      }
    },
    [3] = {
      ["id"] = "node.raen.C3",
      ["character"] = "char.raen",
      ["name"] = "기술 연구",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.raen.C2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.raen.intervention_step"
        }
      }
    },
    [4] = {
      ["id"] = "node.raen.C4",
      ["character"] = "char.raen",
      ["name"] = "고유 필살기",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.raen.C3"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.raen.boundary_cut"
        }
      }
    },
    [5] = {
      ["id"] = "node.raen.A1",
      ["character"] = "char.raen",
      ["name"] = "개입 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.raen.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "ATK",
          ["amount"] = 4
        }
      }
    },
    [6] = {
      ["id"] = "node.raen.A2",
      ["character"] = "char.raen",
      ["name"] = "선행 중재",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.raen.A1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_balance_add",
          ["skill"] = "skill.raen.intercept_thrust",
          ["amount"] = 1,
          ["condition"] = "target_preparing"
        }
      }
    },
    [7] = {
      ["id"] = "node.raen.A3",
      ["character"] = "char.raen",
      ["name"] = "약점 회수",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.raen.A2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "after_weak_sp",
          ["amount"] = 4
        }
      }
    },
    [8] = {
      ["id"] = "node.raen.A4",
      ["character"] = "char.raen",
      ["name"] = "흐름 절단",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.raen.A3"
      },
      ["event_gate"] = "event.personal.raen",
      ["excludes"] = {
        [1] = "node.raen.B4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "after_break_move_ally",
          ["bp"] = -1000
        }
      }
    },
    [9] = {
      ["id"] = "node.raen.B1",
      ["character"] = "char.raen",
      ["name"] = "절제 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.raen.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "WIL",
          ["amount"] = 8
        },
        [2] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.raen.steady_stance"
        }
      }
    },
    [10] = {
      ["id"] = "node.raen.B2",
      ["character"] = "char.raen",
      ["name"] = "받아넘기기 강화",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.raen.B1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_cover_delta",
          ["skill"] = "skill.raen.parry",
          ["bp"] = -1000
        }
      }
    },
    [11] = {
      ["id"] = "node.raen.B3",
      ["character"] = "char.raen",
      ["name"] = "나눔 준비",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.raen.B2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "after_guard_next_support",
          ["bonus_bp"] = 1500
        }
      }
    },
    [12] = {
      ["id"] = "node.raen.B4",
      ["character"] = "char.raen",
      ["name"] = "안정의 완성",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.raen.B3"
      },
      ["event_gate"] = "event.personal.raen",
      ["excludes"] = {
        [1] = "node.raen.A4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.raen.steady_stance"
        },
        [2] = {
          ["kind"] = "skill_sp_delta",
          ["skill"] = "skill.raen.steady_stance",
          ["delta"] = -4
        }
      }
    },
    [13] = {
      ["id"] = "node.seria.C1",
      ["character"] = "char.seria",
      ["name"] = "기초 체력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {},
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_pct",
          ["stat"] = "HP",
          ["bp"] = 800
        }
      }
    },
    [14] = {
      ["id"] = "node.seria.C2",
      ["character"] = "char.seria",
      ["name"] = "기초 기력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {
        [1] = "node.seria.C1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SP",
          ["amount"] = 10
        }
      }
    },
    [15] = {
      ["id"] = "node.seria.C3",
      ["character"] = "char.seria",
      ["name"] = "기술 연구",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.seria.C2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.seria.analyze"
        }
      }
    },
    [16] = {
      ["id"] = "node.seria.C4",
      ["character"] = "char.seria",
      ["name"] = "고유 필살기",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.seria.C3"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.seria.blindspot_lock"
        }
      }
    },
    [17] = {
      ["id"] = "node.seria.A1",
      ["character"] = "char.seria",
      ["name"] = "추적 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.seria.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SPD",
          ["amount"] = 5
        }
      }
    },
    [18] = {
      ["id"] = "node.seria.A2",
      ["character"] = "char.seria",
      ["name"] = "정확한 분석",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.seria.A1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_sp_delta",
          ["skill"] = "skill.seria.analyze",
          ["delta"] = -4
        }
      }
    },
    [19] = {
      ["id"] = "node.seria.A3",
      ["character"] = "char.seria",
      ["name"] = "약점 집중",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.seria.A2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "weapon_balance_add",
          ["weapon"] = "bow",
          ["amount"] = 1,
          ["condition"] = "weak_hit"
        }
      }
    },
    [20] = {
      ["id"] = "node.seria.A4",
      ["character"] = "char.seria",
      ["name"] = "귀환 동선",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.seria.A3"
      },
      ["event_gate"] = "event.personal.seria",
      ["excludes"] = {
        [1] = "node.seria.B4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "field_escape_grace_bonus_bp",
          ["bp"] = 5000
        }
      }
    },
    [21] = {
      ["id"] = "node.seria.B1",
      ["character"] = "char.seria",
      ["name"] = "선제 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.seria.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SP",
          ["amount"] = 5
        },
        [2] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.seria.cover_retreat"
        }
      }
    },
    [22] = {
      ["id"] = "node.seria.B2",
      ["character"] = "char.seria",
      ["name"] = "선제 교대",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.seria.B1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "entry_interval",
          ["multiplier_bp"] = 9000
        }
      }
    },
    [23] = {
      ["id"] = "node.seria.B3",
      ["character"] = "char.seria",
      ["name"] = "절맥 보강",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.seria.B2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_push_bonus",
          ["skill"] = "skill.seria.cut_pulse",
          ["bp"] = 500
        }
      }
    },
    [24] = {
      ["id"] = "node.seria.B4",
      ["character"] = "char.seria",
      ["name"] = "첫 대응",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.seria.B3"
      },
      ["event_gate"] = "event.personal.seria",
      ["excludes"] = {
        [1] = "node.seria.A4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "first_ready_shift",
          ["bp"] = -800
        }
      }
    },
    [25] = {
      ["id"] = "node.kael.C1",
      ["character"] = "char.kael",
      ["name"] = "기초 체력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {},
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_pct",
          ["stat"] = "HP",
          ["bp"] = 1000
        }
      }
    },
    [26] = {
      ["id"] = "node.kael.C2",
      ["character"] = "char.kael",
      ["name"] = "기초 기력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {
        [1] = "node.kael.C1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SP",
          ["amount"] = 10
        }
      }
    },
    [27] = {
      ["id"] = "node.kael.C3",
      ["character"] = "char.kael",
      ["name"] = "기술 연구",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.kael.C2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.kael.repair"
        }
      }
    },
    [28] = {
      ["id"] = "node.kael.C4",
      ["character"] = "char.kael",
      ["name"] = "고유 필살기",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.kael.C3"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.kael.limit_line"
        }
      }
    },
    [29] = {
      ["id"] = "node.kael.A1",
      ["character"] = "char.kael",
      ["name"] = "출력 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.kael.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "ATK",
          ["amount"] = 6
        }
      }
    },
    [30] = {
      ["id"] = "node.kael.A2",
      ["character"] = "char.kael",
      ["name"] = "출력 조율",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.kael.A1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_status_bonus",
          ["skill"] = "skill.kael.overdrive",
          ["status"] = "status.atk_up",
          ["bp"] = 500
        }
      }
    },
    [31] = {
      ["id"] = "node.kael.A3",
      ["character"] = "char.kael",
      ["name"] = "중량 숙련",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.kael.A2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "heavy_balance_add",
          ["amount"] = 1,
          ["delay_min_bp"] = 12000
        }
      }
    },
    [32] = {
      ["id"] = "node.kael.A4",
      ["character"] = "char.kael",
      ["name"] = "한계 판단",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.kael.A3"
      },
      ["event_gate"] = "event.personal.kael",
      ["excludes"] = {
        [1] = "node.kael.B4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "damage_bonus",
          ["bp"] = 1000,
          ["condition"] = "caster_hp_le_50"
        }
      }
    },
    [33] = {
      ["id"] = "node.kael.B1",
      ["character"] = "char.kael",
      ["name"] = "정비 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.kael.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "DEF",
          ["amount"] = 6
        },
        [2] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.kael.release"
        }
      }
    },
    [34] = {
      ["id"] = "node.kael.B2",
      ["character"] = "char.kael",
      ["name"] = "신속 정비",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.kael.B1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_cleanse_count",
          ["skill"] = "skill.kael.repair",
          ["count"] = 2
        }
      }
    },
    [35] = {
      ["id"] = "node.kael.B3",
      ["character"] = "char.kael",
      ["name"] = "안전 휴식",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.kael.B2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "guard_sp_flat",
          ["amount"] = 4
        }
      }
    },
    [36] = {
      ["id"] = "node.kael.B4",
      ["character"] = "char.kael",
      ["name"] = "잔압 억제",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.kael.B3"
      },
      ["event_gate"] = "event.personal.kael",
      ["excludes"] = {
        [1] = "node.kael.A4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_debuff_multiplier",
          ["skill"] = "skill.kael.overdrive",
          ["bp"] = 5000
        }
      }
    },
    [37] = {
      ["id"] = "node.mirea.C1",
      ["character"] = "char.mirea",
      ["name"] = "기초 체력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {},
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_pct",
          ["stat"] = "HP",
          ["bp"] = 800
        }
      }
    },
    [38] = {
      ["id"] = "node.mirea.C2",
      ["character"] = "char.mirea",
      ["name"] = "기초 기력",
      ["cost"] = 1,
      ["min_level"] = 3,
      ["requires"] = {
        [1] = "node.mirea.C1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "SP",
          ["amount"] = 15
        }
      }
    },
    [39] = {
      ["id"] = "node.mirea.C3",
      ["character"] = "char.mirea",
      ["name"] = "기술 연구",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.mirea.C2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.mirea.cleanse"
        }
      }
    },
    [40] = {
      ["id"] = "node.mirea.C4",
      ["character"] = "char.mirea",
      ["name"] = "고유 필살기",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.mirea.C3"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.mirea.clear_cycle"
        }
      }
    },
    [41] = {
      ["id"] = "node.mirea.A1",
      ["character"] = "char.mirea",
      ["name"] = "교감 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.mirea.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "RES",
          ["amount"] = 8
        }
      }
    },
    [42] = {
      ["id"] = "node.mirea.A2",
      ["character"] = "char.mirea",
      ["name"] = "정화 교감",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.mirea.A1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "on_cleanse_resonance",
          ["amount"] = 2,
          ["within_action_cap"] = true
        }
      }
    },
    [43] = {
      ["id"] = "node.mirea.A3",
      ["character"] = "char.mirea",
      ["name"] = "흐름 읽기",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.mirea.A2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "trace_ttl_bonus",
          ["element"] = "water",
          ["tl"] = 250
        }
      }
    },
    [44] = {
      ["id"] = "node.mirea.A4",
      ["character"] = "char.mirea",
      ["name"] = "치유 연결",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.mirea.A3"
      },
      ["event_gate"] = "event.personal.mirea",
      ["excludes"] = {
        [1] = "node.mirea.B4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "heal_next_contract_cost",
          ["delta"] = -5,
          ["ttl"] = 2000
        }
      }
    },
    [45] = {
      ["id"] = "node.mirea.B1",
      ["character"] = "char.mirea",
      ["name"] = "보호 기초",
      ["cost"] = 1,
      ["min_level"] = 5,
      ["requires"] = {
        [1] = "node.mirea.C4"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "stat_flat",
          ["stat"] = "WIL",
          ["amount"] = 6
        },
        [2] = {
          ["kind"] = "unlock_skill",
          ["skill"] = "skill.mirea.slow_current"
        }
      }
    },
    [46] = {
      ["id"] = "node.mirea.B2",
      ["character"] = "char.mirea",
      ["name"] = "흘리기 강화",
      ["cost"] = 2,
      ["min_level"] = 8,
      ["requires"] = {
        [1] = "node.mirea.B1"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_cover_delta",
          ["skill"] = "skill.mirea.flow_away",
          ["bp"] = -1000
        }
      }
    },
    [47] = {
      ["id"] = "node.mirea.B3",
      ["character"] = "char.mirea",
      ["name"] = "즉시 완류",
      ["cost"] = 2,
      ["min_level"] = 12,
      ["requires"] = {
        [1] = "node.mirea.B2"
      },
      ["event_gate"] = nil,
      ["excludes"] = {},
      ["effects"] = {
        [1] = {
          ["kind"] = "skill_hot_immediate",
          ["skill"] = "skill.mirea.slow_current"
        }
      }
    },
    [48] = {
      ["id"] = "node.mirea.B4",
      ["character"] = "char.mirea",
      ["name"] = "위기 돌봄",
      ["cost"] = 3,
      ["min_level"] = 20,
      ["requires"] = {
        [1] = "node.mirea.B3"
      },
      ["event_gate"] = "event.personal.mirea",
      ["excludes"] = {
        [1] = "node.mirea.A4"
      },
      ["effects"] = {
        [1] = {
          ["kind"] = "heal_bonus",
          ["bp"] = 1500,
          ["condition"] = "target_hp_lt_40"
        }
      }
    }
  },
  ["reactions"] = {
    [1] = {
      ["id"] = "reaction.frost",
      ["elements"] = {
        [1] = "water",
        [2] = "wind"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.frost",
          ["ttl"] = 2000,
          ["actions"] = 1
        }
      }
    },
    [2] = {
      ["id"] = "reaction.lava",
      ["elements"] = {
        [1] = "fire",
        [2] = "earth"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.lava",
          ["ttl"] = 2000
        }
      }
    },
    [3] = {
      ["id"] = "reaction.lightning",
      ["elements"] = {
        [1] = "wind",
        [2] = "light"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "chain",
          ["last_direct_damage_bp"] = 6000,
          ["max_extra_targets"] = 1,
          ["critical"] = false
        },
        [2] = {
          ["kind"] = "move",
          ["bp"] = 1000
        }
      }
    },
    [4] = {
      ["id"] = "reaction.corrosion",
      ["elements"] = {
        [1] = "water",
        [2] = "shadow"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.corrosion",
          ["ttl"] = 2000,
          ["tier"] = 1
        }
      }
    },
    [5] = {
      ["id"] = "reaction.eclipse",
      ["elements"] = {
        [1] = "light",
        [2] = "shadow"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "dispel",
          ["count"] = 1,
          ["filter"] = "any_buff",
          ["fallback"] = {
            ["kind"] = "status",
            ["status"] = "status.wil_down",
            ["ttl"] = 1000,
            ["strength_bp"] = 1500
          }
        }
      }
    },
    [6] = {
      ["id"] = "reaction.steam",
      ["elements"] = {
        [1] = "fire",
        [2] = "water"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.steam",
          ["ttl"] = 2000,
          ["actions"] = 1
        }
      }
    },
    [7] = {
      ["id"] = "reaction.bind",
      ["elements"] = {
        [1] = "earth",
        [2] = "water"
      },
      ["target"] = "enemy",
      ["effects"] = {
        [1] = {
          ["kind"] = "status",
          ["status"] = "status.bind",
          ["ttl"] = 2000,
          ["actions"] = 1
        }
      }
    },
    [8] = {
      ["id"] = "reaction.crystal",
      ["elements"] = {
        [1] = "earth",
        [2] = "light"
      },
      ["target"] = "ally",
      ["effects"] = {
        [1] = {
          ["kind"] = "shield",
          ["formula"] = "target_max_hp",
          ["coefficient_bp"] = 1000,
          ["ttl"] = 2000
        },
        [2] = {
          ["kind"] = "next_contract_damage",
          ["bonus_bp"] = 1000,
          ["ttl"] = 2000
        }
      }
    }
  },
  ["upgrade"] = {
    ["cumulative_stat_bp"] = {
      [1] = 10000,
      [2] = 10800,
      [3] = 11600,
      [4] = 12500
    },
    ["gold_step_bp"] = {
      [1] = 2500,
      [2] = 5000,
      [3] = 10000
    },
    ["materials_by_step"] = {
      [1] = {
        ["material.drill_iron"] = 1,
        ["material.echo_powder"] = 1
      },
      [2] = {
        ["material.drill_iron"] = 2,
        ["material.echo_powder"] = 2
      },
      [3] = {
        ["material.drill_iron"] = 3,
        ["material.echo_powder"] = 3
      }
    },
    ["scales"] = {
      [1] = "weapon.attack",
      [2] = "armor.DEF",
      [3] = "armor.WIL"
    },
    ["does_not_scale"] = {
      [1] = "SPD",
      [2] = "RES",
      [3] = "conditional_effects"
    },
    ["refund_material_bp"] = 5000,
    ["sell_gold_bp"] = 4000
  },
  ["ai"] = {
    ["ai.residue"] = {
      ["cycle"] = {
        [1] = "skill.enemy.scratch",
        [2] = "skill.enemy.scratch",
        [3] = "skill.enemy.dust"
      },
      ["target_rule"] = "lowest_front_slot"
    },
    ["ai.carapace"] = {
      ["cycle"] = {
        [1] = "skill.enemy.harden",
        [2] = "skill.enemy.drill"
      },
      ["target_rule"] = "next_front_slot_round_robin"
    },
    ["ai.sensor"] = {
      ["cycle"] = {
        [1] = "skill.enemy.mark",
        [2] = "skill.enemy.memory",
        [3] = "skill.enemy.memory"
      },
      ["target_rule"] = "mark_same_entity_until_dead_then_lowest_hp_ratio_front"
    },
    ["ai.core"] = {
      ["phase1"] = {
        [1] = "skill.core.needle",
        [2] = "skill.core.pressure",
        [3] = "skill.core.rectifier"
      },
      ["phase2_purify"] = {
        [1] = "skill.core.pressure_ii",
        [2] = "skill.core.rectifier",
        [3] = "skill.core.backflow",
        [4] = "skill.core.amplify",
        [5] = "skill.core.pressure_ii"
      },
      ["phase2_discharge"] = {
        [1] = "skill.core.backflow",
        [2] = "skill.core.pressure_ii",
        [3] = "skill.core.amplify",
        [4] = "skill.core.backflow",
        [5] = "skill.core.pressure_ii"
      },
      ["target_rule"] = "slot_round_robin_entity_lowest_front_hp_ratio",
      ["phase_trigger"] = "hp_le_55_percent_or_first_break",
      ["phase_timing"] = "after_source_action_before_next_enemy_decision",
      ["phase_does_not_heal"] = true,
      ["cancelled_intent_counts_cycle_position"] = true,
      ["transition_has_no_extra_action"] = true
    }
  },
  ["boss_route"] = {
    ["purify"] = {
      ["start_balance"] = 16,
      ["start_phase"] = 1,
      ["stat_bp"] = 10000
    },
    ["discharge"] = {
      ["start_balance"] = 18,
      ["start_phase"] = 2,
      ["stat_bp"] = 11000
    },
    ["c1_read"] = {
      ["first_needle_coefficient_bp"] = 13500,
      ["auto_reveal_new_phase_weakness"] = true,
      ["extra_material"] = "material.echo_powder",
      ["extra_quantity"] = 1
    }
  },
  ["dungeon"] = {
    ["id"] = "dungeon.sediment_mine",
    ["canonical_available_after"] = "chapter1",
    ["public_is_noncanonical"] = true,
    ["rooms"] = {
      [1] = {
        ["id"] = "A",
        ["size"] = {
          [1] = 22,
          [2] = 14
        },
        ["next"] = {
          [1] = "B"
        },
        ["flags"] = {
          [1] = "shop",
          [2] = "craft",
          [3] = "save"
        }
      },
      [2] = {
        ["id"] = "B",
        ["size"] = {
          [1] = 24,
          [2] = 16
        },
        ["next"] = {
          [1] = "A",
          [2] = "C1",
          [3] = "C2"
        },
        ["encounter"] = "encounter.B",
        ["exit_requires"] = "reward.B"
      },
      [3] = {
        ["id"] = "C1",
        ["size"] = {
          [1] = 18,
          [2] = 12
        },
        ["next"] = {
          [1] = "B",
          [2] = "C2"
        },
        ["encounter"] = "encounter.C1",
        ["optional"] = true
      },
      [4] = {
        ["id"] = "C2",
        ["size"] = {
          [1] = 26,
          [2] = 16
        },
        ["next"] = {
          [1] = "B",
          [2] = "C1",
          [3] = "D"
        },
        ["encounter"] = "encounter.C2",
        ["exit_requires"] = "reward.C2"
      },
      [5] = {
        ["id"] = "D",
        ["size"] = {
          [1] = 22,
          [2] = 14
        },
        ["next"] = {
          [1] = "C2",
          [2] = "E"
        },
        ["encounter"] = "encounter.elite",
        ["optional_encounter"] = true,
        ["choice"] = "choice.pump",
        ["exit_requires"] = "choice.pump"
      },
      [6] = {
        ["id"] = "E",
        ["size"] = {
          [1] = 20,
          [2] = 12
        },
        ["next"] = {
          [1] = "D",
          [2] = "F"
        },
        ["encounter"] = "encounter.E",
        ["camp_after_encounter"] = true,
        ["rest_limit_per_run"] = 1,
        ["rest_hp_sp_bp"] = 10000,
        ["camp_services"] = {
          [1] = "save",
          [2] = "equipment",
          [3] = "skills",
          [4] = "imprints",
          [5] = "contracts",
          [6] = "pairs"
        },
        ["craft"] = false,
        ["exit_requires"] = "reward.E"
      },
      [7] = {
        ["id"] = "F",
        ["size"] = {
          [1] = 26,
          [2] = 16
        },
        ["next"] = {
          [1] = "E",
          [2] = "A"
        },
        ["encounter"] = "encounter.F",
        ["exit_shortcut_requires"] = "reward.F"
      }
    },
    ["respawn_in_demo"] = false,
    ["route_short"] = {
      [1] = "A",
      [2] = "B",
      [3] = "C2",
      [4] = "D",
      [5] = "E",
      [6] = "F",
      [7] = "A"
    },
    ["route_full"] = {
      [1] = "A",
      [2] = "B",
      [3] = "C1",
      [4] = "C2",
      [5] = "D",
      [6] = "E",
      [7] = "F",
      [8] = "A"
    },
    ["choice"] = {
      ["id"] = "choice.pump",
      ["timing"] = "after_optional_elite_or_skip",
      ["purify_cost"] = {
        ["material.rectifier_plate"] = 1
      },
      ["discharge_cost"] = {

      },
      ["atomic_flag"] = "choice.pump",
      ["scope"] = "region_only"
    },
    ["rewards"] = {
      [1] = {
        ["id"] = "reward.C1_cache",
        ["room"] = "C1",
        ["gold"] = 150,
        ["materials"] = {
          ["material.clear_resin"] = 2
        },
        ["requires"] = "reward.C1",
        ["sets"] = {
          ["c1_read"] = true
        }
      },
      [2] = {
        ["id"] = "reward.C2_ore",
        ["room"] = "C2",
        ["gold"] = 0,
        ["materials"] = {
          ["material.drill_iron"] = 2
        },
        ["requires"] = nil
      },
      [3] = {
        ["id"] = "reward.goal",
        ["room"] = "A",
        ["gold"] = 250,
        ["materials"] = {

        },
        ["requires"] = "reward.F",
        ["purify_bonus"] = {
          ["material.echo_powder"] = 1
        },
        ["discharge_bonus"] = {
          ["material.drill_iron"] = 2
        },
        ["c1_bonus"] = {
          ["material.echo_powder"] = 1
        }
      }
    },
    ["full_run_income_target"] = {
      ["min_gold"] = 1040,
      ["max_gold"] = 1060,
      ["excludes_start_gold"] = true,
      ["does_not_price_materials_as_cash"] = true
    }
  },
  ["growth_rules"] = {
    ["next_exp"] = "round_half_up(80+45*L^1.45)",
    ["ranks"] = {
      [1] = 0,
      [2] = 40,
      [3] = 120,
      [4] = 260,
      [5] = 460,
      [6] = 700
    },
    ["rp_battle_base"] = 8,
    ["rp_effective_contract"] = 4,
    ["rp_role_bonus"] = 2,
    ["rp_role_cap"] = 2,
    ["rp_elite"] = 8,
    ["rp_boss"] = 20,
    ["low_rank_bp"] = 15000,
    ["low_rank_range"] = {
      [1] = 0,
      [2] = 1
    },
    ["public_level_lock"] = true,
    ["demo_growth_events"] = {
      ["event.growth.rumo"] = {
        ["consent"] = true,
        ["description"] = "비정사 성장 검증 전용 사건 완료 플래그; 본편 개인 사건 대사를 대체하지 않음"
      },
      ["event.growth.asel"] = {
        ["consent"] = true,
        ["description"] = "비정사 성장 검증 전용 사건 완료 플래그; 본편 개인 사건 대사를 대체하지 않음"
      },
      ["event.growth.kara"] = {
        ["consent"] = true,
        ["description"] = "비정사 성장 검증 전용 사건 완료 플래그; 본편 개인 사건 대사를 대체하지 않음"
      },
      ["event.growth.nema"] = {
        ["consent"] = true,
        ["description"] = "비정사 성장 검증 전용 사건 완료 플래그; 본편 개인 사건 대사를 대체하지 않음"
      }
    },
    ["contract_next_exp"] = "round_half_up(60+35*L^1.4)",
    ["growth_points_per_level"] = 1,
    ["event_goal_experience"] = 0,
    ["dormant_exp_bp"] = 3500
  },
  ["save_contract"] = {
    ["version"] = 1,
    ["mode"] = {
      [1] = "campaign",
      [2] = "demo"
    },
    ["autosave_boundary"] = "field_idle_or_completed_event_step_only",
    ["no_disk_save_mid_battle"] = true,
    ["event_fields"] = {
      [1] = "event_id",
      [2] = "definition_version",
      [3] = "next_step",
      [4] = "locals",
      [5] = "committed_transaction_ids"
    },
    ["transaction_fields"] = {
      [1] = "inventory_delta",
      [2] = "flags_delta",
      [3] = "next_step",
      [4] = "receipt_id"
    },
    ["battle_retry_restore"] = {
      [1] = "units",
      [2] = "inventory",
      [3] = "flags",
      [4] = "rng",
      [5] = "reward_receipts"
    },
    ["battle_retry_keep"] = {
      [1] = "knowledge"
    },
    ["demo_exit_export"] = {},
    ["future_version"] = "reject_without_mutation",
    ["migration_zero"] = "fill_missing_mode_as_campaign_and_empty_receipts_no_guess_of_renamed_flags",
    ["disk_commit"] = "write temporary same directory, flush, atomic replace, retain previous backup; runtime I/O untested"
  }
}
