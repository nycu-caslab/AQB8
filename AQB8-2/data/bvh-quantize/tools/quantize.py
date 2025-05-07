import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
import sys
from tqdm import tqdm

if len(sys.argv) != 4:
    print('usage: python quantize.py DELTA FP_BINS INT_BINS')
    exit(1)

delta = float(sys.argv[1])
fp_bins = int(sys.argv[2])
int_bins = int(sys.argv[3])

w = np.random.uniform(size=100000)
iw = 1. / w
sns.ecdfplot(iw)

# PDF: y = 1 / x^2
x = np.arange(1, 1000, 0.01)
y = 1. - 1. / x
plt.plot(x, y)

plt.xlim(0, 1000)
plt.show()

xs = np.zeros(fp_bins)
pmf = np.zeros(fp_bins)
for b in range(fp_bins):
    left = 1 + b * delta
    right = left + delta
    xs[b] = (left + right) / 2
    pmf[b] = 1. / left - 1. / right
print(f'before normalization {pmf.sum()}')
#pmf /= pmf.sum()
pmf[-1] += 1. - pmf.sum()
print(f'after normalization {pmf.sum()}')

plt.plot(xs, pmf, 'x')
plt.show()

''' these two are similar
plt.plot(xs, pmf, 'x')
ys = delta / (xs * xs)
plt.plot(xs, ys, 'x')
plt.show()
'''

seps = []
squared_errors = []
sep = 1
for sep in tqdm(range(1, int(fp_bins / int_bins))):
    squared_error = 0.
    for i in range(int_bins):
        if i == int_bins - 1:
            quant_x = (xs[i * sep] + xs[-1]) / 2
            for j in range(i * sep, fp_bins):
                squared_error += pmf[j] * ((xs[j] - quant_x) ** 2)
        else:
            quant_x = (xs[i * sep] + xs[i * sep + sep - 1]) / 2
            for j in range(sep):
                squared_error += pmf[i * sep + j] * ((xs[i * sep + j] - quant_x) ** 2)
    seps.append(sep)
    squared_errors.append(squared_error)
    sep += 1

plt.plot(seps, squared_errors, 'x')
plt.show()