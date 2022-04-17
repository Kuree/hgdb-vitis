import subprocess
import os
import tempfile


def test_dct():
    # convert it first
    with tempfile.TemporaryDirectory() as temp:
        # remap to a temp file
        target_dir = os.path.join(temp, "src")
        root_dir = os.path.dirname(os.path.abspath(__file__))
        vector_files = os.path.join(root_dir, "vectors", "dct")
        output_db = os.path.join(temp, "debug.db")
        subprocess.call(["hgdb-vitis", vector_files, "-o", output_db, "-r",
                         "/home/keyi/AHA/Vitis-Tutorials/Getting_Started/Vitis_HLS/reference-files/src/:" + temp])

        # load up the hgdb-db tool
        pipe = subprocess.Popen(["hgdb-db", output_db], stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        commands = "breakpoint\nid 42\n"
        output = pipe.communicate(commands.encode())[0]
        output = output.decode("ascii")
        assert "id: 42" in output
        pipe.kill()


if __name__ == "__main__":
    test_dct()
