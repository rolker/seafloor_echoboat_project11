"""Compose nav2 params from a shared base + a per-hull-model overlay (issue #3).

Pure helpers (no ROS imports) so they can be unit-tested directly. The launch
file (`nav2_bringup_launch.py`) adds this directory to ``sys.path`` and imports
``compose_merged_params``; the test suite imports ``deep_merge`` directly.
"""

import atexit
import os
import tempfile

import yaml


def deep_merge(base, overlay):
    """Recursively merge ``overlay`` into ``base``.

    Dicts merge key-by-key; scalars and lists (including the footprint string and
    velocity arrays) replace wholesale. Returns a new dict; inputs are not mutated.
    """
    out = dict(base)
    for key, value in overlay.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = deep_merge(out[key], value)
        else:
            out[key] = value
    return out


def merged_params(cfg_dir, model):
    """Return the merged base + ``nav2_params.<model>.yaml`` param tree (a dict)."""
    overlay_path = os.path.join(cfg_dir, f'nav2_params.{model}.yaml')
    if not os.path.exists(overlay_path):
        raise RuntimeError(
            f"Unknown echoboat model '{model}': {overlay_path} not found "
            f"(expected nav2_params.<model>.yaml in {cfg_dir})"
        )
    with open(os.path.join(cfg_dir, 'nav2_params.base.yaml')) as f:
        merged = yaml.safe_load(f)
    with open(overlay_path) as f:
        return deep_merge(merged, yaml.safe_load(f))


def compose_merged_params(cfg_dir, model):
    """Merge base + per-model overlay and write to a temp file; return its path."""
    merged = merged_params(cfg_dir, model)
    tmp = tempfile.NamedTemporaryFile(
        mode='w', prefix=f'nav2_params_{model}_', suffix='.yaml', delete=False
    )
    yaml.safe_dump(merged, tmp, default_flow_style=False, sort_keys=False)
    tmp.close()
    # The file must outlive composition (nav2 reads it at node startup, incl.
    # respawns), so it can't be auto-deleted; clean it up at launch-process exit
    # to avoid accumulating temps on long-lived field hosts.
    atexit.register(lambda p=tmp.name: os.path.exists(p) and os.unlink(p))
    return tmp.name
