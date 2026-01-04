# Communication in Distributed Architecture
Design and implementation of data transfer and network communication solutions targeted at the requirements of simulation tools. The course 
will be broken into three parts: 
1. Review of current message passing protocols and implementations, such as `MPI`
2. An implementation appropriate for `Stage2`;
3. A demonstration of the solution's capabilities within `Stage2`, including performance characterization. 


## Course Weighting
The primary course outcome is to present a software design for a network-based communication system that includes good practice in documentation, testing, and presentation of results. 
Considering this and the quality of the work, the grades for the course will be weighted as follows: 
- 40% Documenting the communication design within Stage 2 including within its repositories.
- 30% Preparation and documentation of a working example of the communication system.
- 20% Thirty-minute presentation of the system’s design and integration within Stage 2 .
- 10% Timely updates on progress via regular meetings.

## Resources
Message-passing interfaces and protocols used in scientific software: 
- [https://www.open-mpi.org/ ](open-mpi.org/) - Industry-standard implementation of the message-passing interface (MPI) protocol
- [https://www.openmp.org/wp-content/uploads/OpenMP-API-Specification-6-0.pdf](OpenMP-API-Specification-6-0) - Heterogeneous computing API

Tools:
- [https://kitware.github.io/paraview-catalyst/guide/concepts.html](Pareview) - open-source software framework for simulations with in situ visualization and analysis capabilities
- [https://kernel.dk/io_uring.pdf](IOuring) - Efficient IO with io_uring
- [https://github.com/schlad/liburing](liburing) - Library providing helpers for the Linux kernel io_uring support
