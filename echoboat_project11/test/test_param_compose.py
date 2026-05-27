"""Tests for the nav2 base + per-hull-model param composition (issue #3).

Locks the composition invariants: the shared base carries no hull-model knobs,
the per-model overlays supply them, and the merge reconstructs a complete param
tree. merge(base, 240) reproduced the pre-split nav2_params.yaml byte-for-byte at
split time (git history preserves the original) — except the 240 footprint, which
was subsequently re-tuned to the real hull (#3 step 4), so that equality no longer
holds for the footprint alone. These tests guard the structure going forward.
"""

import ast
import os
import sys

import pytest
import yaml

_PKG = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_LAUNCH = os.path.join(_PKG, 'launch')
_CONFIG = os.path.join(_PKG, 'config')
sys.path.insert(0, _LAUNCH)

from param_compose import deep_merge, merged_params  # noqa: E402


def _lc(params):
    return params['local_costmap']['local_costmap']['ros__parameters']


def _gc(params):
    return params['global_costmap']['global_costmap']['ros__parameters']


# --- deep_merge semantics ---

def test_deep_merge_recurses_dicts():
    base = {'a': {'x': 1, 'y': 2}}
    overlay = {'a': {'y': 20, 'z': 30}}
    assert deep_merge(base, overlay) == {'a': {'x': 1, 'y': 20, 'z': 30}}


def test_deep_merge_replaces_scalars_and_lists():
    base = {'v': [1, 2, 3], 's': 'old'}
    overlay = {'v': [9], 's': 'new'}
    assert deep_merge(base, overlay) == {'v': [9], 's': 'new'}


def test_deep_merge_does_not_mutate_inputs():
    base = {'a': {'x': 1}}
    overlay = {'a': {'y': 2}}
    deep_merge(base, overlay)
    assert base == {'a': {'x': 1}}


# --- base carries no hull-model knobs ---

def test_base_omits_hull_knobs():
    with open(os.path.join(_CONFIG, 'nav2_params.base.yaml')) as f:
        base = yaml.safe_load(f)
    assert 'footprint' not in _lc(base)
    assert 'footprint' not in _gc(base)
    assert 'robot_radius' not in _gc(base)
    bs = base['behavior_server']['ros__parameters']
    assert 'max_rotational_vel' not in bs
    assert 'minimum_radius' not in bs['hover']
    assert 'max_velocity' not in base['velocity_smoother']['ros__parameters']
    assert 'minimum_turning_radius' not in base['planner_server']['ros__parameters']['GridBased']


# --- merged 240 carries today's values ---

def _footprint_extent(footprint_str):
    """Bounding-box (length_x, width_y) of a nav2 footprint polygon string."""
    pts = ast.literal_eval(footprint_str)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return max(xs) - min(xs), max(ys) - min(ys)


def test_merged_240_hull_values():
    p = merged_params(_CONFIG, '240')
    assert _gc(p)['robot_radius'] == 1.5
    assert 'footprint' in _lc(p) and 'footprint' in _gc(p)
    assert p['planner_server']['ros__parameters']['GridBased']['minimum_turning_radius'] == 1.5
    assert p['velocity_smoother']['ros__parameters']['max_velocity'] == [2.75, 0.0, 0.45]
    bs = p['behavior_server']['ros__parameters']
    assert bs['max_rotational_vel'] == 1.0
    assert bs['hover']['maximum_speed'] == 1.0
    # shared base params survive the merge
    assert p['controller_server']['ros__parameters']['controller_frequency'] == 5.0


# --- merged 160 is well-formed (placeholder values until step 3) ---

def test_240_footprint_matches_real_hull():
    """240 footprint was re-tuned to the real hull (#3 step 4): ~1.95 x 1.0 m
    from bizzyboat.urdf.xacro. Guards against a silent revert to the inherited
    Izzy 1.5 x 0.6 footprint (which would understate the deployed boat's size)."""
    p = merged_params(_CONFIG, '240')
    for footprint in (_lc(p)['footprint'], _gc(p)['footprint']):
        length, width = _footprint_extent(footprint)
        assert length >= 1.8, f'240 footprint too short ({length:.2f} m) — Izzy hull?'
        assert width >= 0.9, f'240 footprint too narrow ({width:.2f} m) — Izzy hull?'
    # local and global costmaps share one hull footprint
    assert _lc(p)['footprint'] == _gc(p)['footprint']


def test_merged_160_wellformed():
    p = merged_params(_CONFIG, '160')
    assert 'footprint' in _gc(p)
    assert 'robot_radius' in _gc(p)
    assert 'max_velocity' in p['velocity_smoother']['ros__parameters']


def test_unknown_model_raises():
    with pytest.raises(RuntimeError):
        merged_params(_CONFIG, '999')


# --- optional instance overlay (third layer) ---

def test_instance_overlay_overrides_model(tmp_path):
    overlay = tmp_path / 'instance.yaml'
    overlay.write_text(
        'global_costmap:\n'
        '  global_costmap:\n'
        '    ros__parameters:\n'
        '      robot_radius: 9.9\n'
    )
    p = merged_params(_CONFIG, '240', str(overlay))
    # instance layer wins over the model overlay's robot_radius
    assert _gc(p)['robot_radius'] == 9.9
    # untouched model/base values survive
    assert p['controller_server']['ros__parameters']['controller_frequency'] == 5.0


def test_no_instance_overlay_matches_two_layer():
    assert merged_params(_CONFIG, '240', None) == merged_params(_CONFIG, '240')


def test_missing_instance_overlay_raises():
    with pytest.raises(RuntimeError):
        merged_params(_CONFIG, '240', '/nonexistent/instance.yaml')


# --- base is rig-agnostic; sensor rig + reflex come from the instance overlay (#3 step 2) ---

def test_base_has_no_sensor_rig_or_reflex():
    with open(os.path.join(_CONFIG, 'nav2_params.base.yaml')) as f:
        base = yaml.safe_load(f)
    lc = _lc(base)
    assert lc['plugins'] == ['chart_layer', 'inflation_layer']
    assert not [k for k in lc if 'sea_surface' in k]
    cm = base['collision_monitor']['ros__parameters']
    # gating wiring stays; reflex zones/sources move to the instance overlay
    assert cm['cmd_vel_in_topic'] == 'cmd_vel_nav'
    assert cm['polygons'] == []
    assert cm['observation_sources'] == []
