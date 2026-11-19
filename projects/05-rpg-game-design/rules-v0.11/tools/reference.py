"""Small deterministic specification oracle, NOT the SFML game / full combat simulator.
Only the operations exercised by verify.py are implemented. Python 3.10+; no dependencies.
"""
from __future__ import annotations
from copy import deepcopy
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import json, math
ROOT=Path(__file__).resolve().parents[1]
def load_data(): return json.loads((ROOT/'data/demo.json').read_text(encoding='utf-8'))
def round_half_up(value):
    if value < 0: raise ValueError('round_half_up expects a nonnegative value')
    return int(Decimal(str(value)).quantize(Decimal('1'),rounding=ROUND_HALF_UP))
def index(rows): return {r['id']:r for r in rows}
def final_stats(data, unit, weapon_id=None):
    char=index(data['characters'])[unit['id']]
    stats=deepcopy(char['stats']); bonuses={k:0 for k in stats}
    eq=index(data['equipment']); ns=index(data['growth_nodes'])
    for item,up in zip(unit['equipment'],unit['upgrade_levels']):
        if item is None: continue
        it=eq[item]
        if it['slot']=='weapon' and item!=weapon_id: continue
        for key,v in it['stats'].items():
            if it['slot']=='armor' and key in ['DEF','WIL']:
                v=round_half_up(v*data['upgrade']['cumulative_stat_bp'][up]/10000)
            stats[key]+=v
    for nid in unit['nodes']:
        for ef in ns[nid]['effects']:
            if ef['kind']=='stat_flat': stats[ef['stat']]+=ef['amount']
            if ef['kind']=='stat_pct': bonuses[ef['stat']]+=ef['bp']
    return {k:round_half_up(v*(10000+bonuses[k])/10000) for k,v in stats.items()}
def weapon_attack(data,unit,weapon_id):
    pos=unit['equipment'].index(weapon_id); it=index(data['equipment'])[weapon_id]
    return round_half_up(it['attack']*data['upgrade']['cumulative_stat_bp'][unit['upgrade_levels'][pos]]/10000)
def damage(power,defence,coef_bp=10000,weak_bp=10000,crit_bp=10000,variance_bp=10000):
    if min(power,defence,coef_bp,weak_bp,crit_bp,variance_bp)<0: raise ValueError('negative input')
    if power==0 or coef_bp==0: return 0
    return max(1,round_half_up(power*coef_bp/10000*100/(100+defence)*weak_bp/10000*crit_bp/10000*variance_bp/10000))
def shield(res,defence,protection,coef_bp=10000): return round_half_up((res*.7+defence*.4+protection)*coef_bp/10000)
def healing(res,power=0,coef_bp=10000): return round_half_up((res+power)*coef_bp/10000)
def base_delay(spd): return max(300,round_half_up(100000/(100+min(100,max(0,spd)))))
def next_tick(now,spd,delay_bp=10000,interval_bp=10000):
    return now+max(300,round_half_up(base_delay(spd)*delay_bp/10000*interval_bp/10000))
def move_tick(due,now,base,direct_bp,break_bp=0):
    direct_bp=max(-3000,min(3000,direct_bp))
    total_bp=min(6000,direct_bp+break_bp)
    return max(now+1,due+round_half_up(base*abs(total_bp)/10000)*(1 if total_bp>=0 else -1))
def balance_bp(base_amount,match='normal',receives_bp=10000,reorganize=False):
    mult={'weak':2,'normal':1,'resist':.5}[match]
    v=base_amount*2*mult*receives_bp/10000*(.5 if reorganize else 1)
    return max(1,round_half_up(v)) if v>0 else 0
def apply_trace(data,old,new,now,ttl=1000,target='enemy'):
    """Trace expired exactly at now is removed before an action at now."""
    if old and old['expires']<=now: old=None
    if new=='none': return old,None
    if old:
        for r in data['reactions']:
            if r['target']==target and set(r['elements'])=={old['element'],new} and old['element']!=new:
                return None,r['id']
    return {'element':new,'expires':now+ttl},None

def resolve_targets(state,mode,locked):
    units=state['units']
    if mode=='slot':
        unit_id = state['front'].get(str(locked))
        return [unit_id] if unit_id and units[unit_id]['hp'] > 0 else []
    if mode=='entity': return [locked] if locked in units and units[locked]['hp']>0 else []
    if mode=='formation': return [u for _,u in sorted(state['front'].items()) if u and units[u]['hp']>0]
    raise ValueError(mode)
def snapshot_multihit(state,targets,hits,damage_each):
    s=deepcopy(state); log=[]
    for target in targets:
        for n in range(hits):
            if s['units'][target]['hp']<=0: break
            s['units'][target]['hp']=max(0,s['units'][target]['hp']-damage_each)
            log.append({'hit':n+1,'target':target,'hp':s['units'][target]['hp']})
    # no promotion until ALL hits/targets complete
    for slot,front in list(s['front'].items()):
        if front and s['units'][front]['hp']<=0:
            rear=s.get('rear',{}).get(slot)
            if rear and s['units'][rear]['hp']>0:
                s['front'][slot],s['rear'][slot]=rear,front
                log.append({'event':'promote','target':rear})
    return s,log

def share_damage(amount,protector_hp):
    transfer=min(math.floor(amount*.25),max(0,protector_hp-1))
    return amount-transfer,transfer

def same_time_batch(state,events):
    """Oracle for periodic expiry/rear ordering. Action executor is intentionally external."""
    s=deepcopy(state); log=[]
    kinds=['dot','expiry','rear']
    for kind in kinds:
        chosen=sorted((x for x in events if x['kind']==kind), key=lambda x:x['id'])
        # all dots at one time calculated before death; no shields or guard used
        pending={}
        if kind=='dot':
            for ev in chosen: pending[ev['unit']]=pending.get(ev['unit'],0)+ev['amount']
            for uid,total in sorted(pending.items()):
                s[uid]['hp']=max(0,s[uid]['hp']-total);log.append('dot:'+uid)
            for uid in sorted(s):
                if s[uid]['hp']==0: log.append('dead:'+uid)
        elif kind=='expiry':
            for ev in chosen: log.append('expiry:'+ev['id'])
        else:
            for ev in chosen:
                u=s[ev['unit']]
                if u['hp']>0:
                    u['hp']=min(u['max_hp'],u['hp']+ev['amount']);log.append('rear:'+ev['unit'])
    return s,log

def start_resonance(rank): return 25+rank*5
def add_resonance(value,meaningful,role=False,weak=False):
    gain=min(10,4+3*int(role)+3*int(weak)) if meaningful else 0
    return min(100,value+gain),gain

def pay_cost(unit,sp,res):
    if min(sp,res)<0: raise ValueError('negative cost')
    if unit['sp']<sp or unit['resonance']<res: return False
    unit['sp']-=sp;unit['resonance']-=res;return True

def boss_cycle(data,phase,route):
    ai=data['ai']['ai.core'];return ai['phase1'] if phase==1 else ai['phase2_'+route]
def boss_plan(data,route,cycles=1):
    """State-machine trace for action preparation/execution, without damage or player actions."""
    phase=1 if route=='purify' else 2
    cycle=boss_cycle(data,phase,route); skills=index(data['skills']);log=[];tick=0
    for sid in cycle*cycles:
        sk=skills[sid];prep=sk.get('telegraph',0)
        for i in range(prep):
            tick+=base_delay(32);log.append({'tl':tick,'kind':'prepare' if i==0 else 'wait','skill':sid,'remaining':prep-i})
        tick+=base_delay(32);log.append({'tl':tick,'kind':'execute','skill':sid})
    return log

def empty_run(): return {'mode':'demo','gold':400,'materials':{},'receipts':[],'flags':{},'inventory':{},'equipment':{}}
def apply_reward(run,rid,gold=0,materials=None):
    if rid in run['receipts']: return False
    proposed=deepcopy(run);proposed['gold']+=gold
    for k,v in (materials or {}).items(): proposed['materials'][k]=proposed['materials'].get(k,0)+v
    proposed['receipts'].append(rid)
    run.clear();run.update(proposed);return True

def pump_choice(run,choice):
    if 'choice.pump' in run['receipts']: return False
    if choice not in ['purify','discharge']: raise ValueError(choice)
    if choice=='purify' and run['materials'].get('material.rectifier_plate',0)<1: return False
    if choice=='purify': run['materials']['material.rectifier_plate']-=1
    run['flags']['route']=choice;run['receipts'].append('choice.pump');return True

def route_ledger(data,c1=False,elite=False,choice='purify'):
    enc=index(data['encounters']);run=empty_run();rows=[]
    def record(label): rows.append({'step':label,'gold':run['gold'],**{k:run['materials'].get(k,0) for k in ['material.echo_powder','material.drill_iron','material.rectifier_plate','material.sediment_crystal','material.clear_resin']}})
    def battle(key):
        e=enc['encounter.'+key];apply_reward(run,e['reward_id'],e['gold'],e['materials']);record(key)
    record('start');battle('B')
    if c1:
        battle('C1');cache=data['dungeon']['rewards'][0];apply_reward(run,cache['id'],cache['gold'],cache['materials']);run['flags']['c1_read']=True;record('C1_cache')
    battle('C2');ore=data['dungeon']['rewards'][1];apply_reward(run,ore['id'],ore['gold'],ore['materials']);record('C2_ore')
    if elite: battle('elite')
    if not pump_choice(run,choice): raise ValueError('Unreachable pump choice')
    record(choice);battle('E');battle('F')
    goal=data['dungeon']['rewards'][2]; mats=deepcopy(goal[choice+'_bonus'])
    if c1:
        for k,v in goal['c1_bonus'].items(): mats[k]=mats.get(k,0)+v
    apply_reward(run,goal['id'],goal['gold'],mats);record('goal')
    return run,rows

def can_craft(run,recipe):
    return run['gold']>=recipe['gold'] and all(run['materials'].get(k,0)>=v for k,v in recipe['materials'].items()) and recipe['base'] in run['equipment']
def craft(run,recipe,txid):
    if txid in run['receipts']: return False
    if not can_craft(run,recipe): return False
    nxt=deepcopy(run);nxt['gold']-=recipe['gold']
    for k,v in recipe['materials'].items(): nxt['materials'][k]-=v
    up=nxt['equipment'].pop(recipe['base']);nxt['equipment'][recipe['result']]=up
    nxt['receipts'].append(txid);run.clear();run.update(nxt);return True

def event_reward_step(save,event_id,next_step,txid,gold):
    """Example all-or-nothing event transaction. Disk replacement belongs to SaveManager."""
    if txid in save['receipts']: return deepcopy(save)
    out=deepcopy(save);out['gold']+=gold;out['receipts'].append(txid)
    out['event']={'event_id':event_id,'definition_version':1,'next_step':next_step,'locals':{},'committed_transaction_ids':[txid]}
    return out

def retry(snapshot,failed):
    result=deepcopy(snapshot);result['knowledge']=deepcopy(failed['knowledge']);return result

def begin_demo(campaign,preset):
    # No aliases to the caller's campaign, including knowledge.
    out=deepcopy(preset);out['mode']='demo';out['campaign_origin_digest']=None
    return out

def migrate(save):
    out=deepcopy(save);v=out.get('save_version',0)
    if v>1: raise ValueError('Unsupported future save version')
    if v==0:
        out.setdefault('mode','campaign');out.setdefault('receipts',[]);out.setdefault('event',None);out['save_version']=1
    return out

class XorShift32:
    def __init__(self,seed): self.state=(seed&0xffffffff) or 0x6d2b79f5
    def next(self):
        x=self.state;x^=(x<<13)&0xffffffff;x^=x>>17;x^=(x<<5)&0xffffffff;self.state=x&0xffffffff;return self.state
    def uniform(self,n):
        if n<=0: raise ValueError(n)
        threshold=((1<<32)-n)%n
        while True:
            x=self.next()
            if x>=threshold: return x%n
