# cJSON vendoring note

BasilEngine vendors cJSON version 1.7.19 from the official repository:
https://github.com/DaveGamble/cJSON/tree/v1.7.19

Included files:

- `cJSON.c` SHA-256: `298581A04A36C0165DA4B0AADE235C23088CB2FAA58651D720EA2F3706ED0B0D`
- `cJSON.h` SHA-256: `25B0145150D500498E4D209CEC69C18C42CF818BFFCC54690BE3B895A2A16DEE`
- `LICENSE` SHA-256: `A36DDA207C36DB5818729C54E7AD4E8B0C6FBA847491BA64F372C1A2037B6D5C`

The files are kept local so project creation and manifest parsing do not require
a package manager or network access.

