# U2Link-VIP

The motivation for U2Link-VIP originates from the MADSIM project developed at the Tambe Lab,
Stanford University (hosted at code.stanford.edu/tambe-lab/madsim). As part of the groundwork for
this project, the MADSIM repository is successfully replicated and its simulation environment is crosschecked. 

This exercise provided key insights into the UCIe protocol stack, specifically the three-layer hierarchy of PHY, D2D Adapter, and Protocol Layer (PL). 
I have found the feasibility of driving these layers from a structured UVM test bench and integrating systemC based DUT through DPI call.
The UVM testbench architecture is developed as part of this replication effort (shown in Figure)
comprises three dedicated UVM agents:
PL_AGENT: Drives and monitors the Protocol Layer with a sequencer, driver, and monitor,
alongside protocol-level functional coverage collection.
D2D_AGENT: Handles the Die-to-Die Adapter layer transactions.
PHY_AGENT: Interfaces with the physical layer signals including phy_tx_lanes,
phy_rx_lanes, phy_tx_valid, phy_rx_valid, and sb_rx.
The DUT (Die_0) is wrapped within a (dpl_bridge.cpp) model and interfaced via SystemVerilog DPI.
This supports parallel developments for design and the UVM verification environment. The Loopback
block at the DUT level enables self-checking by feeding transmitted data back to the receiver. This
block is intended to observe results propagated to the Scoreboard for end-to-end transaction
checking.

<img width="1031" height="576" alt="image" src="https://github.com/user-attachments/assets/581debbd-e8ac-408f-b9e5-fde818ce9950" />
#UVM test bench architecture for madsim/cmod repo
