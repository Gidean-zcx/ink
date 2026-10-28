> ## Documentation Index
> Fetch the complete documentation index at: https://docs.ankiweb.net/llms.txt
> Use this file to discover all available pages before exploring further.

# Display Issues on Linux

> Display Issues

Hardware acceleration defaults to on. If you experience blank screens
or display issues, you can try enabling software rendering.

### Changing the Driver From the Preferences Screen

In Anki 23.10+, you can change the graphics driver from preferences screen by
navigating to **Tools → Preferences** and then selecting the driver from the
dropdown menu.

### Changing the Driver From the Terminal

```
echo software > ~/.local/share/Anki2/gldriver6
```

If you wish to switch back to the default, change `software` to `auto`, or
remove that file.


This documentation is built and hosted on [Mintlify](https://mintlify.com), a developer documentation platform.