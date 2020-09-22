conf-tool
=========

Utility for manipulating INI-style config files from the command line.
May be useful for scripting.

```console
Usage:
  conf-tool [OPTION...] FILE [KEY [VALUE]]

Tool for parsing config files.

Help Options:
  -h, --help        Show help options

Application Options:
  -s, --section     Config section
  -r, --remove      Remove the specified key or section
  -q, --quiet       Don't print errors
```
If no key is specified, lists the keys. If neither key or section
is specified, lists sections in the config file. If the key is
specified but the section is not, the first section is used, which
may be convenient if there's only one section in the config file.

Special file name `-` means standard input.

If standard input is modified, the modified version of the input
config is dumped to standard output. That can be used for piping.
Although it wouldn't be real piping because the whole thing is
first sucked in, then modified and only then sent to standard
output. Config files are usually quite small, though, so it
shouldn't really matter.
