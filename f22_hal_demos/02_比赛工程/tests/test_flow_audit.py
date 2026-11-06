"""Check trial button boundaries without aircraft hardware."""
import importlib.util
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))
spec = importlib.util.spec_from_file_location("audit_flow", root / "scripts/audit_flow_calibration.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
keys = [{"key": k, "event": i} for i, k in enumerate([2, 2, 2, 3, 3, 2, 3])]
pairs = list(m.marker_pairs(keys))
assert [(s["event"], e["event"], repeats) for s, e, repeats in pairs] == [(0, 3, 2), (5, 6, 0)]
assert not list(m.marker_pairs([{"key": 2}, {"key": 4}, {"key": 3}]))
print("PASS: first button edges retained; repeats ignored; F4 cancels pending trial (synthetic markers)")
