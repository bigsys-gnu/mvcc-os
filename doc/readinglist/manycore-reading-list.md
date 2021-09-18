# Many-core scability reading list

## References for concurrency
- [The Art of Multiprocessor Programming](https://booksite.elsevier.com/9780123705914/?ISBN=9780123705914)
	- AMP for short
	- Lecture slides are available from the website
- [The First Summer School on Practice and Theory of Concurrent
  Computing](http://neerc.ifmo.ru/sptcc/index.html)
    - SPTCC for short
	- Lecture slides and videos are available from the website

## References for programming
- [Linux kernel development](https://www.amazon.com/Linux-Kernel-Development-Robert-Love/dp/0672329468)
- [x86 Assembly Primer for C Programmers](https://github.com/vsergeev/apfcp/blob/master/apfcp.pdf)

## Reading list
1. Overview of manycore, NUMA optimization
	- [Tiny Little Things for Manycore Scalability: Scalable Lockin
      and Lockless Data Structures](https://sites.google.com/site/multics69/full-list-of-publications/Min-TLT-ETRITalk14-slide.ppsx?attredirects=0)

1. Locking overview
	- [AMP] Chapter 7. Spin Locks and Contention
	- [SPTCC] Locking, from traditional to modern:
      [Video 1](https://www.youtube.com/watch?v=FsXB-tKUeUw),
	  [Video 2](https://www.youtube.com/watch?v=RPVKu4UtpWo),
      [Slides](http://neerc.ifmo.ru/sptcc/slides/slides-shavit.pdf)

1. Pseudo Code of Spinlock Algorithms
	- [Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors](https://www.cs.rochester.edu/research/synchronization/pseudocode/ss.html)
    - See following algorithms
		- Simple test_and_set lock with exponential backoff
		- Ticket lock with proportional backoff
		- The MCS list-based queue lock
		- The CLH list-based queue lock

1. Practical Impact of Non-Scalble Spinlock
	- [Non-scalable locks are dangerous](https://people.csail.mit.edu/nickolai/papers/boyd-wickizer-locks.pdf)
	- [Scalability Techniques for Practical Synchronization Primitives](http://dl.acm.org/citation.cfm?id=2687882)

1. Numa-Aware Locking
	- [Lock cohorting: a general technique for designing NUMA locks](http://dl.acm.org/citation.cfm?id=2145848)

1. Lock implementations
	- [LiTL](https://github.com/multicore-locks/litl)

1. Lock-Free Data Structures
	- [AMP] Chapter 10.5 An Unbounded Lock-Free Queue
    - [Simple, Fast, and Practical Non-Blocking and Blocking Concurrent
       Queue Algorithms](http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf)
	- [SPTCC] Lock-free concurrent data structures:
	  [Slides](http://neerc.ifmo.ru/sptcc/slides/slides-hendler.pdf),
	  [Video 1](https://www.youtube.com/watch?v=DdAV7891-OA),
	  [Video 2](https://www.youtube.com/watch?v=LHjBMFRFtt4),
	  [Video 3](https://www.youtube.com/watch?v=an6YheTOHJg),
	  [Video 4](https://www.youtube.com/watch?v=XjyE5EqCAsI)

1. Memory Reclamation
	- [Structured Deferral: Synchronization via
      Procrastination](http://queue.acm.org/detail.cfm?id=2488549)
	- [AMP] Chapter 10.6 Memory Reclamation and the ABA Problem
	- [SPTCC] Memory management for concurrent data structures:
	  [Video 1](https://www.youtube.com/watch?v=aedEe0Zx_g0),
	  [Video 2](https://www.youtube.com/watch?v=BCXrG1M65HU)

1. Lock-Free Case Study
	- [Nonblocking Algorithms and Scalable Multicore
      Programming](http://queue.acm.org/detail.cfm?id=2492433)

1. Read-Copy-Update
	- What is RCU?:
	  [Slides](http://www.rdrop.com/~paulmck/RCU/RCU.IISc-Bangalore.2013.06.03a.pdf),
	  [Video](https://www.youtube.com/watch?v=obDzjElRj9c)

1. Combining and Delegation Approach
	- [Revisiting the combining synchronization technique](http://dl.acm.org/citation.cfm?id=2145849)
	- [ffwd: delegation is (much) faster than you think](https://www.sigops.org/sosp/sosp17/program.html)

1. Introduction to transactional memory
	- [Transactional Memory](https://www.youtube.com/watch?v=Zv4Zdsp6vF0)
	- [Transactional locking II](http://dl.acm.org/citation.cfm?id=2136065)

1. Advanced Software Transactional Memory
	- [SPTCC] Transactional Memory:
	  [Slides](http://neerc.ifmo.ru/sptcc/slides/slides-herlihy.pdf),
	  [Video 1](https://www.youtube.com/watch?v=ZkUrl8BZHjk),
	  [Video 2](https://www.youtube.com/watch?v=FhgkyhXSDm0),
	  [Video 3](https://www.youtube.com/watch?v=VH5pTjK54Q4),
      [Video 4](https://www.youtube.com/watch?v=3TfCXEtDKvk)

1. More Advanced Software Transactional Memory
	- [SPTCC] Implementation techniques for libraries of transactional
      concurrent data types:
	  [Slides](http://neerc.ifmo.ru/sptcc/slides/slides-shrira.pdf),
	  [Video 1](https://www.youtube.com/watch?v=SJk7Nc2X9ew),
	  [Video 2](https://www.youtube.com/watch?v=Z_lHken3xog&t=14s)

1. Read-Log-Update
	- [Read-log-update: a lightweight synchronization mechanism for
      concurrent programming](https://dl.acm.org/citation.cfm?id=2815406)

1. Scalability meets persistency
	- [DudeTM: Building Durable Transactions with Decoupling for Persistent Memory](https://www.microsoft.com/en-us/research/publication/dudetm-building-durable-transactions-decoupling-persistent-memory/)
