module dataflow_logic (
    input [1:0] a,
    input b,
    output y,
    output z
);

    // Internal logic expressed via Boolean equations
    assign y = (a[0] & b) | (~a[1]); 
    assign z = (a[0] ^ b) & y;

endmodule