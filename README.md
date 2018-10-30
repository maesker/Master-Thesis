# Master-Thesis

Title: Overcoming write penalties of conflicting client operations in distributed storage systems

Basically, it's a redundant, lock-free distributed RAID-4/5 controller 

For details please refer to the presentation or thesis. 

# Problem:
Erasure coding in distributed environments:
<img src="docs/stripelock.png" align="center" width="600" />


# P2P Cluster: 

Solution: A P2P Cluster with distributed, redundant, multi-version state management and a lock-free commit protocol

<img src="docs/distributedraid.png" align="center" width="600" />

<img src="docs/commitprot0.png" align="center" width="600" />
<img src="docs/commitprot1.png" align="center" width="600" />
<img src="docs/commitprot2.png" align="center" width="600" />
<img src="docs/commitprot3.png" align="center" width="600" />


# Evaluation 

The system has been evaluated at the Grid5000 Clusters Suno, Helios, Griffon, Granduc and Luxembourg
The graph show that my lock-free cluster saturate the hardware by over 90%.


<img src="docs/eval_fig30.png" align="center" width="600" />

<img src="docs/eval_fig31.png" align="center" width="600" />

<img src="docs/eval_fig32.png" align="center" width="600" />

<img src="docs/eval_fig33.png" align="center" width="600" />

