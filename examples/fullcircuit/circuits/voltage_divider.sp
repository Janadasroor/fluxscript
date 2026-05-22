* Voltage Divider
* R1=1k between node 1 and 2
* R2=2k between node 2 and 0
* V1=5V DC between node 1 and 0
* Expected: V(1)=5V, V(2)=3.333V
R1 1 2 1k
R2 2 0 2k
V1 1 0 DC 5
.op
.end
