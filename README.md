# About
Flow-based network measurement enables operators to perform a wide range of network management tasks in a scalable manner. Recently, various algorithms have been proposed for flow record collection at very high speed. Specifically, sketch (i.e., some succinct data structure) based algorithms have been proposed for efficient tracking and accurate estimation of specific traffic statistics such as the total number of flows and the flow size distribution.

In this repository, we implemented five sketch based algorithm and compared their throughput in PC.
* **SpaceSaving** ([Efficient Computation of Frequent and Top-k
Elements in Data Streams](http://www.cse.ust.hk/~raywong/comp5331/References/EfficientComputationOfFrequentAndTop-kElementsInDataStreams.pdf),  *Ahmed Metwally, Divyakant Agrawal, and Amr El Abbadi*, ICDT 2005.)
* **HashPipe** ([Heavy-Hitter Detection Entirely in the Data Plane](https://www.cs.princeton.edu/~jrex/papers/hashpipe17.pdf), *Vibhaalakshmi Sivaraman, Srinivas Narayana, Ori Rottenstreich, S. Muthukrishnan, Jennifer Rexford*, SOSR 2017)
* **PRECISION** ([Efficient Measurement on Programmable Switches Using Probabilistic Recirculation](https://ieeexplore.ieee.org/document/8526835), *Ran Ben Basat, Xiaoqi Chen, Gil Einziger, Ori Rottenstreich*, ICNP 2018)
* **Elastic** ([Elastic Sketch: Adaptive and Fast Network-wide Measurements](https://conferences.sigcomm.org/events/apnet2018/papers/elastic_sketch.pdf), *T. Yang, J. Jiang, P. Liu, Q. Huang, J. Gong, Y. Zhou, R. Miao, X. Li, and S. Uhlig*, SIGCOMM 2018)
  * Note: In order to control variable, We did a little change to Elastic algorithm and made it more effective, so we named it AElastic(Advanced Elastic).
* **SuperFlow** (Continuous Flow Measurement with SuperFlow, *Zhao Zongyi, Xingang Shi, Arpit Gupta, Qing Li, Zhiliang Wang, Bin Xiong, Xia Yin*, Posting)