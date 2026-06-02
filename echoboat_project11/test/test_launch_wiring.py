"""Tests for the cmd_vel chain wiring in navigation_launch.py (issue #36).

Locks the re-enabled velocity_smoother routing so it can't silently regress to
the #27 double-publisher bug. The intended chain is:

    controller / behaviors --cmd_vel_nav--> velocity_smoother
        --cmd_vel_smoothed--> collision_monitor --> piloting_mode/autonomous/cmd_vel

The Collision Monitor must stay the SOLE publisher on the helm topic
(piloting_mode/autonomous/cmd_vel). The #27 regression was the smoother
publishing its output straight to the helm topic, competing with the monitor.

These parse the launch file's AST (no ROS runtime needed) plus the shared base
params, matching the static style of test_param_compose.py.
"""

import ast
import os

import yaml

_PKG = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_LAUNCH_FILE = os.path.join(_PKG, 'launch', 'navigation_launch.py')
_CONFIG = os.path.join(_PKG, 'config')

_HELM_TOPIC = 'piloting_mode/autonomous/cmd_vel'
_NODE_CALLS = ('LifecycleNode', 'ComposableNode', 'Node')


def _launch_tree():
    with open(_LAUNCH_FILE) as f:
        return ast.parse(f.read())


def _str(node):
    """Return the string value of a Constant node, else None."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    return None


def _lifecycle_nodes(tree):
    """The string elements of the `lifecycle_nodes = [...]` list literal."""
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == 'lifecycle_nodes' for t in node.targets
        ):
            return [_str(el) for el in node.value.elts if _str(el) is not None]
    raise AssertionError('lifecycle_nodes assignment not found in launch file')


def _remap_tuples(expr):
    """All 2-string (from, to) remap pairs in any list literal within expr.

    Handles both `remappings + [('a', 'b')]` (BinOp) and a bare list literal.
    """
    pairs = []
    for sub in ast.walk(expr):
        if isinstance(sub, ast.List):
            for el in sub.elts:
                if isinstance(el, ast.Tuple) and len(el.elts) == 2:
                    a, b = _str(el.elts[0]), _str(el.elts[1])
                    if a is not None and b is not None:
                        pairs.append((a, b))
    return pairs


def _node_calls(tree):
    """List of (name, remap_pairs) for every node-constructing call in the launch."""
    out = []
    for node in ast.walk(tree):
        if (
            isinstance(node, ast.Call)
            and isinstance(node.func, ast.Name)
            and node.func.id in _NODE_CALLS
        ):
            name, remaps = None, []
            for kw in node.keywords:
                if kw.arg == 'name':
                    name = _str(kw.value)
                elif kw.arg == 'remappings':
                    remaps = _remap_tuples(kw.value)
            out.append((name, remaps))
    return out


# --- velocity_smoother is back in the active lifecycle ---

def test_velocity_smoother_in_lifecycle_nodes():
    assert 'velocity_smoother' in _lifecycle_nodes(_launch_tree())


def test_velocity_smoother_node_present_both_paths():
    """One non-composition LifecycleNode + one composition ComposableNode."""
    smoothers = [c for c in _node_calls(_launch_tree()) if c[0] == 'velocity_smoother']
    assert len(smoothers) == 2, (
        f'expected velocity_smoother in both launch paths, found {len(smoothers)}'
    )


def test_velocity_smoother_input_remapped_to_cmd_vel_nav():
    """Subscribes to the controller/behaviors output, in every path it appears."""
    smoothers = [c for c in _node_calls(_launch_tree()) if c[0] == 'velocity_smoother']
    assert smoothers, 'no velocity_smoother node found'
    for _, remaps in smoothers:
        assert ('cmd_vel', 'cmd_vel_nav') in remaps


def test_velocity_smoother_output_not_redirected():
    """Output stays at the Nav2 default cmd_vel_smoothed — not remapped anywhere
    (and certainly not to the helm topic). Redirecting cmd_vel_smoothed was the
    #27 double-publish bug."""
    for name, remaps in _node_calls(_launch_tree()):
        if name == 'velocity_smoother':
            froms = [src for src, _ in remaps]
            assert 'cmd_vel_smoothed' not in froms, (
                'velocity_smoother output is remapped — leave it at the default'
            )


# --- #27 guard: the monitor is the SOLE helm-topic publisher ---

def test_no_launch_node_remaps_to_helm_topic():
    """The helm topic must only be produced by the Collision Monitor's
    cmd_vel_out_topic param — never by a launch remap. A remap target equal to
    the helm topic means a second publisher (the #27 regression)."""
    for name, remaps in _node_calls(_launch_tree()):
        targets = [dst for _, dst in remaps]
        assert _HELM_TOPIC not in targets, (
            f'{name} remaps an output to the helm topic ({_HELM_TOPIC}) — '
            'reintroduces the #27 double-publisher'
        )


# --- chain consistency: monitor input == smoother output ---

def test_monitor_input_chains_to_smoother_output():
    with open(os.path.join(_CONFIG, 'nav2_params.base.yaml')) as f:
        base = yaml.safe_load(f)
    cm = base['collision_monitor']['ros__parameters']
    # smoother publishes the Nav2 default cmd_vel_smoothed; the monitor reads it
    assert cm['cmd_vel_in_topic'] == 'cmd_vel_smoothed'
    # and the monitor alone emits the helm topic
    assert cm['cmd_vel_out_topic'] == _HELM_TOPIC
