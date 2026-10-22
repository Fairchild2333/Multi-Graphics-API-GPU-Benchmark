# Kbuild file for the Phytium GMAC glue, built out of tree by DKMS
# (install-phytium-gmac.sh). stmmac/ holds the stmmac driver's internal
# headers (stmmac.h, stmmac_platform.h and what they include), taken from the
# Ubuntu kernel source, as no headers package ships them.
obj-m := dwmac-phytium.o
ccflags-y := -I$(src)/stmmac
