module FullSet (
  input A,
  input B,
  output Y_nand,
  output Y_nor,
  output Y_xnor,
  output Y_buf,
  output Y_not);
  nand g1 (Y_nand, A, B);
  nor  g2 (Y_nor, A, B);
  xnor g3 (Y_xnor, A, B);
  buf  g4 (Y_buf, A);
  not  g5 (Y_not, B);
endmodule
