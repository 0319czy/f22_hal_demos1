"""
Self-contained STL viewer generator.

``write_viewer("prop.stl", "prop.html")`` embeds the mesh in a single HTML file
that can be opened in any browser - orbit, zoom, wireframe, auto-spin.  Nothing
is loaded from disk at view time, so the page can be emailed or shared as-is.
"""

from __future__ import annotations

import base64
import os

_HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
__THREE__
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  html, body { margin:0; height:100%; background:#14161a; color:#e8eaed;
               font:13px/1.5 ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif; }
  #app { position:fixed; inset:0; }
  canvas { display:block; touch-action:none; }
  .panel { position:absolute; top:16px; left:16px; background:rgba(24,27,32,.86);
           border:1px solid #2c3038; border-radius:10px; padding:12px 14px; min-width:180px;
           backdrop-filter:blur(8px); }
  .panel h1 { margin:0 0 8px; font-size:13px; font-weight:600; letter-spacing:.01em; }
  .stat { display:flex; justify-content:space-between; gap:18px; color:#9aa2ad; }
  .stat b { color:#e8eaed; font-weight:500; font-variant-numeric:tabular-nums; }
  .row { margin-top:10px; display:flex; gap:6px; flex-wrap:wrap; }
  button { background:#22262d; color:#cfd4db; border:1px solid #333941; border-radius:7px;
           padding:5px 10px; font:inherit; font-size:12px; cursor:pointer; }
  button:hover { background:#2b3038; color:#fff; }
  button.on { background:#3f6ae0; border-color:#3f6ae0; color:#fff; }
  .hint { position:absolute; bottom:14px; left:16px; color:#666d78; font-size:11px; }
</style>
</head>
<body>
<div id="app"></div>
<div class="panel">
  <h1>__TITLE__</h1>
  <div class="stat"><span>size</span><b id="dim">-</b></div>
  <div class="stat"><span>volume</span><b id="vol">-</b></div>
  <div class="stat"><span>triangles</span><b id="tri">-</b></div>
  <div class="row">
    <button id="bSpin" class="on">spin</button>
    <button id="bWire">wireframe</button>
    <button id="bFit">reset view</button>
  </div>
</div>
<div class="hint">drag = orbit &nbsp;·&nbsp; wheel = zoom &nbsp;·&nbsp; right-drag / shift-drag = pan</div>

<script>
const STL_B64 = "__STL__";

function b64ToBuffer(b64) {
  const bin = atob(b64), len = bin.length, bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
}

function parseBinarySTL(buf) {
  const dv = new DataView(buf);
  const n = dv.getUint32(80, true);
  const pos = new Float32Array(n * 9), nor = new Float32Array(n * 9);
  let o = 84;
  for (let i = 0; i < n; i++) {
    const nx = dv.getFloat32(o, true), ny = dv.getFloat32(o + 4, true), nz = dv.getFloat32(o + 8, true);
    o += 12;
    for (let v = 0; v < 3; v++) {
      const k = i * 9 + v * 3;
      pos[k]     = dv.getFloat32(o, true);
      pos[k + 1] = dv.getFloat32(o + 4, true);
      pos[k + 2] = dv.getFloat32(o + 8, true);
      nor[k] = nx; nor[k + 1] = ny; nor[k + 2] = nz;
      o += 12;
    }
    o += 2;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  return g;
}

function signedVolume(geo) {
  const p = geo.attributes.position.array; let v = 0;
  for (let i = 0; i < p.length; i += 9) {
    const ax=p[i],ay=p[i+1],az=p[i+2], bx=p[i+3],by=p[i+4],bz=p[i+5], cx=p[i+6],cy=p[i+7],cz=p[i+8];
    v += ax*(by*cz-bz*cy) - ay*(bx*cz-bz*cx) + az*(bx*cy-by*cx);
  }
  return Math.abs(v) / 6;
}

const app = document.getElementById('app');
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x14161a);

const geo = parseBinarySTL(b64ToBuffer(STL_B64));
geo.computeBoundingBox();
const bb = geo.boundingBox, size = new THREE.Vector3(), ctr = new THREE.Vector3();
bb.getSize(size); bb.getCenter(ctr);
geo.translate(-ctr.x, -ctr.y, -ctr.z);
const radius = size.length() / 2;

const mat = new THREE.MeshStandardMaterial({ color: 0xd7dbe0, metalness: 0.25, roughness: 0.45,
                                             flatShading: false, side: THREE.DoubleSide });
const mesh = new THREE.Mesh(geo, mat);
const model = new THREE.Group();
model.add(mesh);
scene.add(model);

const wireMat = new THREE.LineBasicMaterial({ color: 0x5c6ab0, transparent: true, opacity: 0.35 });
const wire = new THREE.LineSegments(new THREE.WireframeGeometry(geo), wireMat);
wire.visible = false;
model.add(wire);

const grid = new THREE.GridHelper(radius * 4, 24, 0x2a2f37, 0x21252c);
grid.position.y = -size.z / 2;
scene.add(grid);

scene.add(new THREE.AmbientLight(0xffffff, 0.45));
const key = new THREE.DirectionalLight(0xffffff, 0.85); key.position.set(1, 2, 1.5); scene.add(key);
const fill = new THREE.DirectionalLight(0x9fb6ff, 0.35); fill.position.set(-2, 1, -1); scene.add(fill);
const rim = new THREE.DirectionalLight(0xffffff, 0.3); rim.position.set(0, -2, -1); scene.add(rim);

// Z-up model -> Y-up view
model.rotation.x = -Math.PI / 2;

const camera = new THREE.PerspectiveCamera(38, 1, radius / 100, radius * 100);
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
app.appendChild(renderer.domElement);

let theta = 0.9, phi = 1.05, dist = radius * 3.1;
const target = new THREE.Vector3(0, 0, 0);
function place() {
  camera.position.set(
    target.x + dist * Math.sin(phi) * Math.cos(theta),
    target.y + dist * Math.cos(phi),
    target.z + dist * Math.sin(phi) * Math.sin(theta));
  camera.lookAt(target);
}
function resize() {
  const w = app.clientWidth, h = app.clientHeight;
  camera.aspect = w / h; camera.updateProjectionMatrix(); renderer.setSize(w, h);
}
addEventListener('resize', resize); resize(); place();

let drag = null;
renderer.domElement.addEventListener('pointerdown', e => {
  drag = { x: e.clientX, y: e.clientY, pan: e.button === 2 || e.shiftKey };
  renderer.domElement.setPointerCapture(e.pointerId);
});
renderer.domElement.addEventListener('pointerup', e => { drag = null; });
renderer.domElement.addEventListener('contextmenu', e => e.preventDefault());
renderer.domElement.addEventListener('pointermove', e => {
  if (!drag) return;
  const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
  drag.x = e.clientX; drag.y = e.clientY;
  if (drag.pan) {
    const right = new THREE.Vector3(), up = new THREE.Vector3();
    camera.matrixWorld.extractBasis(right, up, new THREE.Vector3());
    const k = dist * 0.0015;
    target.addScaledVector(right, -dx * k).addScaledVector(up, dy * k);
  } else {
    theta -= dx * 0.008;
    phi = Math.max(0.05, Math.min(Math.PI - 0.05, phi - dy * 0.008));
    spinning = false; bSpin.classList.remove('on');
  }
  place();
});
renderer.domElement.addEventListener('wheel', e => {
  e.preventDefault();
  dist = Math.max(radius * 0.4, Math.min(radius * 30, dist * Math.exp(e.deltaY * 0.0012)));
  place();
}, { passive: false });

let spinning = true;
const bSpin = document.getElementById('bSpin');
const bWire = document.getElementById('bWire');
bSpin.onclick = () => { spinning = !spinning; bSpin.classList.toggle('on', spinning); };
bWire.onclick = () => { wire.visible = !wire.visible; bWire.classList.toggle('on', wire.visible); };
document.getElementById('bFit').onclick = () => {
  theta = 0.9; phi = 1.05; dist = radius * 3.1; target.set(0, 0, 0); place();
};

document.getElementById('dim').textContent =
  `${size.x.toFixed(1)} x ${size.y.toFixed(1)} x ${size.z.toFixed(1)} mm`;
document.getElementById('vol').textContent = (signedVolume(geo) / 1000).toFixed(2) + ' cm3';
document.getElementById('tri').textContent =
  (geo.attributes.position.count / 3).toLocaleString();

(function loop() {
  requestAnimationFrame(loop);
  if (spinning) { theta += 0.0035; place(); }
  renderer.render(scene, camera);
})();
</script>
</body>
</html>
"""


CDN = '<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>'


def _three_tag() -> str:
    """Inline the bundled three.js if present, otherwise fall back to the CDN."""
    local = os.path.join(os.path.dirname(os.path.abspath(__file__)), "three.min.js")
    if os.path.exists(local):
        with open(local, encoding="utf-8") as fh:
            return "<script>" + fh.read() + "</script>"
    return CDN


def write_viewer(stl_path: str, html_path: str | None = None, title: str | None = None) -> str:
    """Embed ``stl_path`` in a standalone HTML viewer and return the html path."""
    html_path = html_path or os.path.splitext(stl_path)[0] + ".html"
    title = title or os.path.basename(stl_path)
    with open(stl_path, "rb") as fh:
        raw = fh.read()
    if raw[:5].lower() == b"solid" and b"facet" in raw[:512]:
        raise ValueError("Viewer expects a binary STL (save_stl writes one).")
    b64 = base64.b64encode(raw).decode("ascii")
    html = (
        _HTML.replace("__TITLE__", title)
        .replace("__THREE__", _three_tag())
        .replace("__STL__", b64)
    )
    with open(html_path, "w", encoding="utf-8") as fh:
        fh.write(html)
    return html_path


if __name__ == "__main__":
    import sys

    print(write_viewer(*sys.argv[1:]))
