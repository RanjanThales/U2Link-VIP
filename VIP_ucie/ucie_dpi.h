#ifndef UCIE_DPI_H
#define UCIE_DPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  UCIE_DPI_WRITE = 1,
  UCIE_DPI_READ = 2,
  UCIE_DPI_SEND_WORD = 3,
  UCIE_DPI_SEND_FRAME = 4,
  UCIE_DPI_CLEAR_RX = 5,
  UCIE_DPI_CLEAR_TX = 6
};

void     ucie_dpi_write(unsigned int addr, uint64_t data);
void     ucie_dpi_read(unsigned int addr);
void     ucie_dpi_send_word(uint64_t data);
void     ucie_dpi_send_frame(uint64_t base_pattern);
void     ucie_dpi_clear_rx(void);
void     ucie_dpi_clear_tx(void);

int      ucie_dpi_done(void);
int      ucie_dpi_busy(void);
int      ucie_dpi_error(void);
uint64_t ucie_dpi_result(void);

uint64_t ucie_dpi_rx_count(void);
uint64_t ucie_dpi_tx_count(void);
uint64_t ucie_dpi_rx_word(unsigned int index);

#ifdef __cplusplus
}
#endif

#endif
