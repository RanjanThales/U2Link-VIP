QUESTA_HOME := /home/Questa2025/questasim

SCCOM := $(QUESTA_HOME)/bin/sccom
VLOG  := $(QUESTA_HOME)/bin/vlog
VSIM  := $(QUESTA_HOME)/bin/vsim
VLIB  := $(QUESTA_HOME)/bin/vlib
VMAP  := $(QUESTA_HOME)/bin/vmap

WORK := work

CPP_FILE := sc_top_dpi.cpp
SV_DPI_PKG  := ucie_dpi_pkg.sv
SV_FILE  := sv_top_dpi.sv

DEFINES := \
  -DHLS_CATAPULT \
  -DHLS_ALGORITHMICC \
  -DSC_INCLUDE_DYNAMIC_PROCESSES \
  -DVECTOR_SIZE=9 \
  -DARRAY_SIZE=6 \
  -DCOSIDE_BUILD

INCDIR := \
  -I. \
  -I$(QUESTA_HOME)/include \
  -I$(QUESTA_HOME)/include/systemc \
  -I$(HOME)/ucie_related/madsim/cmod/include \
  -I$(HOME)/ucie_related/madsim/cmod/include/ac_types/include \
  -I$(HOME)/ucie_related/madsim/cmod/include/ac_simutils/include \
  -I$(HOME)/ucie_related/madsim/cmod/include/matchlib/cmod/include \
  -I$(HOME)/ucie_related/madsim/cmod/include/matchlib_connections/include \
  -I$(HOME)/ucie_related/madsim/cmod/include/Comet/include/catapult \
  -I$(HOME)/ucie_related/madsim/cmod/include/Comet/include/catapult/ccs_p2p


# Default target
all: compile link sv_compile run


# Create Questa work library
lib:
	$(VLIB) $(WORK)
	$(VMAP) work $(WORK)


# Compile SystemC source
compile: lib
	$(SCCOM) -work $(WORK) -std=c++17 $(DEFINES) $(INCDIR) $(CPP_FILE)


# Link SystemC model
link:
	$(SCCOM) -link


# Compile SystemVerilog top
sv_compile:
	$(VLOG) -work $(WORK) $(SV_DPI_PKG)
	$(VLOG) -work $(WORK) $(SV_FILE)


# Run SV top
run:
	$(VSIM) -c SV_TOP -t 1ps -do "run -all; q -f;" -l vsim.log


# Clean
clean:
	rm -rf work transcript vsim.wlf modelsim.ini vsim.log
