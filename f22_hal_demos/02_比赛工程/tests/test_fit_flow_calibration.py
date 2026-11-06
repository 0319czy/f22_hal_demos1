#!/usr/bin/env python3
"""Recover a known coupled optical/gyro model and reject invalid trial data."""
import importlib.util
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("fit_flow", root/"scripts/fit_flow_calibration.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
k = .0022
r = np.array([[.1, -.95], [1.05, -.07]])
trials = []
for name, kind, d, b in (
    ("forward", "translation", [.2,0], [.001,-.002]),
    ("right", "translation", [0,.2], [-.001,.001]),
    ("roll", "rotation", [0,0], [.06,0]),
    ("pitch", "rotation", [0,0], [0,.05]),
    ("holdout", "holdout", [-.2,0], [.002,-.001])):
    b=np.array(b); c=(np.array(d)+r@b)/k
    trials.append(dict(name=name,kind=kind,displacement_m=d,c=c,b=b,angles=np.zeros(3)))
result=m.fit(trials)
assert abs(result["radians_per_count"]-k)<1e-10
assert np.allclose(result["rotation"],r.flatten())
assert max(t["error_m"] for t in result["trials"])<1e-10
assert not result["calibration_accepted"]
assert all("gyro_z_integral_deg" in t and "yaw_change_deg" not in t for t in result["trials"])
base=dict(PWM=[0,0,0,0],output_locked=1,sat=0,id=73,inv=182,epoch=0,ms=100,
          imu=1,tof=1,flow=1,valid=1,mm=200,
          bad=4,pollerr=0,n=100,hy_mcount_milli=1000,hx_mcount_milli=2000,
          angle_urad=[0,0,0],hangle_umrad=[1000,2000,0])
end=dict(base,ms=400,n=110,hy_mcount_milli=4000,hx_mcount_milli=6000,
         hangle_umrad=[4000,5000,0])
c,b,_=m.interval(base,end)
assert np.allclose(c,[3,4]) and np.allclose(b,[.003,.003])
for change in (dict(bad=5),dict(epoch=1),dict(sat=1),dict(pollerr=1),dict(n=101),dict(valid=0),dict(n=200)):
    try:m.interval(base,dict(end,**change))
    except ValueError:pass
    else:raise AssertionError(change)
try:m.fit([t for t in trials if t["kind"]=="translation"])
except ValueError:pass
else:raise AssertionError("Missing rotations accepted")
print("Calibration fitter: shared scale/coupled rotation, height weighting units, holdout prediction and invalid intervals passed (synthetic model)")
