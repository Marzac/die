# A new shoot at raycasting

## Preface

The DIE renderer is originally based on the classic raycasting technique used in older 2.5D games (such as *Wolfenstein 3D* or *Doom*).
For each column of the viewport, a ray is cast against an array of walls. When an intersection is found, a height and a set of texture
coordinates can be computed, letting us draw a small vertical strip of the wall.

Where the DIE renderer departs from older, more efficient implementations is that those engines usually place walls on a grid, group them
into sectors connected by portals, or organize them in some kind of spatial tree. Visibility and occlusion are then deduced directly from
that data structure. This was great for raw rendering efficiency, but it forced the environment to be very static, and often relied on
the structure being precomputed ahead of time.

Computers are much faster now, so the DIE renderer evaluates the scene topology on the fly, as it goes.

## Building bricks

### Node

A node is an anchor in 3D space, with coordinates (x, y, z) in a right-handed, y-up system.
Nodes are the shared building blocks every other object is positioned with: a wall needs two of them (one for each end), while a sprite,
a light, or any other point object only needs one.

A node also carries three free meta parameters. The renderer and the engine give them no fixed meaning; they are free for the game designer
to use for whatever an object needs, for example pan, pitch and roll angles, or any other per-object characteristic.

Except for walls and paths, every other map object is created from a node, in the Nodes page.

### Wall

A wall is a one-sided flat surface, perpendicular to the floor. DIE does not support slanted walls. A wall is defined by its two end nodes
(which give its position, length and base height) and a height, measured upward from the base. Walls define the playable space by blocking
the view, and in DIE they also project floors and ceilings.

By default a wall is one-sided: only its front face (the side its normal points to) is rendered, and the back is backculled, i.e. invisible
and ignored entirely. Clearing the backculling flag turns the wall into a two-sided wall: its back face is then rendered too, using the back
surface's texture, so the wall is visible from either side.

A wall carries four independent surfaces, each with its own texture and its own scale/shift: the front face, the back face, the projected
floor and the projected ceiling. This lets a single wall show a different material on each side, and yet another material on the floor and
ceiling it projects.

### Floors and ceilings

Floors and ceilings are not separate objects: they are surfaces projected by the walls themselves. A wall's base height defines a floor plane,
and its top height (base + height) defines a ceiling plane. Each wall can be flagged to project its floor and/or its ceiling, independently
on its front side and on its back side. During rendering, these projected planes are what the renderer stacks and clips against each other
to figure out, column by column, which floor and which ceiling are actually visible at a given height : this is the "topology evaluated on
the fly" mentioned in the preface.

Since floors and ceilings are just projections from individual walls, the renderer has no notion of a "room" with a single, consistent ceiling
height. It is up to the scene designer to keep the geometry coherent: if two walls bordering the same space project ceilings at different heights,
both projections are valid as far as the renderer is concerned, and the result will look broken. Coherent spaces are a convention the designer
must uphold, not a constraint the renderer enforces.

A wall's floor and ceiling projections can each be enabled independently on the front side, on the back side, both, or neither. This is what
lets a closed room or box be built from a loop of walls: every wall projects its floor and ceiling toward the inside of the room (typically
its back side), while the outside remains unaffected.

## Simple objects

Sprites, lights and speakers are all single-node objects: a designer attaches one to a node, and its position simply follows that node.

### Sprite

A sprite is a textured quad standing at a node, with its own width, height and texture. Internally it is rendered as a single alpha wall,
so it benefits from the same texturing and lighting as a regular wall. Its orientation is either fixed, set by a pan angle, or automatic:
with the autopan flag set, the sprite always turns to face the camera, like a classic billboard. A sprite can also be made invisible,
backculled (one-sided), and can optionally cast shadows.

### Light

A light is a point light source bound to a node. It holds two colors and a strength, and an animation mode picks how it moves between them:
a fixed color (A or B), a smooth cycle, a pulse, a flash, or a random flicker. This animation mode also decides how the light is handled by
the glowmap: lights fixed on color A or color B are "still" and baked once into the static glowmap, while animated lights are recomputed
into the glowmap every frame.

### Speaker

A speaker is a spatial sound source bound to a node, referencing a sound file with its own volume and a size that shapes its falloff range.
Flags control its behaviour: autoplay on map load, triggering, toggling, looping, and omni (non-directional) playback.

## Advanced objects

Submaps, staircases, doors and lifts are all built on top of the same nodes and walls described above: they are generated as ordinary
walls (and, for submaps, a whole nested scene) at render time, but the designer edits them through their own higher-level parameters.

### Submap

A submap nests a whole separate map file inside the current one, as a self-contained unit. It is anchored to a node, which becomes the
submap's origin, and carries a pan (a rotation around Y) and a scale, both applied to the submap's entire content. Every node, wall,
light, sprite, and so on inside the nested map is positioned relative to that origin, then rotated and scaled as a block before being
merged into the parent scene. This is the main tool for reusing a piece of geometry — a room, a prop, a whole building — at multiple
places, sizes and orientations without duplicating it.

### Staircase

A staircase is generated from a single node (its base) plus a handful of parameters: a pan (orientation), an overall height and length
(the total rise and run), a width, and a number of steps. From these, the renderer builds the full flight automatically — one riser and
one tread per step, plus side walls — evenly spaced along the run. Three surfaces can be textured separately: the riser faces (step-fall),
the tread tops (step-top), and the sides of the staircase.

### Door

A door is a rectangular slab anchored to a node (its hinge), with its own width, height and thickness. Its closed orientation is set by
an angle, and opening rotates it by a swing amount around that hinge — currently the only supported door movement is this pivot/rotation.
An animation state (pan, 0.0 = closed .. 1.0 = open) drives this rotation over a given time, shaped by an easing curve; a separate shake
state plays a brief wobble, typically used as feedback when a locked door is triggered. Three surfaces can be textured independently:
front, back, and the side (the edge of the slab, seen while it's swinging open). Flags mark the door as alpha-blended, currently opening,
closing or shaking, and locked (which blocks opening and triggers the shake/feedback instead).

### Lift

A lift is a flat platform anchored to a node (its rest position), with its own width, length and thickness. It travels a given distance
along one axis (its travel), over a given time, shaped by an easing curve, with its current position tracked by an animation state (pan,
0.0 .. 1.0 along the travel). Three surfaces can be textured independently: top, bottom, and sides. Flags describe how it behaves once
triggered: haltable (it can be stopped mid-travel), continuous (it loops back and forth automatically) or return (it does a single round
trip then stops), plus runtime flags tracking whether it is currently going, returning, or halted, and whether it is locked.

## Lighting

The scene is lit from three independent sources, which are combined together at every shaded point.

### Ambient light

A flat, directionless light applied uniformly to every surface, regardless of its orientation or position. It is defined by a color and
a strength (0.0 .. 1.0) and represents the light bouncing around the scene as a whole. Without it, anything not directly facing a light
source would be pure black.

### Ray (directional / sun light)

A single light source infinitely far away, like the sun, hitting every surface from the same direction. Its direction is given by an hour
and an angle, both expressed on a 0 .. 24 scale like a clock face: hour is the sun's azimuth (which way it comes from around the horizon),
angle is its tilt (how high it sits above or below the horizon). It also has its own color and strength (0.0 .. 1.0). Each wall, floor and
ceiling is lit according to how directly it faces this direction: surfaces facing the sun get the ray color added on top of the ambient light,
surfaces facing away get little to none.

### Point lights

These are the Light objects described above: a position (via their node), a color (possibly animated) and a strength.
Unlike the ambient and ray lights, point lights are local: their contribution falls off with distance, and they are the only source of light
that is computed through the glowmap.

## The glowmap

The glowmap is a 2D grid of accumulated light values laid flat over the world's X/Z plane, centered on the world origin.
It is configured by two parameters: its resolution (a power-of-two size, e.g. 256x256 cells) and the area it covers, in world units.
Together they set the size of one cell, and therefore how fine the lighting detail can be.

For each point light, every glowmap cell within its reach accumulates that light's color, scaled down by the squared distance to the light
(the falloff). While accumulating, a 2D ray is cast from the light to the cell against every wall segment: if a wall (that isn't flagged to
skip shadows) crosses that ray, the cell is considered shadowed and the light contributes nothing to it. While rendering, the glowmap is
sampled (with bilinear filtering) at the position of each wall column and each floor/ceiling pixel, and added to the ambient and ray
lighting already computed for that surface.

To keep the cost down, the glowmap is split into two layers. Lights left on a fixed color (A or B, not animated) are "still": they are
baked once into a static layer, and only recomputed when something asks for a rebuild (e.g. the static geometry or a still light changed).
Animated lights (cycle, pulse, flash, flicker) cannot be baked, since their color possibly changes every frame. Therefore they are recomputed
into a second, dynamic layer on top of the static one, every single frame. The more animated lights and the larger the area they cover,
the more this costs per frame, so animated lights should be used sparingly.

The glowmap is purely 2D: it has no notion of height. A light and the shadows it casts are computed only from the X/Z positions of the light
and the walls, completely ignoring the light's Y position and the walls' height. In practice this means a light placed on one floor will shine
on the X/Z footprint of every other floor stacked above or below it, walls or no walls, since the glowmap cannot tell those levels apart.
Scene designers need to keep this in mind when stacking spaces vertically: a light "leaking" into the level below or above is a glowmap
limitation to design around.

## Ambient occlusion

The renderer also applies a cheap, "fake" ambient occlusion: it does not actually test whether nearby geometry blocks ambient light,
it simply darkens the screen-space band that sits right where a wall meets the floor or ceiling it borders. Because most corners in this
kind of geometry are exactly that, a wall meeting a floor or a ceiling, darkening that band consistently looks like a soft contact shadow,
without ever having to look at neighbouring surfaces.

Concretely, for every wall strip, a band of a fixed world-space depth is measured inward from its top and bottom edges.
Within that band, the lit color is scaled down by a darkening factor that fades smoothly from full darkening right at the edge, to no darkening
at all at the inner end of the band. The same band is applied symmetrically on the floor and ceiling surfaces themselves, darkening the strip
of floor or ceiling closest to the wall that borders them. Outside these bands, surfaces are shaded normally.

The two parameters are occlusion length, how far the darkened band reaches from a wall/floor or wall/ceiling edge, and occlusion darken,
how strong the darkening gets at the edge itself (0.0 = no effect, 1.0 = fully black at the edge). The effect is subtle but effective:
it adds contact shadows to every corner in the scene for almost no cost, giving flat-shaded rooms a sense of depth and weight that pure
directional and point lighting alone cannot provide.

## Fog

A simple distance fog can be enabled, defined by a near distance, a far distance and a color. Below the near distance, surfaces are shown
with their normal lit color, untouched by fog. Beyond the far distance, surfaces are fully replaced by the fog color. In between, the surface
color is blended toward the fog color following a quadratic (ease-in) curve: the blend stays close to the lit color just past the near
distance, then ramps up faster as it approaches the far distance. This is computed per pixel, both for wall strips (using the wall's
distance along the ray) and for floor/ceiling surfaces (using the distance to that particular pixel on the surface), so fog follows the
actual ground and ceiling rather than just the nearest wall.

## Post effects

After the scene itself is rendered, a chain of optional post-processing passes runs over the whole frame. Each can be toggled independently.

### Motion blur

Blends the previous frame into the current one, by a percentage (0 .. 100). At 0, each frame is shown as rendered; as the percentage rises,
more of the previous frame lingers into the current one, smearing fast motion and smoothing out temporal noise such as glowmap or AO flicker.

### Vignette

Darkens the corners of the frame in a circular gradient. An inner radius marks where the darkening starts, and an outer radius marks where
the frame becomes fully black; both are expressed as a percentage of half the frame height. Between the two radii, the darkening increases
smoothly from the center outwards.

### Gamma / tone curve

Applies an S-shaped (smoothstep) tone curve to each color channel independently, with its own gain for red, green and blue (0.0 = channel
left unchanged, 1.0 = full curve applied). This pushes dark tones darker and bright tones brighter, increasing contrast, and can be used
per channel for basic color grading.

## Advanced tools

### Tags

A tag is a name/value pair stored in a shared, project-wide database. Most map objects — nodes, submaps, doors, lifts, sprites, lights,
speakers, paths — carry a reference to one tag. This is not used by the renderer itself; it is a generic label designers can attach to
any of these objects, with an associated free-form value. Because several objects can reference the same tag name, it also doubles as a
grouping mechanism: looking up a tag by name returns every object of a given kind that carries it, which is how related objects scattered
across a map — say, every light or every door belonging to the same event — can be found and acted on together.

### Paths

A path is a named, ordered sequence of existing nodes — up to a fixed maximum per path — used as a list of waypoints. A path does not
introduce new geometry of its own: it simply records an ordering over nodes that already exist for other purposes. Like other objects, a
path can carry a tag, so a set of paths can be grouped and looked up together. Paths are the building block for anything that needs to
move along a route or follow a sequence of points in the world.


