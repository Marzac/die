"""
DIE ENGINE - test map generator: 6 rooms, 5 corridors, a courtyard and a skybox.

Cap convention (as the renderer reads it):
  - a wall carries a cap at its base (FLOOR flag) and at its top (CEILING flag),
    on its front and / or back side; a cap above the camera shows as a ceiling,
    below the camera as a floor.
  - every cap is a plane: rise along the wall (from the node heights) and
    drop away from the wall (drop = -8 * gradient . side normal).
  - any change of floor / ceiling plane is delimited by a wall:
    a step (floors differ), a lintel (ceilings differ) or an invisible
    threshold (same height, different slope).
  - a wall has ONE floor surface and ONE ceiling surface for both of its sides:
    sectors meeting at the same floor height must share their floor texture,
    else use a step (door sill), which keeps the two textures apart
    (base cap = floor surface, top cap = ceiling surface).

Run: python gen_complex.py  ->  writes test_complex.map next to this script.
"""

import math
import os

EPS = 1e-3
SKY = 256.0

# Texture tiles of default.png
TEX_CHECKER, TEX_BRICK, TEX_STONE, TEX_WOOD, TEX_MARBLE, TEX_SKY = 0, 1, 2, 3, 4, 5

# Surface flags
SURFACE_NO_ENV_LIGHT, SURFACE_NO_GLOW = 0x0008, 0x0010

# Wall flags
WALL_INVISIBLE, WALL_BACKCULLED, WALL_NO_SHADOW = 0x0001, 0x0002, 0x0008
WALL_FLOOR_FRONT, WALL_FLOOR_BACK = 0x0010, 0x0020
WALL_CEILING_FRONT, WALL_CEILING_BACK = 0x0040, 0x0080


class Plane:
    """y = h0 + gx * x + gz * z"""
    def __init__(self, h0, gx=0.0, gz=0.0):
        self.h0, self.gx, self.gz = h0, gx, gz

    def at(self, x, z):
        return self.h0 + self.gx * x + self.gz * z

    def same(self, other):
        return (abs(self.h0 - other.h0) < EPS and abs(self.gx - other.gx) < EPS
                and abs(self.gz - other.gz) < EPS)


class Sector:
    def __init__(self, name, rect, floor, ceil, wall_tex, floor_tex, ceil_tex, ceil_scale=4.0):
        self.name = name
        self.rect = rect                    # (x1, z1, x2, z2) or None
        self.floor = floor                  # Plane
        self.ceil = ceil                    # Plane
        self.wall_tex, self.floor_tex, self.ceil_tex = wall_tex, floor_tex, ceil_tex
        self.ceil_scale = ceil_scale

    def center(self):
        x1, z1, x2, z2 = self.rect
        return ((x1 + x2) * 0.5, (z1 + z2) * 0.5)


class MapWriter:
    def __init__(self):
        self.nodes = []         # (x, y, z)
        self.node_ids = {}
        self.walls = []
        self.lights = []

    def node(self, x, y, z):
        key = (round(x, 4), round(y, 4), round(z, 4))
        if key not in self.node_ids:
            self.node_ids[key] = len(self.nodes)
            self.nodes.append(key)
        return self.node_ids[key]

    def wall(self, p1, p2, side_a, point_a, side_b, base, top, extra_flags=0,
             face_tex=None, scale=4.0, surf_flags=0):
        """
        Wall from p1 to p2 (x, z), base / top: Plane of its bottom and top edges.
        side_a: sector on the side of point_a, side_b: sector on the other side (None = solid).
        """
        (x1, z1), (x2, z2) = p1, p2
        dx, dz = x2 - x1, z2 - z1
        length = math.hypot(dx, dz)
        ux, uz = dx / length, dz / length
        nfx, nfz = uz, -ux                                  # front normal
        mx, mz = (x1 + x2) * 0.5, (z1 + z2) * 0.5
        a_front = (point_a[0] - mx) * nfx + (point_a[1] - mz) * nfz > 0.0
        front, back = (side_a, side_b) if a_front else (side_b, side_a)

        b1, b2 = base.at(x1, z1), base.at(x2, z2)
        h1, h2 = top.at(x1, z1) - b1, top.at(x2, z2) - b2
        assert abs(h1 - h2) < EPS, f"wall height not constant {p1} {p2}"
        height = h1
        assert height > -EPS, f"negative wall height {p1} {p2}"

        flags = extra_flags
        drops = {"ff": 0.0, "fb": 0.0, "cf": 0.0, "cb": 0.0}
        floor_tex = ceil_tex = None
        ceil_scale = 4.0

        def on_plane(edge, plane):
            return (abs(edge.at(x1, z1) - plane.at(x1, z1)) < EPS and
                    abs(edge.at(x2, z2) - plane.at(x2, z2)) < EPS)

        for sector, is_front in ((front, True), (back, False)):
            if sector is None:
                continue
            nx, nz = (nfx, nfz) if is_front else (-nfx, -nfz)

            def drop(plane):
                return -8.0 * (plane.gx * nx + plane.gz * nz)

        # Base cap: the sector floor seen from above, or its ceiling seen from below
            for plane, tex in ((sector.floor, sector.floor_tex), (sector.ceil, sector.ceil_tex)):
                if plane is not None and on_plane(base, plane):
                    flags |= WALL_FLOOR_FRONT if is_front else WALL_FLOOR_BACK
                    drops["ff" if is_front else "fb"] = drop(plane)
                    floor_tex = floor_tex if floor_tex is not None else tex
                    break
        # Top cap: the sector ceiling seen from below, or its floor seen from above
            for plane, tex, sc in ((sector.ceil, sector.ceil_tex, sector.ceil_scale),
                                   (sector.floor, sector.floor_tex, 4.0)):
                if plane is not None and on_plane(top, plane):
                    flags |= WALL_CEILING_FRONT if is_front else WALL_CEILING_BACK
                    drops["cf" if is_front else "cb"] = drop(plane)
                    if ceil_tex is None:
                        ceil_tex, ceil_scale = tex, sc
                    break

        def tex_of(sector):
            if face_tex is not None:
                return face_tex
            return sector.wall_tex if sector is not None else TEX_BRICK

        surfaces = [
            (tex_of(front), scale, scale, surf_flags),
            (tex_of(back), scale, scale, surf_flags),
            (floor_tex if floor_tex is not None else TEX_STONE, 4.0, 4.0, 0),
            (ceil_tex if ceil_tex is not None else TEX_STONE, ceil_scale, ceil_scale, surf_flags),
        ]
        n1 = self.node(x1, b1, z1)
        n2 = self.node(x2, b2, z2)
        self.walls.append((n1, n2, height, flags, drops, surfaces))

    def light(self, x, z, color, strength, falloff, y=8.0):
        self.lights.append((self.node(x, y, z), color, strength, falloff))

    def save(self, path, header):
        with open(path, "w", newline="\n") as f:
            f.write("# == DIE MAP == \n")
            for line in header:
                f.write(f"# {line}\n")
            f.write("# 07.11.26 - V1.2\n")
            f.write("# == NODES ==\n")
            f.write("# format: N x, y, z, metaA, metaB, metaC, tag, flags\n")
            for x, y, z in self.nodes:
                f.write(f"N {x:+.4f}, {y:+.4f}, {z:+.4f}, +0.0000, +0.0000, +0.0000, None, 0001\n")
            f.write("\n# == WALLS ==\n")
            f.write("# format: W nodeID1, nodeID2, height, flags, floorFrontDrop, floorBackDrop, ceilingFrontDrop, ceilingBackDrop\n")
            f.write("# format: S surfaceID, textureID, scaleX, scaleY, shiftX, shiftY, flags\n")
            for n1, n2, height, flags, d, surfaces in self.walls:
                f.write(f"W {n1:04d}, {n2:04d}, {height:+.4f}, {flags:04x}, "
                        f"{d['ff']:+.4f}, {d['fb']:+.4f}, {d['cf']:+.4f}, {d['cb']:+.4f}\n")
                for j, (tex, sx, sy, sflags) in enumerate(surfaces):
                    f.write(f"\tS {j:02d}, {tex:04d}, {sx:+.4f}, {sy:+.4f}, +0.0000, +0.0000, {sflags:04x}\n")
            f.write("\n# == LIGHTS ==\n")
            f.write("# format: L nodeID, colorA, colorB, strength, falloff, speed, anim, tag, flags\n")
            for nid, color, strength, falloff in self.lights:
                f.write(f"L {nid:04d}, {color:08x}, 00404040, {strength:+.4f}, {falloff:+.4f}, +1.0000, 0000, None, 0002\n")
            f.write("\n# == ENVIRONMENT ==\n")
            f.write("R bbbbbb, +0.6000, ffffdd, +1.0000, +10.0000, +1.0000\n")
            f.write("F 000000, +64.0000, +512.0000, 0000\n")


# =============================================================================
# Layout (x right, z down in the editor top view), units: player radius = 2, eye ~ 12
# =============================================================================
DOOR_H = 20.0       # corridor / door clear height

def flat(h):
    return Plane(h)

def room(name, rect, floor, height, wall_tex, floor_tex, ceil_tex):
    return Sector(name, rect, flat(floor), flat(floor + height), wall_tex, floor_tex, ceil_tex)


THICK = 2.0         # wall thickness

# Interior rects; the walls grow outwards by THICK
R1 = room("R1 hall",     (-48, -92, 16, -46),  0.0, 40.0, TEX_MARBLE, TEX_STONE, TEX_WOOD)
R2 = room("R2 office",   (40, -88, 72, -56),   1.0, 24.0, TEX_WOOD, TEX_WOOD, TEX_MARBLE)
R3 = room("R3 storage",  (64, -24, 88, 16),    0.0, 24.0, TEX_BRICK, TEX_STONE, TEX_WOOD)
R4 = room("R4 raised",   (64, 40, 104, 72),    4.0, 28.0, TEX_MARBLE, TEX_WOOD, TEX_MARBLE)
R5 = room("R5 gallery",  (-104, -56, -80, 24), 1.0, 32.0, TEX_WOOD, TEX_MARBLE, TEX_WOOD)
R6 = room("R6 chapel",   (-104, 48, -56, 96), -4.0, 48.0, TEX_MARBLE, TEX_MARBLE, TEX_STONE)

def corridor(name, rect, floor=None, height=DOOR_H):
    floor = floor if floor is not None else flat(0.0)
    ceil = Plane(floor.h0 + height, floor.gx, floor.gz)
    return Sector(name, rect, floor, ceil, TEX_BRICK, TEX_STONE, TEX_STONE)

# Corridors run from interior face to interior face, through both wall thicknesses
K0 = corridor("K0 hall porch", (-20, -46, -4, -40), height=24.0)
K1 = corridor("K1", (16, -68, 40, -60))
K2 = corridor("K2", (48, -8, 64, 0))
K3 = corridor("K3 ramp", (72, 16, 80, 40), Plane(-16.0 / 12.0, 0.0, 1.0 / 12.0))  # 0 at z=16 -> 2 at z=40, then a step
K4 = corridor("K4", (-80, -12, -48, -4))
K5 = corridor("K5", (-96, 24, -88, 48))

# Outdoors: courtyard and grounds are the same sector (floor 0, sky)
GROUNDS = Sector("grounds", (-256, -256, 256, 256), flat(0.0), flat(SKY), TEX_BRICK, TEX_STONE, TEX_SKY, 0.25)

def band(top):
    """Solid wall mass, its top seen from above as a floor."""
    return Sector("band", None, top, None, TEX_BRICK, TEX_STONE, TEX_STONE)

m = MapWriter()


def opening(p1, p2, a, a_point, b, host_top, door_top=None):
    """Fill a doorway between sectors a and b, under a host wall reaching host_top (Plane)."""
    x, z = (p1[0] + p2[0]) * 0.5, (p1[1] + p2[1]) * 0.5
    fa, fb = a.floor.at(x, z), b.floor.at(x, z)
    lo_floor = a.floor if fa <= fb else b.floor
    hi_floor = b.floor if fa <= fb else a.floor
    ceils = [a.ceil, b.ceil] + ([door_top] if door_top is not None else [])
    hi = min(ceils, key=lambda p: p.at(x, z))
    flags = WALL_NO_SHADOW

# Step: the floors differ in height
    if abs(fa - fb) > EPS:
        m.wall(p1, p2, a, a_point, b, lo_floor, hi_floor, flags)
# Invisible threshold: same floor height, different slope
    elif not a.floor.same(b.floor):
        m.wall(p1, p2, a, a_point, b, hi_floor, hi, flags | WALL_INVISIBLE)
# Lintel: from the door top to the host wall top
    if host_top.at(x, z) - hi.at(x, z) > EPS:
        m.wall(p1, p2, a, a_point, b, hi, host_top, flags)
    elif abs(a.ceil.at(x, z) - b.ceil.at(x, z)) < EPS and not a.ceil.same(b.ceil):
        m.wall(p1, p2, a, a_point, b, hi_floor, hi, flags | WALL_INVISIBLE)


def face(p1, p2, side, side_point, other, base, top, doors=()):
    """
    Wall face from p1 to p2, split around the doors given as absolute coordinates
    along its axis: (a1, a2, other_sector or None, door_top). With other_sector,
    the gap gets an opening (step / lintel / threshold), else it is left open
    (a corridor crossing the wall thickness bounds it).
    """
    (x1, z1), (x2, z2) = p1, p2
    length = math.hypot(x2 - x1, z2 - z1)
    along_x = abs(z2 - z1) < EPS

    def lerp(t):
        return (x1 + (x2 - x1) * t / length, z1 + (z2 - z1) * t / length)

    gaps = []
    for a1, a2, door_other, door_top in doors:
        o = x1 if along_x else z1
        t1, t2 = abs(a1 - o), abs(a2 - o)
        gaps.append((min(t1, t2), max(t1, t2), door_other, door_top))
    gaps.sort(key=lambda g: g[0])

    t = 0.0
    for t1, t2, door_other, door_top in gaps + [(length, length, None, None)]:
        if t1 > t + EPS:
            m.wall(lerp(t), lerp(t1), side, side_point, other, base, top)
        if door_other is not None:
            opening(lerp(t1), lerp(t2), side, side_point, door_other, top, door_top)
        t = t2


def rect_sides(rect, grow=0.0):
    x1, z1, x2, z2 = rect
    x1, z1, x2, z2 = x1 - grow, z1 - grow, x2 + grow, z2 + grow
    return {
        "N": ((x1, z1), (x2, z1)),
        "E": ((x2, z1), (x2, z2)),
        "S": ((x2, z2), (x1, z2)),
        "W": ((x1, z2), (x1, z1)),
    }


def thick_room(r, doors=None):
    """
    Room as a closed thick box. doors: per side, (a1, a2, corridor) along the side axis.
    Interior faces carry the room floor / ceiling; exterior faces stand on the ground
    and carry the wall top (band) and the ground outside.
    """
    doors = doors or {}
    top = r.ceil
    ground = GROUNDS.floor
    for side, (p1, p2) in rect_sides(r.rect).items():
        face(p1, p2, r, r.center(), None, r.floor, top,
             [(a1, a2, k, None) for a1, a2, k in doors.get(side, [])])
    for side, (p1, p2) in rect_sides(r.rect, THICK).items():
        ds = doors.get(side, [])
        face(p1, p2, band(top), r.center(), GROUNDS, ground, top, [(a1, a2, None, None) for a1, a2, k in ds])
        outer_openings(p1, p2, r.center(), ds, top)


def outer_openings(p1, p2, inner_point, doors, top):
    """
    Corridor holes in an exterior face. The corridor continues on both sides of the
    face line, so only the wall mass above its ceiling is needed: a lintel carrying
    the corridor ceiling on both of its sides.
    """
    (x1, z1), (x2, z2) = p1, p2
    along_x = abs(z2 - z1) < EPS
    for a1, a2, k in doors:
        q1 = (a1, z1) if along_x else (x1, a1)
        q2 = (a2, z1) if along_x else (x1, a2)
        x, z = (q1[0] + q2[0]) * 0.5, (q1[1] + q2[1]) * 0.5
        if top.at(x, z) - k.ceil.at(x, z) > EPS:
            m.wall(q1, q2, k, inner_point, k, k.ceil, top, WALL_NO_SHADOW)


def corridor_walls(k, axis):
    """Side walls along the corridor axis ('x' or 'z'): jambs through the walls, then the corridor."""
    x1, z1, x2, z2 = k.rect
    if axis == "x":
        pairs = [((x1, z1), (x2, z1)), ((x2, z2), (x1, z2))]
    else:
        pairs = [((x2, z1), (x2, z2)), ((x1, z2), (x1, z1))]
    for p1, p2 in pairs:
        m.wall(p1, p2, k, k.center(), GROUNDS, k.floor, k.ceil)


# Rooms ------------------------------------------------------------------------
thick_room(R1, {"S": [(-20, -4, K0)], "E": [(-68, -60, K1)]})
thick_room(R2, {"W": [(-68, -60, K1)]})
thick_room(R3, {"W": [(-8, 0, K2)], "S": [(72, 80, K3)]})
thick_room(R4, {"N": [(72, 80, K3)]})
thick_room(R5, {"E": [(-12, -4, K4)], "S": [(-96, -88, K5)]})
thick_room(R6, {"N": [(-96, -88, K5)]})

# Corridors ----------------------------------------------------------------------
corridor_walls(K0, "z")
corridor_walls(K1, "x")
corridor_walls(K2, "x")
corridor_walls(K3, "z")
corridor_walls(K4, "x")
corridor_walls(K5, "z")

# Courtyard: thick U-shaped facade (height 24), open to the south --------------------
FACADE = flat(24.0)
FACADE_BAND = band(FACADE)
COURT_CENTER = (0.0, 0.0)
door_n = [(-20, -4, K0, None)]
door_e = [(-8, 0, K2, None)]
door_w = [(-12, -4, K4, None)]
# Courtyard faces
face((-48, 40), (-48, -40), GROUNDS, COURT_CENTER, None, GROUNDS.floor, FACADE, door_w)
face((-48, -40), (48, -40), GROUNDS, COURT_CENTER, None, GROUNDS.floor, FACADE, door_n)
face((48, -40), (48, 40), GROUNDS, COURT_CENTER, None, GROUNDS.floor, FACADE, door_e)
# Outer faces, corridors crossing them
gaps = lambda ds: [(a1, a2, None, None) for a1, a2, k, t in ds]
face((-50, -42), (-50, 40), FACADE_BAND, (-49, 0), GROUNDS, GROUNDS.floor, FACADE, gaps(door_w))
face((50, -42), (-50, -42), FACADE_BAND, (0, -41), GROUNDS, GROUNDS.floor, FACADE, gaps(door_n))
face((50, 40), (50, -42), FACADE_BAND, (49, 0), GROUNDS, GROUNDS.floor, FACADE, gaps(door_e))
outer_openings((-50, -42), (-50, 40), (-49, 0), [(a1, a2, k) for a1, a2, k, t in door_w], FACADE)
outer_openings((50, -42), (-50, -42), (0, -41), [(a1, a2, k) for a1, a2, k, t in door_n], FACADE)
outer_openings((50, 40), (50, -42), (49, 0), [(a1, a2, k) for a1, a2, k, t in door_e], FACADE)
# Ends of the U
m.wall((-50, 40), (-48, 40), FACADE_BAND, (-49, 0), GROUNDS, GROUNDS.floor, FACADE)
m.wall((48, 40), (50, 40), FACADE_BAND, (49, 0), GROUNDS, GROUNDS.floor, FACADE)

# Free-standing garden wall south of the courtyard (height 8, its top seen from above)
GARDEN = (-32, 60, 32, 62)
for side, (p1, p2) in rect_sides(GARDEN).items():
    m.wall(p1, p2, band(flat(8.0)), (0, 61), GROUNDS, GROUNDS.floor, flat(8.0))

# Pillars: 2 in the hall, 1 in the chapel (solid inside)
def pillar(r, cx, cz, half):
    pts = [(cx - half, cz - half), (cx + half, cz - half), (cx + half, cz + half), (cx - half, cz + half)]
    for i in range(4):
        m.wall(pts[i], pts[(i + 1) % 4], None, (cx, cz), r, r.floor, r.ceil)

pillar(R1, -24, -64, 4)
pillar(R1, -4, -64, 4)
pillar(R6, -80, 72, 6)

# Skybox: inward faces (front side), standing on the ground (carries it up to the horizon),
# ceiling at the top, backculled, sky surfaces not lit
SKY_BASE, SKY_HALF = GROUNDS.floor, 256.0
corners = [(-SKY_HALF, -SKY_HALF), (-SKY_HALF, SKY_HALF), (SKY_HALF, SKY_HALF), (SKY_HALF, -SKY_HALF)]   # inside = front side
for i in range(4):
    m.wall(corners[i], corners[(i + 1) % 4], GROUNDS, (0, 0), None, SKY_BASE, flat(SKY),
           WALL_BACKCULLED | WALL_NO_SHADOW, face_tex=TEX_SKY, scale=0.25,
           surf_flags=SURFACE_NO_ENV_LIGHT | SURFACE_NO_GLOW)

# Lights ---------------------------------------------------------------------------
m.light(-16, -56, 0xffd8a0, 1.5, 48)        # hall
m.light(56, -72, 0xfff0d0, 1.2, 32)         # office
m.light(76, -4, 0xa0c0ff, 1.2, 32)          # storage
m.light(84, 56, 0xffa060, 1.4, 40)          # raised room
m.light(-92, -16, 0xfff0e0, 1.2, 48)        # gallery
m.light(-80, 60, 0xc080ff, 1.6, 56)         # chapel
m.light(0, 0, 0xffe0b0, 1.0, 40)            # courtyard lantern

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_complex_thick.map")
m.save(out, [
    "Test map: 6 rooms, 6 corridors, a courtyard (start at 0, 12, 0) and a skybox, thick walls (2 units).",
    "Generated by gen_complex.py - steps, lintels and invisible thresholds at the openings.",
])
print(f"{out}: {len(m.nodes)} nodes, {len(m.walls)} walls, {len(m.lights)} lights")