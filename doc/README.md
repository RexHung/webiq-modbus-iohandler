# WebIQ ioHandler SDK Reference (for internal development)

This folder contains **public header excerpts** and **minimal example sources** from the official
WebIQ ioHandler SDK, included only for interface reference and integration purposes in this repo.

- Source: WebIQ ioHandler SDK (Smart HMI GmbH / Beijer)
- Redistribution: Do **not** redistribute beyond this repository/team scope.
- Excluded: All binaries (.dll/.so/.lib/.exe), full PDFs/HTML docs, installers, tools.

See also:
- sdk_ref.md – our concise API table mapping to project code
- ioHandlerSDK/include – C API headers used by our implementation
- ioHandlerSDK/examples – minimal example code (text only)

``` bash
sdk/include/shmi/
├── shmi_cxx11_compat.h
├── variant_value.h
├── variant_value_compare.h
└── variant_value_helpers.h
```

These headers define the variant value system used by WebIQ's internal SHMI SDK.
Only the `variant_value*.h` files are included for type reference.
Compatibility headers (e.g., shmi_cxx11_compat.h) are omitted since C++14+ is required.