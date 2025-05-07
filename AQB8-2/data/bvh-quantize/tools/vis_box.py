import pyvista as pv
import numpy as np
import sys

if len(sys.argv) != 2:
    print(f'usage: python {sys.argv[0]} MODEL_FILE')
    exit(1)

ply_mesh = pv.read(sys.argv[1])
p = pv.Plotter()

node_offsets = np.fromfile('node_offsets.bin', np.uint32)
full_bboxes = np.fromfile('full_bboxes.bin', np.float32).reshape(-1, 6)
quant_bboxes = np.fromfile('quant_bboxes.bin', np.float32).reshape(-1, 6)
trigs = np.fromfile('trigs.bin', np.float32).reshape(-1, 3, 3)
trigs_size = np.fromfile('trigs_size.bin', np.uint32)

cluster_idx = 0
trigs_idx = 0
quant_actor = None
def click_callback(pos):
    global cluster_idx
    global trigs_idx
    global quant_actor
    p.clear()
    p.add_mesh(ply_mesh, style='wireframe')
    full_box_mesh = pv.PolyData()
    quant_box_mesh = pv.PolyData()
    trig_mesh = None
    for i in range(node_offsets[cluster_idx], node_offsets[cluster_idx + 1]):
        full_box_mesh += pv.Box(full_bboxes[i])
        quant_box_mesh += pv.Box(quant_bboxes[i])
    for _ in range(trigs_size[cluster_idx]):
        if trig_mesh is None:
            trig_mesh = pv.PolyData()
        trig_mesh += pv.Triangle(trigs[trigs_idx])
        trigs_idx += 1
    p.add_mesh(full_box_mesh, color='red', style='wireframe')
    if trig_mesh is not None:
        p.add_mesh(trig_mesh, color='yellow', opacity=0.1)
    quant_actor = p.add_mesh(quant_box_mesh, color='green', style='wireframe')
    cluster_idx += 1
    p.add_text(f'{cluster_idx}/{len(node_offsets)}')

def keyboard_callback():
    quant_actor.visibility = not quant_actor.visibility
    p.update()

p.track_click_position(click_callback)
p.add_key_event('z', keyboard_callback)
p.add_mesh(ply_mesh, opacity=0.1)
p.add_axes_at_origin(labels_off=True)
p.show()