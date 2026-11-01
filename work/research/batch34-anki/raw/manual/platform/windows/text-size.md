> ## Documentation Index
> Fetch the complete documentation index at: https://docs.ankiweb.net/llms.txt
> Use this file to discover all available pages before exploring further.

# Text Size

If you find the text is the wrong size, there are two environment
variables you can try:

* ANKI\_NOHIGHDPI=1 will turn off some of Qt’s high dpi support

* ANKI\_WEBSCALE=1 will alter the scale of Anki’s web views (like the
  deck list, study screen, etc), while leaving interface elements like
  the menu bar alone. Replace 1 with the desired scale, such as 1.5 or
  0.75.

On Windows you can add these to a batch file to make it easier to start
Anki. For example, create a file called startanki.bat on your desktop
with the following text:

```text theme={null}
    set ANKI_WEBSCALE=0.75
    start "Anki" "%LocalAppData%\Programs\Anki\anki.exe"
```

After saving, you can double click on the file to start Anki with that
setting.


This documentation is built and hosted on [Mintlify](https://mintlify.com), a developer documentation platform.