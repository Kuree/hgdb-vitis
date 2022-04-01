from setuptools import setup
import os


current_directory = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(current_directory, 'README.rst')) as f:
    long_description = f.read()


setup(
    name='hgdb-vitis',
    version='0.0.1',
    author='Keyi Zhang',
    author_email='keyi@cs.stanford.edu',
    long_description=long_description,
    long_description_content_type='text/x-rst',
    url="https://github.com/Kuree/hgdb-vitis",
    install_requires=[
        "llvmlite",
    ],
    scripts=["hgdb-vitis"],
    python_requires=">=3.6"
)
