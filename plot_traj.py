import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import math

p = r"C:\Users\wxw\Desktop\200hz_0818.txt"
rows = []
with open(p, encoding="gb18030", errors="ignore") as f:
    for line in f:
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            float(parts[0])
        except ValueError:
            continue
        try:
            lat = float(parts[2])
            lon = float(parts[3])
        except ValueError:
            continue
        rows.append((lat, lon))

if not rows:
    print("No data")
    exit()

lat_min, lat_max = min(r[0] for r in rows), max(r[0] for r in rows)
lon_min, lon_max = min(r[1] for r in rows), max(r[1] for r in rows)
lon0 = lon_min
lat0 = (lat_min + lat_max) / 2.0
km = 111320.0
cos_lat = math.cos(math.radians(lat0))

x = [(r[1] - lon0) * km * cos_lat for r in rows]
y = [(r[0] - lat0) * km for r in rows]

step = 20
xs = x[::step]
ys = y[::step]

fig, ax = plt.subplots(figsize=(10, 8))
ax.plot(xs, ys, color="steelblue", linewidth=0.5, alpha=0.8)
ax.set_xlabel("East (m)")
ax.set_ylabel("North (m)")
ax.set_title("Trajectory 200hz (10Hz downsampled, N=%.1f m E=%.1f m)" % (max(y)-min(y), max(x)-min(x)))
ax.set_aspect("equal")
ax.grid(True, alpha=0.3)
plt.tight_layout()
out = r"C:\Users\wxw\Desktop\QT\TASoftware\forensen_combnitation\trajectory_0818.png"
fig.savefig(out, dpi=150)
plt.close(fig)
print("saved:", out)
print("total pts:", len(x), "sampled:", len(xs))
print("X range (m): %.2f ~ %.2f" % (min(x), max(x)))
print("Y range (m): %.2f ~ %.2f" % (min(y), max(y)))
print("Lat range (deg): %.6f ~ %.6f" % (lat_min, lat_max))
print("Lon range (deg): %.6f ~ %.6f" % (lon_min, lon_max))
