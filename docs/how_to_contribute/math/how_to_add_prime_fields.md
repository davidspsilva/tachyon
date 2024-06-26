# How to add prime fields

Follow this guide to add a new prime field for Tachyon.

_Note_: We have our own development conventions. Please read the [conventions doc](/docs/how_to_contribute/conventions.md) before contributing.

## 1. Add a `BUILD.bazel` file

Begin by creating a directory named `<new_prime_field>_prime` in `/tachyon/math/finite_field/`. Add a `BUILD.bazel` file into this directory. Note that once parameters are added to `BUILD.bazel`, Bazel will automatically generate the prime field code based on these parameters when it builds the target.

### Prime field generator

Choose the [prime field generator](/tachyon/math/finite_fields/generator/prime_field_generator/build_defs.bzl) depending on the prime field type. The list of required properties for each prime field generator is included below. Note that each successive field type in this list additionally requires all properties mentioned in previous generator types.

1. **`generate_prime_fields()`**:
    - `name`: The name of the prime field.
    - `namespace`: The selected namespace, commonly set as `tachyon::math`.
    - `class_name`: The name of the class, usually the same as the name of the prime field (in PascalCase).
    - `modulus`: The modulus value of the prime field.
2. **`generate_fft_prime_fields()`**:
    - `subgroup_generator`: The value used to generate elements of a subgroup within the prime field, facilitating Fast Fourier Transform (FFT) operations.
      - _Note_: Every prime can be expressed in the following form: $p = 2^s * T + 1$, where $s$ and $T$ are integers, and $T$ is odd. According to Fermat's little Theorem, $a^{p-1} = 1 \mod p$ holds for any element $a$ in $F_p$. That is to say, $a^{2^s * T} = 1 \mod p$. A subgroup generator $g$ satisfies $(g^{2^{s-k} * T})^{2^k} = 1 \mod p$ for some $k \le s$, where $2^k = n$, making $\omega = g^{2^{s-k} * T}$ a $n$-th root of unity.
3. **`generate_large_fft_prime_fields()`**:
    - `small_subgroup_base`: Refers to the base element $b$ of a small subgroup.
    - `small_subgroup_adicity`: Refers to the largest adicity $a$ (the exponent of the base) of a small subgroup such that $b^a$ is a divisor of $T$.
      - _Note_: Large FFT prime fields can construct a larger domain than FFT prime fields through a small subgroup. Given `small_subgroup_base` = $b$ and `small_subgroup_adicity` = $a$, if $b^a$ is a divisor of $T$ and $g^{2^s * T} = 1 \mod p$, then $(g^{T/b^a})^{2^s*b^a} = 1 \mod p$. This manipulation enables a domain size up to $b^a$ times larger than the maximum domain size of $2^s$ that general FFT prime fields can create. Additionally, the $n$-th root of unity is now $\omega = g^{2^{s-k} * T/b^{a-q}}$, where $n = 2^k * b^{a-q} (k \le s, q \le a)$ since $\omega$ satisfies $\omega^{2^k * b^{a-q}} = (g^{2^{s-k} * T/b^{a-q}})^{2^k * b^{a-q}} = 1 \mod p$.

For instance, to implement a FFT prime field, create a directory (`/tachyon/math/finite_fields/<new_prime_field>`) and add a `BUILD.bazel` file as shown below:

```bazel
# /tachyon/math/finite_fields/<new_prime_field>/BUILD.bazel

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("//bazel:tachyon_cc.bzl", "tachyon_cc_library")
load(
    "//tachyon/math/finite_fields/generator/prime_field_generator:build_defs.bzl",
    "SUBGROUP_GENERATOR",
    "generate_fft_prime_fields", # NOTE: Choose generator type
)

package(default_visibility = ["//visibility:public"])

string_flag(
    name = SUBGROUP_GENERATOR,
    build_setting_default = "{subgroup_generator}", # input Subgroup generator value
)

generate_fft_prime_fields( # NOTE: Choose generator type
    name = "new_prime_field",
    namespace = "tachyon::math",
    class_name = "NewPrimeField",
    modulus = "{modulus_value}", # input modulus value
    subgroup_generator = ":" + SUBGROUP_GENERATOR,
)
```

Use the following files for reference:

- [Goldilocks `BUILD.bazel`](/tachyon/math/finite_fields/goldilocks/BUILD.bazel)
- [Mersenne31 `BUILD.bazel`](/tachyon/math/finite_fields/mersenne31/BUILD.bazel)

## 2. Add to `prime_field_generator_unittest.cc`

Finally, ensure the prime field works well by incorporating it into `prime_field_generator_unittest.cc` at the two points shown below:

```cpp
...
#include "tachyon/math/finite_fields/goldilocks/goldilocks_prime_field.h"
#include "tachyon/math/finite_fields/mersenne31/mersenne31.h"
// ADD NEW PRIME FIELD HEADER FILE HERE
// #include "tachyon/math/finite_fields/..."
...
// ADD NEW PRIME FIELD NAME HERE
using PrimeFieldTypes =
    testing::Types<bls12_381::Fq, bls12_381::Fr, bn254::Fq, bn254::Fr,
                   secp256k1::Fr, secp256k1::Fq, Goldilocks, Mersenne31 /*, NEW PRIME FIELD*/>;
...
```
