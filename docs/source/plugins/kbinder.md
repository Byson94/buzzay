# Kbinder

Kbinder is a keybinding plugin that makes adding keybindings to buzzay easy.

## Loading and Messaging

Starting off with the most basic thing, loading and messaging kbinder.

Loading is simple. Just load it with `--load kbinder`.

```bash
$ buzzay --load kbinder
```

When coming to messaging, kbinder takes two arguments.

```bash 
$ buzzay --msg kbinder "<arg1>" "<arg2>"
```

- `<arg1>` Keybinding to bind to.
- `<arg2>` Command to run when keybinding is triggered.

## Keybinding Syntax

Unlike `<arg2>` which just takes a command to run, `<arg1>` requires a keybinding in a specific format.
That being the `Modifier+Key` syntax.

### Modifiers

- `Super` The Mod key (or the windows key).
- `Shift` Shift key.
- `Ctrl` Control Key.

Modifier keys can also be merged like so:

```
Super+Shift+Ctrl
```

### Keys

All the keys supported in kbinder:

**Numbers:**

- `1` One
- `2` Two
- `3` Three
- `4` Four
- `5` Five
- `6` Six
- `7` Seven
- `8` Eight
- `9` Nine
- `0` Zero

**Alphabet:**

- `A` Alphabet "A"
- `B` Alphabet "B"
- `C` Alphabet "C"
- `D` Alphabet "D"
- `E` Alphabet "E"
- `F` Alphabet "F"
- `G` Alphabet "G"
- `H` Alphabet "H"
- `I` Alphabet "I"
- `J` Alphabet "J"
- `K` Alphabet "K"
- `L` Alphabet "L"
- `M` Alphabet "M"
- `N` Alphabet "N"
- `O` Alphabet "O"
- `P` Alphabet "P"
- `Q` Alphabet "Q"
- `R` Alphabet "R"
- `S` Alphabet "S"
- `T` Alphabet "T"
- `U` Alphabet "U"
- `V` Alphabet "V"
- `W` Alphabet "W"
- `X` Alphabet "X"
- `Y` Alphabet "Y"
- `Z` Alphabet "Z"
