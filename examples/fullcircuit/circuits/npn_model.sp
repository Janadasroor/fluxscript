NPN BJT with .model card (BF=200, different from inline)
.MODEL HIGHBETA NPN(IS=1e-14 BF=200 VAF=50)
Vcc 1 0 5
Rb 1 2 100k
Rc 1 3 1k
Q1 3 2 0 HIGHBETA
