import pyvista as pv
import numpy as np
import sys

if len(sys.argv) != 5:
    print('usage: python vis_ray.py MODEL_FILE RAY_FILE MAX_T NUM_RAYS')
    exit(1)

p = pv.Plotter()
ply_mesh = pv.read(sys.argv[1])
p.add_mesh(ply_mesh, opacity=0.1)

rs = np.fromfile(sys.argv[2], dtype=np.float32).reshape(-1, 7)
max_t = float(sys.argv[3])
num_rays = int(argv[4])

for i in range(num_rays):
    ls = np.array([rs[i][0], rs[i][1], rs[i][2]])
    le = ls + min(max_t, rs[i][6]) * np.array([rs[i][3], rs[i][4], rs[i][5]])
    pt, _ = ply_mesh.ray_trace(ls, le, first_point=True)
    if pt.size > 0:
        ray = pv.Line(ls, pt)
        p.add_mesh(ray, color='r')

p.show()