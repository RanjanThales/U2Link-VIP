#include <systemc.h>
#include <nvhls_connections.h>
#include <nvhls_int.h>
#include <testbench/nvhls_rand.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <deque>
#include <cstdint>
#include <mutex>

#include "ucie_top.h"
#include "ucie_dpi.h"

static const int WORDS_PER_FRAME =
    madsim::STREAMING_DATA_BYTES / madsim::AXI_BYTES;

/*
 * SystemC-side driver.
 *
 * IMPORTANT:
 * All timed Connections operations happen from SystemC processes.
 * DPI functions only enqueue a command and return; they never call
 * wait(), Push(), or Pop() directly.
 */
SC_MODULE(UcieDriver) {
  sc_in<bool> clk;
  sc_in<bool> rst;

  Connections::Out<spec::Axi::SlaveToRVA::Write> rva_in;
  Connections::In<spec::Axi::SlaveToRVA::Read>  rva_out;

  std::vector<NVUINT64> sent_data;
  std::vector<NVUINT64> received_data;

  // --------------------------------------------------------------------------
  // DPI command mailbox
  // --------------------------------------------------------------------------
  struct Command {
    int cmd;
    unsigned addr;
    uint64_t data;
    uint64_t arg;
    uint64_t seq;
  };

  std::deque<Command> cmd_q;
  sc_event cmd_event;

  bool dpi_busy;
  bool dpi_done;
  bool dpi_error;
  uint64_t dpi_result;
  uint64_t next_seq;

  // Current register-read transaction. RX monitor consumes the corresponding
  // RVA response and completes the command.
  bool read_pending;
  uint64_t read_result;
  bool read_done;

  bool test_pass;

  SC_CTOR(UcieDriver)
      : dpi_busy(false),
        dpi_done(false),
        dpi_error(false),
        dpi_result(0),
        next_seq(1),
        read_pending(false),
        read_result(0),
        read_done(false),
        test_pass(true) {

    SC_THREAD(command_service);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);

    SC_THREAD(rx_monitor);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  }

  // --------------------------------------------------------------------------
  // Existing low-level helpers
  // --------------------------------------------------------------------------
  void ctrl_write(unsigned addr, NVUINT64 val) {
    spec::Axi::SlaveToRVA::Write w;
    w.wstrb = 0xFF;
    w.rw    = 1;
    w.addr  = addr;
    w.data  = val;
    rva_in.Push(w);
    wait();
  }

  NVUINT64 poll_reg(unsigned addr) {
    read_done = false;
    read_pending = true;

    spec::Axi::SlaveToRVA::Write w;
    w.wstrb = 0;
    w.rw    = 0;
    w.addr  = addr;
    w.data  = 0;

    rva_in.Push(w);
    wait();

    while (!read_done)
      wait();

    read_pending = false;
    return read_result;
  }

  void send_word(NVUINT64 data) {
    ctrl_write(0x01, data);
    sent_data.push_back(data);
  }

  void send_frame(NVUINT64 base_pattern) {
    for (int i = 0; i < WORDS_PER_FRAME; ++i) {
      NVUINT64 data = base_pattern | NVUINT64(i);
      send_word(data);
    }
  }

  // --------------------------------------------------------------------------
  // DPI command service
  // --------------------------------------------------------------------------
  void command_service() {
    rva_in.Reset();
    wait();

    while (true) {
      if (cmd_q.empty()) {
        wait(cmd_event);
        continue;
      }

      Command c = cmd_q.front();
      cmd_q.pop_front();

      dpi_busy  = true;
      dpi_done  = false;
      dpi_error = false;
      dpi_result = 0;

      switch (c.cmd) {
        case UCIE_DPI_WRITE:
          ctrl_write(c.addr, c.data);
          break;

        case UCIE_DPI_READ:
          dpi_result = poll_reg(c.addr);
          break;

        case UCIE_DPI_SEND_WORD:
          send_word(c.data);
          break;

        case UCIE_DPI_SEND_FRAME:
          send_frame(c.data);
          break;

        case UCIE_DPI_CLEAR_RX:
          received_data.clear();
          break;

        case UCIE_DPI_CLEAR_TX:
          sent_data.clear();
          break;

        default:
          std::cerr << sc_time_stamp()
                    << " [DPI] Unknown command " << c.cmd << std::endl;
          dpi_error = true;
          break;
      }

      dpi_busy = false;
      dpi_done = true;
    }
  }

  // --------------------------------------------------------------------------
  // RX monitor
  // --------------------------------------------------------------------------
  void rx_monitor() {
    rva_out.Reset();
    wait();

    while (true) {
      spec::Axi::SlaveToRVA::Read r;

      if (rva_out.PopNB(r)) {
        if (read_pending && !read_done) {
          read_result = r.data;
          read_done = true;
        } else {
          received_data.push_back(r.data);

          if (received_data.size() <= 4) {
            std::cout << sc_time_stamp()
                      << " [RX] Word[" << std::setw(3)
                      << received_data.size() - 1 << "]: "
                      << std::hex << std::setfill('0')
                      << std::setw(16) << r.data
                      << std::dec << std::setfill(' ') << std::endl;
          } else if (received_data.size() == 5) {
            std::cout << "  ..." << std::endl;
          }
        }
      }

      wait();
    }
  }

  // --------------------------------------------------------------------------
  // Called by the exported C DPI functions.
  //
  // These functions are intentionally non-timed. They only enqueue work.
  // --------------------------------------------------------------------------
  void enqueue(int cmd, unsigned addr, uint64_t data, uint64_t arg = 0) {
    Command c;
    c.cmd  = cmd;
    c.addr = addr;
    c.data = data;
    c.arg  = arg;
    c.seq  = next_seq++;

    cmd_q.push_back(c);
    cmd_event.notify(sc_core::SC_ZERO_TIME);
  }

  void dpi_start_write(unsigned addr, uint64_t data) {
    enqueue(UCIE_DPI_WRITE, addr, data);
  }

  void dpi_start_read(unsigned addr) {
    enqueue(UCIE_DPI_READ, addr, 0);
  }

  void dpi_start_send_word(uint64_t data) {
    enqueue(UCIE_DPI_SEND_WORD, 0, data);
  }

  void dpi_start_send_frame(uint64_t base_pattern) {
    enqueue(UCIE_DPI_SEND_FRAME, 0, base_pattern);
  }

  void dpi_clear_rx() {
    enqueue(UCIE_DPI_CLEAR_RX, 0, 0);
  }

  void dpi_clear_tx() {
    enqueue(UCIE_DPI_CLEAR_TX, 0, 0);
  }
};

// -----------------------------------------------------------------------------
// Die Loopback
// -----------------------------------------------------------------------------
SC_MODULE(UcieDieLoopback) {
  sc_in<bool> clk;
  sc_in<bool> rst_n;

  Connections::In<LaneWord>   die0_tx_lanes;
  Connections::In<NVUINT16>   die0_tx_valid;
  Connections::Out<LaneWord>  die0_rx_lanes;
  Connections::Out<NVUINT16>  die0_rx_valid;

  Connections::In<SbPkt>  die0_sb_tx;
  Connections::Out<SbPkt> die0_sb_rx;

  int lane_word_count;
  int sb_pkt_count;

  SC_CTOR(UcieDieLoopback)
      : lane_word_count(0), sb_pkt_count(0) {

    SC_THREAD(lane_loopback_proc);
    sensitive << clk.pos();
    async_reset_signal_is(rst_n, false);

    SC_THREAD(sideband_loopback_proc);
    sensitive << clk.pos();
    async_reset_signal_is(rst_n, false);
  }

  void lane_loopback_proc() {
    die0_tx_lanes.Reset();
    die0_tx_valid.Reset();
    die0_rx_lanes.Reset();
    die0_rx_valid.Reset();

    wait();

    while (true) {
      LaneWord word;
      NVUINT16 valid;

      if (die0_tx_valid.PopNB(valid) && die0_tx_lanes.PopNB(word)) {
        lane_word_count++;
        die0_rx_valid.Push(valid);
        die0_rx_lanes.Push(word);

        if (lane_word_count <= 2) {
          std::cout << sc_time_stamp()
                    << " | Loopback: Lane word "
                    << lane_word_count - 1
                    << ", valid=0x" << std::hex
                    << (int)valid << std::dec << std::endl;
        }
      }

      wait();
    }
  }

  void sideband_loopback_proc() {
    die0_sb_tx.Reset();
    die0_sb_rx.Reset();

    wait();

    while (true) {
      SbPkt pkt;

      if (die0_sb_tx.PopNB(pkt)) {
        sb_pkt_count++;

        SbPkt rsp = pkt;

        if (rsp.code == LINKMGMT_RDI) {
          rsp.data = encode_mailbox_rsp(get_mailbox_state(pkt.data));
        } else if ((rsp.code & 1) == 0) {
          rsp.code |= 1;
        }

        rsp.info = 0xFFFF;
        die0_sb_rx.Push(rsp);

        if (sb_pkt_count <= 5) {
          std::cout << sc_time_stamp()
                    << " | SB Loopback: pkt #"
                    << sb_pkt_count - 1
                    << " code=0x" << std::hex
                    << (int)pkt.code << std::dec << std::endl;
        }
      }

      wait();
    }
  }
};

// -----------------------------------------------------------------------------
// SC_TOP
// -----------------------------------------------------------------------------
SC_MODULE(sc_top) {
  sc_clock clk;
  sc_signal<bool> rst;
  sc_signal<bool> rst_n;

  ucie_top dut;
  UcieDriver drv;
  UcieDieLoopback loopback;

  Connections::Combinational<spec::Axi::SlaveToRVA::Write> ch_rva_in;
  Connections::Combinational<spec::Axi::SlaveToRVA::Read>  ch_rva_out;

  Connections::Combinational<LaneWord>  ch_phy_tx_lanes;
  Connections::Combinational<NVUINT16>  ch_phy_tx_valid;
  Connections::Combinational<LaneWord>  ch_phy_rx_lanes;
  Connections::Combinational<NVUINT16>  ch_phy_rx_valid;

  Connections::Combinational<SbPkt> ch_sb_tx;
  Connections::Combinational<SbPkt> ch_sb_rx;

  // Reset only. Test sequencing is now owned by SV.
  SC_CTOR(sc_top)
      : clk("clk", 1, SC_NS, 0.5, 0, SC_NS, true),
        rst("rst"),
        rst_n("rst_n"),
        dut("dut"),
        drv("drv"),
        loopback("loopback") {

    Connections::set_sim_clk(&clk);

    dut.clk(clk);
    dut.rst(rst_n);
    dut.rst_n(rst_n);
    dut.rva_in(ch_rva_in);
    dut.rva_out(ch_rva_out);
    dut.phy_tx_lanes(ch_phy_tx_lanes);
    dut.phy_tx_valid(ch_phy_tx_valid);
    dut.phy_rx_lanes(ch_phy_rx_lanes);
    dut.phy_rx_valid(ch_phy_rx_valid);
    dut.sb_tx(ch_sb_tx);
    dut.sb_rx(ch_sb_rx);

    drv.clk(clk);
    drv.rst(rst_n);
    drv.rva_in(ch_rva_in);
    drv.rva_out(ch_rva_out);

    loopback.clk(clk);
    loopback.rst_n(rst_n);
    loopback.die0_tx_lanes(ch_phy_tx_lanes);
    loopback.die0_tx_valid(ch_phy_tx_valid);
    loopback.die0_rx_lanes(ch_phy_rx_lanes);
    loopback.die0_rx_valid(ch_phy_rx_valid);
    loopback.die0_sb_tx(ch_sb_tx);
    loopback.die0_sb_rx(ch_sb_rx);

    SC_THREAD(reset_sequence);
  }

  void reset_sequence() {
    wait(2, SC_NS);

    std::cout << "@" << sc_time_stamp()
              << " Assert reset" << std::endl;
    rst_n = 0;

    wait(10, SC_NS);

    std::cout << "@" << sc_time_stamp()
              << " Deassert reset" << std::endl;
    rst_n = 1;
  }

  bool check_results() const {
    const auto& sent = drv.sent_data;
    const auto& received = drv.received_data;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Self-Check Results" << std::endl;
    std::cout << "  TX data words: " << sent.size() << std::endl;
    std::cout << "  RX data words: " << received.size() << std::endl;

    bool pass = drv.test_pass;

    if (sent.size() != received.size()) {
      std::cout << "  FAIL: count mismatch (sent "
                << sent.size() << " received "
                << received.size() << ")" << std::endl;
      pass = false;
    }

    const size_t cmp_len = std::min(sent.size(), received.size());
    int mismatches = 0;

    for (size_t i = 0; i < cmp_len; ++i) {
      if (sent[i] != received[i]) {
        if (mismatches < 10) {
          std::cout << "  FAIL word[" << i << "]: expected "
                    << std::hex << std::setfill('0')
                    << std::setw(16) << sent[i]
                    << " got " << std::setw(16)
                    << received[i]
                    << std::dec << std::setfill(' ') << std::endl;
        }
        mismatches++;
        pass = false;
      }
    }

    if (mismatches > 10)
      std::cout << "  ... (" << mismatches
                << " total mismatches)" << std::endl;

    std::cout << "  Lane words looped: "
              << loopback.lane_word_count << std::endl;
    std::cout << "  SB packets looped: "
              << loopback.sb_pkt_count << std::endl;
    std::cout << "  Overall: "
              << (pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "========================================"
              << std::endl;

    return pass;
  }
};

// -----------------------------------------------------------------------------
// DPI global instance.
//
// SC_MODULE_EXPORT creates the SystemC object inside Questa. We keep a pointer
// to the actual tb instance from an initialization callback.
// -----------------------------------------------------------------------------
static sc_top* g_sc_top = nullptr;

SC_MODULE(tb_export) {
  sc_top tb;

  SC_CTOR(tb_export) : tb("tb") {
    g_sc_top = &tb;
  }

  void end_of_simulation() {
    std::cout << (tb.check_results()
                      ? "\nCMODEL PASS"
                      : "\nCMODEL FAIL")
              << std::endl;
  }
};

static struct TimeResolutionInit {
  TimeResolutionInit() {
    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);
  }
} time_resolution_init;

// -----------------------------------------------------------------------------
// DPI implementation
// -----------------------------------------------------------------------------
extern "C" void ucie_dpi_write(unsigned int addr, uint64_t data) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_start_write(addr, data);
}

extern "C" void ucie_dpi_read(unsigned int addr) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_start_read(addr);
}

extern "C" void ucie_dpi_send_word(uint64_t data) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_start_send_word(data);
}

extern "C" void ucie_dpi_send_frame(uint64_t base_pattern) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_start_send_frame(base_pattern);
}

extern "C" void ucie_dpi_clear_rx(void) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_clear_rx();
}

extern "C" void ucie_dpi_clear_tx(void) {
  if (!g_sc_top) return;
  g_sc_top->drv.dpi_clear_tx();
}

extern "C" int ucie_dpi_done(void) {
  if (!g_sc_top) return 1;
  return g_sc_top->drv.dpi_done ? 1 : 0;
}

extern "C" int ucie_dpi_busy(void) {
  if (!g_sc_top) return 0;
  return g_sc_top->drv.dpi_busy ? 1 : 0;
}

extern "C" int ucie_dpi_error(void) {
  if (!g_sc_top) return 1;
  return g_sc_top->drv.dpi_error ? 1 : 0;
}

extern "C" uint64_t ucie_dpi_result(void) {
  if (!g_sc_top) return 0;
  return g_sc_top->drv.dpi_result;
}

extern "C" uint64_t ucie_dpi_rx_count(void) {
  if (!g_sc_top) return 0;
  return static_cast<uint64_t>(g_sc_top->drv.received_data.size());
}

extern "C" uint64_t ucie_dpi_tx_count(void) {
  if (!g_sc_top) return 0;
  return static_cast<uint64_t>(g_sc_top->drv.sent_data.size());
}

extern "C" uint64_t ucie_dpi_rx_word(unsigned int index) {
  if (!g_sc_top) return 0;
  if (index >= g_sc_top->drv.received_data.size()) return 0;
  return g_sc_top->drv.received_data[index];
}

#ifdef MTI_SYSTEMC
SC_MODULE_EXPORT(tb_export);
#else
int sc_main(int argc, char* argv[]) {
  nvhls::set_random_seed();
  sc_top tb("tb");
  sc_start();

  return tb.check_results() ? 0 : 1;
}
#endif
