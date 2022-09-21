# NodeMCU Board Connection Setup

## Config import
The config.h file attached to the NodeMCU folder contains global config for the NodeMCU in order to maintain the constant management more user-friendly.

To import any of this constants simply add the
```cpp
#include <config.h>
```
to the desired file an after that the corresponding contant must be imported by adding the *extern* expression before.

Example:
*In the config file*
```cpp
const char* config = 'Test'
```

*In the desired file*
```cpp
external const char* config
```