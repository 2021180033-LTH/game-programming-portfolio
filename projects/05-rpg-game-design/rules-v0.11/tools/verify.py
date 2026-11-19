"""Run: python tools/verify.py. Deterministic spec tests; not a complete RPG simulation."""
from pathlib import Path
import sys,json,hashlib,csv
from copy import deepcopy
from reference import *
D=load_data();results=[]
def check(name,fn):
    try:
        answer=fn()
        if answer is False: raise AssertionError('False result')
        results.append({'name':name,'status':'PASS'})
    except Exception as e: results.append({'name':name,'status':'FAIL','error':str(e)})
def eq(actual,expected):
    assert actual==expected,(actual,expected)

def ids():
    allids=[r['id'] for k in ['characters','contracts','skills','passives','statuses','equipment','items','recipes','enemies','encounters','presets','growth_nodes','reactions'] for r in D[k]]
    eq(len(allids),len(set(allids)))
check('data.unique_ids',ids)
S=index(D['skills']); C=index(D['characters']); K=index(D['contracts']); N=index(D['growth_nodes']); Q=index(D['equipment']); P=index(D['presets']); I=index(D['items']); EN=index(D['enemies'])
def complete_skills():
    for s in S.values():
        for key in ['target','target_mode','weapon','delay_bp','cost','effects','trace','unlock_rank']: assert key in s
        assert s['delay_bp']>0 and s['cost']['sp']>=0 and s['cost']['resonance']>=0
        assert s['owner'] in C or s['owner'] in K or s['owner'] in EN or s['owner']=='common'
        for e in s['effects']:
            if e['kind']=='damage': assert all(x in e for x in ['scale','coefficient_bp','balance','element','hits']) and e['coefficient_bp']>0
check('data.all_61_skills_have_explicit_inputs',complete_skills)
def enemy_stats():
    for e in EN.values():
        assert all(e['stats'][k]>0 for k in ['ATK','RES','HP','DEF','WIL','SPD'])
        assert e['ai'] in D['ai']
check('data.enemy_attacks_resolved',enemy_stats)
def enemy_no_bp():
    for s in S.values():
        if s['owner'] in EN:
            for e in s['effects']:
                if e['kind']=='damage': eq(e['balance'],0)
check('data.no_player_balance_assumed',enemy_no_bp)
def references():
    statusids={x['id'] for x in D['statuses']}
    def walk(x):
        if isinstance(x,dict):
            for k,v in x.items():
                if k=='status' and isinstance(v,str): assert v in statusids,v
                if k=='skill' and isinstance(v,str) and v.startswith('skill.'): assert v in S,v
                walk(v)
        elif isinstance(x,list):
            for v in x: walk(v)
    walk(D)
    for r in D['recipes']:
        assert r['base'] in Q and r['result'] in Q
        assert all(k in I for k in r['materials'])
    for e in D['encounters']: assert all(k in EN for k in e['enemies'])
check('data.cross_references',references)
def profiles():
    for p in P.values():
        ids=[u['id'] for u in p['units']]
        eq(len(ids),len(set(ids)))
        positions=[x for pair in p['pairs'] for x in [pair['front'],pair['rear']] if x]
        eq(set(positions),set(ids));eq(len(positions),len(ids))
        for u in p['units']:
            assert len(u['actives'])==3 and all(s in S for s in u['actives'])
            assert S[u['contract_active']]['owner']==u['contract']
            assert S[u['contract_active']]['unlock_rank']<=u['rank']
            assert all(n in N for n in u['nodes'])
            spent=sum(N[n]['cost'] for n in u['nodes']);eq(spent,u['growth_points_spent']);eq(spent+u['growth_points_unspent'],u['growth_points_total'])
            for n in u['nodes']:
                assert N[n]['min_level']<=u['level']
                assert set(N[n]['requires']).issubset(u['nodes'])
            assert len(u['equipment'])==5
            for item in u['equipment']:
                if item: assert item in Q
check('data.all_7_presets_valid',profiles)
check('rounding.half_up_0_5',lambda:eq(round_half_up(.5),1))
check('rounding.half_up_1_5',lambda:eq(round_half_up(1.5),2))
check('timing.base_spd32',lambda:eq(base_delay(32),758))
check('timing.base_spd100',lambda:eq(base_delay(100),500))
check('timing.positive_interval_floor',lambda:eq(next_tick(1000,100,1000),1300))
check('timing.max_general_delay',lambda:eq(move_tick(2000,1000,1000,9000),2300))
check('timing.combined_break_cap',lambda:eq(move_tick(2000,1000,1000,3000,3500),2600))
check('timing.no_same_tick_requeue',lambda:eq(move_tick(1001,1000,1000,-3000),1001))
check('damage.physical_known',lambda:eq(damage(117,45,10000,11500),93))
check('damage.enemy_physical_known',lambda:eq(damage(125,92,14500),94))
check('damage.guard_both_classes',lambda:eq(round_half_up(100*D['rules']['guard_damage_bp']/10000),70))
check('shield.common_formula_130',lambda:eq(shield(70,78,50),130))
check('shield.common_formula_equipped_136',lambda:eq(shield(70,92,50),136))
check('heal.contract_common_formula',lambda:eq(healing(117,38,12500),194))
check('balance.resistance_half',lambda:eq(balance_bp(1,'resist'),1))
check('balance.weak_with_fraction',lambda:eq(balance_bp(1.5,'weak',12500),8))
check('balance.zero_support',lambda:eq(balance_bp(0),0))
check('resonance.rank0_start',lambda:eq(start_resonance(0),25))
check('resonance.rank5_start',lambda:eq(start_resonance(5),50))
check('resonance.no_overheal_gain',lambda:eq(add_resonance(25,False,True,True),(25,0)))
check('resonance.action_cap',lambda:eq(add_resonance(95,True,True,True),(100,10)))
def overspend():
    u={'sp':10,'resonance':20};old=deepcopy(u);eq(pay_cost(u,0,25),False);eq(u,old)
check('cost.failure_is_atomic',overspend)
def waterwind():
    u=next(u for u in P['preset.reaction']['units'] if u['id']=='char.mirea');v=next(u for u in P['preset.reaction']['units'] if u['id']=='char.seria')
    w=S[u['contract_active']];a=S[v['contract_active']]
    old,r=apply_trace(D,None,w['trace'],400,1250);eq(r,None)
    old,r=apply_trace(D,old,a['trace'],500);eq(old,None);eq(r,'reaction.frost')
check('reaction.demo_water_wind_reachable',waterwind)
check('reaction.reverse_order',lambda:eq(apply_trace(D,{'element':'wind','expires':1100},'water',500)[1],'reaction.frost'))
check('reaction.expiry_before_action',lambda:eq(apply_trace(D,{'element':'water','expires':1000},'wind',1000)[1],None))
check('reaction.same_element_refresh',lambda:eq(apply_trace(D,{'element':'water','expires':1000},'water',500)[0],{'element':'water','expires':1500}))
check('reaction.no_support_water_shortcut',lambda:eq(apply_trace(D,{'element':'water','expires':1000},'wind',500,target='ally')[1],None))
check('reaction.crystal_ally_only',lambda:eq(apply_trace(D,{'element':'earth','expires':1000},'light',500,target='ally')[1],'reaction.crystal'))
state={'units':{'A':{'hp':100},'B':{'hp':100}},'front':{'0':'B'},'rear':{'0':'A'}}
check('target.slot_follows_position',lambda:eq(resolve_targets(state,'slot',0),['B']))
check('target.entity_tracks_rear',lambda:eq(resolve_targets(state,'entity','A'),['A']))
check('target.dead_fizzle',lambda:eq(resolve_targets({'units':{'A':{'hp':0}},'front':{}},'entity','A'),[]))
def multi():
    st={'units':{'A':{'hp':50},'B':{'hp':100}},'front':{'0':'A'},'rear':{'0':'B'}}
    out,log=snapshot_multihit(st,['A'],5,20);eq(out['units']['B']['hp'],100);eq(len([x for x in log if 'hit'in x]),3);eq(log[-1],{'event':'promote','target':'B'})
check('target.multihit_no_spill',multi)
check('shield.share_cannot_kill_protector',lambda:eq(share_damage(100,11),(90,10)))
def same_tl():
    st={'A':{'hp':20,'max_hp':100}}
    out,log=same_time_batch(st,[{'kind':'rear','id':'r','unit':'A','amount':30},{'kind':'expiry','id':'burn','unit':'A'},{'kind':'dot','id':'d','unit':'A','amount':20}]);eq(out['A']['hp'],0);eq(log,['dot:A','dead:A','expiry:burn'])
check('clock.dot_death_before_rear',same_tl)
check('ai.deterministic_cycle',lambda:eq(boss_plan(D,'purify'),boss_plan(D,'purify')))
def prep():
    x=boss_plan(D,'discharge');first=x[:3];eq([i['kind'] for i in first],['prepare','wait','execute']);eq(first[-1]['skill'],'skill.core.backflow')
check('ai.two_opportunity_telegraph',prep)
check('ai.phase2_purify_first_two_not_backflow',lambda:eq(boss_cycle(D,2,'purify')[:2],['skill.core.pressure_ii','skill.core.rectifier']))
def graph():
    rs={r['id']:r for r in D['dungeon']['rooms']}
    assert all(x in rs for r in rs.values() for x in r['next'])
    # all authored routes valid, compulsory C2 before D
    for k in ['route_short','route_full']:
        path=D['dungeon'][k];assert path.index('C2')<path.index('D')
        for a,b in zip(path,path[1:]): assert b in rs[a]['next'],(a,b)
check('dungeon.graph_and_mandatory_C2',graph)
ledgers=[]
for c1 in [False,True]:
 for elite in [False,True]:
  for route in ['purify','discharge']:
   name=f'route.c1{int(c1)}.elite{int(elite)}.{route}'
   def f(c1=c1,elite=elite,route=route):
    run,rows=route_ledger(D,c1,elite,route)
    assert min(run['materials'].values())>=0
    eq(run['gold'],1180+75*int(c1)+150*int(c1)+85*int(elite)) # start400 + main195+boss300+goal250 =1145; corrected below
   # independent explicit expectation, not implementation summation
   def f(c1=c1,elite=elite,route=route):
    run,rows=route_ledger(D,c1,elite,route);assert min(run['materials'].values())>=0
    eq(run['gold'],1145+225*int(c1)+85*int(elite))
    for w in ['field_sword','survey_dagger','resonance_staff']: run['equipment']['equip.'+w]=0
    assert can_craft(run,D['recipes'][0])
    assert can_craft(run,D['recipes'][1])
    if c1: assert can_craft(run,D['recipes'][2])
    ledgers.append({'case':f'c1{int(c1)}_elite{int(elite)}_{route}','final':deepcopy(run),'steps':rows})
   check(name,f)
check('economy.full_income_1055',lambda:eq(route_ledger(D,True,True,'purify')[0]['gold']-400,1055))
def reward_once():
    r=empty_run();apply_reward(r,'x',100,{'material.echo_powder':2});apply_reward(r,'x',100,{'material.echo_powder':2});eq(r['gold'],500);eq(r['materials']['material.echo_powder'],2)
check('economy.rewards_idempotent',reward_once)
def craft_once():
    r,_=route_ledger(D,True,True,'purify');r['equipment']['equip.field_sword']=2
    assert craft(r,D['recipes'][0],'craft.1');eq(r['equipment']['equip.sediment_sword'],2)
    r['equipment']['equip.survey_dagger']=0;assert not can_craft(r,D['recipes'][1]);assert not craft(r,D['recipes'][0],'craft.1')
check('economy.craft_preserves_upgrade_one_core',craft_once)
def insufficient():
    r=empty_run();old=deepcopy(r);eq(craft(r,D['recipes'][0],'t'),False);eq(r,old)
check('economy.invalid_craft_no_partial_cost',insufficient)
def save_event():
    r=empty_run();r=event_reward_step(r,'event.reward',3,'event.reward.2',250)
    r2=event_reward_step(r,'event.reward',3,'event.reward.2',250);eq(r,r2);eq(r['gold'],650);eq(r['event']['next_step'],3)
check('save.event_step_atomic_receipt',save_event)
def retrycheck():
    snap={'gold':400,'knowledge':{'weaknesses':[]}};failed={'gold':900,'knowledge':{'weaknesses':['spear']}}
    eq(retry(snap,failed),{'gold':400,'knowledge':{'weaknesses':['spear']}})
check('save.retry_rollback_except_knowledge',retrycheck)
def demo_isolation():
    campaign={'gold':1234,'knowledge':{'weaknesses':['sword']},'flags':{'STABILITY':2}};old=deepcopy(campaign)
    demo=begin_demo(campaign,P['preset.public']);demo['gold']=0;demo['knowledge']['weaknesses']['foo']='water';demo['world_flags']['STABILITY']=100
    eq(campaign,old)
check('save.demo_knowledge_isolated',demo_isolation)
check('save.v0_migration',lambda:eq(migrate({'gold':5})['save_version'],1))
def future():
    s={'save_version':2,'gold':5};old=deepcopy(s)
    try: migrate(s)
    except ValueError: pass
    else: raise AssertionError('future save accepted')
    eq(s,old)
check('save.future_version_rejected',future)
check('rng.xorshift_known_vector',lambda:eq([XorShift32(1).next()], [270369]))
def rng_repeat():
    a=XorShift32(11);b=XorShift32(11);eq([a.uniform(1001) for _ in range(30)],[b.uniform(1001) for _ in range(30)])
check('rng.replayable_sequence',rng_repeat)
# Produce reference values and short golden fixture; not a 30-battle balance claim.
values=[]
for u in P['preset.public']['units']:
 primary=u['equipment'][0];cast=next((q for q in u['equipment'][:2] if Q[q]['stats'].get('RES',0)),primary)
 values.append({'character':u['id'],'normal':final_stats(D,u,primary),'casting':final_stats(D,u,cast),'weapons':{w:weapon_attack(D,u,w) for w in u['equipment'][:2]}})
report={'version':D['version'],'scope':'data consistency + deterministic primitive/regression cases; NOT full game or full-battle balance test','passed':sum(r['status']=='PASS' for r in results),'failed':sum(r['status']=='FAIL' for r in results),'cases':results}
ROOT.joinpath('reports/validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
ROOT.joinpath('reports/final_stats.json').write_text(json.dumps(values,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
ROOT.joinpath('reports/route_ledgers.json').write_text(json.dumps(ledgers,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
fixture={'initial':{'trace':None,'hp':1000,'wil':55},'commands':[{'at':400,'skill':'skill.nema.water_lance'},{'at':500,'skill':'skill.asel.feather'}], 'expected':[{'at':400,'trace':'water','expires':1650},{'at':500,'trace':None,'reaction':'reaction.frost'}]}
ROOT.joinpath('tests/reaction_golden.json').write_text(json.dumps(fixture,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':report['passed'],'failed':report['failed'],'failures':[r for r in results if r['status']=='FAIL']},ensure_ascii=False,indent=2))
sys.exit(1 if report['failed'] else 0)
