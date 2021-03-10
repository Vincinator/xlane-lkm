#!/usr/bin/env python3
import click

from asgardcli.configuration.configure import generate_bench_config, configure_kernel_module
from asgardcli.evaluation.asgard import eval_kernel_module_no_userspace
from asgardcli.evaluation.evalRedis import eval_asgard_with_redis
from asgardcli.generation.generate import generate_test_data
from asgardcli.verification.verify import verify_test_data


@click.group()
def entry_point():
    """
    Asgard CLI Entrypoint for the main click group 2
    :return:
    """
    pass


entry_point.add_command(generate_test_data)
entry_point.add_command(verify_test_data)
entry_point.add_command(eval_asgard_with_redis)
entry_point.add_command(generate_bench_config)
entry_point.add_command(configure_kernel_module)
entry_point.add_command(eval_kernel_module_no_userspace)
#entry_point.add_command(orchestra.orchestrate)


if __name__ == '__main__':
    entry_point()