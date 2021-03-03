#!/usr/bin/env python3
import click

from cli.evaluation.asgard import evalAsgard
from cli.configuration.configure import generateConfig

@click.group()
def entry_point():
    """
    Asgard CLI Entrypoint for the main click group
    :return:
    """
    pass


#entry_point.add_command(gen.generateData)
#entry_point.add_command(v.verifyData)
#entry_point.add_command(evalRedis.evalRedis)
entry_point.add_command(generateConfig)
entry_point.add_command(evalAsgard)
#entry_point.add_command(orchestra.orchestrate)


if __name__ == '__main__':
    entry_point()