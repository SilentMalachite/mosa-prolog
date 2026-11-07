English | [日本語](../ja/02-install.md)

# 2. Getting and starting Mosa

In this chapter you download Mosa Prolog, unpack it, make your shell find it, and start and quit it.

## Before you start

Mosa Prolog 0.1.0 runs on these computers:

- macOS 14 or later on Apple Silicon.
- Linux on x86_64 with the glibc of Ubuntu 24.04 or later.
- Windows 10 22H2 or later and Windows 11, on x64, with Smart App Control off.

The packages are not signed. Step 3 (macOS) and step 5 (Windows) show how to start Mosa anyway. Windows 11 has a
setting called Smart App Control. When it is on, it blocks programs that are not signed, and it cannot be told to
let one program through. So Mosa does not run on a PC where Smart App Control is on, and such a PC is not
supported. You can see the setting in Windows Security, under App & browser control, then Smart App Control
settings.

`mosa` needs nothing else. Only `mosac` (chapter 13) needs a C++ compiler.

## Steps

1. Open the [Releases page](https://github.com/SilentMalachite/mosa-prolog/releases) of Mosa Prolog and download
   the package of release 0.1.0 for your computer.

| Computer | Package |
| --- | --- |
| macOS on Apple Silicon | `mosa-0.1.0-macos-arm64.tar.gz` |
| Linux on x86_64 | `mosa-0.1.0-linux-x86_64.tar.gz` |
| Windows on x64 | `mosa-0.1.0-windows-x86_64.zip` |

2. Unpack the package in your home folder. Each package holds one folder with the same name as the package, and
   the programs are in the `bin` folder inside it. The commands below expect the package in your Downloads folder.
   On macOS, open Terminal (press Command-Space, type Terminal and press Enter) and type these two lines. The first
   time Terminal reads your Downloads folder, macOS asks: "“Terminal” would like to access files in your Downloads
   folder." Click the button that allows it (OK or Allow), not Don't Allow, or the `tar` and `mv` lines below
   cannot read the folder. If you clicked Don't Allow, turn Downloads Folder on for Terminal in System Settings,
   under Privacy & Security, then Files and Folders.

<!-- skip: unpack-macos -->
```console
$ cd ~
$ tar -xzf ~/Downloads/mosa-0.1.0-macos-arm64.tar.gz
```

Safari may have unpacked the package for you, when its setting "Open 'safe' files after downloading" is on. If your
Downloads folder already holds the folder `mosa-0.1.0-macos-arm64`, do not run `tar`: move that folder to your home
folder instead, with `mv ~/Downloads/mosa-0.1.0-macos-arm64 ~`. Step 3 is still needed.

On Linux, open a terminal (on Ubuntu, press Ctrl-Alt-T) and type:

<!-- skip: unpack-linux -->
```console
$ cd ~
$ tar -xzf ~/Downloads/mosa-0.1.0-linux-x86_64.tar.gz
```

On Windows, open PowerShell (open Start, type PowerShell, and choose Windows PowerShell) and type this line.
PowerShell shows its own prompt (`PS C:\Users\you>`), so this manual writes PowerShell lines without one:

<!-- skip: unpack-windows -->
```powershell
Expand-Archive "$HOME\Downloads\mosa-0.1.0-windows-x86_64.zip" -DestinationPath $HOME
```

3. On macOS only, remove the quarantine mark. macOS puts this mark on files downloaded with a web browser, and
   `tar` and Safari copy it to every file they unpack. With the mark, macOS refuses to open a program that is not
   signed and says that it cannot verify it. If you have already tried to start `mosa` and such a dialog appeared,
   click Done or Cancel, whichever it shows, but not Move to Trash, which deletes the program. This command removes
   the mark from the folder (`-d`) and from everything inside it (`-r`):

<!-- skip: quarantine-macos -->
```console
$ xattr -dr com.apple.quarantine ~/mosa-0.1.0-macos-arm64
```

4. Add the `bin` folder to your PATH. PATH is the list of folders your shell looks in when you type the name of a
   command. On macOS, where the shell is zsh:

<!-- skip: path-macos -->
```console
$ echo 'export PATH="$HOME/mosa-0.1.0-macos-arm64/bin:$PATH"' >> ~/.zshrc
```

On Linux, where the shell is usually bash:

<!-- skip: path-linux -->
```console
$ echo 'export PATH="$HOME/mosa-0.1.0-linux-x86_64/bin:$PATH"' >> ~/.bashrc
```

On Windows, type these three lines in PowerShell. Type each line whole, the `$` at its start included: in
PowerShell, `$bin` and `$old` are names that hold a value, not a prompt.

<!-- skip: path-windows -->
```powershell
$bin = "$HOME\mosa-0.1.0-windows-x86_64\bin"
$old = [Environment]::GetEnvironmentVariable("Path", "User")
[Environment]::SetEnvironmentVariable("Path", "$old;$bin", "User")
```

On Windows you can also do this in a dialog: open Start, search for "environment variables", choose "Edit
environment variables for your account", select Path, click Edit, then New, and type the path of the `bin`
folder, `C:\Users\<name>\mosa-0.1.0-windows-x86_64\bin`, with your user name for `<name>`. Then click OK in that
dialog and OK in the Environment Variables dialog: closing them with the X discards the change. On every system,
the new PATH applies only to windows opened afterwards, so close the terminal or PowerShell window and open a new
one.

5. Check that your shell finds Mosa. Type `mosa --version`:

<!-- check: version -->
```console
$ mosa --version
mosa 0.1.0
```

If the shell says that it cannot find `mosa`, compare the folder name in step 4 with the folder that step 2 made,
and open a new window. On Windows, if a blue window titled "Windows protected your PC" (Microsoft Defender
SmartScreen) appears when you start `mosa`, click More info, then Run anyway.

6. Start Mosa by typing `mosa` alone. It shows the prompt `?- ` and waits. Type `halt.` and press Enter to quit:

<!-- check: start-quit -->
```console
$ mosa
?- halt.
```

## What happened

You unpacked a folder that holds `mosa`, `mosac` and `mosatest` in its `bin` folder, next to the libraries that
they use. Keep the folder together: `mosa` looks for its libraries beside itself, so it cannot find them if you move
it out of the folder. That is why step 4 adds the folder to PATH instead of copying the program somewhere else.

`mosa --version` printed the version and quit. `mosa` alone started an interactive session: it showed `?- ` and
waited for a query. `halt.` is a query that ends the session. Every query ends with a period, and Mosa reads it when
you press Enter after the period.

You can also end the session by ending its input: press Ctrl-D on macOS and Linux, or Ctrl-Z then Enter on Windows,
at the start of a line. After Ctrl-D, Mosa ends without printing a newline. In bash (common on Linux) your shell's
prompt appears right after `?- `. In zsh (the macOS default) a highlighted `%` appears there, and the prompt starts
on the next line.

## Exercises

1. Start `mosa`, type `halt` without the period, and press Enter. What happens, and how do you finish the query?
2. Start `mosa` again and quit it without typing `halt.`.

### Answers

1. Nothing seems to happen: Mosa waits, because a query is not finished until its period. Type `.` and press Enter,
   and Mosa quits:

<!-- check: no-period
halt
.
-->
```console
$ mosa
?- halt
.
```

2. Press Ctrl-D (macOS, Linux) or Ctrl-Z then Enter (Windows). You are back in your shell, as after `halt.`.

---

[Index](README.md) · Previous: [1. Introduction](01-introduction.md) · Next:
[3. Your first query](03-first-query.md)
