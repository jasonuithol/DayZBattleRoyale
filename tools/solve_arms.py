#!/usr/bin/env python3
"""The arms-down corrections for the jump (tools/make_anims.py reads them from here): the
rotations, in model space about each bone's joint, parents first, that put the lying pose's
arms where the standing pose has them, relative to Spine3 (the chest).

The bone frames come from a player's own game (the server doesn't animate the arm bones):
BR_ChuteCommand.s_Measure logs them standing in the plane and lying 3 s into the fall, as
[BRMeasure] lines in the game's script log; tools/data/arm_bones.log holds a set. Solved as
standard rotations, each turned into DayZ's (a rotation by t about an axis is the standard one
by -t, as the jump's pitch showed) and split into X, Y, Z steps, applied in that order.

  tools/solve_arms.py LOG [LOG_STANDING]     prints the levels (and the fit, on stderr)
"""
import math, re, sys
import numpy as np

def load(path, label):
    bones = {}
    for line in open(path):
        m = re.search(r'\[BRMeasure\] %s (\S+) -?\d+ (.*)$' % label, line)
        if not m:
            continue
        rows = [list(map(float, part.split())) for part in m.group(2).split('|')]
        R = np.array(rows[:3]).T  # columns = the bone's axes in model space
        p = np.array(rows[3])
        bones[m.group(1)] = (R, p)
    return bones

def solve(prone_log, stand_log):
    prone = load(prone_log, 'prone')
    stand = load(stand_log, 'stand')
    return _solve(prone, stand)

SIDES = {'Left': ['LeftShoulder', 'LeftArm', 'LeftArmRoll', 'LeftForeArm', 'LeftForeArmRoll', 'LeftHand'],
         'Right': ['RightShoulder', 'RightArm', 'RightArmRoll', 'RightForeArm', 'RightForeArmRoll', 'RightHand']}

def rel(b, pose):  # a bone's frame relative to Spine3's
    Rs, ps = pose['Spine3']
    R, p = pose[b]
    return Rs.T @ R, Rs.T @ (p - ps)

def euler_xyz(C):
    """C = Rz(c) Ry(b) Rx(a), standard right-handed matrices"""
    b = math.asin(max(-1, min(1, -C[2, 0])))
    a = math.atan2(C[2, 1], C[2, 2])
    c = math.atan2(C[1, 0], C[0, 0])
    return a, b, c

def _solve(prone, stand):
    return solve_groups(prone, stand, [('Spine3', chain) for chain in SIDES.values()])

FINGERS = ['Thumb', 'Index', 'Middle', 'Ring', 'Pinky']

def solve_fingers(log, frm='hands', to='fist'):
    """The fingers closed into the fist's grip (relative to their hand), from the hands-up pose"""
    groups = [(side + 'Hand', [side + 'Hand' + f + str(n) for n in (1, 2, 3)]) for side in ('Left', 'Right') for f in FINGERS]
    return solve_groups(load(log, frm), load(log, to), groups)

def solve_groups(prone, stand, groups):
    """Each group is (the reference bone, a chain of bones from it outwards): the corrections
    that give each chain, relative to its reference, the frames the second pose has. Returns
    levels: per position in the chains, [(bone, [(axis, DayZ amount in radians)])]."""
    levels = []
    for ref, chain in groups:
        Rr_p, _ = prone[ref]
        Rr_s, _ = stand[ref]
        cur = {b: (prone[b][0].copy(), prone[b][1].copy()) for b in chain}
        for i, b in enumerate(chain):
            want = Rr_p @ (Rr_s.T @ stand[b][0])  # the second pose's frame, on this pose's reference
            C = want @ cur[b][0].T                  # the correction, model space
            a, bb, c = euler_xyz(C)
            # DayZ's rotate by t about an axis is the standard rotation by -t (measured on the pitch)
            items = [('X', -a), ('Y', -bb), ('Z', -c)]
            joint = cur[b][1].copy()
            for d in chain[i:]:                     # the rest of the chain follows, about this joint
                R, p = cur[d]
                cur[d] = (C @ R, joint + C @ (p - joint))
            if len(levels) <= i:
                levels.append([])
            levels[i].append((b, items))
        for b in chain:
            got = Rr_p.T @ cur[b][0]
            wanted = Rr_s.T @ stand[b][0]
            err = math.degrees(math.acos(max(-1, min(1, (np.trace(got.T @ wanted) - 1) / 2))))
            print('# %s: %.3f deg off after solving' % (b, err), file=sys.stderr)
    return levels

if __name__ == '__main__':
    levels = solve(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else sys.argv[1])
    for i, level in enumerate(levels):
        print('level %d' % i)
        for b, items in level:
            print('  %s %s' % (b, ' '.join('%s %.6f' % it for it in items)))
