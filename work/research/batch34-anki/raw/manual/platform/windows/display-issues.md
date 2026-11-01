> ## Documentation Index
> Fetch the complete documentation index at: https://docs.ankiweb.net/llms.txt
> Use this file to discover all available pages before exploring further.

# Windows Display Issues

> Display Issues

On Windows, there are three ways of content being displayed on screen. The
default is *software*, which is slower, but the most compatible. There are two
other options that are faster: *OpenGL* and *ANGLE*. They are faster, but they
may not work, or may cause display issues such as missing menubars, blank
windows, and so on. Which one works best will depend on your computer.

## Changing the Driver From the Preferences Screen

In Anki 23.10+, you can change the graphics driver from preferences screen by
navigating to **Tools → Preferences** and then selecting the driver from the
dropdown menu.

## Changing the Driver From the Command Line

If you run into display issues, you can try switching to software mode via cmd:

```bat theme={null}
echo software > %APPDATA%\Anki2\gldriver6
```

Or you can do it via PowerShell:

```powershell theme={null}
echo software > $env:APPDATA\Anki2\gldriver6
```

It will not print anything. You can then start Anki again.

To revert to the default behaviour, change `software` to `auto`, or delete that file.

## Full Screen

Anki 2.1.50+ comes with a full screen mode, but due to various issues, it had to
be disabled while `OpenGL` is used. Turning on software rendering as described
above will allow the full screen option to be used, though please bear in mind
that rendering performance may suffer.

In Anki 23.10+, full screen mode is supported with the default Direct3D driver.


This documentation is built and hosted on [Mintlify](https://mintlify.com), a developer documentation platform.