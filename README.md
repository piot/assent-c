# Assent

> To admit a thing as true.

A library for handling Authoritative state, a state that should be agreed by all clients in a deterministic game simulation.

The state is once set, it is only updated and is not rollbacked or resimulated.

It reads Steps from an incoming steps and uses Transmute to simulate a new authoritative State.
