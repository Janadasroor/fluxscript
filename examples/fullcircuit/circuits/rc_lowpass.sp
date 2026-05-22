* RC Low-Pass Filter
* R=1k between node 1 and 2
* C=1uF between node 2 and 0
* V1=5V DC between node 1 and 0
* Expected: V(1)=5V, V(2)=5V (cap blocks DC)
R1 1 2 1k
C1 2 0 1u
V1 1 0 DC 5
.op
.end
