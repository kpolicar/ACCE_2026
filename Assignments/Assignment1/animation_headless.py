import matplotlib
matplotlib.use('Agg')  # headless backend
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import sys
import os


def read_frames(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
    rows, cols = map(int, lines[0].split())
    num_matrices = int(lines[1].strip())
    data = [list(map(float, l.split())) for l in lines[2:] if l.strip()]
    data = np.array(data).flatten()
    if len(data) != num_matrices * rows * cols:
        print(f"Error: the file does not contain the correct number of data points for {num_matrices} matrices of {rows}x{cols}, it contains {len(data)}.")
        sys.exit()
    matrices = [data[i * rows * cols : (i + 1) * rows * cols].reshape(rows, cols) for i in range(num_matrices)]
    print(f"{num_matrices} matrices with dimensions {rows} x {cols} have been loaded")
    terrain = matrices[0]
    return rows, cols, terrain, matrices[1:]


def animate_matrices(rows, cols, terrain, matrices, filename):
    fig, ax = plt.subplots(figsize=(30, 30))
    ax.set_aspect('equal')
    img_terrain = ax.imshow(terrain, cmap="YlOrBr_r", interpolation="nearest", extent=[0, 30, 0, 30])
    cmap = plt.cm.Blues
    matrices = [np.where(m == 0, np.nan, m) for m in matrices]
    img_water = ax.imshow(matrices[0], cmap=cmap, interpolation='nearest', extent=[0,30,0,30], vmin=0, vmax=np.nanmax(matrices), alpha=0.7)
    ax.set_title("Water Evolution")
    cbar = plt.colorbar(img_water, ax=ax)
    cbar.set_label("Water Level")

    def update_frame(frame):
        img_water.set_array(matrices[frame])
        ax.set_title(f"Time: {frame + 1}")
        return [img_water]

    ani = animation.FuncAnimation(fig, update_frame, frames=len(matrices), interval=200, repeat=True)
    base, _ = os.path.splitext(filename)
    output_file = base + ".mp4"
    print("Saving file ...")
    ani.save(output_file, writer="ffmpeg", fps=5)
    print(f"Saved to {output_file}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Use: python animation_headless.py <file_name>")
        sys.exit(1)
    filename = sys.argv[1]
    rows, cols, terrain, frames = read_frames(filename)
    animate_matrices(rows, cols, terrain, frames, filename)
