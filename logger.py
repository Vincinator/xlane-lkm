#!/usr/bin/env python3

import logging


class TermColors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    ERROR = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


logger = logging.getLogger('logger')


def print_error(msg):
    print(TermColors.ERROR + "" + msg + TermColors.ENDC)
    logger.debug(msg)


def print_info(msg):
    print(TermColors.OKBLUE + "" + msg + TermColors.ENDC)
    logger.debug(msg)


def print_warning(msg):
    print(TermColors.WARNING + "" + msg + TermColors.ENDC)
    logger.debug(msg)


def print_success(msg):
    print(TermColors.OKGREEN + "" + msg + TermColors.ENDC)
    logger.debug(msg)


def setup_logger(name):
    global logger
    logger.setLevel(logging.DEBUG)
    fh = logging.FileHandler(name)
    fh.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    fh.setFormatter(formatter)
    logger.addHandler(fh)