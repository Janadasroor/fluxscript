* Series RLC Circuit
* R=1k between 1 and 2
* L=1mH between 2 and 3
* C=1uF between 3 and 0
* V1=5V DC between 1 and 0
* Expected: V(2)=5V, V(3)=5V (L short, C open)
R1 1 2 1k
L1 2 3 1m
C1 3 0 1u
V1 1 0 DC 5
.op
.end
