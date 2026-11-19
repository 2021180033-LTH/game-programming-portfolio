"""Replay a fixed trace-only command fixture (not a full battle). Python 3.10+."""
from __future__ import annotations
import json
from pathlib import Path
from reference import load_data,index,apply_trace
ROOT=Path(__file__).resolve().parents[1]
def main()->int:
    data=load_data(); skills=index(data['skills']); contracts=index(data['contracts']); chars=index(data['characters'])
    preset=index(data['presets'])['preset.reaction']; fx=json.loads((ROOT/'tests/reaction_golden.json').read_text(encoding='utf-8'))
    current=fx['initial']['trace']; actual=[]
    for command in fx['commands']:
        sk=skills[command['skill']]
        unit=next(u for u in preset['units'] if u['contract_active']==sk['id'])
        c=contracts[unit['contract']]; role=chars[unit['id']]['role']
        bonus=c['role_variants'][role]
        ttl=sk['trace_ttl']
        if unit['rank']>=3 and bonus['kind']=='trace_ttl_bonus' and bonus['element']==sk['trace']:
            ttl+=bonus['tl']
        current,reaction=apply_trace(data,current,sk['trace'],command['at'],ttl)
        row={'at':command['at'],'trace':current['element'] if current else None}
        if current:row['expires']=current['expires']
        if reaction:row['reaction']=reaction
        actual.append(row)
    report={'scope':'trace-only fixed command replay; no damage/AI/complete battle', 'status':'PASS' if actual==fx['expected'] else 'FAIL','expected':fx['expected'],'actual':actual}
    (ROOT/'reports/golden_trace.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2));return 0 if report['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
