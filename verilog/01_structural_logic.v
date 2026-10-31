module structural_logic (
    input a,
    input b,
    input c,
    output out_y,
    output out_z
);

    // Internal wires used to interconnect the gates
    wire w1;
    wire w2;

    // Instantiation of internal logic gates 
    xor u1 (w1, a, b);
    not u2 (w2, c);
    
    and u3 (out_y, w1, c);
    or  u4 (out_z, w1, w2);

endmodule
