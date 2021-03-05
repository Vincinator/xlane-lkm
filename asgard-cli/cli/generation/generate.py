import binascii
import json
import random
import struct

import click
import scipy
import scipy.stats as stats
import string
import random


def generate_truncated_norm_dist(entries, min, max):

    mu = max/2 + min
    sigma = max/5

    samples = scipy.stats.truncnorm.rvs(
        (min - mu) / sigma, (max - mu) / sigma, loc=mu, scale=sigma, size=entries)

    print("done generating truncated normal distribution of entry sizes")

    return samples.astype(int)



def _gen_data(byte_size, key):

    if byte_size <= 8:
        print("error, byte size must be larger than 8")
        return -1

    # first 4 bytes are used as key size, which is not random but given via the key parameter of this function
    value =''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(byte_size - 4))

    encoded_key = key
    encoded_value = value

    return {'key': encoded_key, 'value': encoded_value, 'size': int(byte_size)}



def generate_random_data(sizes_array):
    arr = []
    key = 0
    for x in sizes_array:
        arr.append(_gen_data(x,key))
        key = key + 1
    print("done generating random data for given sizes")
    return arr

def dump_to_file(data_array, filepath):

    with open(filepath, 'w') as fp:
        json.dump(data_array, fp)


@click.command()
@click.option('--entries', default=1000, help="Number of Entries to generate")
@click.option('--min', default=16, help="Minimum value for each randomly generated entry")
@click.option('--max', default=32, help="Maximum value for each randomly generated entry")
@click.option('--output', default='generated_data.json', help="Output file path. Output will be a json file.")
def generate_test_data(entries, min, max, output):

    sizes = generate_truncated_norm_dist(entries, min, max)
    data = generate_random_data(sizes)
    print(data)
    dump_to_file(data, output)

