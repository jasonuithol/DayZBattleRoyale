#!/usr/bin/env python3
"""Builds mission/br_spots.json, the map data the mod spawns loot and cars from:

  protos     every building type that has vanilla loot points, with those points in the
             building's own model space (x y z triples, from mapgroupproto.xml)
  buildings  every such building on the map: index into protos and world position
             (from mapgrouppos.xml); the game turns a point into world space with the
             building's own ModelToWorld, so no rotation maths here
  cars       the vanilla car and truck spawn points (the "exact" spots of DayZMissions'
             roles.nasdara spots.json, which came from cfgeventspawns.xml)

usage: tools/make_spots.py [terrain]   (default nasdara)"""
import json, os, re, sys

terrain = sys.argv[1] if len(sys.argv) > 1 else 'nasdara'
steam = os.environ.get('STEAM', os.path.expanduser('~/.steam/debian-installation'))
vanilla = os.path.join(steam, 'steamapps/common/DayZServer/mpmissions/dayzOffline.' + terrain)
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cars_src = os.path.join(here, '../DayZMissions/missions/roles.' + terrain + '/spots.json')
out = os.path.join(here, 'mission', 'br_spots.json')

protos, index = [], {}
proto_xml = open(os.path.join(vanilla, 'mapgroupproto.xml')).read()
for name, body in re.findall(r'<group name="([^"]+)"[^>]*>(.*?)</group>', proto_xml, re.S):
    pts = []
    for p in re.findall(r'<point pos="([^"]+)"', body):
        pts += [round(float(v), 3) for v in p.split()]
    if pts:
        index[name] = len(protos)
        protos.append({'type': name, 'pts': pts})

buildings = []
pos_xml = open(os.path.join(vanilla, 'mapgrouppos.xml')).read()
for name, pos in re.findall(r'<group name="([^"]+)" pos="([^"]+)"', pos_xml):
    if name in index:
        x, y, z = (round(float(v), 2) for v in pos.split())
        buildings.append({'t': index[name], 'x': x, 'y': y, 'z': z})

cars = []
if os.path.exists(cars_src):
    for s in json.load(open(cars_src))['spots']:
        if s['exact'] and s['building'].startswith('Vehicle'):
            cars.append({'x': s['pos'][0], 'z': s['pos'][2], 'h': s['heading'], 'types': s['types']})
else:
    print('no car spots (%s missing)' % cars_src)

json.dump({'protos': protos, 'buildings': buildings, 'cars': cars}, open(out, 'w'), separators=(',', ':'))
print('%s: %d building types, %d buildings, %d loot points, %d car spots' % (
    out, len(protos), len(buildings), sum(len(protos[b['t']]['pts']) // 3 for b in buildings), len(cars)))
