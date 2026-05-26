NJF CS Amplifier
Vdd 1 0 DC 5
Rd 1 2 1k
J1 2 3 0 NJF1
.model NJF1 NJF(BETA=1e-3 VTO=-2)
Vgg 3 0 DC 0
.DC Vgg -5 0 0.1
