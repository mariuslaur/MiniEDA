module UniversalShiftRegister2Bit (
    input CLK,    // Clock signal
    input RST,    // Active-high asynchronous reset
    input S0,     // Operation select bit 0
    input S1,     // Operation select bit 1
    input LIN,    // Left serial input (for shift-left)
    input RIN,    // Right serial input (for shift-right)
    input I0,     // Parallel data input 0
    input I1,     // Parallel data input 1
    output Q0,    // Registered output bit 0
    output Q1     // Registered output bit 1
);
    // Wires for inverted control signals
    wire S0_not;
    wire S1_not;
    wire CLK_not;
    wire RST_not;

    // Wires for D Flip-Flop 0 (DFF0) inputs and feedback
    wire mux0_w0, mux0_w1, mux0_w2, mux0_w3, d0;
    wire q0_not;

    // Wires for D Flip-Flop 1 (DFF1) inputs and feedback
    wire mux1_w0, mux1_w1, mux1_w2, mux1_w3, d1;
    wire q1_not;

    // Inverter stage
    not inv_s0 (S0_not, S0);
    not inv_s1 (S1_not, S1);
    not inv_clk (CLK_not, CLK);
    not inv_rst (RST_not, RST);

    // ==========================================
    // STAGE 1: MULTIPLEXER FOR DFF0 INPUT (d0)
    // Mode Selection: 00=Hold, 01=Shift-Right, 10=Shift-Left, 11=Parallel Load
    // ==========================================
    and m0_g0 (mux0_w0, Q0,  S1_not, S0_not); // Mode 00: Hold Q0
    and m0_g1 (mux0_w1, RIN, S1_not, S0);     // Mode 01: Shift-Right (takes RIN)
    and m0_g2 (mux0_w2, Q1,  S1,     S0_not); // Mode 10: Shift-Left (takes neighbor Q1)
    and m0_g3 (mux0_w3, I0,  S1,     S0);     // Mode 11: Parallel Load I0
    or  m0_out (d0, mux0_w0, mux0_w1, mux0_w2, mux0_w3);

    // ==========================================
    // STAGE 2: MULTIPLEXER FOR DFF1 INPUT (d1)
    // Mode Selection: 00=Hold, 01=Shift-Right, 10=Shift-Left, 11=Parallel Load
    // ==========================================
    and m1_g0 (mux1_w0, Q1,  S1_not, S0_not); // Mode 00: Hold Q1
    and m1_g1 (mux1_w1, Q0,  S1_not, S0);     // Mode 01: Shift-Right (takes neighbor Q0)
    and m1_g2 (mux1_w2, LIN, S1,     S0_not); // Mode 10: Shift-Left (takes LIN)
    and m1_g3 (mux1_w3, I1,  S1,     S0);     // Mode 11: Parallel Load I1
    or  m1_out (d1, mux1_w0, mux1_w1, mux1_w2, mux1_w3);

    // ==========================================
    // STAGE 3: STRUCTURAL D FLIP-FLOP 0 (DFF0) WITH ASYNCHRONOUS RESET
    // ==========================================
    wire dff0_master_nand1, dff0_master_nand2;
    wire dff0_master_q, dff0_master_q_not;
    wire dff0_slave_nand1, dff0_slave_nand2;

    // Master Latch (Gated by CLK_not)
    nand dff0_m1 (dff0_master_nand1, d0, CLK_not);
    nand dff0_m2 (dff0_master_nand2, dff0_master_nand1, CLK_not);
    nand dff0_m3 (dff0_master_q, dff0_master_nand1, dff0_master_q_not);
    nand dff0_m4 (dff0_master_q_not, dff0_master_nand2, dff0_master_q, RST_not); // Reset clears master

    // Slave Latch (Gated by CLK)
    nand dff0_s1 (dff0_slave_nand1, dff0_master_q, CLK);
    nand dff0_s2 (dff0_slave_nand2, dff0_master_q_not, CLK);
    nand dff0_s3 (Q0, dff0_slave_nand1, q0_not);
    nand dff0_s4 (q0_not, dff0_slave_nand2, Q0, RST_not); // Reset clears slave (Q0 = 0)

    // ==========================================
    // STAGE 4: STRUCTURAL D FLIP-FLOP 1 (DFF1) WITH ASYNCHRONOUS RESET
    // ==========================================
    wire dff1_master_nand1, dff1_master_nand2;
    wire dff1_master_q, dff1_master_q_not;
    wire dff1_slave_nand1, dff1_slave_nand2;

    // Master Latch (Gated by CLK_not)
    nand dff1_m1 (dff1_master_nand1, d1, CLK_not);
    nand dff1_m2 (dff1_master_nand2, dff1_master_nand1, CLK_not);
    nand dff1_m3 (dff1_master_q, dff1_master_nand1, dff1_master_q_not);
    nand dff1_m4 (dff1_master_q_not, dff1_master_nand2, dff1_master_q, RST_not);

    // Slave Latch (Gated by CLK)
    nand dff1_s1 (dff1_slave_nand1, dff1_master_q, CLK);
    nand dff1_s2 (dff1_slave_nand2, dff1_master_q_not, CLK);
    nand dff1_s3 (Q1, dff1_slave_nand1, q1_not);
    nand dff1_s4 (q1_not, dff1_slave_nand2, Q1, RST_not);

endmodule