module Mux4to1 (
    input D0,     // Data input 0
    input D1,     // Data input 1
    input D2,     // Data input 2
    input D3,     // Data input 3
    input S0,     // Select line 0 (LSB)
    input S1,     // Select line 1 (MSB)
    output Y      // Multiplexer output
);
    // Internal wires for inverted selection signals
    wire S0_not;
    wire S1_not;

    // Internal wires for intermediate AND gate outputs
    wire w0;
    wire w1;
    wire w2;
    wire w3;

    // Generate inverted selection signals
    not inv0 (S0_not, S0);
    not inv1 (S1_not, S1);

    // Conditional AND logic for each data path
    // w0 is active only if S1=0 and S0=0 (selects D0)
    and gate0 (w0, D0, S1_not, S0_not);

    // w1 is active only if S1=0 and S0=1 (selects D1)
    and gate1 (w1, D1, S1_not, S0);

    // w2 is active only if S1=1 and S0=0 (selects D2)
    and gate2 (w2, D2, S1, S0_not);

    // w3 is active only if S1=1 and S0=1 (selects D3)
    and gate3 (w3, D3, S1, S0);

    // Final OR stage to route the selected path to the output
    or gate4 (Y, w0, w1, w2, w3);

endmodule