> ## Documentation Index
> Fetch the complete documentation index at: https://docs.ankiweb.net/llms.txt
> Use this file to discover all available pages before exploring further.

# Anki not picking up GTK theme on Gnome/Linux

> Incorrect GTK Theme

You can work around this issue by explicitly telling Anki what the GTK theme is. Run the following commands in a terminal:

```shell theme={null}
theme=$(gsettings get org.gnome.desktop.interface gtk-theme)
echo "gtk-theme-name=$theme" >> ~/.gtkrc-2.0
echo "export GTK2_RC_FILES=$HOME/.gtkrc-2.0" >> ~/.profile
```

Then log out and log back into your computer, and Anki should pick up the GTK theme.


This documentation is built and hosted on [Mintlify](https://mintlify.com), a developer documentation platform.