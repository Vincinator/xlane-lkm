import configparser
import json
import click
import os.path

from utils import load_config



@click.command()
@click.option('--config_path', default='../node.ini', help="Path to the config file to read from")
@click.option('--benchmark', required=True, help="The benchmark to execute")
def exec_bench(config_path, benchmark):

    config = load_config(config_path)
    if config is None:
        print(f"Config File {config_path} does not exist. Generate it via generate-config command")
        return

    print(f"NOT IMPLEMENTED. Imagine a benchmark being executed now. Maybe you triggered it in a parallel universe?")

