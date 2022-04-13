Some notes on reverse engineering the code generation process of Vitis

1. array seems to be created as a reg file
2. If it's multi-dimensional array, flattens to the last dim
3. Last array dim can be accessed through `${name}_U.ram`
4. Labeled basic block will be extracted out as a separate module
5. `${design_top}.bc` actually built with new LLVM toolchain that can be disassembly with `llvm-dis` 7+
6. `a.o.3.bc` is actually built with much ancient LLVM, e.g. LLVM 3.1. This is however, optimized build where basic block gets pulled out. This is the bitcode that's closest to RTL
