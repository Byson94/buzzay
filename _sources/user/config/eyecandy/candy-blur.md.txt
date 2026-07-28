# Candy Blur Options

All the options in `[candy.blur]` table. The blur effect is always enabled. It can be made visible by adjusting opacity.

## strength

Strength of the blur effect. Must be a floating number (a.k.a decimal number) and in between 0.0 and 1.0.

**Example:**

```toml
[candy.blur]
strength = 1.0
```


## alpha

Alpha of the blur effect. Must be a floating number (a.k.a decimal number) and in between 0.0 and 1.0 just like strength.

**Example:**

```toml
[candy.blur]
alpha = 1.0
```

## passes

The number of passes for the blur. Must be a itenger.

**Example:**

```toml
[candy.blur]
passes = 3
```

## noise

Noise to add to the blur. Can be used to give a glassy effect. Must be a floating number.

**Example:**

```toml
[candy.blur]
noise = 0.5
```
