> ## Documentation Index
> Fetch the complete documentation index at: https://docs.ankiweb.net/llms.txt
> Use this file to discover all available pages before exploring further.

# Missing Libraries

If Anki fails to start, please run it from a terminal with `anki`. If it says a
library is missing, please install it and try again.

If it complains about no platform being available, please start Anki with the
following command line, which should reveal a missing library:

```shell theme={null}
QT_DEBUG_PLUGINS=1 anki
```

After installing the library with `apt-get` or similar, repeat the process. You
may need to do this a few times before all required libraries are installed.


This documentation is built and hosted on [Mintlify](https://mintlify.com), a developer documentation platform.