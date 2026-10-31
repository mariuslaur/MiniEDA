module Decoder2to4 (
    input E,      // Enable input (must be 1 for outputs to activate)
    input A0,     // Selection bit 0 (LSB)
    input A1,     // Selection bit 1 (MSB)
    output Y0,    // Output line 0
    output Y1,    // Output line 1
    output Y2,    // Output line 2
    output Y3     // Output line 3
);
    // Internal wires for inverted selection signals
    wire A0_not;
    wire A1_not;

    // Generate inverted signals
    not inv0 (A0_not, A0);
    not inv1 (A1_not, A1);

    // Output decoding logic (active high only when E is 1)
    // Y0 = E & ~A1 & ~A0
    and gate0 (Y0, E, A1_not, A0_not);
    
    // Y1 = E & ~A1 & A0
    and gate1 (Y1, E, A1_not, A0);
    
    // Y2 = E & A1 & ~A0
    and gate2 (Y2, E, A1, A0_not);
    
    // Y3 = E & A1 & A0
    and gate3 (Y3, E, A1, A0);

endmodule