from dataclasses import dataclass, field


@dataclass
class Pixel:
    px_bytes: list = field(init=False)

    def __post_init__(self):
        self.px_bytes = [0, 0, 0, 255]

    def set(self, value):
        self.px_bytes = value.copy()

    def __str__(self):
        r, g, b, a = self.px_bytes
        return f"R: {r} G: {g} B: {b} A: {a}"

    @property
    def red(self) -> int:
        return self.px_bytes[0]

    @property
    def green(self) -> int:
        return self.px_bytes[1]

    @property
    def blue(self) -> int:
        return self.px_bytes[2]

    @property
    def alpha(self) -> int:
        return self.px_bytes[3]

empty_pixel = [0, 0, 0, 255]
null_pixel = [0, 0, 0, 0]

def encode(img, width, height):
    total_size = height * width
    channels = 4
    pixel_data = img.reshape(-1, 4)
    hash_array = [null_pixel.copy() for _ in range(64)]

    run_array = []

    # encode pixels
    run = 0
    prev_px_value = empty_pixel.copy()
    px_value = empty_pixel.copy()
    for i, px in enumerate(pixel_data):
        prev_px_value = px_value.copy()
        px_value = px.copy()
        
        if px_value == prev_px_value:
            run += 1
            if run == 62 or (i + 1) >= total_size:
                writer.write(QOI_OP_RUN | (run - 1))
                run = 0
            continue

        if run:
            writer.write(QOI_OP_RUN | (run - 1))
            run = 0

        index_pos = px_value.hash
        if hash_array[index_pos] == px_value:
            writer.write(QOI_OP_INDEX | index_pos)
            continue
        hash_array[index_pos].update(px_value.bytes)

        if px_value.alpha != prev_px_value.alpha:
            writer.write(QOI_OP_RGBA)
            writer.write(px_value.red)
            writer.write(px_value.green)
            writer.write(px_value.blue)
            writer.write(px_value.alpha)
            continue

        vr = (384 + px_value.red - prev_px_value.red) % 256 - 128
        vg = (384 + px_value.green - prev_px_value.green) % 256 - 128
        vb = (384 + px_value.blue - prev_px_value.blue) % 256 - 128

        vg_r = (384 + vr - vg) % 256 - 128
        vg_b = (384 + vb - vg) % 256 - 128

        if all(-3 < x < 2 for x in (vr, vg, vb)):
            writer.write(QOI_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2))
            continue

        elif all(-9 < x < 8 for x in (vg_r, vg_b)) and -33 < vg < 32:
            writer.write(QOI_OP_LUMA | (vg + 32))
            writer.write((vg_r + 8) << 4 | (vg_b + 8))
            continue

        writer.write(QOI_OP_RGB)
        writer.write(px_value.red)
        writer.write(px_value.green)
        writer.write(px_value.blue)

    write_end(writer)
    return writer.output()