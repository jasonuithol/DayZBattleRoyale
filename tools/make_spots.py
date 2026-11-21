#!/usr/bin/env python3
"""Builds missions/<terrain>/br_spots.json, the map data the mod spawns loot and cars from:

  protos     every building type that has vanilla loot points, with those points in the
             building's own model space (x y z triples, from mapgroupproto.xml)
  buildings  every such building on the map: index into protos and world position
             (from mapgrouppos.xml); the game turns a point into world space with the
             building's own ModelToWorld, so no rotation maths here
  bare       the tower blocks (ROOF_ONLY): no vanilla loot points, so no loot inside, but a
             flat roof with a way down, which has some
  cars       the vanilla car, bike and truck spawn points (the Vehicle* events of
             cfgeventspawns.xml)
  boats      its boat spawn points (VehicleBoat)

All three come from the map's vanilla mission folder: dayzOffline.<terrain> in the DayZ
server's mpmissions, or any folder given with --from (a third-party map's own mission).

usage: tools/make_spots.py TERRAIN [--from MISSION_FOLDER]
  tools/make_spots.py nasdara
  tools/make_spots.py chernarusplus"""
import json, os, random, re, sys

args = sys.argv[1:]
if not args or args[0].startswith('-'):
    sys.exit(__doc__)
terrain = args[0]
steam = os.environ.get('STEAM', os.path.expanduser('~/.steam/debian-installation'))
vanilla = os.path.join(steam, 'steamapps/common/DayZServer/mpmissions/dayzOffline.' + terrain)
if '--from' in args:
    vanilla = args[args.index('--from') + 1]
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.join(here, 'missions', terrain, 'br_spots.json')
if not os.path.isdir(os.path.dirname(out)):
    sys.exit('no mission folder ' + os.path.dirname(out))

protos, index = [], {}
proto_xml = open(os.path.join(vanilla, 'mapgroupproto.xml')).read()
for name, body in re.findall(r'<group name="([^"]+)"[^>]*>(.*?)</group>', proto_xml, re.S):
    pts = []
    for p in re.findall(r'<point pos="([^"]+)"', body):
        pts += [round(float(v), 3) for v in p.split()]
    index[name] = len(protos)
    protos.append({'type': name, 'pts': pts})

# buildings with no loot points whose roofs still get loot: a ladder and stairs down from the roof
ROOF_ONLY = {'Land_Tenement_Big', 'Land_Tenement_Small'}

buildings, bare = [], []
pos_xml = open(os.path.join(vanilla, 'mapgrouppos.xml')).read()
for name, pos in re.findall(r'<group name="([^"]+)" pos="([^"]+)"', pos_xml):
    if name in index:
        x, y, z = (round(float(v), 2) for v in pos.split())
        spot = {'t': index[name], 'x': x, 'y': y, 'z': z}
        if protos[index[name]]['pts']:
            buildings.append(spot)
        elif name in ROOF_ONLY:
            bare.append(spot)

cars, boats = [], []
events = open(os.path.join(vanilla, 'cfgeventspawns.xml')).read()
for name, body in re.findall(r'<event name="(Vehicle[^"]*)">(.*?)</event>', events, re.S):
    for tag in re.findall(r'<pos\b[^>]*>', body):
        v = dict(re.findall(r'(\w+)="([^"]*)"', tag))
        if 'x' in v and 'z' in v:   # no heading ("a") on some maps: any heading then
            h = float(v['a']) if 'a' in v else random.uniform(0, 360)
            spot = {'x': round(float(v['x']), 2), 'z': round(float(v['z']), 2), 'h': round(h, 1), 'types': name}
            (boats if 'Boat' in name else cars).append(spot)

json.dump({'protos': protos, 'buildings': buildings, 'bare': bare, 'cars': cars, 'boats': boats}, open(out, 'w'), separators=(',', ':'))
print('%s: %d building types, %d buildings, %d loot points, %d bare buildings, %d car spots, %d boat spots' % (
    out, len(protos), len(buildings), sum(len(protos[b['t']]['pts']) // 3 for b in buildings), len(bare), len(cars), len(boats)))
