// A single module showcasing multiple 1-bit inputs, outputs, and interconnected internal gates
// Note: 'wire' keyword is omitted from port definitions as it is the default type
module complex_gates (
    // 6 distinct 1-bit inputs
    input in_a,
    input in_b,
    input in_c,
    input in_d,
    input in_e,
    input in_f,
    
    // 3 distinct 1-bit outputs
    output out_x,
    output out_y,
    output out_z
);

    // Internal wires acting as temporary nodes to connect gates together
    wire w_and1;
    wire w_not1;
    wire w_xor1;
    wire w_nor1;
    wire w_nand1;
    wire w_or1;

    // --- LEVEL 1: First layer of logic gates processing the raw inputs ---
    and u1 (w_and1, in_a, in_b, in_c);
    not u2 (w_not1, in_d);
    xor u3 (w_xor1, in_e, in_f);

    // --- LEVEL 2: Intermediate layer connecting inputs with Layer 1 outputs ---
    nor u4 (w_nor1, w_and1, in_d);
    nand u5 (w_nand1, w_not1, w_xor1);
    or u6 (w_or1, w_and1, w_xor1);

    // --- LEVEL 3: Final layer driving the structural outputs ---
    xnor u7 (out_x, w_nor1, w_nand1);
    assign out_y = w_or1;
    and u8 (out_z, w_or1, w_not1);

endmodule