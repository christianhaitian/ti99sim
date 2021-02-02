# Build for rg351p using arm64 Ubuntu environment:

Prerequisites:
 - Either: g++ v4.8 or newer, or clang++ v3.4 or newer
 - libsdl2-dev
 - libssl-dev
 - libboost-regex-dev
 - libssl-ocaml-dev

```bash
git clone https://github.com/christianhaitian/ti99sim.git
cd ti99sim
make 
```

Default controls
A = Fire
B = keyboard key 1
X = keyboard key 2
Y = keyboard key 3
L1 = FCTN+QUIT
R1 = FCTN+REDO
L2 = keyboard key Space
R2 = keyboard key 0
L3 = FCTN+BACK
R3 = FCTN+QUIT
Start = keyboard key Enter

See https://www.mrousseau.org/programs/ti99sim/README.html for more info on usage and controls
