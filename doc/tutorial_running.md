## Launching

Launch Empirical from the command line to get the REPL.

```
$ path/to/empirical
Empirical version 0.1.0
Copyright (C) 2019--2020 Empirical Software Solutions, LLC

>>>

```

Alternatively, include a file name to run it.

```
$ path/to/empirical file_to_run.emp

```

There are some advanced options on the command line to see the internal state of the compiler. Get the `--help` for the full list.

```
$ path/to/empirical --help

```

### Magic commands

The REPL has some magic commands that may make development easier. For example, we can time an expression:

```
>>> \t let p = load$("prices.csv")

 1ms

```

We can enter multiline statements (pasting is currently not permitted, unfortunately):

```
>>> \multiline
# Entering multiline mode (Ctrl-D to exit)
func add(x: Int64, y: Int64):
  return x + y
end
^D

>>> add(2, 3)
5

```

To see all available magic commands, just ask for `\help`.

```
>>> \help

```
