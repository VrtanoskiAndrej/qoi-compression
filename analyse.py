import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

HASH_SIZE = 64

def pixel_hash_distribution(pixel_grid, hash_function):
    dist = []
    rows = len(pixel_grid)
    cols = len(pixel_grid[0])

    for r in range(rows):
        for c in range(cols):
            dist.append(hash_function(pixel_grid[r][c]))

    plt.hist(dist, color='blue', edgecolor='black', bins=64)
    plt.show()


def mult_hash(pixel):
    return (pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % HASH_SIZE

def main():
    im = Image.open('dataset/imgbench/artificial.png')
    pix = np.array(im)
    print(pix[0])
    pixel_hash_distribution(pix, mult_hash)

if __name__ == "__main__":
    main()