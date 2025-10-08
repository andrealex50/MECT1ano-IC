import matplotlib.pyplot as plt

def load_hist(filename):
    values, counts = [], []
    with open(filename) as f:
        for line in f:
            v, c = line.strip().split()
            values.append(int(v))
            counts.append(int(c))
    return values, counts

# Load histograms
mid_values, mid_counts = load_hist("mid_hist.txt")
side_values, side_counts = load_hist("side_hist.txt")

# Plot MID
plt.figure(figsize=(10, 4))
plt.bar(mid_values, mid_counts, width=2, align="center")
plt.title("MID Channel Histogram")
plt.xlabel("Value")
plt.ylabel("Count")
plt.tight_layout()
plt.savefig("mid_hist.png", dpi=200)

# Plot SIDE
plt.figure(figsize=(10, 4))
plt.bar(side_values, side_counts, width=2, align="center")
plt.title("SIDE Channel Histogram")
plt.xlabel("Value")
plt.ylabel("Count")
plt.tight_layout()
plt.savefig("side_hist.png", dpi=200)

print("Plots saved as mid_hist.png and side_hist.png")
