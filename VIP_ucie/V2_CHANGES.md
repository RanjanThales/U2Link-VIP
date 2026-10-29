# V2 changes from the original architecture

- SystemC `timeout_guard()` removed.
- SystemC test sequence removed.
- SystemC retains reset, DUT, driver service, RX monitor, and loopback.
- DPI calls enqueue commands; SystemC processes perform all timed channel operations.
- SV owns TEST1..TEST6 and regression completion.
- DPI read/write/send operations have explicit completion/error polling.
- SV DPI wait has a 20 ms default transaction timeout.
