
import redis
import click
import scipy.stats as stats
import scipy
import sys
import random
import binascii
import numpy as np
import json
import base64
import os.path
from os import path



def read_from_file(filepath):

    with open(filepath) as f:
        data = json.loads(f.read())

    return data




def _verify_data(min, max, data):

    errors = 0
    i = 0
    for entry in data:
        if entry['size'] > max:
            errors += 1
            print("Found entry at index {0} with size {2} greater than max {1}".format(i, max, entry['size']))
        if entry['size'] < min:
            errors += 1
            print("Found entry at index {0} with size {2} less than min {1}".format(i, min, entry['size']))

        i += 1

    if errors == 0:
        return True

    return False

@click.command()
@click.option('--min', default=1, help="Minimum valid value for each entry")
@click.option('--max', default=500, help="Maximum valid value for each entry")
@click.option('--input', default='generated_data.json', help="Json file to validate")
def verify_test_data(min, max, input):

    if os.path.exists(input) != True:
        print("File {0} does not exist".format(input))
        return False

    asgardBenchData = read_from_file(input)

    return _verify_data(min, max, asgardBenchData)

    # plot_hist_data(sizes)

