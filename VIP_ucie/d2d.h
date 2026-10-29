#ifndef D2D_TX_H
#define D2D_TX_H

#include <systemc.h>
#include <iomanip>
#include <iostream>

#include <nvhls_int.h>
#include <nvhls_connections.h>

#include "SysSpec.h"

SC_MODULE(d2d_tx) {
public:
  sc_in<bool> clk;
  sc_in<bool> rst;

  Connections::In<NVUINT64> data_in;
  Connections::In<bool>     valid_in;

  Connections::In<NVUINT3>  tx_format_in;
  Connections::In<NVUINT8>  tx_config_in;

  Connections::Out<NVUINT8> data_out;
  Connections::Out<bool>    valid_out;

  Connections::Out<NVUINT8> tx_status_out;
  Connections::Out<bool>    clear_pds_out;

  // --------------------------------------------------------------------------
  // Internal state
  // --------------------------------------------------------------------------
  bool    valid_signal;
  bool    valid_format;

  NVUINT3 format;
  NVUINT3 format_cfg;
  NVUINT8 tx_config_cfg;

  NVUINT8 byte_count;

  NVUINT2 protocol_id;
  bool    stack_id;
  bool    pds_requested;

  // Input AXI data storage
  NVUINT64 flit_68b_data[
      madsim::FLIT_68B_DATA_BYTES / madsim::AXI_BYTES];

  // Serialized 68B flit buffer:
  //   Header + Data
  // CRC is calculated separately
  NVUINT8 flit_68b_buffer[
      madsim::FLIT_68B_HEADER_BYTES +
      madsim::FLIT_68B_DATA_BYTES];

  SC_HAS_PROCESS(d2d_tx);

  d2d_tx(sc_module_name name_) : sc_module(name_) {
    SC_THREAD(process_data);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  }

  // ==========================================================================
  // Print complete 68B packet
  // ==========================================================================
  void print_68b_packet(NVUINT16 crc, bool is_pds = false) {

    cout << "\n";
    cout << "============================================================\n";
    cout << sc_time_stamp()
         << " D2D_TX MONITOR : "
         << (is_pds ? "PDS FLIT" : "68B FLIT")
         << "\n";
    cout << "------------------------------------------------------------\n";

    cout << "Protocol ID : "
         << std::dec << (int)protocol_id << "\n";

    cout << "Stack ID    : "
         << (int)stack_id << "\n";

    cout << "PDS         : "
         << (is_pds ? "YES" : "NO") << "\n";

    // ------------------------------------------------------------------------
    // Header
    // ------------------------------------------------------------------------
    cout << "\nHEADER (" << madsim::FLIT_68B_HEADER_BYTES
         << " bytes): ";

    for (int i = 0; i < madsim::FLIT_68B_HEADER_BYTES; i++) {
      cout << std::hex
           << std::setw(2)
           << std::setfill('0')
           << (unsigned int)flit_68b_buffer[i].to_uint()
           << " ";
    }

    // ------------------------------------------------------------------------
    // Payload
    // ------------------------------------------------------------------------
    cout << "\nDATA   (" << madsim::FLIT_68B_DATA_BYTES
         << " bytes): ";

    for (int i = madsim::FLIT_68B_HEADER_BYTES;
         i < madsim::FLIT_68B_HEADER_BYTES +
             madsim::FLIT_68B_DATA_BYTES;
         i++) {

      cout << std::hex
           << std::setw(2)
           << std::setfill('0')
           << (unsigned int)flit_68b_buffer[i].to_uint()
           << " ";
    }

    // ------------------------------------------------------------------------
    // CRC
    // ------------------------------------------------------------------------
    cout << "\nCRC    (" << madsim::FLIT_68B_CRC_BYTES
         << " bytes): ";

    for (int i = 0; i < madsim::FLIT_68B_CRC_BYTES; i++) {

      cout << std::hex
           << std::setw(2)
           << std::setfill('0')
           << (unsigned int)crc.slc<8>(8 * i).to_uint()
           << " ";
    }

    // ------------------------------------------------------------------------
    // Complete serialized packet
    // ------------------------------------------------------------------------
    cout << "\n\nCOMPLETE PACKET: ";

    // Header + payload
    for (int i = 0;
         i < madsim::FLIT_68B_HEADER_BYTES +
             madsim::FLIT_68B_DATA_BYTES;
         i++) {

      cout << std::hex
           << std::setw(2)
           << std::setfill('0')
           << (unsigned int)flit_68b_buffer[i].to_uint()
           << " ";
    }

    // CRC
    for (int i = 0; i < madsim::FLIT_68B_CRC_BYTES; i++) {

      cout << std::hex
           << std::setw(2)
           << std::setfill('0')
           << (unsigned int)crc.slc<8>(8 * i).to_uint()
           << " ";
    }

    cout << "\n";
    cout << "============================================================\n";
    cout << std::dec;
  }

  // ==========================================================================
  // Main TX process
  // ==========================================================================
  void process_data() {

    // ------------------------------------------------------------------------
    // Reset connections
    // ------------------------------------------------------------------------
    data_out.Reset();
    valid_out.Reset();

    data_in.Reset();
    valid_in.Reset();

    tx_format_in.Reset();
    tx_config_in.Reset();

    tx_status_out.Reset();
    clear_pds_out.Reset();

    // ------------------------------------------------------------------------
    // Reset internal state
    // ------------------------------------------------------------------------
    valid_signal  = 0;
    valid_format  = 1;

    format        = NVUINT3(madsim::STREAMING);
    format_cfg    = NVUINT3(madsim::STREAMING);

    tx_config_cfg = 0;

    byte_count    = 0;

    protocol_id   = madsim::PROTOCOL_ID_STREAMING;
    stack_id      = 0;

    pds_requested = false;

    // ------------------------------------------------------------------------
    // Clear buffers
    // ------------------------------------------------------------------------
    for (int i = 0;
         i < madsim::FLIT_68B_DATA_BYTES / madsim::AXI_BYTES;
         i++) {

      flit_68b_data[i] = 0;
    }

    for (int i = 0;
         i < madsim::FLIT_68B_HEADER_BYTES +
             madsim::FLIT_68B_DATA_BYTES;
         i++) {

      flit_68b_buffer[i] = 0;
    }

    // =========================================================================
    // Main loop
    // =========================================================================
    while (1) {

      // ----------------------------------------------------------------------
      // Capture format/configuration
      // ----------------------------------------------------------------------
      NVUINT3 fmt_tmp;
      NVUINT8 cfg_tmp;

      if (tx_format_in.PopNB(fmt_tmp))
        format_cfg = fmt_tmp;

      if (tx_config_in.PopNB(cfg_tmp))
        tx_config_cfg = cfg_tmp;

      // ----------------------------------------------------------------------
      // Between flits: latch configuration
      // ----------------------------------------------------------------------
      if (byte_count == 0) {

        format = format_cfg;
        valid_format = 1;

        // ====================================================================
        // FLIT_68B configuration
        // ====================================================================
        if (format == madsim::FLIT_68B) {

          protocol_id   = tx_config_cfg.slc<2>(0);
          stack_id      = tx_config_cfg[2];
          pds_requested = tx_config_cfg[3];

          // ==================================================================
          // Immediate PDS insertion
          // ==================================================================
          if (pds_requested) {

            cout << sc_time_stamp()
                 << " D2D_TX: Sending PDS flit"
                 << endl;

            // ---------------------------------------------------------------
            // Create PDS header
            // ---------------------------------------------------------------
            NVUINT16 header =
                madsim::make_68b_header(
                    protocol_id,
                    stack_id,
                    true);

            flit_68b_buffer[0] = header.slc<8>(0);
            flit_68b_buffer[1] = header.slc<8>(8);

            // ---------------------------------------------------------------
            // PDS payload = zero
            // ---------------------------------------------------------------
            for (int i = 0;
                 i < madsim::FLIT_68B_DATA_BYTES;
                 i++) {

              flit_68b_buffer[
                  madsim::FLIT_68B_HEADER_BYTES + i] = 0;
            }

            // ---------------------------------------------------------------
            // Calculate CRC
            // ---------------------------------------------------------------
            NVUINT16 crc =
                madsim::compute_ucie_crc(
                    flit_68b_buffer,
                    madsim::FLIT_68B_HEADER_BYTES +
                    madsim::FLIT_68B_DATA_BYTES);

            // ---------------------------------------------------------------
            // Print complete PDS packet
            // ---------------------------------------------------------------
            print_68b_packet(crc, true);

            // ---------------------------------------------------------------
            // Emit header
            // ---------------------------------------------------------------
            for (int i = 0;
                 i < madsim::FLIT_68B_HEADER_BYTES;
                 i++) {

              valid_out.Push(1);
              data_out.Push(flit_68b_buffer[i]);
              wait();
            }

            // ---------------------------------------------------------------
            // Emit zero payload
            // ---------------------------------------------------------------
            for (int i = 0;
                 i < madsim::FLIT_68B_DATA_BYTES;
                 i++) {

              valid_out.Push(1);
              data_out.Push(NVUINT8(0));
              wait();
            }

            // ---------------------------------------------------------------
            // Emit CRC
            // ---------------------------------------------------------------
            for (int i = 0;
                 i < madsim::FLIT_68B_CRC_BYTES;
                 i++) {

              valid_out.Push(1);
              data_out.Push(crc.slc<8>(8 * i));
              wait();
            }

            // ---------------------------------------------------------------
            // TX status
            // ---------------------------------------------------------------
            NVUINT8 status = 0;

            status.set_slc<1>(
                1,
                NVUINT1(1));

            status.set_slc<1>(
                2,
                NVUINT1(1));

            tx_status_out.Push(status);
            clear_pds_out.Push(true);

            // ---------------------------------------------------------------
            // Reset accumulator
            // ---------------------------------------------------------------
            byte_count = 0;

            wait();
            continue;
          }
        }
      }

      // =========================================================================
      // Process selected format
      // =========================================================================
      if (valid_format) {

        switch (format) {

          // =====================================================================
          // STREAMING
          // =====================================================================
          case madsim::STREAMING:

            if (byte_count < madsim::STREAMING_DATA_BYTES) {

              if (valid_in.PopNB(valid_signal) &&
                  valid_signal) {

                NVUINT64 data_in_tmp;

                data_in.PopNB(data_in_tmp);

                // -------------------------------------------------------------
                // Send AXI64 as 8 individual bytes
                // -------------------------------------------------------------
                for (int i = 0;
                     i < madsim::AXI_BYTES;
                     i++) {

                  valid_out.Push(1);

                  data_out.Push(
                      data_in_tmp.slc<8>(8 * i));

                  wait();
                }

                byte_count += madsim::AXI_BYTES;
              }
            }

            // -----------------------------------------------------------------
            // Streaming packet complete
            // -----------------------------------------------------------------
            if (byte_count ==
                madsim::STREAMING_DATA_BYTES) {

              NVUINT8 status = 0;

              status.set_slc<1>(
                  0,
                  NVUINT1(1));

              tx_status_out.Push(status);

              byte_count = 0;
            }

            break;

          // =====================================================================
          // FLIT_68B
          // =====================================================================
          case madsim::FLIT_68B:

            // -----------------------------------------------------------------
            // Accumulate input data
            // -----------------------------------------------------------------
            if (byte_count <
                madsim::FLIT_68B_DATA_BYTES) {

              if (valid_in.PopNB(valid_signal) &&
                  valid_signal) {

                data_in.PopNB(
                    flit_68b_data[byte_count / 8]);

                byte_count += 8;
              }
            }

            // -----------------------------------------------------------------
            // Complete 68B data accumulated
            // -----------------------------------------------------------------
            if (byte_count ==
                madsim::FLIT_68B_DATA_BYTES) {

              cout << sc_time_stamp()
                   << " D2D_TX: Sending 68B flit"
                   << " proto_id="
                   << std::dec
                   << (int)protocol_id
                   << " stack_id="
                   << (int)stack_id
                   << endl;

              // ---------------------------------------------------------------
              // Create header
              // ---------------------------------------------------------------
              NVUINT16 header =
                  madsim::make_68b_header(
                      protocol_id,
                      stack_id,
                      false);

              flit_68b_buffer[0] =
                  header.slc<8>(0);

              flit_68b_buffer[1] =
                  header.slc<8>(8);

              // ---------------------------------------------------------------
              // Convert AXI64 words to bytes
              // ---------------------------------------------------------------
              int buf_idx =
                  madsim::FLIT_68B_HEADER_BYTES;

              for (int i = 0;
                   i < madsim::FLIT_68B_DATA_BYTES /
                       madsim::AXI_BYTES;
                   i++) {

                for (int j = 0;
                     j < madsim::AXI_BYTES;
                     j++) {

                  flit_68b_buffer[buf_idx++] =
                      flit_68b_data[i].slc<8>(8 * j);
                }
              }

              // ---------------------------------------------------------------
              // Calculate CRC
              // ---------------------------------------------------------------
              NVUINT16 crc =
                  madsim::compute_ucie_crc(
                      flit_68b_buffer,
                      madsim::FLIT_68B_HEADER_BYTES +
                      madsim::FLIT_68B_DATA_BYTES);

              // ---------------------------------------------------------------
              // PRINT COMPLETE PACKET
              // ---------------------------------------------------------------
              print_68b_packet(crc, false);

              // ---------------------------------------------------------------
              // Emit header
              // ---------------------------------------------------------------
              for (int i = 0;
                   i < madsim::FLIT_68B_HEADER_BYTES;
                   i++) {

                valid_out.Push(1);
                data_out.Push(
                    flit_68b_buffer[i]);

                wait();
              }

              // ---------------------------------------------------------------
              // Emit data
              // ---------------------------------------------------------------
              for (int i =
                       madsim::FLIT_68B_HEADER_BYTES;

                   i <
                       madsim::FLIT_68B_HEADER_BYTES +
                       madsim::FLIT_68B_DATA_BYTES;

                   i++) {

                valid_out.Push(1);
                data_out.Push(
                    flit_68b_buffer[i]);

                wait();
              }

              // ---------------------------------------------------------------
              // Emit CRC
              // ---------------------------------------------------------------
              for (int i = 0;
                   i < madsim::FLIT_68B_CRC_BYTES;
                   i++) {

                valid_out.Push(1);

                data_out.Push(
                    crc.slc<8>(8 * i));

                wait();
              }

              // ---------------------------------------------------------------
              // TX status
              // ---------------------------------------------------------------
              NVUINT8 status = 0;

              status.set_slc<1>(
                  0,
                  NVUINT1(1));

              status.set_slc<1>(
                  2,
                  NVUINT1(1));

              tx_status_out.Push(status);

              // ---------------------------------------------------------------
              // Reset accumulator
              // ---------------------------------------------------------------
              byte_count = 0;

              for (int i = 0;
                   i < madsim::FLIT_68B_DATA_BYTES /
                       madsim::AXI_BYTES;
                   i++) {

                flit_68b_data[i] = 0;
              }

              for (int i = 0;
                   i < madsim::FLIT_68B_HEADER_BYTES +
                       madsim::FLIT_68B_DATA_BYTES;
                   i++) {

                flit_68b_buffer[i] = 0;
              }

              break;
            }

            break;

          default:
            break;
        }
      }

      wait();
    }
  }
};

#endif
