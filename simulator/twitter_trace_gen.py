import os
import numpy as np
import pandas as pd
outfile = 'synthetic_twemcache.csv'
header = ['timestamp',
          'key',
          'ksize',
          'vsize',
          'cid',
          'op',
          'ttl']
alpha = 2.64 # from https://github.com/twitter/cache-trace/blob/master/stat/2020Mar.md
total_clients = 5
#total_clients = num_clients * 1000
# preprocessing - approximately covers the key space. original (sampled) trace had ~9.2k unique keys 
vsizes = {k:np.random.randint(300, 400) for k in range(10000)} 
data = []
for c in range(total_clients):
    nrequests = np.random.randint(120, 207)
    keys = np.random.zipf(alpha, nrequests)
    print(keys)
    interarrivals = [np.random.exponential(0.5) for k in keys] 
    arrivals = np.cumsum(interarrivals)
    print(arrivals)
    for k, a in zip(keys, arrivals):
        data.append((a, k, vsizes[k], 32, c, 'get', 0)) # almost everything was a get... can sprinkle in some puts if needed
data = sorted(data)
with open(outfile, 'w') as f:
    for d in data:
        f.write('{:.03f},{:d},{:d},{:d},{:d},{:s},{:d}\n'.format(*d))
        f.flush()
