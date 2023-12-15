#!/usr/bin/env python3

import os
import re
import json
import subprocess
import tabulate
import optparse
from PIL import Image

CLOCKS_PER_SEC = 1000000
dataset_dir = 'dataset'
qoi_tmp_file = 'tmp.qoi'
png_tmp_file = 'out.png'
stats_output = []

header = ['impl', 'encode ms', 'decode ms', 'size bytes']
detailed_header = ['test', 'impl', 'encode ms',
                   'decode ms', 'pixels', 'size bytes', 'bytes/pixel']

qoi_ops = ['QOI_OP_RUN', 'QOI_OP_INDEX', 'QOI_OP_DIFF',
           'QOI_OP_LUMA', 'QOI_OP_RGB', 'QOI_OP_RGBA']

# helper functions
# ---------------


def msec(clks):
    return (clks*1000)/CLOCKS_PER_SEC


def aggregate(a, b):
    return {impl: {stat: a[impl][stat] + b[impl][stat] for stat in a[impl]} for impl in ['png', 'qoi']}


def average(struct, ittr):
    return {impl: {stat: struct[impl][stat]/ittr for stat in struct[impl]} for impl in ['png', 'qoi']}


def get_size(file_path):
    output = subprocess.check_output(['ls', '-l', file_path]).decode()
    return int(re.findall(r'\d+', output)[1])


def pix_count(file_path):
    img = Image.open(file_path)
    dimension = [img.width, img.height]
    img.close()
    return dimension

def image_score(time, size, pixel_count):
    return (size / pixel_count) * time


# ---------------
def run_benchmark(file_path):
    output = subprocess.check_output(['./qoiconvert', file_path]).decode()
    values = [int(v) for v in re.findall(r'\d+', output)]
    stats_struct = {'png': {
        'encode': values[0], 'decode': values[1], 'size': get_size(png_tmp_file)},
        'qoi': {
        'encode': values[2], 'decode': values[3], 'size': get_size(qoi_tmp_file)}}
    return stats_struct


# ---------------
def aggregate_stats(struct):
    qoi_stats = [0, 0, 0]
    png_stats = [0, 0, 0]
    for test_name, test_stats in struct.items():
        png_stats[0] += msec(test_stats['png']['encode'])
        png_stats[1] += msec(test_stats['png']['decode'])
        png_stats[2] += test_stats['png']['size']
        qoi_stats[0] += msec(test_stats['qoi']['encode'])
        qoi_stats[1] += msec(test_stats['qoi']['decode'])
        qoi_stats[2] += test_stats['qoi']['size']
    return [png_stats, qoi_stats]


# ---------------
def op_freq(file_path):
    output = subprocess.check_output(['./qoiconvert', file_path]).decode()
    output = output.split('\n')
    op_dict = {op: 0 for op in qoi_ops}
    for op in output:
        if op in op_dict:
            op_dict[op] += 1
    return op_dict

# ---------------


def main():
    usage = 'usage: %prog [options] arg'
    parser = optparse.OptionParser()
    parser.add_option('-e', '--epochs', dest='epochs',
                      default=1, type=int,
                      help='number of iterations (%default)')
    parser.add_option('-a', '--all', dest='all',
                      default=False, action='store_true',
                      help='use all files in the dataset (%default)')
    parser.add_option('-q', '--frequency', dest='freq',
                      default=False, action='store_true',
                      help='analyse instruction frequency qoi (%default)')
    parser.add_option('-v', '--verbose', dest='verbose',
                      default=False, action='store_true',
                      help='print detailed analysis for each test (%default)')
    (options, args) = parser.parse_args()

    if (len(args) == 0):
        print('usage: %prog [options] arg')
        print(os.listdir(dataset_dir))
        exit()

    # if argument all is specified, use all run all benchmarks
    if (args[0] == 'all'):
        datasets = os.listdir(dataset_dir)
    else:
        datasets = args
    
    print(f"dataset {datasets}, epochs: {options.epochs}")
    
    # frequency analysis of qoi opcodes
    if (options.freq):
        op_list = []
        for dir in datasets:
            for test in os.listdir(os.path.join(dataset_dir, dir)):
                test_path = os.path.join(dataset_dir, dir, test)
                freq = op_freq(test_path)
                total = sum(freq.values())
                op_list.append([test] + [freq[op]/total for op in qoi_ops])
        print('\n--- frequency analysis ---')
        print(tabulate.tabulate(op_list, headers=["test"] + qoi_ops,
                                floatfmt='.2f', tablefmt='plain'))
        return

    # run benchmark
    for dir in datasets:
        aggregate_stats = {'png': {'encode': 0, 'decode': 0, 'size': 0},
                           'qoi': {'encode': 0, 'decode': 0, 'size': 0}}
        # iterate multiple epochs for average
        for e in range(0, options.epochs):
            for test in os.listdir(os.path.join(dataset_dir, dir)):
                test_path = os.path.join(dataset_dir, dir, test)
                # print('%d %s' % (e, test_path))
                run_stats = run_benchmark(test_path)
                aggregate_stats = aggregate(aggregate_stats, run_stats)
                run_stats['name'] = test
                run_stats['epoch'] = e
                run_stats['dimension'] = pix_count(test_path)
                run_stats['pixels'] = pix_count(
                    test_path)[0] * pix_count(test_path)[1]
                stats_output.append(run_stats)

        stats_struct = average(aggregate_stats, options.epochs)
        table_data = [['png', stats_struct['png']['encode'],  stats_struct['png']['decode'],
                       int(stats_struct['png']['size'])],
                      ['qoi', stats_struct['qoi']['encode'],  stats_struct['qoi']['decode'],
                       int(stats_struct['qoi']['size'])]]

        print('\n--- %s benchmark data ---' %(dir))
        print(tabulate.tabulate(table_data, headers=header,
                                floatfmt='.2f', tablefmt='plain'))

    # display per test statistics
    if options.verbose:
        for dir in datasets:
            table_rows = []
            for test in os.listdir(os.path.join(dataset_dir, dir)):
                samples = list(filter(lambda s: s['name'] == test, stats_output))
                stats_struct = {'png': {'encode': 0, 'decode': 0, 'size': 0},
                                'qoi': {'encode': 0, 'decode': 0, 'size': 0}}
                pixel_count = samples[0]['pixels']

                for ittr in samples:
                    stats_struct = aggregate(stats_struct, ittr)
                stats_struct = average(stats_struct, options.epochs)
                table_data = [[test, 'png', stats_struct['png']['encode'],  stats_struct['png']['decode'],
                            pixel_count, stats_struct['png']['size'], stats_struct['png']['size'] / pixel_count],
                            [test, 'qoi', stats_struct['qoi']['encode'],  stats_struct['qoi']['decode'],
                            pixel_count, stats_struct['qoi']['size'], stats_struct['qoi']['size'] / pixel_count]]
                table_rows += table_data
            print('\n--- per test statistic ---')
            print(tabulate.tabulate(table_rows, headers=detailed_header,
                                    floatfmt='.2f', tablefmt='plain'))

    with open('stats.json', 'w') as jsonfile:
        json_struct = json.dumps(stats_output, indent=4)
        jsonfile.write(json_struct)


if __name__ == '__main__':
    main()
