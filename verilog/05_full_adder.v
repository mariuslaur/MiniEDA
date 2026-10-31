module FullAdder (
    input A,
    input B,
    input Cin,
    output Sum,
    output Cout
);
    // Internal wires for gate interconnections
    wire w1;
    wire w2;
    wire w3;

    // Sum logic: Sum = A ^ B ^ Cin
    xor gate1 (w1, A, B);
    xor gate2 (Sum, w1, Cin);

    // Carry-out logic: Cout = (A & B) | (Cin & (A ^ B))
    and gate3 (w2, A, B);
    and gate4 (w3, w1, Cin);
    or  gate5 (Cout, w2, w3);

endmodule