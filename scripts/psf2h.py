import gzip, struct, sys
d = gzip.open(sys.argv[1]).read()
magic, _, size, _, count, glyph, height, width = struct.unpack("<8I", d[:32])
assert magic == 0x864AB572 and count >= 256 and glyph == 16 and height == 16 and width == 8
data = d[size:size + 256 * 16]
print("static const unsigned char font8x16[256 * 16] = {")
for i in range(0, len(data), 16):
    print("\t" + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
print("};")
