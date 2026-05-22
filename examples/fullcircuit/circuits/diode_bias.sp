* Diode Bias Circuit
* R=1k between node 1 and 2
* D1 between node 2 and 0 (IS=1e-14, N=1, VT=25.85mV)
* V1=5V DC between node 1 and 0
R1 1 2 1k
D1 2 0 diode_model
V1 1 0 DC 5
.op
.end
