# Context menu regression checks

Run from the repository root with a C++20 compiler:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic tests/context-menu-policy.test.cpp -o /tmp/npp-context-menu-test
/tmp/npp-context-menu-test
```

In a Visual Studio Developer Command Prompt:

```bat
cl /nologo /std:c++20 /EHsc /W4 /WX tests\context-menu-policy.test.cpp /Fe:%TEMP%\npp-context-menu-test.exe /Fo:%TEMP%\npp-context-menu-test.obj
%TEMP%\npp-context-menu-test.exe
```

These eight checks exercise the production routing policy. They do not build
the Windows DLL or verify Win32 message delivery, resource layout or INI I/O.

Before merging, build the x64 DLL using the repository's build path, then test
in an isolated Notepad++ host (both primary and secondary editing views):

| Case | Expected |
| --- | --- |
| Select text, ordinary right-click | Native Notepad++ menu |
| Select text, Ctrl + mouse right-click | AI menu |
| No selection, Ctrl + mouse right-click | Native menu |
| Select text, Shift+F10 / Menu key, with and without Ctrl | Native menu |
| Disable the AI context option, click OK | All context gestures stay native; Plugin commands still work |
| Reopen Settings and restart the host | Disabled value is retained |
| Change the option and click Cancel | Previously saved behavior is retained |
| Existing schema-1 INI without the new key / fresh configuration | Ctrl gesture enabled; ordinary right-click native |
| English and Traditional Chinese settings | Label visible, no overlapping controls; bottom buttons inside the dialog |
| Load and unload the plugin | No crash; native hooks restored |

Current change is source-only. Do not mark the host matrix passed or publish a
release based solely on the portable policy test.
