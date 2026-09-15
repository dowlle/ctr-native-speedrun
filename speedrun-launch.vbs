' Windowless launcher: starts the timer bridge hidden, then the client.
' Use this as the Steam target if you do not want a console window. The bridge
' exits on its own once the client stops updating the surface.

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

dirPath = fso.GetParentFolderName(WScript.ScriptFullName)
shell.CurrentDirectory = dirPath

shell.Run "python """ & dirPath & "\speedrun-bridge.py"" --source surface --surface-file """ & dirPath & "\speedrun-surface.bin"" --events """ & dirPath & "\speedrun-events.log"" --idle-timeout 20", 0, False
shell.Run """" & dirPath & "\ctr_native.exe""", 1, True
