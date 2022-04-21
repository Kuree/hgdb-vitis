Xilinx Vitis to HGDB converter
==============================

This tools allows designers to debug RTL generated from HLS with hgdb.
It works with pipeline/un-pipelined design.

How to build
------------

You need a C++17 compatible compiler as well as a recent llvm
development library. On Ubuntu, you can install llvm via

.. code::

   sudo apt install llvm-dev

You also need to clone the submodules via

.. code::

   git submodule update --init --recursive

After that, install the python package via

.. code::

   pip install -e .

Once ``hgdb-vitis`` is installed, you will find ``hgdb-vitis``
in the ``$PATH``. It's recommended to use a virtualenv. It may take
a while to install since it's building a full-fledged SystemVerilog parser.

How to use
----------

``hgdb-vitis`` follows the standard command line arguments. Notice that
it supports path mapping on the client side (you can also use ``hgdb``'s
path remapping in the debug configuration). The syntax is
``[src_path]:[dst_path]``; use ``;`` to separate multiple paths.

.. code::

   usage: hgdb-vitis [-h] [-o OUTPUT] [-r REMAP] solution

   positional arguments:
     solution              Xilinx Vitis solution dir

   optional arguments:
     -h, --help            show this help message and exit
     -o OUTPUT             Output symbol table name
     -r REMAP, --remap REMAP

Notice that the solution folder is the folder under the project folder.
Typically, it follows the pattern of ``solution#``, where ``#`` is a
number. Your solution also needs to have ``config_debug`` enabled.

Caveat
------

``hgdb-vitis`` tries its best to discover symbol definition and match it
with the RTL signals. In fact, it parses the generated RTL to discover
RTL signal definition. A list of heuristics is used to reverse the HLS
transformation as much as possible, you can see the notes `here`_.
However, due to limited symbol information produced by Vitis, the
ability to debug using the C++ source variable name is still unsatisfactory.
If you have any idea of improving ``hgdb-vitis``, feel free to file an
issue.

.. _here: https://github.com/Kuree/hgdb-vitis/blob/master/notes.md
