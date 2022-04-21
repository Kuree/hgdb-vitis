Some notes on reverse engineering the code generation process of Vitis

1. array seems to be created as a reg file
2. If it's multidimensional array, flattens to the last dim, or composed as couple register files (see #3)
3. Last array dim can be accessed through `${name}_U.ram`
4. Labeled basic block will be extracted out as a separate module/function.
5. `${design_top}.bc` actually built with new LLVM toolchain that can be disassembled with `llvm-dis` 7+ and contains proper debug information.
6. `a.o.3.bc` is actually built with much ancient LLVM, e.g. LLVM 3.1. This is however, optimized build where basic block gets pulled out. This is the bitcode that's closest to RTL
7. The state information in verbose.rpt does not represent the total state "properly". In pipelined cases, the number of states shown there and the one-hot encoded state variable do not match. Use `.xrf` file in `.debug` instead.
8. llvm declared symbols are not directly generated into RTL signals. Need to follow its use to see which signals are generated.
9. If a function is inlined but some of its basic blocks are ripped out as an individual function (separate module in RTL), then the new function args are usually carried from the caller site. In this case, we need to know the actual RTL reference from the caller since all the signals are passed in.
10. Inputs passed in to the top functions are typically streamed in, so there won't be any memory device mapping in the RTL.
11. In a pipelined design, loops can get unrolled and Vitis may yse dual-port memory to read/write data.
