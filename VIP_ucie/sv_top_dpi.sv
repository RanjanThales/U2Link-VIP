`timescale 1ns/1ps

module SV_TOP;

  import ucie_dpi_pkg::*;

  // SystemC exported top.
  tb_export u_sc_top();

  localparam int unsigned RESET_DWELL_CYCLES = 1000;
  localparam int unsigned SB_PAT_CYC = 3000;

  localparam int unsigned REG_START_TX      = 'h00;
  localparam int unsigned REG_STREAM_DATA   = 'h01;
  localparam int unsigned REG_EXPECT_RX     = 'h02;
  localparam int unsigned REG_FORMAT        = 'h03;
  localparam int unsigned REG_LP_STATE_REQ  = 'h06;
  localparam int unsigned REG_LP_CONTROL    = 'h07;
  localparam int unsigned REG_AFE_LINK_DONE = 'h10;
  localparam int unsigned REG_TX_FLIT_FMT   = 'h11;
  localparam int unsigned REG_RX_FLIT_FMT   = 'h12;
  localparam int unsigned REG_SPEED_CAP     = 'h20;
  localparam int unsigned REG_WIDTH_CAP     = 'h21;
  localparam int unsigned REG_RDI_STATE     = 'h24;
  localparam int unsigned REG_ACTIVE_SPEED  = 'h26;

  initial begin
    // Allow SystemC reset/link initialization to start.
    #20ns;

    $display("");
    $display("========================================");
    $display("SV UCIe DPI Testbench");
    $display("========================================");

    test1_link_bringup();
    test2_streaming();
    test3_l1();
    test4_l2();
    test5_retrain();
    test6_speed_width();

    $display("");
    $display("========================================");
    $display("SV TESTS COMPLETE");
    $display("TX words = %0d", ucie_tx_count());
    $display("RX words = %0d", ucie_rx_count());
    $display("========================================");

    // Let SystemC end_of_simulation/check_results run.
    #100ns;
    $finish;
  end

  task automatic test1_link_bringup();
    longint unsigned link_done;

    $display("");
    $display("========================================");
    $display("TEST1: Link Bring-up");
    $display("========================================");

    ucie_write(REG_LP_CONTROL, 64'h10);

    // The SystemC model reaches ACTIVE after several milliseconds.
    // Poll the DUT status rather than assuming a fixed startup duration.
    link_done = 0;

    for (int attempts = 0; attempts < 200 && link_done == 0; attempts++) begin
      ucie_read(REG_AFE_LINK_DONE, link_done);

      $display("[%0t] TEST1 AFE_LINK_DONE=%0d", $time, link_done);

      if (link_done == 0)
        #100us;
    end

    if (link_done != 1)
      $fatal(1,
        "[%0t] TEST1 FAIL: AFE_LINK_DONE=%0d",
        $time, link_done);

    $display("[%0t] TEST1 PASS: AFE_LINK_DONE=1", $time);

    ucie_write(REG_START_TX, 64'h1);
    ucie_write(REG_EXPECT_RX, 64'h1);

    #(10ns);
  endtask

  task automatic test2_streaming();
    longint unsigned expected;

    $display("");
    $display("========================================");
    $display("TEST2: Streaming Data Transfer");
    $display("========================================");

    ucie_write(REG_FORMAT, 64'h1);
    ucie_write(REG_TX_FLIT_FMT, 64'h1);
    ucie_write(REG_RX_FLIT_FMT, 64'h1);

    ucie_send_frame(64'hA000000000000000);

    #(500ns);

    expected = ucie_tx_count();

    $display("[%0t] TEST2 RX count=%0d expected=%0d",
             $time, ucie_rx_count(), expected);

    if (ucie_rx_count() != expected)
      $error("[%0t] TEST2 count mismatch", $time);
    else
      $display("[%0t] TEST2 PASS", $time);
  endtask

  task automatic test3_l1();
    longint unsigned rdi_state;

    $display("");
    $display("========================================");
    $display("TEST3: L1 Power Management");
    $display("========================================");

    ucie_write(REG_LP_STATE_REQ, 64'h4);

    rdi_state = 0;

    for (int attempts = 0; attempts < 200 && rdi_state != 4; attempts++) begin
      ucie_read(REG_RDI_STATE, rdi_state);

      if (rdi_state != 4)
        #(10ns);
    end

    $display("[%0t] TEST3 RDI_STATE=%0d", $time, rdi_state);

    if (rdi_state != 4)
      $error("[%0t] TEST3 FAIL: PHY did not enter L1", $time);
    else
      $display("[%0t] TEST3 PASS: PHY entered L1", $time);

    ucie_write(REG_LP_STATE_REQ, 64'h1);

    #((SB_PAT_CYC + 500) * 1ns);

    ucie_send_frame(64'hB000000000000000);

    #(500ns);

    $display("[%0t] TEST3 RX count=%0d", $time, ucie_rx_count());
  endtask

  task automatic test4_l2();
    longint unsigned rdi_state;

    $display("");
    $display("========================================");
    $display("TEST4: L2 Power Management");
    $display("========================================");

    ucie_write(REG_LP_STATE_REQ, 64'h8);

    rdi_state = 0;

    for (int attempts = 0; attempts < 200 && rdi_state != 8; attempts++) begin
      ucie_read(REG_RDI_STATE, rdi_state);

      if (rdi_state != 8)
        #(10ns);
    end

    $display("[%0t] TEST4 RDI_STATE=%0d", $time, rdi_state);

    if (rdi_state != 8)
      $error("[%0t] TEST4 FAIL: PHY did not enter L2", $time);
    else
      $display("[%0t] TEST4 PASS: PHY entered L2", $time);

    ucie_write(REG_LP_STATE_REQ, 64'h1);

    // L2 exit includes RESET + full retrain in the current model.
    #((RESET_DWELL_CYCLES + SB_PAT_CYC + 500) * 1ns);

    ucie_read(REG_RDI_STATE, rdi_state);
    $display("[%0t] TEST4 RDI_STATE after L2 exit=%0d",
             $time, rdi_state);

    ucie_send_frame(64'hC000000000000000);

    #(500ns);

    $display("[%0t] TEST4 RX count=%0d", $time, ucie_rx_count());
  endtask

  task automatic test5_retrain();
    longint unsigned rdi_state;

    $display("");
    $display("========================================");
    $display("TEST5: Retrain");
    $display("========================================");

    ucie_write(REG_LP_STATE_REQ, 64'hB);

    rdi_state = 0;

    for (int attempts = 0; attempts < 200 && rdi_state != 11; attempts++) begin
      ucie_read(REG_RDI_STATE, rdi_state);

      if (rdi_state != 11)
        #(10ns);
    end

    $display("[%0t] TEST5 RDI_STATE=%0d", $time, rdi_state);

    if (rdi_state != 11)
      $error("[%0t] TEST5 FAIL: PHY did not enter RETRAIN", $time);
    else
      $display("[%0t] TEST5 PASS: PHY entered RETRAIN", $time);

    ucie_write(REG_LP_STATE_REQ, 64'h1);

    #((SB_PAT_CYC + 500) * 1ns);

    ucie_send_frame(64'hD000000000000000);

    #(500ns);

    $display("[%0t] TEST5 RX count=%0d", $time, ucie_rx_count());
  endtask

  task automatic test6_speed_width();
    longint unsigned speed;
    longint unsigned width;
    longint unsigned active_speed;

    $display("");
    $display("========================================");
    $display("TEST6: Speed/Width Configuration");
    $display("========================================");

    ucie_write(REG_SPEED_CAP, 64'h3);
    ucie_write(REG_WIDTH_CAP, 64'h2);

    #(5ns);

    ucie_read(REG_SPEED_CAP, speed);
    ucie_read(REG_WIDTH_CAP, width);

    if ((speed != 3) || (width != 2))
      $error("[%0t] TEST6 FAIL: speed=%0d width=%0d",
             $time, speed, width);
    else
      $display("[%0t] TEST6 PASS: speed=3 width=2", $time);

    ucie_read(REG_ACTIVE_SPEED, active_speed);

    $display("[%0t] TEST6 REG_ACTIVE_SPEED=%0d",
             $time, active_speed);

    ucie_send_frame(64'hE000000000000000);

    #(500ns);
  endtask

endmodule
