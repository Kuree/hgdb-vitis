import subprocess
import os
import tempfile
import pytest


def test_ex1():
    # convert it first
    with tempfile.TemporaryDirectory() as temp:
        # remap to a temp file
        root_dir = os.path.dirname(os.path.abspath(__file__))
        vector_files = os.path.join(root_dir, "vectors", "ex1")
        output_db = os.path.join(temp, "debug.db")
        subprocess.call(["hgdb-vitis", vector_files, "-o", output_db, "-r",
                         "/home/nathan/Desktop/vitis_examples/code_for_vitis/pc_example/:" + temp])

        # load up the hgdb-db tool
        pipe = subprocess.Popen(["hgdb-db", output_db], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        commands = f"context\nid 5\n"
        output = pipe.communicate(commands.encode())[0]
        output = output.decode("ascii")
        assert f"name: num" in output
        assert f"name: i" in output
        pipe.kill()


if __name__ == "__main__":
    test_ex1()
