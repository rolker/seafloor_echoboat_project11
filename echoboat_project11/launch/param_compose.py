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


def _load_layer(path):
    """Load one YAML param layer as a mapping.

    An empty / comment-only file (``yaml.safe_load`` → ``None``) is treated as an
    empty no-op layer. A non-mapping top level (list/scalar) is a config error and
    raises a clear ``RuntimeError`` naming the file rather than a downstream
    ``AttributeError`` from ``deep_merge``.
    """
    with open(path) as f:
        data = yaml.safe_load(f)
    if data is None:
        return {}
    if not isinstance(data, dict):
        raise RuntimeError(f"param layer {path} must be a YAML mapping, got {type(data).__name__}")
    return data


def merged_params(cfg_dir, model, instance_params=None):
    """Return the merged param tree: base, then the per-model overlay, then an
    optional per-instance overlay (each later layer wins).

    Layering order (lowest → highest precedence):
      nav2_params.base.yaml  →  nav2_params.<model>.yaml  →  instance_params

    ``instance_params`` is an absolute path (e.g. a boat-instance package's
    overlay) or falsy to skip the third layer.
    """
    overlay_path = os.path.join(cfg_dir, f"nav2_params.{model}.yaml")
    if not os.path.exists(overlay_path):
        raise RuntimeError(
            f"Unknown echoboat model '{model}': {overlay_path} not found "
            f"(expected nav2_params.<model>.yaml in {cfg_dir})"
        )
    merged = _load_layer(os.path.join(cfg_dir, "nav2_params.base.yaml"))
    merged = deep_merge(merged, _load_layer(overlay_path))
    if instance_params:
        if not os.path.exists(instance_params):
            raise RuntimeError(f"instance_params file not found: {instance_params}")
        merged = deep_merge(merged, _load_layer(instance_params))
    return merged


def compose_merged_params(cfg_dir, model, instance_params=None):
    """Merge base + per-model (+ optional instance) overlays to a temp file path."""
    merged = merged_params(cfg_dir, model, instance_params)
    tmp = tempfile.NamedTemporaryFile(
        mode="w", prefix=f"nav2_params_{model}_", suffix=".yaml", delete=False
    )
    yaml.safe_dump(merged, tmp, default_flow_style=False, sort_keys=False)
    tmp.close()
    # The file must outlive composition (nav2 reads it at node startup, incl.
    # respawns), so it can't be auto-deleted; clean it up at launch-process exit
    # to avoid accumulating temps on long-lived field hosts.
    atexit.register(lambda p=tmp.name: os.path.exists(p) and os.unlink(p))
    return tmp.name
