# Configuration Macros

## Contracts

A contract is a formal interface specification for a software component such as a class or member-function. It is a set of conditions that express expectations on how the component interoperates with other components of the program.

A contract violation occurs when a condition that is a part of the contract does not hold.

A pre-condition is part of the function contract where the responsibility of satisfying it rests with the caller. Generally, these requirements are placed on the arguments passed to the function upon entry into the function.

A postcondition is a part of the function contract where the responsibilty of satisfying the condition rests with the callee; the implementer of the function itself. These are generally conditions that will hold true regarding the return value of the function when it completes execution normally.

An invariant is a condition on the state of an object, or a set of objects, that is maintained over a certain amount of time. 

A class invariant is a condition that a class type maintains throughout the lifetime of an object. There are other kinds of invariants such as loop invariants. Often these are expected to hold on the entry or exit of functions

libsoa provides several hardening modes, where each mode enables a set of assertions that prevent UB caused by contract violations of the library. Different hardening modes make different tradeoffs between checking and runtime performance.

If the user does not configure the hardening level, default to `HARDENING_MODE_NONE`

## Hardening and Contract Checks

Runtime precondition checks (null pointers, out-of-bounds indices, invalid iterator ranges) can be configured globally:

| Macro | Options / Default | Description |
|---|---|---|
| `HARDENING_MODE` | `HARDENING_MODE_NONE` (Default)<br>`HARDENING_MODE_FAST`<br>`HARDENING_MODE_EXTENSIVE`<br>`HARDENING_MODE_DEBUG` | Enables precondition checks across all API boundaries. |
| `ASSERT_MODE` | `ASSERT_MODE_LOG_TRAP`(Default)<br>`ASSERT_MODE_NONE`<br>`ASSERT_MODE_TRAP`<br>`ASSERT_MODE_ABORT`<br>`ASSERT_MODE_LOG_ABORT` | Defines failure action when a hardening assertion triggers. |

## Example

```cpp
#define HARDENING_MODE HARDENING_MODE_DEBUG
#define ASSERT_MODE ASSERT_MODE_LOG_TRAP
#include <soa/vector.h>
```

## Description

- `HARDENING_MODE_NONE`. When hardening mode is turned off, all checks compile away entirely with zero runtime overhead.
- `HARDENING_MODE_FAST`. A set of securitiy critical checks that can be done with relatively little overhead in constant time and intended to be used in production. 
- `HARDENING_MODE_EXTENSIVE`. Contains all the checks from the fast mode and some additional checks that incur relatively little overhead but aren't security critical. 
- `DEBUG_MODE`. This mode should be used in non-production environments(such as test-suites, CI or local development).

