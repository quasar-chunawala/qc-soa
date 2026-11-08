# Configuration Macros

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

