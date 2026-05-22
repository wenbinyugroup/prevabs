#!/usr/bin/env python3
"""
dp_matcher_prototype.py — Phase A of plan-20260522-staircase-dp-matcher.md.

Python prototype of a constrained monotone curve matcher
(DTW-style min-sum DP) for the PreVABS base/offset staircase generation
problem. Goal: validate the algorithm choice on real failing cases
(mh104, mh104_te_only) before committing to the ~3-day C++ port.

Input  : *.base_offset_map.json files emitted by prevabs --debug geo
         (see include/debug/baseOffsetMapJson.hpp)
Output : *.dp_compare.svg files showing base curve, offset curve,
         DP-matched staircase, and the production-staircase reference,
         overlaid for visual comparison.

Usage  : python dp_matcher_prototype.py <json_file> [<json_file> ...]
         python dp_matcher_prototype.py                # picks defaults
"""

from __future__ import annotations

import json
import math
import os
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

XY = Tuple[float, float]


def dist(a: XY, b: XY) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def dist2(a: XY, b: XY) -> float:
    dx, dy = a[0] - b[0], a[1] - b[1]
    return dx * dx + dy * dy


def normalize(v: XY) -> XY:
    n = math.hypot(v[0], v[1])
    return (v[0] / n, v[1] / n) if n > 0 else (0.0, 0.0)


def tangents(pts: List[XY], closed: bool) -> List[XY]:
    """Per-vertex unit tangent: centered diff for interior, one-sided
    at endpoints (open) or wrap (closed)."""
    n = len(pts)
    out: List[XY] = []
    for i in range(n):
        if closed:
            ip = (i + 1) % n
            im = (i - 1) % n
        else:
            ip = min(i + 1, n - 1)
            im = max(i - 1, 0)
        t = (pts[ip][0] - pts[im][0], pts[ip][1] - pts[im][1])
        out.append(normalize(t))
    return out


def cumulative_arclength(pts: List[XY], closed: bool) -> List[float]:
    """Normalized [0, 1] arclength parameter per vertex."""
    n = len(pts)
    if n == 0:
        return []
    raw = [0.0]
    for i in range(1, n):
        raw.append(raw[-1] + dist(pts[i - 1], pts[i]))
    if closed and n >= 1:
        # Don't append the wrap segment to per-vertex u: the trailing
        # duplicate is the JSON convention for closed PDCEL baselines
        # but here we want N distinct vertices. The caller is expected
        # to have stripped the dup before calling this.
        pass
    total = raw[-1] if raw[-1] > 0 else 1.0
    return [r / total for r in raw]


# ---------------------------------------------------------------------------
# DP matcher (the algorithm under evaluation)
# ---------------------------------------------------------------------------


@dataclass
class DPWeights:
    wd: float = 1.0     # distance² (normalized by dist²)
    wu: float = 0.2     # arclength progress
    wt: float = 0.1     # tangent alignment
    wn: float = 0.5     # normal direction (parallel-offset prior)
    gap: float = 0.05   # one-to-many / many-to-one penalty
    eps: float = 1e-9   # normalization epsilon


@dataclass
class DPResult:
    pairs: List[Tuple[int, int]] = field(default_factory=list)
    dropped_base_ranges: List[Tuple[int, int]] = field(default_factory=list)
    total_cost: float = 0.0


def dp_match(
    base: List[XY],
    offset: List[XY],
    closed: bool,
    dist_thickness: float,
    weights: DPWeights | None = None,
) -> DPResult:
    """Constrained monotone matcher via min-sum DP.

    Allowed transitions: (i-1, j-1) [diag], (i-1, j) [b stays],
    (i, j-1) [a stays]. Cost = wd·|a-b|² / dist² + wu·(u-v)²
                                + wt·(1 - |t_A·t_B|) + wn·(r·t_A)²/(|r|²+ε).

    For closed inputs: seed at j_seed = argmin_m |a_0 - b_m|, then rotate
    the offset sequence so off[0] == b[j_seed]. Linear DP follows.
    """
    if weights is None:
        weights = DPWeights()

    A = list(base)
    B = list(offset)

    # Defensive: drop trailing duplicate on closed polylines.
    if closed and len(A) >= 2 and A[0] == A[-1]:
        A = A[:-1]
    if closed and len(B) >= 2 and B[0] == B[-1]:
        B = B[:-1]

    N, M = len(A), len(B)
    if N < 2 or M < 2:
        return DPResult()

    # Seed + rotate for closed inputs.
    if closed:
        j_seed = min(range(M), key=lambda m: dist2(A[0], B[m]))
        B = B[j_seed:] + B[:j_seed]
    else:
        j_seed = 0

    TA = tangents(A, closed)
    TB = tangents(B, closed)
    uA = cumulative_arclength(A, closed)
    uB = cumulative_arclength(B, closed)

    dist2_thick = max(dist_thickness * dist_thickness, weights.eps)

    INF = float("inf")
    D: List[List[float]] = [[INF] * M for _ in range(N)]
    P: List[List[Optional[Tuple[int, int]]]] = [[None] * M for _ in range(N)]

    def local_cost(i: int, j: int) -> float:
        d2 = dist2(A[i], B[j]) / dist2_thick
        prog = (uA[i] - uB[j]) ** 2
        cos_ang = TA[i][0] * TB[j][0] + TA[i][1] * TB[j][1]
        tang = 1.0 - abs(cos_ang)
        rx, ry = B[j][0] - A[i][0], B[j][1] - A[i][1]
        r2 = rx * rx + ry * ry
        r_dot_t = rx * TA[i][0] + ry * TA[i][1]
        norm_pen = (r_dot_t * r_dot_t) / (r2 + weights.eps)
        return (weights.wd * d2 + weights.wu * prog
                + weights.wt * tang + weights.wn * norm_pen)

    D[0][0] = local_cost(0, 0)
    for j in range(1, M):
        D[0][j] = D[0][j - 1] + local_cost(0, j) + weights.gap
        P[0][j] = (0, j - 1)
    for i in range(1, N):
        D[i][0] = D[i - 1][0] + local_cost(i, 0) + weights.gap
        P[i][0] = (i - 1, 0)

    for i in range(1, N):
        for j in range(1, M):
            c = local_cost(i, j)
            cands = [
                (D[i - 1][j - 1] + c,           (i - 1, j - 1)),
                (D[i - 1][j]     + c + weights.gap, (i - 1, j)),
                (D[i][j - 1]     + c + weights.gap, (i, j - 1)),
            ]
            best = min(cands, key=lambda x: x[0])
            D[i][j], P[i][j] = best

    # Backtrack.
    path: List[Tuple[int, int]] = []
    i, j = N - 1, M - 1
    while True:
        path.append((i, j))
        if (i, j) == (0, 0):
            break
        prev = P[i][j]
        if prev is None:
            break
        i, j = prev
    path.reverse()

    # Renumber offset indices back to original (unrotated) frame.
    pairs = [(bi, (oj + j_seed) % M if closed else oj) for (bi, oj) in path]

    # Dropped-range extraction: any base vertex whose paired offset
    # vertex sits beyond `1.5·dist` away has no real offset
    # correspondence — the Clipper2 inset effectively skipped that
    # region. This catches both "vertical-step stuck on same offset"
    # (a[i+1] inherits b[j] because no new offset vertex exists nearby)
    # and "borderline diagonal" where DP advanced but the distance is
    # still well above the offset shell.
    radius = 1.5 * dist_thickness
    dropped_indices: List[int] = []
    for (bi, oj) in path:
        d = dist(A[bi], B[oj])
        if d > radius:
            dropped_indices.append(bi)

    # Collapse consecutive indices into ranges.
    ranges: List[Tuple[int, int]] = []
    for idx in dropped_indices:
        if ranges and ranges[-1][1] == idx - 1:
            lo, hi = ranges[-1]
            ranges[-1] = (lo, idx)
        else:
            ranges.append((idx, idx))

    return DPResult(pairs=pairs, dropped_base_ranges=ranges,
                    total_cost=D[N - 1][M - 1])


# ---------------------------------------------------------------------------
# Reference: simplified planReverseMatchByNearest (Python port)
# ---------------------------------------------------------------------------


def nearest_match(
    base: List[XY],
    offset: List[XY],
    closed: bool,
    dist_thickness: float,
) -> DPResult:
    """Mirror of planReverseMatchByNearest's forward-walk pairing rule.

    Used as an A/B reference. Greedy local: for each base[i], walk j
    forward from the previous pairing until distance starts increasing.
    Then fill skipped offsets with whichever of {i-1, i} is closer.
    """
    A = list(base)
    B = list(offset)
    if closed and len(A) >= 2 and A[0] == A[-1]:
        A = A[:-1]
    if closed and len(B) >= 2 and B[0] == B[-1]:
        B = B[:-1]
    N, M = len(A), len(B)
    if N < 2 or M < 2:
        return DPResult()

    if closed:
        j_seed = min(range(M), key=lambda m: dist2(A[0], B[m]))
        B = B[j_seed:] + B[:j_seed]
    else:
        j_seed = 0

    # Forward pass.
    pair_for_base = [0] * N
    pair_for_base[0] = 0
    for i in range(1, N):
        j_last = pair_for_base[i - 1]
        j_min = j_last
        d_min = dist2(A[i], B[j_last % M])
        j_limit = (j_last + M) if closed else (M - 1)
        j = j_last + 1
        while j <= j_limit:
            jm = j % M if closed else j
            d = dist2(A[i], B[jm])
            if d < d_min:
                d_min = d
                j_min = j
            elif d > d_min:
                break
            j += 1
        pair_for_base[i] = j_min

    # Reverse pass: gaps between consecutive forward-pass slots.
    walked = [-1] * M
    for i in range(N):
        jm = pair_for_base[i] % M
        walked[jm] = i
    for i in range(1, N):
        j_prev = pair_for_base[i - 1]
        j_curr = pair_for_base[i]
        if j_curr <= j_prev + 1:
            continue
        switched = False
        for j in range(j_prev + 1, j_curr):
            jm = j % M if closed else j
            if walked[jm] >= 0:
                continue
            d_prev = dist2(A[i - 1], B[jm])
            d_curr = dist2(A[i], B[jm])
            if not switched and d_prev <= d_curr:
                walked[jm] = i - 1
            else:
                walked[jm] = i
                switched = True

    # Tail fill.
    if closed:
        for m in range(M):
            if walked[m] < 0:
                walked[m] = N - 1
    else:
        j_tail = pair_for_base[N - 1]
        for m in range(j_tail + 1, M):
            if walked[m] < 0:
                walked[m] = N - 1
    for m in range(M):
        if walked[m] < 0:
            walked[m] = 0

    # Monotonize.
    for m in range(1, M):
        if walked[m] < walked[m - 1]:
            walked[m] = walked[m - 1]

    # Build pairs: emit (walked[k], k) for k in 0..M-1.
    pairs_rot: List[Tuple[int, int]] = []
    pairs_rot.append((walked[0], 0))
    for k in range(1, M):
        prev_b = walked[k - 1]
        cur_b = walked[k]
        if cur_b == prev_b + 1:
            pairs_rot.append((cur_b, k))
        elif cur_b == prev_b:
            pairs_rot.append((cur_b, k))
        else:
            for b in range(prev_b + 1, cur_b):
                pairs_rot.append((b, k - 1))
            pairs_rot.append((cur_b, k))

    # Renumber offset back to unrotated frame.
    pairs = [(bi, (oj + j_seed) % M if closed else oj) for (bi, oj) in pairs_rot]
    return DPResult(pairs=pairs)


# ---------------------------------------------------------------------------
# SVG visualization
# ---------------------------------------------------------------------------


def write_compare_svg(
    out_path: str,
    title: str,
    base: List[XY],
    offset: List[XY],
    closed: bool,
    prod_pairs: List[Tuple[int, int]],
    dp_result: DPResult,
    nearest_result: DPResult,
) -> None:
    # Defensive strip closed trailing dup for matching arrays.
    A = list(base)
    B = list(offset)
    if closed and len(A) >= 2 and A[0] == A[-1]:
        A = A[:-1]
    if closed and len(B) >= 2 and B[0] == B[-1]:
        B = B[:-1]

    all_pts = A + B
    xs = [p[0] for p in all_pts]
    ys = [p[1] for p in all_pts]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    dx = max(max_x - min_x, 1e-12)
    dy = max(max_y - min_y, 1e-12)

    W, H, margin = 1800.0, 700.0, 40.0
    # Three panels side by side: production, DP, nearest.
    panel_w = (W - 4 * margin) / 3.0
    panel_h = H - 2 * margin
    s = min(panel_w / dx, panel_h / dy)

    def xform(panel_idx: int, p: XY) -> Tuple[float, float]:
        px = margin + panel_idx * (panel_w + margin)
        py = margin
        tx = px - s * min_x + 0.5 * (panel_w - s * dx)
        ty = py + s * max_y + 0.5 * (panel_h - s * dy)
        return (s * p[0] + tx, -s * p[1] + ty)

    parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
        f'width="{W}" height="{H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="20" y="22" font-family="sans-serif" font-size="16" '
        f'fill="#222">DP matcher prototype: {title}</text>',
    ]

    panel_labels = ["production (planReverseMatch*)",
                    "DP (this prototype)",
                    "nearest (Python port reference)"]
    panel_pair_sets = [prod_pairs, dp_result.pairs, nearest_result.pairs]
    panel_dropped = [None, dp_result.dropped_base_ranges, None]

    for idx in range(3):
        px = margin + idx * (panel_w + margin)
        parts.append(
            f'<text x="{px}" y="{margin - 5}" font-family="sans-serif" '
            f'font-size="13" fill="#444">{panel_labels[idx]}</text>')

        # Mapping lines.
        parts.append('<g stroke="#888" stroke-width="0.7" opacity="0.7">')
        for (bi, oj) in panel_pair_sets[idx]:
            if bi < 0 or oj < 0:
                continue
            if bi >= len(A) or oj >= len(B):
                continue
            x1, y1 = xform(idx, A[bi])
            x2, y2 = xform(idx, B[oj])
            parts.append(
                f'<line x1="{x1:.2f}" y1="{y1:.2f}" '
                f'x2="{x2:.2f}" y2="{y2:.2f}" />')
        parts.append('</g>')

        # Base polyline (blue) + vertex markers.
        pts_str = " ".join(f"{xform(idx, p)[0]:.2f},{xform(idx, p)[1]:.2f}"
                           for p in A)
        parts.append(
            f'<polyline fill="none" stroke="#1f77b4" stroke-width="1.4" '
            f'points="{pts_str}"/>')
        # Offset polyline (orange).
        pts_str = " ".join(f"{xform(idx, p)[0]:.2f},{xform(idx, p)[1]:.2f}"
                           for p in B)
        parts.append(
            f'<polyline fill="none" stroke="#ff7f0e" stroke-width="1.4" '
            f'points="{pts_str}"/>')

        # Markers.
        for k, p in enumerate(A):
            cx, cy = xform(idx, p)
            parts.append(
                f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="2.5" '
                f'fill="#1f77b4" stroke="#0b3d61" stroke-width="0.5"/>')
            if idx == 0:  # labels only on first panel to reduce clutter
                parts.append(
                    f'<text x="{cx+3:.2f}" y="{cy-3:.2f}" font-size="8" '
                    f'fill="#0b3d61">b{k}</text>')
        for k, p in enumerate(B):
            cx, cy = xform(idx, p)
            parts.append(
                f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="2.5" '
                f'fill="#ff7f0e" stroke="#7a3d05" stroke-width="0.5"/>')

        # Dropped ranges (DP panel only) — highlight in red.
        if panel_dropped[idx]:
            for (lo, hi) in panel_dropped[idx]:
                for bi in range(lo, hi + 1):
                    if bi < len(A):
                        cx, cy = xform(idx, A[bi])
                        parts.append(
                            f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="5" '
                            f'fill="none" stroke="#d62728" stroke-width="1.5"/>')

    parts.append('</svg>')

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(parts))


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def process(json_path: str, weights: DPWeights | None = None) -> dict:
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    title = data["title"]
    closed = bool(data["closed"])
    dist_thickness = float(data["dist"])
    base = [tuple(p) for p in data["base"]]
    offset = [tuple(p) for p in data["offset"]]
    prod_pairs = [tuple(p) for p in data["pairs"]]

    dp_res = dp_match(base, offset, closed, dist_thickness, weights)
    nrst_res = nearest_match(base, offset, closed, dist_thickness)

    out_path = json_path.replace(".base_offset_map.json", ".dp_compare.svg")
    write_compare_svg(out_path, title, base, offset, closed,
                      prod_pairs, dp_res, nrst_res)

    return {
        "json": os.path.basename(json_path),
        "title": title,
        "closed": closed,
        "dist": dist_thickness,
        "N_base": len(base) - (1 if closed and len(base) > 1
                               and base[0] == base[-1] else 0),
        "N_offset": len(offset) - (1 if closed and len(offset) > 1
                                   and offset[0] == offset[-1] else 0),
        "prod_pairs": len(prod_pairs),
        "dp_pairs": len(dp_res.pairs),
        "nrst_pairs": len(nrst_res.pairs),
        "dp_dropped_ranges": dp_res.dropped_base_ranges,
        "prod_dropped_lo": data.get("dropped_base_ranges_lo", []),
        "prod_dropped_hi": data.get("dropped_base_ranges_hi", []),
        "dp_total_cost": dp_res.total_cost,
        "svg": os.path.basename(out_path),
    }


DEFAULTS = [
    "test/integration/t9_airfoil/mh104_te_only.sg_te.base_offset_map.json",
    "test/integration/t9_airfoil/mh104.sg_te.base_offset_map.json",
    "test/integration/t9_airfoil/mh104.sg_le.base_offset_map.json",
    "test/integration/t9_airfoil/mh104.sg_hps_2.base_offset_map.json",
    "test/integration/t1_strip/strip.sgm_1.base_offset_map.json",
]


def main(argv: List[str]) -> int:
    if len(argv) > 1:
        paths = argv[1:]
    else:
        # Resolve defaults relative to repo root (script lives in scripts/).
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        paths = [os.path.join(repo_root, p) for p in DEFAULTS]

    results = []
    for p in paths:
        if not os.path.exists(p):
            print(f"SKIP (missing): {p}")
            continue
        try:
            r = process(p)
            results.append(r)
        except Exception as e:
            print(f"FAIL {p}: {e}")
            continue

    print("\n=== Summary ===")
    print(f"{'case':40} {'closed':6} {'N':4} {'M':4} {'prod_p':6} "
          f"{'dp_p':5} {'nrst_p':6} {'dp_drop':10}")
    for r in results:
        case = f"{r['json'][:35]:35}"
        closed = "c" if r['closed'] else "o"
        drop = ",".join(f"[{lo},{hi}]" for (lo, hi) in r['dp_dropped_ranges'])
        prod_drop = (",".join(f"[{lo},{hi}]"
                              for lo, hi
                              in zip(r['prod_dropped_lo'], r['prod_dropped_hi']))
                     if r['prod_dropped_lo'] else "-")
        print(f"{case:35} {closed:6} {r['N_base']:4d} {r['N_offset']:4d} "
              f"{r['prod_pairs']:6d} {r['dp_pairs']:5d} {r['nrst_pairs']:6d} "
              f"dp:{drop or '-'} prod:{prod_drop}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
