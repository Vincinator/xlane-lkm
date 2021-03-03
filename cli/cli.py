#!/usr/bin/env python3
import click

from .generation import generate as gen
from .verification import verify as v
from .evaluation import evalRedis
from .configuration import configure
from .evaluation import asgard as asg
from .evaluation import orchestrate as orchestra

@click.group()
def entry_point():
    pass


entry_point.add_command(gen.generateData)
entry_point.add_command(v.verifyData)
#entry_point.add_command(evalRedis.evalRedis)
entry_point.add_command(configure.generateConfig)
entry_point.add_command(asg.evalAsgard)
entry_point.add_command(orchestra.orchestrate)


if __name__ == '__main__':
    entry_point()