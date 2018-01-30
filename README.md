
GPAClib
=======

What is GPAClib
--------------

GPAClib is a header-only library implemented in `C++` for defining and simulating analog circuits (GPAC = General Purpose Analog Computer). The library comes with a parser that allows to define circuit in a format that is specifed below.

Prerequisites
------------
In order to use GPAClib, you need to have:
  - a compiler with support for C++14
  - Boost library (more specifically: System, Program Options, Iostreams, Odeint and Spirit)

If you want to compile the example code provided, you will need CMake version >= 3.1.

How to use
----------

Since GPAClib is header-only, the usage is very simple: one just has to include the main header file `GPAC.hpp`. The header file `GPACparser.hpp` provides the tool to load a circuit from a specification file. A simple example of usage of GPAClib (loading a circuit and simulating it) is given in file `GPACsim.cpp`, which can be compiled by entering the following command while being in the main directory:

    mkdir -p build
	cd build
	cmake ..
	make
	
It creates a program called `GPACsim` that takes a specification file name as argument and simulates the corresponding circuit. Execute `GPACsim --help` for more information about the options.

You can generate the documentation of GPAClib using Doxygen: 

	cd doc
	doxygen

The documentation is generated in subdirectories `html` (open `index.html` with your favorite web browser) and `latex`.

Circuit specification format
----------------------------

The first way of defining a circuit is by specifying its gates:

    Circuit <circuit_name>:
	    <gate_name>: ....
	    <gate_name>: ....
	    <gate_name>: ....
	;

Circuit names cannot contain spaces and should not be named as builtin circuits provided in the following. 
Moreover, `t` is a reserved name and circuit names should not be confusable with floating point numbers.

The gate specification is the following:
   
  - for constant gate: `<value>`
  - for product gates: `<operand1> * <operand2>`
  - for addition gate: `<operand1> + <operand2>`
  - for integration gates:  `int <integrated> d(<variable>) | <initial_value>`
  
Operands are names of gates defined (before or after) in the same circuit or `t`. Gate names cannot be empty, cannot start with underscore and `t` is reserved. A value is either an integer or a floating point number, it can be negative. The last gate entered is the *output gate*.

The second way to define a circuit is by combination of previously defined circuits:

    Circuit <circuit_name> = <expression>;

where expressions are defined with the following grammar:

    expr ::= value | identifier | (expr + expr) | (expr * expr) | (expr @ expr)

The `@` operator corresponds to composition of circuits. An identifier is the name of a previously defined circuit, or the name of a builtin circuits, or `t`. 
  
List of builtin circuits:
  - `Exp`: exponential
  - `Sin`, `Cos`, `Tan`
  - `Arctan`
  - `Id`: identity (same as `t`)