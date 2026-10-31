module MixedControlUnit (
    input SystemEnable,
    input SignalA,
    input SignalB,
    output Alert,
    output ReadySignal,
    output SafeStatus
);
    wire netInternal;

    nand check1 (netInternal, SignalA, SignalB);
    nor  check2 (Alert, netInternal, SystemEnable);
    xnor check3 (ReadySignal, SignalA, SystemEnable);
    buf  buffer1 (SafeStatus, SystemEnable);
endmodule
