# Always prefer setuptools over distutils
from setuptools import setup, find_packages
import pathlib

here = pathlib.Path(__file__).parent.resolve()

# Get the long description from the README file
long_description = (here / 'README.md').read_text(encoding='utf-8')

# Arguments marked as "Required" below must be included for upload to PyPI.
# Fields marked as "Optional" may be commented out.

setup(
    name='asgardcli',  # Required
    packages=['asgardcli',
              'asgardcli.configuration',
              'asgardcli.evaluation',
              'asgardcli.generation',
              'asgardcli.verification',
              ],  # Required
    version='0.1.0',  # Required
    description='Asgard Kernel Module controller',  # Optional
    long_description=long_description,  # Optional
    long_description_content_type='text/markdown',  # Optional (see note above)
    url='https://github.com/Distributed-Systems-Programming-Group/asgard-lkm',  # Optional
    author='Vincent Riesop, Patrick Jahnke',  # Optional
    author_email='vincent.riesop@gmail.com',  # Optional

    python_requires='>=3.6, <4',

    install_requires=[
        'click',
        'redis',
        'scipy',
    ],

    entry_points={  # Optional
        'console_scripts': [
            'acli=asgardcli.acli:entry_point',
        ],
    },
    classifiers=[  # Optional
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3 :: Only',
    ],

)