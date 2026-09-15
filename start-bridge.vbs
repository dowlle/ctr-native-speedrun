' Windowless: starts only the timer bridge. Launch the game through Steam as
' usual. The bridge waits for the surface and exits on its own once the game
' stops updating it.

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

dirPath = fso.GetParentFolderName(WScript.ScriptFullName)
shell.CurrentDirectory = dirPath

shell.Run "python """ & dirPath & "\speedrun-bridge.py"" --source surface --surface-file """ & dirPath & "\speedrun-surface.bin"" --events """ & dirPath & "\speedrun-events.log"" --idle-timeout 300", 0, False
