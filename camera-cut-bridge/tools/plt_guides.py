"""Convert a supported SkyCut outer frame to the client's corner-guide layout.

This handles the space-delimited IN/TB26/CT1/U/D dialect in the fixtures,
not general HPGL. Unsupported or ambiguous jobs are returned unchanged.
"""
import argparse
from pathlib import Path
import re

MOVE = re.compile(rb'([UD])(-?\d+),(-?\d+)')
FRAME = re.compile(rb'TB26,(\d+),(\d+)')
TOLERANCE = 40  # Plotter units; fixture corner compensation extends 13 units.
GUIDE_LENGTH = 400


def convert(data):
    tokens = data.split()
    if len(tokens) < 4 or tokens[0] != b'IN':
        return data
    frame = FRAME.fullmatch(tokens[1])
    if not frame or tokens[2] != b'CT1':
        return data
    width, height = map(int, frame.groups())
    if min(width, height) <= 2 * GUIDE_LENGTH:
        return data

    paths = []
    current = None
    ended = False
    for index, token in enumerate(tokens[3:], 3):
        if token == b'@':
            ended = True
            continue
        match = MOVE.fullmatch(token)
        if ended or not match:
            return data
        point = tuple(map(int, match.groups()[1:]))
        if match[1] == b'U':
            current = [index, index + 1, [point]]
            paths.append(current)
        elif current is None:
            return data
        else:
            current[1] = index + 1
            current[2].append(point)
    if not ended:
        return data

    def edge(point):
        x, y = point
        if not (-TOLERANCE <= x <= width + TOLERANCE and
                -TOLERANCE <= y <= height + TOLERANCE):
            return False
        return min(abs(x), abs(x - width), abs(y), abs(y - height)) <= TOLERANCE

    def is_frame(path):
        points = path[2]
        if len(points) < 5 or not all(map(edge, points)):
            return False
        if max(abs(a - b) for a, b in zip(points[0], points[-1])) > TOLERANCE:
            return False
        # Every segment must stay at the perimeter, including corner compensation.
        if not all(edge(((a[0]+b[0])/2, (a[1]+b[1])/2))
                   for a, b in zip(points, points[1:])):
            return False
        # A side must span almost its full length. Bounds alone admit crosses.
        sides = [[] for _ in range(4)]
        for a, b in zip(points, points[1:]):
            for i, x in enumerate((0, width)):
                if abs(a[0]-x) <= TOLERANCE and abs(b[0]-x) <= TOLERANCE:
                    sides[i].append(sorted((a[1], b[1])))
            for i, y in enumerate((0, height), 2):
                if abs(a[1]-y) <= TOLERANCE and abs(b[1]-y) <= TOLERANCE:
                    sides[i].append(sorted((a[0], b[0])))
        for intervals, length in zip(sides, (height, height, width, width)):
            reach = TOLERANCE
            for lo, hi in sorted(intervals):
                if lo > reach + TOLERANCE:
                    return False
                reach = max(reach, hi)
            if reach < length - TOLERANCE:
                return False
        return True

    candidates = [path for path in paths if is_frame(path)]
    if len(candidates) != 1:
        return data
    border = candidates[0]
    # The frame must enclose real artwork, with room for its guide marks.
    artwork = [point for path in paths if path is not border
               for point in path[2] if len(path[2]) > 1]
    if not any(TOLERANCE < x < width-TOLERANCE and
               TOLERANCE < y < height-TOLERANCE for x, y in artwork):
        return data
    if not all(0 <= x <= width and 0 <= y <= height for x, y in artwork):
        return data
    # Keep the short origin mark from the export, but reject other paths in
    # the corner-guide areas rather than risk putting marks across artwork.
    for path in paths:
        if path is border:
            continue
        for x, y in path[2]:
            if x == 0 and 0 <= y <= TOLERANCE:
                continue
            if (min(x, width-x) <= TOLERANCE and min(y, height-y) <= GUIDE_LENGTH) or (min(y, height-y) <= TOLERANCE and min(x, width-x) <= GUIDE_LENGTH):
                return data

    guides = (f'U0,640 D0,520 '
              f'U{width},{height-400} D{width},{height} D{width-400},{height} '
              f'U0,{height-400} D0,{height} D400,{height} '
              f'U0,400 D0,0 D400,0 '
              f'U{width},400 D{width},0 D{width-400},0').encode().split()
    start, end, _ = border
    output = tokens[:start] + guides + tokens[end:]
    del output[1:3]  # The guided example has no TB26/CT1 frame header.
    while len(output) > 1 and output[-2:] == [b'@', b'@']:
        output.pop()
    return b' ' + b' '.join(output) + b' '


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Choose a separate output path to preserve the original.')
    original = args.input.read_bytes()
    converted = convert(original)
    # Exclusive creation also protects an existing destination.
    with args.output.open('xb') as stream:
        stream.write(converted)
    print('Converted outer frame to guides.' if converted != original
          else 'No unambiguous supported frame found; output is unchanged.')


if __name__ == '__main__':
    main()
