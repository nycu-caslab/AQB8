import hashlib
import numpy as np
import pyvista as pv
import matplotlib.pyplot as plt
import ctypes
from matplotlib.colors import ListedColormap
from tqdm import tqdm

NUM_SAMPLES = 2048
NUM_COLORS = 20

print('creating grid...')
u = np.linspace(0, 2 * np.pi, NUM_SAMPLES)
v = np.linspace(0, np.pi, NUM_SAMPLES)
x = np.outer(np.cos(u), np.sin(v))
y = np.outer(np.sin(u), np.sin(v))
z = np.outer(np.ones(np.size(u)), np.cos(v))
grid = pv.StructuredGrid(x, y, z)
del u, v, x, y, z

print('extracting centers...')
centers = grid.cell_centers().points
assert(centers.shape[0] == grid.n_cells)
cx = np.array(centers[:, 0], dtype=np.single)
cy = np.array(centers[:, 1], dtype=np.single)
cz = np.array(centers[:, 2], dtype=np.single)

print('calculating inverse...')
sw = 0.0078125  # 2^(-7)
ix = np.floor(1.0 / (cx * sw)).view(np.uintc)
iy = np.floor(1.0 / (cy * sw)).view(np.uintc)
iz = np.floor(1.0 / (cz * sw)).view(np.uintc)

print('calculating color...')
ixp = ix.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))
iyp = iy.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))
izp = iz.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))
cp = (ctypes.c_uint * grid.n_cells)()
encode_and_hash = ctypes.CDLL('/tmp/encode_and_hash.so').encode_and_hash
encode_and_hash.argtypes = [
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_uint),
    ctypes.POINTER(ctypes.c_uint),
    ctypes.POINTER(ctypes.c_uint),
    ctypes.POINTER(ctypes.c_uint)
]
encode_and_hash(grid.n_cells, ixp, iyp, izp, cp)
grid.cell_data['color'] = cp
del cx, cy, cz, ix, iy, iz

print('plotting...')
p = pv.Plotter()
cmap1 = plt.get_cmap('tab20b')
cmap2 = plt.get_cmap('tab20c')
combined_cmap = ListedColormap(cmap1.colors + cmap2.colors)
p.add_mesh(grid, cmap=combined_cmap)
p.show_grid()
p.remove_scalar_bar()
p.show()
