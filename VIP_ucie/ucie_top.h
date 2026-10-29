#ifndef __UCIE_TOP_H__
#define __UCIE_TOP_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>

#include "../d2d/d2d.h"
#include "../phy_top/phy_top.h"
#include "../protocol_layer/protocol_layer.h"

SC_MODULE(ucie_top) {
  // ========== External Interfaces ==========
  
  // Clock and Reset
  sc_in<bool> clk;
  sc_in<bool> rst;      // Active-low reset (legacy, unused)
  sc_in<bool> rst_n;    // Active-low reset (unified for D2D and PHY)
  
  // SoC-side AXI interface
  Connections::In<spec::Axi::SlaveToRVA::Write> rva_in;
  Connections::Out<spec::Axi::SlaveToRVA::Read> rva_out;
  
  // Physical lane interfaces
  Connections::Out<LaneWord>  phy_tx_lanes;
  Connections::Out<NVUINT16>  phy_tx_valid;
  Connections::In<LaneWord>   phy_rx_lanes;
  Connections::In<NVUINT16>   phy_rx_valid;
  
  // Sideband interface
  Connections::Out<SbPkt> sb_tx;
  Connections::In<SbPkt>  sb_rx;
  
  // ========== Sub-modules ==========
  
  d2d_top d2d;
  protocol_layer pl;
  phy_top phy;
  
  // ========== Internal Channels ==========
  
  Connections::Combinational<NVUINT8> ch_d2d_phy_tx_byte;
  Connections::Combinational<bool>    ch_d2d_phy_tx_valid;
  Connections::Combinational<NVUINT8> ch_phy_d2d_rx_byte;
  Connections::Combinational<bool>    ch_phy_d2d_rx_valid;
  
  Connections::Combinational<spec::Axi::SlaveToRVA::Write> ch_rva_in_local;
  Connections::Combinational<spec::Axi::SlaveToRVA::Read>  ch_rva_out_local;

  Connections::Combinational<NVUINT64> ch_pl_d2d_tx_data;
  Connections::Combinational<bool>     ch_pl_d2d_tx_valid;
  Connections::Combinational<NVUINT64> ch_d2d_pl_rx_data;
  Connections::Combinational<bool>     ch_d2d_pl_rx_valid;
  Connections::Combinational<bool>     ch_pl_d2d_tx_req;
  Connections::Combinational<bool>     ch_pl_d2d_rx_req;
  Connections::Combinational<bool>     ch_d2d_pl_tx_go;
  Connections::Combinational<bool>     ch_d2d_pl_rx_go;
  Connections::Combinational<NVUINT3>  ch_pl_d2d_tx_format;
  Connections::Combinational<NVUINT3>  ch_pl_d2d_rx_format;
  Connections::Combinational<NVUINT8>  ch_pl_d2d_tx_config;
  Connections::Combinational<NVUINT8>  ch_d2d_pl_tx_status;
  Connections::Combinational<bool>     ch_d2d_pl_tx_clear_pds;
  Connections::Combinational<NVUINT8>  ch_d2d_pl_rx_status;
  Connections::Combinational<bool>     ch_phy_d2d_afe_link_done;
  Connections::Combinational<bool>     ch_d2d_pl_tx_enabled;
  Connections::Combinational<bool>     ch_d2d_pl_rx_enabled;

  Connections::Combinational<NVUINT8>  ch_pl_phy_speed_cap;
  Connections::Combinational<NVUINT8>  ch_pl_phy_width_cap;
  Connections::Combinational<bool>     ch_phy_pl_scrambling;
  Connections::Combinational<UINT8>    ch_phy_pl_ltsm_state;
  Connections::Combinational<UINT8>    ch_phy_pl_rdi_state;
  Connections::Combinational<UINT8>    ch_phy_pl_fsm_debug;
  Connections::Combinational<UINT8>    ch_phy_pl_active_speed;
  Connections::Combinational<bool>     ch_phy_pl_pm_state;
  Connections::Combinational<bool>     ch_phy_pl_afe_link_done;
  
  Connections::Combinational<UINT4>  ch_lp_state_req;
  Connections::Combinational<bool>   ch_lp_linkerror;
  Connections::Combinational<UINT4>  ch_pl_state_sts;
  Connections::Combinational<UINT8>  ch_pl_training_sts;
  Connections::Combinational<bool>   ch_pl_inband_pres;
  Connections::Combinational<bool>   ch_pl_error;
  Connections::Combinational<bool>   ch_lp_flit_format_negotiated;
  Connections::Combinational<bool>   ch_internal_recoverable_error;
  Connections::Combinational<bool>   ch_internal_correctable_error;
  Connections::Combinational<bool>   ch_internal_nonfatal_error;
  
  Connections::Combinational<bool> ch_lp_stallack;
  Connections::Combinational<bool> ch_lp_clk_ack;
  Connections::Combinational<bool> ch_lp_wake_req;
  Connections::Combinational<bool> ch_pl_stallreq;
  Connections::Combinational<bool> ch_pl_clk_req;
  Connections::Combinational<bool> ch_pl_wake_ack;
  Connections::Combinational<bool> ch_pl_phyinrecenter;
  Connections::Combinational<bool> ch_pl_trdy;
  Connections::Combinational<bool> ch_pl_cerror;
  Connections::Combinational<bool> ch_pl_nferror;
  Connections::Combinational<bool> ch_pl_trainerror;
  Connections::Combinational<bool> ch_pl_mb_en;
  Connections::Combinational<bool> ch_pl_sb_en;
  
  SC_HAS_PROCESS(ucie_top);
  ucie_top(sc_module_name n)
    : sc_module(n)
    , d2d("d2d")
    , pl("pl")
    , phy("phy")
  {
    d2d.clk(clk); d2d.rst(rst_n);
    pl.clk(clk);  pl.rst(rst_n);
    phy.clk(clk); phy.rst_n(rst_n);

    pl.rva_in(ch_rva_in_local);
    pl.rva_out(ch_rva_out_local);
    phy.phy_tx_lanes_word(phy_tx_lanes);
    phy.phy_tx_lanes_valid(phy_tx_valid);
    phy.phy_rx_lanes_word(phy_rx_lanes);
    phy.phy_rx_lanes_valid(phy_rx_valid);
    phy.sb_tx(sb_tx);
    phy.sb_rx(sb_rx);
    
    pl.data_out(ch_pl_d2d_tx_data);
    pl.valid_out(ch_pl_d2d_tx_valid);
    pl.data_in(ch_d2d_pl_rx_data);
    pl.valid_in(ch_d2d_pl_rx_valid);
    pl.tx_req(ch_pl_d2d_tx_req);
    pl.rx_req(ch_pl_d2d_rx_req);
    pl.tx_go(ch_d2d_pl_tx_go);
    pl.rx_go(ch_d2d_pl_rx_go);
    pl.tx_format_out(ch_pl_d2d_tx_format);
    pl.rx_format_out(ch_pl_d2d_rx_format);
    pl.tx_config_out(ch_pl_d2d_tx_config);
    pl.tx_status_in(ch_d2d_pl_tx_status);
    pl.tx_clear_pds_in(ch_d2d_pl_tx_clear_pds);
    pl.rx_status_in(ch_d2d_pl_rx_status);
    pl.tx_enabled_in(ch_d2d_pl_tx_enabled);
    pl.rx_enabled_in(ch_d2d_pl_rx_enabled);
    pl.speed_cap_out(ch_pl_phy_speed_cap);
    pl.width_cap_out(ch_pl_phy_width_cap);
    pl.phy_scrambling_in(ch_phy_pl_scrambling);
    pl.phy_ltsm_state_in(ch_phy_pl_ltsm_state);
    pl.phy_rdi_state_in(ch_phy_pl_rdi_state);
    pl.phy_fsm_debug_in(ch_phy_pl_fsm_debug);
    pl.phy_active_speed_in(ch_phy_pl_active_speed);
    pl.phy_pm_state_in(ch_phy_pl_pm_state);
    pl.phy_afe_link_done_in(ch_phy_pl_afe_link_done);

    d2d.data_in(ch_pl_d2d_tx_data);
    d2d.valid_in(ch_pl_d2d_tx_valid);
    d2d.data_out(ch_d2d_pl_rx_data);
    d2d.valid_out(ch_d2d_pl_rx_valid);
    d2d.tx_req(ch_pl_d2d_tx_req);
    d2d.rx_req(ch_pl_d2d_rx_req);
    d2d.tx_go(ch_d2d_pl_tx_go);
    d2d.rx_go(ch_d2d_pl_rx_go);
    d2d.tx_format_in(ch_pl_d2d_tx_format);
    d2d.rx_format_in(ch_pl_d2d_rx_format);
    d2d.tx_config_in(ch_pl_d2d_tx_config);
    d2d.tx_status_out(ch_d2d_pl_tx_status);
    d2d.tx_clear_pds_out(ch_d2d_pl_tx_clear_pds);
    d2d.rx_status_out(ch_d2d_pl_rx_status);
    d2d.afe_link_done(ch_phy_d2d_afe_link_done);
    d2d.tx_enabled_out(ch_d2d_pl_tx_enabled);
    d2d.rx_enabled_out(ch_d2d_pl_rx_enabled);

    phy.cfg_speed_cap_in(ch_pl_phy_speed_cap);
    phy.cfg_width_cap_in(ch_pl_phy_width_cap);
    phy.status_scrambling(ch_phy_pl_scrambling);
    phy.status_ltsm_state(ch_phy_pl_ltsm_state);
    phy.status_rdi_state(ch_phy_pl_rdi_state);
    phy.status_fsm_debug(ch_phy_pl_fsm_debug);
    phy.status_active_speed(ch_phy_pl_active_speed);
    phy.status_pm_state(ch_phy_pl_pm_state);
    phy.status_afe_link_done(ch_phy_pl_afe_link_done);
    phy.afe_link_done_to_d2d(ch_phy_d2d_afe_link_done);

    d2d.ser_byte_out(ch_d2d_phy_tx_byte);
    d2d.ser_valid_out(ch_d2d_phy_tx_valid);
    phy.d2d_tx_byte(ch_d2d_phy_tx_byte);
    phy.d2d_tx_valid(ch_d2d_phy_tx_valid);
    
    phy.d2d_rx_byte(ch_phy_d2d_rx_byte);
    phy.d2d_rx_valid(ch_phy_d2d_rx_valid);
    d2d.ser_byte_in(ch_phy_d2d_rx_byte);
    d2d.ser_valid_in(ch_phy_d2d_rx_valid);

    pl.lp_state_req(ch_lp_state_req);
    pl.pl_state_sts(ch_pl_state_sts);
    pl.pl_training_sts(ch_pl_training_sts);
    pl.pl_error(ch_pl_error);
    pl.lp_linkerror(ch_lp_linkerror);
    pl.lp_flit_format_negotiated(ch_lp_flit_format_negotiated);
    pl.lp_stallack(ch_lp_stallack);
    pl.lp_clk_ack(ch_lp_clk_ack);
    pl.lp_wake_req(ch_lp_wake_req);
    pl.internal_recoverable_error(ch_internal_recoverable_error);
    pl.internal_correctable_error(ch_internal_correctable_error);
    pl.internal_nonfatal_error(ch_internal_nonfatal_error);
    pl.pl_stallreq(ch_pl_stallreq);
    pl.pl_clk_req(ch_pl_clk_req);
    pl.pl_wake_ack(ch_pl_wake_ack);
    pl.pl_phyinrecenter(ch_pl_phyinrecenter);
    pl.pl_trdy(ch_pl_trdy);
    pl.pl_cerror(ch_pl_cerror);
    pl.pl_nferror(ch_pl_nferror);
    pl.pl_trainerror(ch_pl_trainerror);
    pl.pl_mb_en(ch_pl_mb_en);
    pl.pl_sb_en(ch_pl_sb_en);
    pl.pl_inband_pres(ch_pl_inband_pres);

    phy.lp_state_req(ch_lp_state_req);
    phy.lp_linkerror(ch_lp_linkerror);
    phy.pl_state_sts(ch_pl_state_sts);
    phy.pl_training_sts(ch_pl_training_sts);
    phy.pl_inband_pres(ch_pl_inband_pres);
    phy.pl_error(ch_pl_error);
    phy.lp_flit_format_negotiated(ch_lp_flit_format_negotiated);

    phy.lp_stallack(ch_lp_stallack);
    phy.lp_clk_ack(ch_lp_clk_ack);
    phy.lp_wake_req(ch_lp_wake_req);
    phy.pl_stallreq(ch_pl_stallreq);
    phy.pl_clk_req(ch_pl_clk_req);
    phy.pl_wake_ack(ch_pl_wake_ack);
    phy.pl_phyinrecenter(ch_pl_phyinrecenter);
    phy.pl_trdy(ch_pl_trdy);
    phy.pl_cerror(ch_pl_cerror);
    phy.pl_nferror(ch_pl_nferror);
    phy.pl_trainerror(ch_pl_trainerror);
    phy.pl_mb_en(ch_pl_mb_en);
    phy.pl_sb_en(ch_pl_sb_en);
    phy.internal_recoverable_error(ch_internal_recoverable_error);
    phy.internal_correctable_error(ch_internal_correctable_error);
    phy.internal_nonfatal_error(ch_internal_nonfatal_error);
    
    SC_THREAD(rva_router);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst_n);

    SC_THREAD(rva_response_mux);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst_n);
  }
  
  void rva_router() {
    rva_in.Reset();
    ch_rva_in_local.ResetWrite();
    wait();
    while (true) {
      spec::Axi::SlaveToRVA::Write w;
      if (!rva_in.Empty() && rva_in.PopNB(w)) {
        ch_rva_in_local.Push(w);
      }
      wait();
    }
  }

  void rva_response_mux() {
    rva_out.Reset();
    ch_rva_out_local.ResetRead();
    wait();
    while (true) {
      spec::Axi::SlaveToRVA::Read rd;
      if (ch_rva_out_local.PopNB(rd)) {
        rva_out.Push(rd);
      }
      wait();
    }
  }
};

#endif // __UCIE_TOP_H__
