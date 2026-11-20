#!/usr/bin/env python3
"""The player's animation graph with the jump's tilt in it, for the BattleDayZ mod.

A script can't tilt a character: it stays upright whatever the script does. Only the
animation graph can, so the mod ships the game's own player graph (the workspace
DZ/anims/workspaces/player/player_main, its text sources) with nine variables and seventeen
nodes added at the very top of it:

  BR_FallPitch   degrees, + leans the whole body forward (head down), - back
  BR_FallRoll    degrees, + rolls it right, - left
  BR_FallSpread  degrees the arms and legs spread further apart (tilted back)
  BR_FallArms    0 to 1: the arms go down the sides as if standing (tilted forward)
  BR_ChuteHands  0 to 1: under the canopy, the pose blends into the surrender one (hands up)
  BR_ChuteGrip   0 to 1: the fingers close round the lines, as a raised fist's
  BR_ChuteSwayX/Y/Z  metres the hips move, so the canopy's tilt swings the body about the canopy

Each is a procedural rotation, in model space, over everything the graph does: of the
pelvis for the tilt (it carries the spine and the legs), of the upper arms and legs
for the spread, and of every bone of the arms for the arms down. At 0 (always, except in the jump) they do
nothing. BR_ChuteCommand sets them from the keys.

The graph is copied from the installed game every build, so it follows game updates: an
old copy of the graph with the new game's animations would break every player's
animations. Written to <out>/$PBOPREFIX$ and the workspace beside it; tools/build.sh puts
it in the staged mod's pbos/ folder, and DayZMissions' build_mod.py packs it as its own pbo
under the game's path, where it takes the place of the game's own.

  tools/make_anims.py OUT_DIR [ANIMS_WORKSPACES_PBO]
"""
import os, random, re, shutil, subprocess, sys, tempfile

PREFIX = 'DZ\\anims\\workspaces\\player\\player_main'
DEFAULT_PBO = os.path.expanduser('~/.steam/debian-installation/steamapps/common/DayZ/Addons/anims_workspaces.pbo')
UNPBO = os.path.expanduser('~/Projects/DayZMissions/tools/unpbo.py')

VARIABLES = '''   AnimSrcGCTVarFloat BR_FallPitch {
    DefaultValue 0
    MinValue -180
    MaxValue 180
   }
   AnimSrcGCTVarFloat BR_FallRoll {
    DefaultValue 0
    MinValue -180
    MaxValue 180
   }
   AnimSrcGCTVarFloat BR_FallSpread {
    DefaultValue 0
    MinValue -180
    MaxValue 180
   }
   AnimSrcGCTVarFloat BR_ChuteSwayX {
    DefaultValue 0
    MinValue -20
    MaxValue 20
   }
   AnimSrcGCTVarFloat BR_ChuteSwayY {
    DefaultValue 0
    MinValue -20
    MaxValue 20
   }
   AnimSrcGCTVarFloat BR_ChuteSwayZ {
    DefaultValue 0
    MinValue -20
    MaxValue 20
   }
   AnimSrcGCTVarFloat BR_ChuteGrip {
    DefaultValue 0
    MinValue 0
    MaxValue 1
   }
   AnimSrcGCTVarFloat BR_ChuteHands {
    DefaultValue 0
    MinValue 0
    MaxValue 1
   }
   AnimSrcGCTVarFloat BR_FallArms {
    DefaultValue 0
    MinValue 0
    MaxValue 1
   }
'''

def guid():
    return '{%016X}' % random.getrandbits(64)

def proc_node(name, child, variable, bones, pos):
    """A procedural rotation, by the variable (degrees), of each (bone, axis, amount): the
    amount scales it (-1: the other way, for the other side's limb)."""
    items = ''.join('''      AnimSrcNodeProcTrBoneItem "%s" {
       Bone "%s"
       Axis %s
       Space Model
       Op Rotate
       Amount %s
      }
''' % (guid(), bone, axis, amount) for bone, axis, amount in bones)
    return '''    AnimSrcNodeProcTransform %s {
     EditorPos %s
     Child "%s"
     Expression "%s * 0.01745329251994329576923690768489"
     Bones {
%s     }
    }
''' % (name, pos, child, variable, items)

def sway_node(name, child, variable, axis, pos):
    """The hips moved along an axis by the variable, in metres"""
    return '''    AnimSrcNodeProcTransform %s {
     EditorPos %s
     Child "%s"
     Expression "%s"
     Bones {
      AnimSrcNodeProcTrBoneItem "%s" {
       Bone "Pelvis"
       Axis %s
       Space Model
       Op Translate
       Amount 1
      }
     }
    }
''' % (name, pos, child, variable, guid(), axis)

def hands_node(below):
    """Under the canopy: the whole pose blends (by BR_ChuteHands, 0 to 1) into the standing
    surrender pose, hands up (holding the steering lines), held still."""
    return '''    AnimSrcNodeBlend BR_ChuteHandsNode {
     EditorPos 4 -17.3
     BlendWeight "BR_ChuteHands"
     Child0 "%s"
     Child1 "BR_ChuteHandsPose"
     OptimizeMin 1
     OptimizeMax 0
    }
    AnimSrcNodePose BR_ChuteHandsPose {
     EditorPos 5 -17.3
     Source "Surrender.Erc.Turn0"
     Time "0.0"
    }
''' % below

def correction_nodes(prefix, variable, levels, below, y):
    """Nodes rotating each level's bones (tools/solve_arms.py) by the variable (0 to 1), the
    first level applied first (nearest the rest of the graph): returns the nodes and the top
    one's name"""
    out = ''
    child = below
    for i, level in enumerate(levels):
        name = '%s%dNode' % (prefix, i)
        bones = [(bone, axis, '%.6f' % amount) for bone, items in level for axis, amount in items]
        node = proc_node(name, child, variable, bones, '4 %.1f' % (y + 0.6 * i))
        # the amounts are radians already: the expression is the 0 to 1 weight itself
        node = node.replace('Expression "%s * 0.01745329251994329576923690768489"' % variable, 'Expression "%s"' % variable)
        out = node + out
        child = name
    return out, child

def solver():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import solve_arms
    return solve_arms, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')

def arm_nodes(below):
    """The arms down the sides, as standing (by BR_FallArms, 0 to 1): a node per bone of the
    arm, collarbone to hand, both arms in each, each a rotation in X, Y and Z steps
    (tools/solve_arms.py, from the bones measured in tools/data/arm_bones.log)."""
    solve_arms, data = solver()
    log = os.path.join(data, 'arm_bones.log')
    return correction_nodes('BR_FallArms', 'BR_FallArms', solve_arms.solve(log, log), below, -16.4)

def grip_nodes(below):
    """Under the canopy, the hands gripping the lines (by BR_ChuteGrip, 0 to 1): the fingers
    closed as the raised fists have them, relative to the hand (tools/data/finger_bones.log)."""
    solve_arms, data = solver()
    levels = solve_arms.solve_fingers(os.path.join(data, 'finger_bones.log'))
    return correction_nodes('BR_ChuteGrip', 'BR_ChuteGrip', levels, below, -18.0)

def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    pbo = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_PBO
    work = tempfile.mkdtemp()
    try:
        subprocess.run([sys.executable, UNPBO, pbo, work], check=True, stdout=subprocess.DEVNULL)
        src = os.path.join(work, 'player', 'player_main')
        if not os.path.isfile(os.path.join(src, 'player_main.agr')):
            sys.exit('no player/player_main/player_main.agr in ' + pbo)
        if os.path.isdir(out):
            shutil.rmtree(out)
        shutil.copytree(src, out)
    finally:
        shutil.rmtree(work)
    with open(os.path.join(out, '$PBOPREFIX$'), 'w') as f:
        f.write(PREFIX + '\n')

    agr = os.path.join(out, 'player_main.agr')
    s = open(agr).read()
    # at the end of the variables: the game's own keep their places
    anchor = '  }\n  Commands {\n'
    if anchor not in s:
        sys.exit('player_main.agr: no Variables block followed by Commands')
    s = s.replace(anchor, VARIABLES + anchor, 1)
    open(agr, 'w').write(s)

    master = os.path.join(out, 'Master.agf')
    s = open(master).read()
    # the top of the graph: MasterControl blends a pose with MasterControlSTM, which runs
    # everything else; the tilt goes between them
    top = re.search(r'(    AnimSrcNodeBlend MasterControl \{\n(?:     .*\n)*?)     Child1 "MasterControlSTM"\n', s)
    if not top:
        sys.exit('Master.agf: MasterControl no longer has Child1 "MasterControlSTM"')
    s = s.replace(top.group(0), top.group(1) + '     Child1 "BR_ChuteSwayXNode"\n', 1)
    nodes = '  AnimSrcGraphSheet MasterControl {\n   Nodes {\n'
    if nodes not in s:
        sys.exit('Master.agf: no MasterControl sheet')
    arms, arms_top = arm_nodes('MasterControlSTM')
    grip, grip_top = grip_nodes('BR_ChuteHandsNode')
    # (a node can't have its variable's name: the expression then can't find the variable).
    # Top down: the roll and the pitch turn the whole body; under them the limbs move.
    # under the canopy the body swings about the canopy, not the hips: the tilt turns it at
    # the hips, these move the hips to where that swing puts them (metres, model space)
    added = sway_node('BR_ChuteSwayXNode', 'BR_ChuteSwayYNode', 'BR_ChuteSwayX', 'X', '4 -20.0') + \
        sway_node('BR_ChuteSwayYNode', 'BR_ChuteSwayZNode', 'BR_ChuteSwayY', 'Y', '4 -19.4') + \
        sway_node('BR_ChuteSwayZNode', 'BR_FallRollNode', 'BR_ChuteSwayZ', 'Z', '4 -18.8') + \
        proc_node('BR_FallRollNode', 'BR_FallPitchNode', 'BR_FallRoll',
                      [('Pelvis', 'Z', '1')], '4 -18.2') + \
        proc_node('BR_FallPitchNode', grip_top, 'BR_FallPitch',
                  [('Pelvis', 'X', '-1')], '4 -17.6') + \
        grip + \
        hands_node('BR_FallSpreadNode') + \
        proc_node('BR_FallSpreadNode', arms_top, 'BR_FallSpread',
                  [('LeftArm', 'Y', '1'), ('RightArm', 'Y', '-1'),
                   ('LeftUpLeg', 'Y', '-1'), ('RightUpLeg', 'Y', '1')], '4 -17.0') + \
        arms
    s = s.replace(nodes, nodes + added, 1)
    open(master, 'w').write(s)
    print('player animation graph with the jump\'s tilt: %s (%d files)' % (out, sum(len(f) for _, _, f in os.walk(out))))

if __name__ == '__main__':
    main()
