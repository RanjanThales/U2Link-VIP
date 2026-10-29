`timescale 1ns/1ps

package ucie_dpi_pkg;

  // --------------------------------------------------------------------------
  // DPI-C declarations
  // --------------------------------------------------------------------------

  import "DPI-C" function void ucie_dpi_write(
    input int unsigned addr,
    input longint unsigned data
  );

  import "DPI-C" function void ucie_dpi_read(
    input int unsigned addr
  );

  import "DPI-C" function void ucie_dpi_send_word(
    input longint unsigned data
  );

  import "DPI-C" function void ucie_dpi_send_frame(
    input longint unsigned base_pattern
  );

  import "DPI-C" function void ucie_dpi_clear_rx();
  import "DPI-C" function void ucie_dpi_clear_tx();

  import "DPI-C" function int ucie_dpi_done();
  import "DPI-C" function int ucie_dpi_busy();
  import "DPI-C" function int ucie_dpi_error();

  import "DPI-C" function longint unsigned ucie_dpi_result();

  import "DPI-C" function longint unsigned ucie_dpi_rx_count();
  import "DPI-C" function longint unsigned ucie_dpi_tx_count();

  import "DPI-C" function longint unsigned ucie_dpi_rx_word(
    input int unsigned index
  );


  // --------------------------------------------------------------------------
  // Wait for completion of one DPI transaction
  // --------------------------------------------------------------------------

  task automatic dpi_wait_done(
    input time timeout = 20ms
  );

    time start_t;

    start_t = $time;

    while (!ucie_dpi_done()) begin

      #1ns;

      if (($time - start_t) >= timeout) begin
        $fatal(1,
          "[%0t] UCIe DPI transaction timeout after %0t",
          $time,
          timeout
        );
      end

    end

    if (ucie_dpi_error()) begin
      $fatal(1,
        "[%0t] UCIe DPI transaction returned ERROR",
        $time
      );
    end

  endtask


  // --------------------------------------------------------------------------
  // Register write
  // --------------------------------------------------------------------------

  task automatic ucie_write(
    input int unsigned addr,
    input longint unsigned data,
    input time timeout = 20ms
  );

    ucie_dpi_write(addr, data);

    dpi_wait_done(timeout);

  endtask


  // --------------------------------------------------------------------------
  // Register read
  // --------------------------------------------------------------------------

  task automatic ucie_read(
    input int unsigned addr,
    output longint unsigned data,
    input time timeout = 20ms
  );

    ucie_dpi_read(addr);

    dpi_wait_done(timeout);

    data = ucie_dpi_result();

  endtask


  // --------------------------------------------------------------------------
  // Send one streaming word
  // --------------------------------------------------------------------------

  task automatic ucie_send_word(
    input longint unsigned data,
    input time timeout = 20ms
  );

    ucie_dpi_send_word(data);

    dpi_wait_done(timeout);

  endtask


  // --------------------------------------------------------------------------
  // Send complete streaming frame
  // --------------------------------------------------------------------------

  task automatic ucie_send_frame(
    input longint unsigned base_pattern,
    input time timeout = 20ms
  );

    ucie_dpi_send_frame(base_pattern);

    dpi_wait_done(timeout);

  endtask


  // --------------------------------------------------------------------------
  // Simulation-time wait
  //
  // SystemC clock is 1 ns in the supplied model.
  // Do NOT reference SV_TOP/SystemC hierarchy from this package.
  // --------------------------------------------------------------------------

  task automatic ucie_wait_cycles(
    input int unsigned cycles
  );

    #(cycles * 1ns);

  endtask


  // --------------------------------------------------------------------------
  // RX/TX accessors
  // --------------------------------------------------------------------------

  function automatic longint unsigned ucie_rx_count();
    return ucie_dpi_rx_count();
  endfunction


  function automatic longint unsigned ucie_tx_count();
    return ucie_dpi_tx_count();
  endfunction


  function automatic longint unsigned ucie_rx_word(
    input int unsigned index
  );
    return ucie_dpi_rx_word(index);
  endfunction

endpackage
