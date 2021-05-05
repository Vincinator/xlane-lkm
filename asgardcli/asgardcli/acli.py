#!/usr/bin/env python3
import click

from configure import generate_bench_config, configure_kernel_module, unload_kernel_module

@click.group()
def entry_point():
    """
    Asgard CLI Entrypoint for the main click group 2
    :return:
    """
    pass


entry_point.add_command(generate_bench_config)
entry_point.add_command(configure_kernel_module)
entry_point.add_command(unload_kernel_module)


if __name__ == '__main__':
    entry_point()