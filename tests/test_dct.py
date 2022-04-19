import subprocess
import os
import tempfile
import pytest

files = [pytest.param(["dct", 42], id="dct"), pytest.param(["dct-pipelined", 10], id="dct-pipelined")]


@pytest.mark.parametrize("case", files)
def test_dct(case):
    name, bp_id = case
    # convert it first
    with tempfile.TemporaryDirectory() as temp:
        # remap to a temp file
        target_dir = os.path.join(temp, "src")
        root_dir = os.path.dirname(os.path.abspath(__file__))
        vector_files = os.path.join(root_dir, "vectors", name)
        output_db = os.path.join(temp, "debug.db")
        subprocess.call(["hgdb-vitis", vector_files, "-o", output_db, "-r",
                         "/home/keyi/AHA/Vitis-Tutorials/Getting_Started/Vitis_HLS/reference-files/src/:" + temp])

        # load up the hgdb-db tool
        pipe = subprocess.Popen(["hgdb-db", output_db], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        commands = f"breakpoint\nid {bp_id}\n"
        output = pipe.communicate(commands.encode())[0]
        output = output.decode("ascii")
        assert f"id: {bp_id}" in output
        pipe.kill()


if __name__ == "__main__":
    test_dct(("dct", 42))
