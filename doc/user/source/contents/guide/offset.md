(input-offset)=
# Layup offset (geometry)

A laminate-type component lays its layup along a base line.
The shape of each layer face is built by offsetting the base line by
the cumulative thickness — the inner side of layer *k* is the base
line offset inward by the sum of layer-1..k thicknesses, and the outer
side is the offset by the sum of layer-1..k-1 thicknesses (the base
line itself for the first layer).

For most cross-section geometry — straight strips, gentle airfoils,
boxes — the offset is geometrically uncomplicated and the user does
not need to think about it.
However, when the base line is a closed contour (such as a full
airfoil profile) and the layup is thick relative to the local
half-thickness of the contour, the offset can become geometrically
fragile: the inward-offset polygon may pinch at a thin trailing edge,
or — in the most extreme case — collapse entirely on a thin slab.

PreVABS 2.1 swapped the multi-vertex offset implementation for the
closed-contour path: a vendored copy of the
[Clipper2](https://github.com/AngusJohnson/Clipper2) library now
performs the polygon inflation, with PreVABS wrapping the result in
its existing base/offset correspondence contract.
This chapter documents the user-visible changes.


## 1. When the new backend kicks in

| Base line shape | Backend |
|---|---|
| 2-vertex open polyline (one segment) | unchanged single-segment offset (a small constant-direction translation) |
| 3+ vertex open polyline | unchanged 5-stage miter-join pipeline (folded-segment filter, sub-line trim, miter-limit bevel surrogate) |
| Closed contour (`base.front() == base.back()`) — typical airfoil case | **new Clipper2 backend** |

There is no user knob to switch backends.
The routing is automatic and depends only on whether the base line
closes back on itself.


## 2. New diagnostic messages

The new closed-contour path emits four kinds of warnings.
They are informational: PreVABS still completes the run and writes a
valid `.sg` file as long as the geometry is meshable.


### 2.1 Local half-thickness summary

Before invoking Clipper2, PreVABS scans every base vertex and
projects an inward-normal ray to measure the local half-thickness
`h_i` against the offset distance `|dist|`:

```
[warning]  offset (multi-vertex, closed): N base vertex/vertices have local half-thickness < |dist| = D (first at base[i] = ...); skin will be locally dropped at those locations
[warning]  offset (multi-vertex, closed): M base vertex/vertices have local half-thickness in [|dist|, 2*|dist|) = [D, 2D); offset will be valid but locally thin
```

- The first form means the layup is locally too thick for the
  contour to support a complete inward offset.
  Clipper2 silently absorbs the region (the resulting offset polygon
  is smaller than naive thickening would suggest) and PreVABS reports
  the actual dropped indices in §2.3.
- The second form means the post-offset thickness is below the
  "two-sided headroom" but the offset is still geometrically valid.

Actionable response:

- If the warnings cluster at the trailing-edge cusp of an airfoil,
  they are usually expected (the cusp is naturally `h ≈ 0`).
- If the warnings cluster elsewhere, consider reducing the layup
  thickness or refining the base line in that region.


### 2.2 Multi-branch (disconnected) output

In extreme pinch scenarios — a thin trailing edge with a thick layup
inward offset — Clipper2 may return multiple disconnected polygons
for a single base line.
PreVABS keeps the polygon with the largest absolute signed area
(with a base bounding-box centroid tie-break if multiple candidates
are within 5% of the largest area) and discards the rest, with:

```
[warning]  offset clipper2 bridge: K disconnected polygons; kept primary (area=A), dropped smaller pieces (areas=A_2, A_3, ...)
```

This is rare in practice.
If it triggers, the dropped pieces almost always correspond to
secondary skin material near the cusp that the run does not need.


### 2.3 Dropped base index range

For every contiguous run of base vertices whose corresponding offset
is geometrically absent (Clipper2 ate it), PreVABS reports the
inclusive range:

```
[warning]  offset (multi-vertex, closed): skin dropped over base indices [L..H] due to insufficient local thickness
```

Internally the affected base vertices are still listed in the
base/offset correspondence map (forward-filled to the closest offset
index), so the downstream mesh stays in-bounds.

Actionable response:

- If `[L..H]` covers a small region around an airfoil trailing edge,
  this is expected and harmless — the local skin is too thin to
  resolve at the configured layup thickness.
- If `[L..H]` covers a substantial portion of the contour, the layup
  thickness is excessive for the geometry and either the thickness or
  the contour should be revised.


### 2.4 Empty offset (offset distance exceeds half-thickness)

When the layup thickness exceeds the half-thickness over the entire
contour, Clipper2 returns no offset polygon and PreVABS aborts the
offset step:

```
[error]    offset (multi-vertex, closed): Clipper2 returned no offset polygon (local thickness < |dist| somewhere along the path?); returning empty result
```

The cross-section build cannot complete.
Reduce the layup thickness or use a thicker base contour.


## 3. Join style and miter limit

The new backend uses Clipper2's `Miter` join with a miter limit
factor of `2.0`, matching the legacy PreVABS implementation.
At a base vertex with interior angle below ~74°, the miter would
extend further than `2 * |dist|` from the vertex; Clipper2 then bevels
the join (replacing the single miter point with two bevel vertices).

The bevel introduces ~1e-8 grid quantisation per vertex
(Clipper2 1.4.0 uses precision = 8 internally), well below the typical
PreVABS mesh size (1e-3 to 1e-2).


## 4. Compatibility

- XML input format is unchanged.
  No new tags, no removed tags.
- The `.sg` output format is unchanged.
- The base/offset correspondence map (used downstream by
  `<component type="laminate">`) is unchanged.
  In particular, the staircase invariant (consecutive base- and
  offset-index deltas in `{0, 1}`) is preserved.

Existing cross-section XML files run with the new backend without
modification.
Output `.sg` files are byte-identical for the test cases tracked in
the integration suite (33 cases as of PreVABS 2.1.0).
