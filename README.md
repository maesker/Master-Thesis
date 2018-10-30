# Master-Thesis

Title: Overcoming write penalties of conflicting client operations in distributed storage systems


Basically, it's a redundant, lock-free distributed RAID-4/5 controller 


Problem: Erasure coding in distributed environments:
<img src="docs/stripelock.png" align="center" />

Solution: A P2P Cluster with distributed, redundant, multi-version state management and a lock-free commit protocol

<img src="docs/distributedraid.png" align="center" />

<img src="docs/commitprot0.png" align="center" />
<img src="docs/commitprot1.png" align="center" />
<img src="docs/commitprot2.png" align="center" />
<img src="docs/commitprot3.png" align="center" />



<img src="docs/eval_fig30.png" align="center" />

<img src="docs/eval_fig31.png" align="center" />

<img src="docs/eval_fig32.png" align="center" />

<img src="docs/eval_fig33.png" align="center" />


