NMOS switch with .model card
.MODEL MYNMOS NMOS(KP=1e-4 VTO=1.0 LAMBDA=0.01)
Vdd 1 0 5
Rd 1 3 10k
Rg 1 2 10k
M1 3 2 0 0 MYNMOS
